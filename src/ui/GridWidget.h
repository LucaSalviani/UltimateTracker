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

signals:
    void dayActivated(const QDate& day);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;

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

    Zoom  m_zoom = Zoom::Day;
    QDate m_anchor;
    QDate m_today;
    bool  m_proportional = false;

    QVector<Pomodoro>            m_pomos;
    QHash<QDate, QVector<int>>   m_byDay;
    QHash<QDate, QString>        m_titles;
    QHash<qint64, QString>       m_paths;
    qint64 m_liveId = 0;
    double m_liveProgress = 0.0;

    // Se reconstruyen en cada paint, para tooltips y clicks.
    QVector<Placed>               m_placed;
    QVector<QPair<QRectF, QDate>> m_dayRects;
};
