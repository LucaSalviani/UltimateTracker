#pragma once

#include "core/Database.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"
#include "core/Sync.h"
#include "core/Live.h"

#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <optional>

class AndroidBridge;

// Puente entre el motor (el mismo de la PC) y la interfaz QML del celular.
class MobileApp : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString state READ state NOTIFY changed)               // idle, ready, running, paused, workdone, break, breakover
    Q_PROPERTY(int remainingSec READ remainingSec NOTIFY ticked)
    Q_PROPERTY(double progress READ progress NOTIFY ticked)
    Q_PROPERTY(int workMin READ workMin WRITE setWorkMin NOTIFY changed)
    Q_PROPERTY(int breakMin READ breakMin WRITE setBreakMin NOTIFY changed)
    Q_PROPERTY(qint64 folderId READ folderId NOTIFY changed)
    Q_PROPERTY(QString folderName READ folderName NOTIFY changed)
    Q_PROPERTY(QString folderColor READ folderColor NOTIFY changed)
    Q_PROPERTY(QVariantList folders READ folders NOTIFY foldersChanged)
    Q_PROPERTY(QVariantList today READ today NOTIFY todayChanged)
    Q_PROPERTY(int todayCount READ todayCount NOTIFY todayChanged)
    Q_PROPERTY(QString todayStudied READ todayStudied NOTIFY ticked)
    Q_PROPERTY(QString dateLabel READ dateLabel NOTIFY todayChanged)
    Q_PROPERTY(int pomodoroNumber READ pomodoroNumber NOTIFY todayChanged)
    // Sincronización con la PC
    Q_PROPERTY(bool syncSignedIn READ syncSignedIn NOTIFY syncChanged)
    Q_PROPERTY(QString syncEmail READ syncEmail NOTIFY syncChanged)
    Q_PROPERTY(QString syncStatus READ syncStatus NOTIFY syncChanged)
    // En vivo: el reloj lo está manejando la PC (vacío si no)
    Q_PROPERTY(QString liveFrom READ liveFrom NOTIFY liveChanged)
    // Objetivo del día (lo mismo que el panel de la PC)
    Q_PROPERTY(QVariantMap goal READ goal NOTIFY todayChanged)
    Q_PROPERTY(QString dayTitle READ dayTitle NOTIFY todayChanged)
    // Cambia cuando se guarda algo que afecta estadísticas e hitos
    Q_PROPERTY(int revision READ revision NOTIFY dataChanged)
    Q_PROPERTY(QString versionText READ version CONSTANT)

public:
    explicit MobileApp(Database* db, QObject* parent = nullptr);
    ~MobileApp() override;

    QString state() const;
    int     remainingSec() const;
    double  progress() const;
    int     workMin() const { return m_session.workSec() / 60; }
    int     breakMin() const { return m_session.breakSec() / 60; }
    void    setWorkMin(int m);
    void    setBreakMin(int m);
    qint64  folderId() const { return m_session.folderId(); }
    QString folderName() const;
    QString folderColor() const;
    QVariantList folders() const;
    QVariantList today() const { return m_today; }
    int     todayCount() const;
    QString todayStudied() const;
    QString dateLabel() const;
    int     pomodoroNumber() const;

    Q_INVOKABLE void primary();      // el botón grande: play / pausa / seguir
    Q_INVOKABLE void finishPomodoro();
    Q_INVOKABLE void startBreak();
    Q_INVOKABLE void skipBreak();
    Q_INVOKABLE void endBreak();
    Q_INVOKABLE void chooseFolder(qint64 id);
    Q_INVOKABLE qint64 addFolder(const QString& name, qint64 parentId = 0);
    Q_INVOKABLE void nudgeWork(int deltaMin);
    Q_INVOKABLE void nudgeBreak(int deltaMin);
    Q_INVOKABLE QString clock(int sec) const;   // "24:59"
    Q_INVOKABLE void appActivated();            // al volver a la app: ponerse al día
    Q_INVOKABLE void appDeactivated();
    void demoLive();                            // capturas: como si la PC tuviera un pomodoro corriendo          // a segundo plano: se pregunta menos seguido
    QString liveFrom() const { return m_live && m_live->mirroring() && m_session.state() != SessionController::State::Idle ? m_live->remoteLabel() : QString(); }
    bool    syncSignedIn() const { return m_sync.signedIn(); }
    QString syncEmail() const { return m_sync.email(); }
    QString syncStatus() const { return m_sync.status(); }
    Q_INVOKABLE void signIn(const QString& email, const QString& password) { m_sync.signIn(email, password); }
    Q_INVOKABLE void signUp(const QString& email, const QString& password) { m_sync.signUp(email, password); }
    Q_INVOKABLE void signOut() { m_sync.signOut(); }
    Q_INVOKABLE void syncNow() { m_sync.syncNow(); }

    // ---- Objetivo del día
    QVariantMap goal() const { return m_goal; }
    QString dayTitle() const;
    Q_INVOKABLE void setDayTitle(const QString& text);
    Q_INVOKABLE QVariantMap goalDraft() const;   // para editar: modo, partes, repetición
    // parts: [{count, minutes, folder}]; weekdays: 0 solo hoy, 127 todos los días, 31 lun–vie
    Q_INVOKABLE void saveGoal(bool timeMode, int totalMin, const QVariantList& parts, int weekdays);
    Q_INVOKABLE void removeGoal(bool fromTodayOn);
    Q_INVOKABLE QString describeParts(bool timeMode, int totalMin, const QVariantList& parts) const;

    // ---- Cuadraditos (tocar uno): nota, actividad, borrar
    Q_INVOKABLE QVariantMap pomodoroInfo(qint64 id) const;
    Q_INVOKABLE void setNote(qint64 id, const QString& note);
    Q_INVOKABLE void movePomodoro(qint64 id, qint64 folderId);
    Q_INVOKABLE void deletePomodoro(qint64 id);
    Q_INVOKABLE void undoDelete();

    // ---- Estadísticas: period = "day" | "week" | "month" | "year"; offset 0 = el actual
    Q_INVOKABLE QVariantMap stats(const QString& period, int offset, qint64 folder) const;
    // ---- Hitos
    Q_INVOKABLE QVariantMap milestones() const;

    // ---- Ajustes y actividades
    Q_INVOKABLE QVariant setting(const QString& key, const QVariant& def) const;
    Q_INVOKABLE void setSetting(const QString& key, const QVariant& value);
    Q_INVOKABLE QVariantMap folderInfo(qint64 id) const;
    Q_INVOKABLE void renameFolder(qint64 id, const QString& name);
    Q_INVOKABLE void setFolderColor(qint64 id, const QString& color);
    Q_INVOKABLE void setFolderPriorHours(qint64 id, double hours);
    Q_INVOKABLE bool deleteFolder(qint64 id);
    Q_INVOKABLE QStringList palette() const;
    int revision() const { return m_revision; }
    QString version() const;

signals:
    void changed();
    void ticked();
    void foldersChanged();
    void todayChanged();
    void completed();      // terminó un pomodoro (vibra / suena)
    void breakOver();
    void syncChanged();
    void liveChanged();
    void syncMessage(const QString& text);
    void dataChanged();
    void toast(const QString& text);
    void celebrate(const QString& kicker, const QString& title, const QString& detail);

private:
    void reloadFolders();
    void reloadToday();
    void syncAndroid();    // notificación con el tiempo y alarma programada
    void reloadGoal();
    void preloadFromGoal();
    void afterSave(qint64 id);           // objetivo cumplido, marca del día, rachas, récords
    void onStudyRecorded(qint64 folderId, qint64 seconds);   // etapas y horas por actividad
    bool once(const QString& key);
    void bump();                         // algo cambió: estadísticas e hitos se recalculan
    QHash<qint64, qint64> studiedByFolder(const QVector<Pomodoro>& dayPomos, bool reserve) const;

    Database*          m_db;
    SessionController  m_session;
    FolderTree         m_tree;
    QHash<qint64, QString> m_colors;
    QVariantList       m_today;
    AndroidBridge*     m_android = nullptr;
    SyncClient         m_sync;
    LiveLink*          m_live = nullptr;
    QVariantMap        m_goal;
    int                m_revision = 0;
    std::optional<Pomodoro> m_deleted;   // para deshacer el último borrado
};
