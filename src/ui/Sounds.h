#pragma once

#include <QObject>
#include <QSoundEffect>
#include <QStringList>
#include <QTimer>

// Sonido de progreso (fin de pomodoro), de logro y alarma de fin de recreo.
// La alarma suena en loop y sube de volumen hasta que se corta.
// M27: cada uno tiene alternativas (Ajustes › Cronómetro › Sonidos) y hay volumen general.
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
    bool alarmActive() const { return m_alarm.isPlaying() || m_ramp.isActive(); }

    // Para probar desde Ajustes (la alarma suena unos segundos).
    void preview(int which, int index, int volume);

private:
    QSoundEffect m_chime;
    QSoundEffect m_achievement;
    QSoundEffect m_alarm;
    QSoundEffect m_preview;
    QTimer       m_ramp;
    QTimer       m_previewStop;
    float        m_volume = 1.0f;
    bool         m_chimeOn = true, m_achievementOn = true;
};
