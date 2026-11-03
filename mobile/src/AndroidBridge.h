#pragma once

#include <QString>

// Lo propio de Android: notificación con el tiempo que queda (servicio en primer
// plano, así el sistema no corta la app con la pantalla apagada) y una alarma
// exacta que suena al terminar aunque el celu esté bloqueado. En la PC no hace nada.
class AndroidBridge {
public:
    AndroidBridge();
    void requestPermissions();
    // Muestra/actualiza la notificación. endEpochMs = cuándo termina (0 = en pausa).
    void showRunning(const QString& title, const QString& text, qint64 endEpochMs, bool isBreak);
    void scheduleEnd(qint64 atEpochMs, const QString& title, const QString& text);
    void cancelEnd();
    void stopRunning();
    void vibrate(int ms);
};
