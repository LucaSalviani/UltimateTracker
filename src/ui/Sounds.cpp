#include "ui/Sounds.h"

#include <QUrl>

namespace {
const char* kChimeFiles[]       = {"chime.wav", "chime_soft.wav", "chime_bell.wav", "chime_wood.wav", "chime_digital.wav", ""};
const char* kAchievementFiles[] = {"achievement.wav", "achievement_soft.wav", ""};
const char* kAlarmFiles[]       = {"alarm.wav", "alarm_soft.wav", "alarm_bells.wav"};

QUrl url(const char* file) { return QUrl(QStringLiteral("qrc:/sounds/") + QString::fromLatin1(file)); }
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
    m_alarm.setLoopCount(QSoundEffect::Infinite);

    // Arranca moderada y en ~20 s llega al máximo: si te dormiste, te despierta.
    m_ramp.setInterval(2000);
    connect(&m_ramp, &QTimer::timeout, this, [this] {
        const float v = qMin(m_volume, m_alarm.volume() + 0.05f * m_volume);
        m_alarm.setVolume(v);
        if (v >= m_volume) m_ramp.stop();
    });
    m_previewStop.setSingleShot(true);
    connect(&m_previewStop, &QTimer::timeout, &m_preview, &QSoundEffect::stop);
}

void Sounds::configure(int chime, int achievement, int alarm, int volume)
{
    m_volume = qBound(0, volume, 100) / 100.0f;
    chime = qBound(0, chime, int(chimeNames().size()) - 1);
    achievement = qBound(0, achievement, int(achievementNames().size()) - 1);
    alarm = qBound(0, alarm, int(alarmNames().size()) - 1);
    m_chimeOn = *kChimeFiles[chime] != 0;
    m_achievementOn = *kAchievementFiles[achievement] != 0;
    if (m_chimeOn) m_chime.setSource(url(kChimeFiles[chime]));
    if (m_achievementOn) m_achievement.setSource(url(kAchievementFiles[achievement]));
    m_alarm.setSource(url(kAlarmFiles[alarm]));
    m_chime.setVolume(0.8f * m_volume);
    m_achievement.setVolume(0.75f * m_volume);
}

void Sounds::playChime()
{
    if (m_chimeOn) m_chime.play();
}

void Sounds::playAchievement()
{
    if (m_achievementOn) m_achievement.play();
}

void Sounds::startAlarm()
{
    m_alarm.setVolume(0.5f * m_volume);
    m_alarm.play();
    m_ramp.start();
}

void Sounds::stopAlarm()
{
    m_ramp.stop();
    m_alarm.stop();
}

void Sounds::preview(int which, int index, int volume)
{
    const char* file = which == 0 ? kChimeFiles[qBound(0, index, int(chimeNames().size()) - 1)]
                     : which == 1 ? kAchievementFiles[qBound(0, index, int(achievementNames().size()) - 1)]
                                  : kAlarmFiles[qBound(0, index, int(alarmNames().size()) - 1)];
    m_preview.stop();
    if (!*file) return;
    m_preview.setSource(url(file));
    m_preview.setVolume(qBound(0, volume, 100) / 100.0f * 0.8f);
    m_preview.setLoopCount(which == 2 ? 3 : 1);
    m_preview.play();
    m_previewStop.start(which == 2 ? 3500 : 2500);
}
