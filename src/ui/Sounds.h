#pragma once

#include <QObject>
#include <QSoundEffect>
#include <QTimer>

// Sonido de progreso (fin de pomodoro) y alarma de fin de recreo.
// La alarma suena en loop y sube de volumen hasta que se corta.
class Sounds : public QObject {
    Q_OBJECT
public:
    explicit Sounds(QObject* parent = nullptr);

    void playChime();
    void startAlarm();
    void stopAlarm();
    bool alarmActive() const { return m_alarm.isPlaying() || m_ramp.isActive(); }

private:
    QSoundEffect m_chime;
    QSoundEffect m_alarm;
    QTimer       m_ramp;
};
