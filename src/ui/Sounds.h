#pragma once

#include <QMediaDevices>
#include <QObject>
#include <QSoundEffect>
#include <QStringList>
#include <QTimer>
#include <memory>

// Sonido de progreso (fin de pomodoro), de logro y alarma de fin de recreo.
// La alarma suena en loop y sube de volumen hasta que se corta.
// M27: cada uno tiene alternativas (Ajustes › Cronómetro › Sonidos) y hay volumen general.
//
// Mejoras 2.0: el reproductor de Qt queda atado a la salida de audio con la que
// se armó. Si Windows cambia la salida (auriculares, Bluetooth, otra app que la
// cambia, la PC que se suspende), seguía mandando el sonido a la vieja sin
// avisar. Ahora se rearma con la salida actual cuando cambia, cuando falla y
// si pasó mucho rato sin sonar; y cada vez que suena (o falla) queda anotado
// en sonidos.log para poder validarlo.
class Sounds : public QObject {
    Q_OBJECT
public:
    explicit Sounds(QObject* parent = nullptr);

    // Nombres para mostrar y archivo de cada alternativa ("" = sin sonido).
    static QStringList chimeNames();
    static QStringList achievementNames();
    static QStringList alarmNames();

    // índices de las listas de arriba; volumen 0..100
    void configure(int chime, int achievement, int alarm, int volume);

    void playChime();
    void playAchievement();   // M8: logro (objetivo, hito)
    void startAlarm();
    void stopAlarm();
    bool alarmActive() const { return (m_alarm && m_alarm->isPlaying()) || m_ramp.isActive(); }

    // Para probar desde Ajustes (la alarma suena unos segundos). Después avisa
    // con previewResult si salió y por qué salida.
    void preview(int which, int index, int volume);

    QString outputName() const;
    void setLogFile(const QString& path) { m_logPath = path; }

signals:
    void previewResult(bool ok, const QString& detail);

private:
    void rebuild(const char* why);
    void ensureFresh();
    void playFx(QSoundEffect* fx, bool on, const char* what);
    void check(QSoundEffect* fx, const char* what);
    void log(const QString& line) const;
    std::unique_ptr<QSoundEffect> make(const QString& file);

    std::unique_ptr<QSoundEffect> m_chime, m_achievement, m_alarm, m_preview;
    QMediaDevices m_devices;
    QByteArray    m_deviceId;
    qint64        m_lastUse = 0;
    QString       m_logPath;
    QString       m_chimeFile, m_achievementFile, m_alarmFile;
    QTimer        m_ramp;
    QTimer        m_previewStop;
    float         m_volume = 1.0f;
    bool          m_chimeOn = true, m_achievementOn = true;
};
