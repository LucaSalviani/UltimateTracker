#pragma once

#include "core/Database.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"

#include <QObject>
#include <QVariantList>

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

signals:
    void changed();
    void ticked();
    void foldersChanged();
    void todayChanged();
    void completed();      // terminó un pomodoro (vibra / suena)
    void breakOver();

private:
    void reloadFolders();
    void reloadToday();
    void syncAndroid();    // notificación con el tiempo y alarma programada

    Database*          m_db;
    SessionController  m_session;
    FolderTree         m_tree;
    QHash<qint64, QString> m_colors;
    QVariantList       m_today;
    AndroidBridge*     m_android = nullptr;
};
