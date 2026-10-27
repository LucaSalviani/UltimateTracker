#pragma once

#include "core/Types.h"

#include <QHash>
#include <QSet>
#include <QSqlDatabase>
#include <QVariant>
#include <QVector>
#include <optional>

// Acceso a SQLite. Todo el estado persistente de la app pasa por acá.
class Database {
public:
    Database();
    ~Database();
    Database(const Database&) = delete;
    Database& operator=(const Database&) = delete;

    // path = ":memory:" para tests.
    bool open(const QString& path);
    QString lastError() const { return m_error; }

    // ---- Carpetas --------------------------------------------------------
    QVector<Folder> folders() const;
    qint64 addFolder(qint64 parentId, const QString& name);
    bool   renameFolder(qint64 id, const QString& name);
    // Borra la carpeta y sus subcarpetas. Sus pomodoros quedan "sin carpeta"
    // para no perder horas de las estadísticas globales.
    bool   deleteFolder(qint64 id);
    bool   moveFolder(qint64 id, qint64 newParentId);
    // Padre y orden entre hermanos (después de arrastrar en el gestor).
    void   setFolderPlacement(qint64 id, qint64 parentId, int sort);

    // ---- Días ------------------------------------------------------------
    QString dayTitle(const QDate& day) const;
    QHash<QDate, QString> dayTitles(const QDate& from, const QDate& to) const;
    void setDayTitle(const QDate& day, const QString& title);

    // ---- Pomodoros -------------------------------------------------------
    qint64 insertPomodoro(const Pomodoro& p);
    void   updatePomodoro(const Pomodoro& p);
    void   deletePomodoro(qint64 id);
    void   setPomodoroFolder(qint64 id, qint64 folderId);
    void   setPomodoroNote(qint64 id, const QString& note);
    void   setPomodoroDuration(qint64 id, int studiedSec);   // solo terminados
    std::optional<Pomodoro> pomodoro(qint64 id) const;
    QVector<Pomodoro> pomodorosBetween(const QDate& from, const QDate& to) const; // inclusivo
    QVector<Pomodoro> pomodorosWithStatus(PomoStatus st) const;
    QSet<QDate> studiedDays() const;
    // Segundos de estudio por carpeta (sin acumular). Fechas inválidas = histórico completo.
    QHash<qint64, qint64> studyByFolder(const QDate& from = {}, const QDate& to = {}) const;

    // ---- Recreos e indebidos --------------------------------------------
    qint64 insertInterval(IntervalKind kind, const QDate& day, qint64 startedAt, qint64 seconds);
    void   updateIntervalSeconds(qint64 id, qint64 seconds);
    void   setIntervalsKind(const QVector<qint64>& ids, IntervalKind kind);
    QVector<Interval> intervalsBetween(const QDate& from, const QDate& to) const;

    // ---- Hitos (avisos de horas) ----------------------------------------
    // true si el hito es nuevo (y queda registrado).
    bool markMilestone(qint64 folderId, int hours);

    // ---- Ajustes --------------------------------------------------------
    QVariant setting(const QString& key, const QVariant& def = {}) const;
    void     setSetting(const QString& key, const QVariant& value);

private:
    bool exec(const QString& sql);
    bool migrate();
    void addColumnIfMissing(const QString& table, const QString& column, const QString& decl);
    static Pomodoro readPomodoro(const class QSqlQuery& q);

    QString      m_conn;
    QSqlDatabase m_db;
    mutable QString m_error;
};
