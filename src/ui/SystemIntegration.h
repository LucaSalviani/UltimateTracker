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

} // namespace sysint

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
