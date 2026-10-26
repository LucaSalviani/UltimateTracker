#pragma once

#include "core/Database.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"
#include "ui/EdgeTab.h"
#include "ui/GridWidget.h"

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
class FolderMenu;
class Sounds;
class StatsWidget;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(Database* db, QWidget* parent = nullptr);
    void bringToFront();

protected:
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void showEvent(QShowEvent*) override;

private:
    // ---- construcción
    QWidget* buildTitleBar();
    QWidget* buildDateBand();
    QWidget* buildControls();
    void     buildTray();

    // ---- vista
    void setZoom(GridWidget::Zoom z);
    void navigate(int dir);
    void goToday();
    void reloadGrid();
    void refreshBand();
    void refreshControls();
    void onTick();
    void reloadFolders();
    void showStats(bool on);
    void resizeAnimated(const QSize& size);
    QSize sizeFor(GridWidget::Zoom z) const;

    // ---- ventana
    void togglePin(bool on);
    void dockTo(EdgeTab::Edge edge);
    void dockToNearestEdge();
    void undock();
    void toast(const QString& text);
    void quitApp();

    // ---- sesión
    void onStateChanged(SessionController::State s);
    void onStudyRecorded(qint64 folderId, qint64 seconds);
    void onSessionEnded(qint64 study, qint64 brk, qint64 undue);
    void togglePlay();
    void chooseFolder(qint64 id);

    Database*          m_db;
    SessionController* m_session;
    Sounds*            m_sounds;
    FolderTree         m_tree;

    // Páginas
    QStackedWidget* m_pages = nullptr;
    StatsWidget*    m_stats = nullptr;

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

    // Controles por estado
    QStackedWidget* m_controls = nullptr;
    QToolButton*    m_folderBtn = nullptr;
    FolderMenu*     m_folderMenu = nullptr;
    QSpinBox*       m_workSpin = nullptr;
    QLabel*         m_todayLabel = nullptr;
    QLabel*         m_runFolder = nullptr;
    QLabel*         m_runClock = nullptr;
    QLabel*         m_pausedLabel = nullptr;
    QSpinBox*       m_breakSpin = nullptr;
    QLabel*         m_doneLabel = nullptr;
    QLabel*         m_breakClock = nullptr;
    QLabel*         m_overLabel = nullptr;

    // Bandeja y borde
    QSystemTrayIcon* m_tray = nullptr;
    EdgeTab*         m_edgeTab = nullptr;
    bool             m_docked = false;
    bool             m_trayHintShown = false;

    // Arrastre de ventana
    bool   m_dragging = false;
    QPoint m_dragOffset;

    QPointer<QPropertyAnimation> m_anim;
    qint64 m_todayBaseSec = 0;   // estudio de hoy sin contar el pomodoro en curso
    QDate  m_today;
    int    m_jokers = 2;
    bool   m_inStats = false;
};
