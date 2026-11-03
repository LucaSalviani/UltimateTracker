#pragma once

#include <QAbstractNativeEventFilter>
#include <QObject>
#include <QString>

class Database;

// M9: integración con Windows. En otras plataformas todo es un no-op seguro.
namespace sysint {

// Copia de seguridad diaria de la base (una por día, se guardan las últimas 7).
// dir vacío = carpeta "backups" junto a los datos. Devuelve la ruta creada o vacío.
QString dailyBackup(Database& db, const QString& dataDir, const QString& dir, int keep = 7);

// Iniciar con Windows: entrada en HKCU\...\Run con "--autostart".
void setAutostart(bool on);
bool autostartEnabled();

// ¿El usuario está en pantalla completa / presentando / jugando? (no molestar)
bool userBusy();

// B2-4: tarea programada de Windows que abre la app con "--wake" al desbloquear la
// sesión o al despertar la PC (la app decide si corresponde saludar: una vez por día).
// Devuelve false si Windows no la dejó crear o borrar.
bool setWakeTask(bool on);
QString wakeTaskName();

} // namespace sysint

// B2-4: avisa cuando la PC se despierta o se desbloquea la sesión (con la app abierta).
class WakeWatcher : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit WakeWatcher(QObject* parent = nullptr);
    ~WakeWatcher() override;
    void attach(quintptr windowId);   // la ventana que recibe los avisos de sesión
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;

signals:
    void woke();

private:
    quintptr m_hwnd = 0;
};

// Atajo global (Ctrl + Alt + P): funciona aunque la app no tenga el foco.
class GlobalHotkey : public QObject, public QAbstractNativeEventFilter {
    Q_OBJECT
public:
    explicit GlobalHotkey(QObject* parent = nullptr);
    ~GlobalHotkey() override;
    bool setEnabled(bool on);   // false si Windows no lo dejó registrar (otra app lo usa)
    bool nativeEventFilter(const QByteArray& type, void* message, qintptr* result) override;

signals:
    void activated();

private:
    bool m_registered = false;
};
