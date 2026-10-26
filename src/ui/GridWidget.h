#pragma once

#include "core/Types.h"

#include <QDate>
#include <QHash>
#include <QVector>
#include <QWidget>
#include <optional>

// La grilla de cuadraditos, pintada a mano. Tres niveles de zoom (día, semana,
// mes) estilo Google Calendar. Un cuadradito por pomodoro.
class GridWidget : public QWidget {
    Q_OBJECT
public:
    enum class Zoom { Day = 0, Week = 1, Month = 2 };

    explicit GridWidget(QWidget* parent = nullptr);

    void  setZoom(Zoom z);
    Zoom  zoom() const { return m_zoom; }
    void  setAnchor(const QDate& d);
    QDate anchor() const { return m_anchor; }
    void  setToday(const QDate& d);
    void  setProportional(bool on);
    bool  proportional() const { return m_proportional; }

    // Rango de días que la vista actual necesita de la base.
    QDate rangeStart() const;
    QDate rangeEnd() const;

    void setData(const QVector<Pomodoro>& pomos, const QHash<QDate, QString>& titles);
    void setLive(const std::optional<Pomodoro>& running, double progress);
    void setFolderPaths(const QHash<qint64, QString>& paths) { m_paths = paths; }
    // Si hay algo en marcha hoy (pomodoro, pausa o recreo), el último pomodoro
    // de hoy todavía no es "el final del día" y no se pinta rojo.
    void setTodayActive(bool active) { m_todayActive = active; update(); }

signals:
    void dayActivated(const QDate& day);
    // Click derecho sobre un cuadradito (MainWindow arma el menú).
    void pomodoroMenuRequested(qint64 pomodoroId, const QPoint& globalPos);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;

private:
    struct Placed { QRectF rect; int index; };

    QVector<Placed> layoutSquares(const QRectF& area, const QVector<int>& indices,
                                  double maxSide, double minSide) const;
    void paintSquares(QPainter& p, const QRectF& area, const QDate& day, double maxSide, double minSide);
    void paintSquare(QPainter& p, const QRectF& r, const Pomodoro& pomo) const;
    void paintDay(QPainter& p);
    void paintWeek(QPainter& p);
    void paintMonth(QPainter& p);
    qint64 studyOn(const QDate& d) const;
    bool   isDayEnd(const Pomodoro& p) const;
    void   recomputeDayEnds();

    Zoom  m_zoom = Zoom::Day;
    QDate m_anchor;
    QDate m_today;
    bool  m_proportional = false;

    QVector<Pomodoro>            m_pomos;
    QHash<QDate, QVector<int>>   m_byDay;
    QHash<QDate, QString>        m_titles;
    QHash<qint64, QString>       m_paths;
    QHash<QDate, qint64>         m_lastOfDay;   // id del último pomodoro de cada día
    QHash<qint64, int>           m_number;      // número del pomodoro dentro de su día (1, 2, …)
    bool   m_todayActive = false;
    qint64 m_liveId = 0;
    double m_liveProgress = 0.0;

    // Se reconstruyen en cada paint, para tooltips y clicks.
    QVector<Placed>               m_placed;
    QVector<QPair<QRectF, QDate>> m_dayRects;
};
