#pragma once

#include "core/Database.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"
#include "ui/EdgeTab.h"
#include "ui/GridWidget.h"

#include <QHash>
#include <QPointer>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QWidget>

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QToolButton;
class QPropertyAnimation;
class QAbstractAnimation;
class ActivitiesWidget;
class FolderMenu;
class LiveStats;
class TimerDeck;
class Sounds;
class StatsWidget;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(Database* db, QWidget* parent = nullptr);
    ~MainWindow() override;
    void bringToFront();

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // Páginas que se superponen a la grilla.
    enum class Overlay { None, Stats, Activities };

    // ---- construcción
    QWidget* buildTitleBar();
    QWidget* buildDateBand();
    QWidget* buildDeck();
    void     buildTray();

    // ---- vista
    void setZoom(GridWidget::Zoom z);
    void navigate(int dir);
    void goToday();
    void reloadGrid();
    void refreshBand();
    void refreshDeck();
    void refreshLiveStats();
    void onTick();
    void reloadFolders();
    void showOverlay(Overlay o);

    // ---- tamaño de la ventana
    int     level() const;               // 0 día, 1 semana, 2 mes, 3 página superpuesta
    QString viewKey() const;             // "day", "week", "month", "stats", "activities"
    QSize   viewSize(const QString& key) const;
    void    applyViewSize();             // anima al tamaño guardado de la vista actual
    void    resetViewSize(bool allViews);
    void    resizeAnimated(const QSize& size, int newLevel);
    void    animateGeometry(const QRect& from, const QRect& to);
    Qt::Edges edgesAt(const QPoint& local) const;
    void    setEdgeCursor(Qt::Edges edges);

    // ---- ventana
    void togglePin(bool on);
    void dockTo(EdgeTab::Edge edge);
    void dockToNearestEdge();
    void undock();
    QRect offscreenRect(const QRect& r, EdgeTab::Edge edge, const QRect& screen) const;
    void toast(const QString& text);
    void quitApp();

    // ---- jornada
    void onStateChanged(SessionController::State s);
    void onStudyRecorded(qint64 folderId, qint64 seconds);
    void togglePlay();
    void chooseFolder(qint64 id);
    void deletePomodoro(qint64 id);
    void showPomodoroMenu(qint64 id, const QPoint& globalPos);
    int  todayCount() const;
    void checkDayRollover();
    void maybeShowDaySummary();

    Database*          m_db;
    SessionController* m_session;
    Sounds*            m_sounds;
    FolderTree         m_tree;

    // Páginas
    QStackedWidget*   m_pages = nullptr;
    StatsWidget*      m_stats = nullptr;
    ActivitiesWidget* m_activities = nullptr;
    Overlay           m_overlay = Overlay::None;

    // Barra de título
    QToolButton* m_btnZoomOut = nullptr;
    QToolButton* m_btnZoomIn = nullptr;
    QToolButton* m_btnProportional = nullptr;
    QToolButton* m_btnPin = nullptr;

    // Banda de fecha
    QWidget*     m_band = nullptr;
    QLabel*      m_dateLabel = nullptr;
    QLineEdit*   m_titleEdit = nullptr;
    QPushButton* m_btnToday = nullptr;

    GridWidget* m_grid = nullptr;
    QLabel*     m_toast = nullptr;
    QTimer      m_toastTimer;

    // Reproductor y stats en vivo
    TimerDeck*  m_deck = nullptr;
    FolderMenu* m_folderMenu = nullptr;
    LiveStats*  m_liveStats = nullptr;
    bool        m_liveActivityScope = true;
    QVector<Pomodoro> m_todayPomos;      // pomodoros de hoy ya guardados (para stats en vivo)
    bool        m_numericDate = true;

    // Bandeja y borde
    QSystemTrayIcon* m_tray = nullptr;
    EdgeTab*         m_edgeTab = nullptr;
    bool             m_docked = false;
    bool             m_trayHintShown = false;

    // Arrastre y redimensión
    bool      m_dragging = false;
    QPoint    m_dragOffset;
    bool      m_resizable = true;
    bool      m_userResizing = false;
    QTimer    m_saveSizeTimer;
    bool      m_cursorOverridden = false;
    Qt::Edges m_cursorEdges;

    QPointer<QPropertyAnimation> m_anim;
    QPointer<QAbstractAnimation> m_dockAnim;
    int                 m_level = 0;       // nivel actual (ver level())
    QHash<int, QRect>   m_savedRects;      // dónde estaba la ventana en cada nivel

    QDate  m_today;
    QDate  m_sessionDay;         // día de la jornada en curso
    int    m_jokers = 2;
};
