#pragma once

#include <QString>
#include <functional>

// M20: modo desarrollador / pruebas.
// Con el modo activo la app trabaja sobre una COPIA de tu base (se rehace en
// cada arranque desde la real), así nada de lo que pruebes queda registrado.
// El interruptor vive en un archivo aparte (no en la base) para poder apagarlo
// desde la copia.
namespace devmode {

bool enabledFlag(const QString& dataDir);           // ¿está prendido? (archivo "devmode")
void setEnabledFlag(const QString& dataDir, bool on);
bool active();                                       // ¿esta sesión corre sobre la copia?
void setActive(bool on);

// Prepara la copia de prueba a partir de la base real. Devuelve su ruta (o vacío).
QString prepareSandbox(const QString& realDbPath, const QString& dataDir);

// Reloj acelerado (×1, ×10, ×60) para no esperar 25 minutos.
double speed();
void   setSpeed(double s);
std::function<qint64()> clock();
// Adelanta el reloj de golpe ("completar ya").
void jump(qint64 ms);

// Reinicia la app (para entrar o salir del modo).
void restartApp();

} // namespace devmode
