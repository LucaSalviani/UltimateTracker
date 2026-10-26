#include "ui/Sounds.h"

#include <QUrl>

Sounds::Sounds(QObject* parent)
    : QObject(parent)
{
    m_chime.setSource(QUrl(QStringLiteral("qrc:/sounds/chime.wav")));
    m_chime.setVolume(0.8f);

    m_alarm.setSource(QUrl(QStringLiteral("qrc:/sounds/alarm.wav")));
    m_alarm.setLoopCount(QSoundEffect::Infinite);

    // Arranca moderada y en ~20 s llega al máximo: si te dormiste, te despierta.
    m_ramp.setInterval(2000);
    connect(&m_ramp, &QTimer::timeout, this, [this] {
        const float v = qMin(1.0f, m_alarm.volume() + 0.05f);
        m_alarm.setVolume(v);
        if (v >= 1.0f) m_ramp.stop();
    });
}

void Sounds::playChime()
{
    m_chime.play();
}

void Sounds::startAlarm()
{
    m_alarm.setVolume(0.5f);
    m_alarm.play();
    m_ramp.start();
}

void Sounds::stopAlarm()
{
    m_ramp.stop();
    m_alarm.stop();
}
