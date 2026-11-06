#include "ui/Sounds.h"

#include <QAudioDevice>
#include <QDateTime>
#include <QFile>
#include <QTextStream>
#include <QUrl>
#include <string_view>

namespace {
const char* kChimeFiles[]       = {"chime.wav", "chime_soft.wav", "chime_bell.wav", "chime_wood.wav", "chime_digital.wav", ""};
const char* kAchievementFiles[] = {"achievement.wav", "achievement_soft.wav", ""};
const char* kAlarmFiles[]       = {"alarm.wav", "alarm_soft.wav", "alarm_bells.wav"};

QUrl url(const QString& file) { return QUrl(QStringLiteral("qrc:/sounds/") + file); }

// Si pasó más que esto sin sonar, se rearma antes (la salida pudo dormirse).
constexpr qint64 kStaleMs = 10 * 60 * 1000;

const char* statusName(QSoundEffect::Status s)
{
    switch (s) {
    case QSoundEffect::Null:    return "sin archivo";
    case QSoundEffect::Loading: return "cargando";
    case QSoundEffect::Ready:   return "listo";
    case QSoundEffect::Error:   return "error";
    }
    return "?";
}
} // namespace

QStringList Sounds::chimeNames()
{
    return {QStringLiteral("Arpegio (el de siempre)"), QStringLiteral("Suave"), QStringLiteral("Campana"),
            QStringLiteral("Madera"), QStringLiteral("Digital"), QStringLiteral("Sin sonido")};
}
QStringList Sounds::achievementNames()
{
    return {QStringLiteral("Obturador y arpegio (el de siempre)"), QStringLiteral("Acorde suave"), QStringLiteral("Sin sonido")};
}
QStringList Sounds::alarmNames()
{
    return {QStringLiteral("Despertador (el de siempre)"), QStringLiteral("Marimba"), QStringLiteral("Campanitas")};
}

Sounds::Sounds(QObject* parent)
    : QObject(parent)
{
    configure(0, 0, 0, 100);

    // Arranca moderada y en ~20 s llega al máximo: si te dormiste, te despierta.
    m_ramp.setInterval(2000);
    connect(&m_ramp, &QTimer::timeout, this, [this] {
        if (!m_alarm) { m_ramp.stop(); return; }
        const float v = qMin(m_volume, m_alarm->volume() + 0.05f * m_volume);
        m_alarm->setVolume(v);
        if (v >= m_volume) m_ramp.stop();
    });
    m_previewStop.setSingleShot(true);
    connect(&m_previewStop, &QTimer::timeout, this, [this] { if (m_preview) m_preview->stop(); });

    // Cambió la salida de audio de Windows: se rearma con la nueva.
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        if (QMediaDevices::defaultAudioOutput().id() != m_deviceId) rebuild("cambió la salida de audio");
    });
}

std::unique_ptr<QSoundEffect> Sounds::make(const QString& file)
{
    auto fx = std::make_unique<QSoundEffect>(QMediaDevices::defaultAudioOutput());
    if (!file.isEmpty()) fx->setSource(url(file));
    return fx;
}

void Sounds::rebuild(const char* why)
{
    const bool alarmWasOn = m_alarm && m_alarm->isPlaying();
    m_deviceId = QMediaDevices::defaultAudioOutput().id();
    m_chime = make(m_chimeFile);
    m_achievement = make(m_achievementFile);
    m_alarm = make(m_alarmFile);
    m_alarm->setLoopCount(QSoundEffect::Infinite);
    m_preview = make(QString());
    m_chime->setVolume(0.8f * m_volume);
    m_achievement->setVolume(0.75f * m_volume);
    m_lastUse = QDateTime::currentMSecsSinceEpoch();
    if (why) log(QStringLiteral("rearmado (%1) · salida: %2").arg(QString::fromUtf8(why), outputName()));
    if (alarmWasOn) startAlarm();
}

void Sounds::ensureFresh()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_chime || QMediaDevices::defaultAudioOutput().id() != m_deviceId)
        rebuild("la salida no es la misma");
    else if (m_chime->status() == QSoundEffect::Error || m_alarm->status() == QSoundEffect::Error)
        rebuild("el reproductor tenía un error");
    else if (now - m_lastUse > kStaleMs)
        rebuild(nullptr);   // mucho rato sin sonar: por las dudas, en silencio
    m_lastUse = now;
}

void Sounds::configure(int chime, int achievement, int alarm, int volume)
{
    m_volume = qBound(0, volume, 100) / 100.0f;
    chime = qBound(0, chime, int(chimeNames().size()) - 1);
    achievement = qBound(0, achievement, int(achievementNames().size()) - 1);
    alarm = qBound(0, alarm, int(alarmNames().size()) - 1);
    m_chimeOn = *kChimeFiles[chime] != 0;
    m_achievementOn = *kAchievementFiles[achievement] != 0;
    m_chimeFile = QString::fromLatin1(kChimeFiles[chime]);
    m_achievementFile = QString::fromLatin1(kAchievementFiles[achievement]);
    m_alarmFile = QString::fromLatin1(kAlarmFiles[alarm]);
    rebuild(nullptr);
}

QString Sounds::outputName() const
{
    const QAudioDevice d = QMediaDevices::defaultAudioOutput();
    return d.isNull() ? QStringLiteral("ninguna (no hay salida de audio)") : d.description();
}

void Sounds::log(const QString& line) const
{
    if (m_logPath.isEmpty()) return;
    QFile f(m_logPath);
    if (f.size() > 200 * 1024) f.remove();   // que no crezca para siempre
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream(&f) << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) << "  " << line << "\n";
}

// Un ratito después de pedirle que suene, se fija si de verdad sonó; si no, se
// rearma y se reintenta una vez.
void Sounds::check(QSoundEffect* fx, const char* what)
{
    QTimer::singleShot(700, this, [this, fx, what] {
        QSoundEffect* cur = std::string_view(what) == "fin de pomodoro" ? m_chime.get()
                          : std::string_view(what) == "logro" ? m_achievement.get()
                          : std::string_view(what) == "alarma" ? m_alarm.get() : nullptr;
        if (cur != fx) return;   // ya se rearmó por otro lado
        const bool ok = !QMediaDevices::defaultAudioOutput().isNull() && (fx->status() == QSoundEffect::Ready || fx->isPlaying());
        log(QStringLiteral("%1: %2 · salida: %3").arg(QString::fromLatin1(what), ok ? QStringLiteral("sonó") : QStringLiteral("falló (%1)").arg(QString::fromLatin1(statusName(fx->status()))), outputName()));
        if (!ok && !QMediaDevices::defaultAudioOutput().isNull()) {
            rebuild("no sonó");
            QSoundEffect* again = std::string_view(what) == "fin de pomodoro" ? m_chime.get()
                                : std::string_view(what) == "logro" ? m_achievement.get() : m_alarm.get();
            if (std::string_view(what) == "alarma") { again->setVolume(0.5f * m_volume); m_ramp.start(); }
            again->play();
        }
    });
}

void Sounds::playFx(QSoundEffect* fx, bool on, const char* what)
{
    if (!on) return;
    fx->play();
    check(fx, what);
}

void Sounds::playChime()
{
    ensureFresh();
    playFx(m_chime.get(), m_chimeOn, "fin de pomodoro");
}

void Sounds::playAchievement()
{
    ensureFresh();
    playFx(m_achievement.get(), m_achievementOn, "logro");
}

void Sounds::startAlarm()
{
    ensureFresh();
    m_alarm->setVolume(0.5f * m_volume);
    m_ramp.start();
    playFx(m_alarm.get(), true, "alarma");
}

void Sounds::stopAlarm()
{
    m_ramp.stop();
    if (m_alarm) m_alarm->stop();
}

void Sounds::preview(int which, int index, int volume)
{
    const char* file = which == 0 ? kChimeFiles[qBound(0, index, int(chimeNames().size()) - 1)]
                     : which == 1 ? kAchievementFiles[qBound(0, index, int(achievementNames().size()) - 1)]
                                  : kAlarmFiles[qBound(0, index, int(alarmNames().size()) - 1)];
    ensureFresh();
    m_preview->stop();
    if (!*file) return;
    m_preview->setSource(url(QString::fromLatin1(file)));
    m_preview->setVolume(qBound(0, volume, 100) / 100.0f * 0.8f);
    m_preview->setLoopCount(which == 2 ? 3 : 1);
    m_preview->play();
    m_previewStop.start(which == 2 ? 3500 : 2500);
    QSoundEffect* fx = m_preview.get();
    QTimer::singleShot(900, this, [this, fx] {
        if (fx != m_preview.get()) return;
        const bool ok = !QMediaDevices::defaultAudioOutput().isNull() && (fx->status() == QSoundEffect::Ready || fx->isPlaying());
        const QString detail = ok ? QStringLiteral("Sonando por: %1").arg(outputName())
                                  : QStringLiteral("No se pudo reproducir (%1) · salida: %2").arg(QString::fromLatin1(statusName(fx->status())), outputName());
        log(QStringLiteral("prueba: %1").arg(detail));
        emit previewResult(ok, detail);
    });
}
