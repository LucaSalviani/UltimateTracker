#pragma once

#include "core/Types.h"

#include <QColor>
#include <QDate>
#include <QElapsedTimer>
#include <QSet>
#include <QTimer>
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
    // M7: minutos de los cuadraditos de la meta que todavía faltan en cada día;
    // se pintan "fantasma" después de los reales.
    void setGoalBlocks(const QHash<QDate, QVector<int>>& blocks);
    void setLive(const std::optional<Pomodoro>& running, double progress);
    void setFolderPaths(const QHash<qint64, QString>& paths) { m_paths = paths; }
    // M8: nombre corto y color de cada actividad, y si se agrupan los cuadraditos.
    void setFolderLooks(const QHash<qint64, QString>& names, const QHash<qint64, QColor>& colors);
    void setGrouping(bool on) { m_grouping = on; update(); }
    // Si hay algo en marcha hoy (pomodoro, pausa o recreo), el último pomodoro
    // de hoy todavía no es "el final del día" y no se pinta rojo.
    void setTodayActive(bool active) { m_todayActive = active; update(); }

signals:
    void dayActivated(const QDate& day);
    // Click derecho sobre un cuadradito (MainWindow arma el menú).
    void pomodoroMenuRequested(qint64 pomodoroId, const QPoint& globalPos);
    // Click derecho en un día fuera de los cuadraditos, o click en un fantasma.
    void dayMenuRequested(const QDate& day, const QPoint& globalPos);
    void goalClicked(const QDate& day);

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void leaveEvent(QEvent*) override;

private:
    struct Placed { QRectF rect; int index; };
    struct Header { QRectF rect; qint64 folderId; qint64 seconds; int ghosts; };
    struct Layout { QVector<Placed> placed; QVector<Header> headers; };

    // Flujo de izquierda a derecha por grupos (cada grupo arranca en su renglón,
    // con un título arriba si headerH > 0). Busca el lado más grande que entra.
    Layout layoutGroups(const QRectF& area, const QVector<QVector<int>>& groups, double headerH,
                        bool breakLines, double maxSide, double minSide) const;
    void paintSquares(QPainter& p, const QRectF& area, const QDate& day, double maxSide, double minSide,
                      double headerH, bool breakLines);
    void paintHeader(QPainter& p, const Header& h) const;
    // Animación de entrada: escala/opacidad de un cuadradito según su orden.
    bool animFrame(qint64 id, int order, double* scale, double* opacity);
    void startIntro();
    void paintSquare(QPainter& p, const QRectF& r, const Pomodoro& pomo) const;
    void paintDay(QPainter& p);
    void paintWeek(QPainter& p);
    void paintMonth(QPainter& p);
    qint64 studyOn(const QDate& d) const;
    bool   isDayEnd(const Pomodoro& p) const;
    void   recomputeDayEnds();
    void   rebuild();                 // reales + fantasmas → m_pomos / m_byDay
    static bool isGhost(const Pomodoro& p) { return p.id < 0; }
    void   paintGhost(QPainter& p, const QRectF& r, const Pomodoro& g) const;
    QDate  dayAt(const QPointF& pos) const;

    Zoom  m_zoom = Zoom::Day;
    QDate m_anchor;
    QDate m_today;
    bool  m_proportional = false;

    QVector<Pomodoro>            m_real;        // lo que viene de la base
    QHash<QDate, QVector<int>>   m_goalBlocks;
    QVector<Pomodoro>            m_pomos;       // reales + fantasmas (id < 0)
    QHash<QDate, QVector<int>>   m_byDay;
    QHash<QDate, QString>        m_titles;
    QHash<qint64, QString>       m_paths;
    QHash<qint64, QString>       m_names;       // nombre corto de cada actividad
    QHash<qint64, QColor>        m_colors;      // color de cada actividad
    bool                         m_grouping = true;
    // Animación de entrada
    QElapsedTimer m_clock;
    QTimer        m_animTimer;
    qint64        m_introAt = -1;               // ms: arrancó la entrada de toda la vista
    QHash<qint64, qint64> m_popAt;              // ms: cuadraditos nuevos que "saltan"
    QSet<qint64>  m_seen;
    int           m_paintOrder = 0;
    bool          m_animating = false;
    QHash<QDate, qint64>         m_lastOfDay;   // id del último pomodoro de cada día
    QHash<qint64, int>           m_number;      // número del pomodoro dentro de su día (1, 2, …)
    bool   m_todayActive = false;
    QDate  m_hoverDay;                          // día bajo el mouse (semana / mes)
    qint64 m_liveId = 0;
    double m_liveProgress = 0.0;

    // Se reconstruyen en cada paint, para tooltips y clicks.
    QVector<Placed>               m_placed;
    QVector<QPair<QRectF, QDate>> m_dayRects;
};
