#include "ui/MainWindow.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Stats.h"
#include "ui/ActivitiesWidget.h"
#include "ui/FolderMenu.h"
#include "ui/LiveStats.h"
#include "ui/TimerDeck.h"
#include "ui/Icons.h"
#include "ui/SettingsDialog.h"
#include "ui/Sounds.h"
#include "ui/StatsWidget.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#include <dwmapi.h>
#endif

// ---------------------------------------------------------------------------
// Detalles específicos de Windows 11: esquinas redondeadas y sombra en la
// ventana sin marco, "siempre visible" sin recrear la ventana y traer al frente.
// ---------------------------------------------------------------------------
namespace win {
void applyChrome(QWidget* w)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const DWORD round = 2;     // DWMWCP_ROUND
    DwmSetWindowAttribute(hwnd, 33 /*DWMWA_WINDOW_CORNER_PREFERENCE*/, &round, sizeof(round));
    const BOOL dark = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /*DWMWA_USE_IMMERSIVE_DARK_MODE*/, &dark, sizeof(dark));
    const MARGINS shadow = {1, 1, 1, 1};
    DwmExtendFrameIntoClientArea(hwnd, &shadow);
#else
    Q_UNUSED(w);
#endif
}

// La ventana no tiene marco, así que hay que pedirle a Windows explícitamente
// que la deje minimizar y restaurar desde la barra de tareas.
void enableMinimize(QWidget* w)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    const LONG_PTR style = GetWindowLongPtr(hwnd, GWL_STYLE);
    SetWindowLongPtr(hwnd, GWL_STYLE, style | WS_MINIMIZEBOX | WS_SYSMENU);
#else
    Q_UNUSED(w);
#endif
}

void setTopmost(QWidget* w, bool on)
{
#ifdef Q_OS_WIN
    SetWindowPos(reinterpret_cast<HWND>(w->winId()), on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    const bool visible = w->isVisible();
    w->setWindowFlag(Qt::WindowStaysOnTopHint, on);
    if (visible) w->show();
#endif
}

void forceForeground(QWidget* w, bool keepTopmost)
{
#ifdef Q_OS_WIN
    HWND hwnd = reinterpret_cast<HWND>(w->winId());
    // Truco clásico: subirla a "topmost" un instante para que Windows la muestre
    // aunque otra app tenga el foco, y después devolverla a la normalidad.
    SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    if (!keepTopmost)
        SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd);
#else
    Q_UNUSED(keepTopmost);
    w->raise();
    w->activateWindow();
#endif
}
} // namespace win

namespace {
QToolButton* iconButton(QWidget* parent, icons::Id id, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setIcon(icons::get(id));
    b->setIconSize(QSize(18, 18));
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    b->setAutoRaise(true);
    return b;
}

QPushButton* textButton(QWidget* parent, const QString& text, const char* role = nullptr)
{
    auto* b = new QPushButton(text, parent);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    if (role) b->setProperty("role", role);
    return b;
}

QToolButton* roundButton(QWidget* parent, icons::Id id, const QString& tip)
{
    auto* b = new QToolButton(parent);
    b->setIcon(icons::get(id, QColor(0x14, 0x14, 0x14)));
    b->setIconSize(QSize(20, 20));
    b->setFixedSize(40, 40);
    b->setToolTip(tip);
    b->setFocusPolicy(Qt::NoFocus);
    b->setCursor(Qt::PointingHandCursor);
    b->setProperty("role", "play");
    return b;
}

QLabel* dimLabel(QWidget* parent, const QString& text = {})
{
    auto* l = new QLabel(text, parent);
    l->setProperty("dim", true);
    return l;
}

enum Page { PageGrid = 0, PageStats = 1, PageActivities = 2 };

constexpr int kResizeMargin = 6;   // píxeles del borde que sirven para redimensionar

QSize defaultSize(const QString& key)
{
    // Día: del tamaño de la calculadora pero apaisada.
    if (key == "week")       return {1180, 640};
    if (key == "month")      return {1340, 900};
    if (key == "stats")      return {1034, 704};
    if (key == "activities") return {594, 560};
    return {700, 400};
}
} // namespace

// ===========================================================================
MainWindow::MainWindow(Database* db, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_db(db)
{
    setObjectName("Root");
    setWindowTitle(QStringLiteral("UltimateTracker"));
    setWindowIcon(icons::get(icons::Id::AppLogo));
    setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_AlwaysShowToolTips);   // tooltips aunque la ventana no tenga foco
    setMinimumSize(380, 240);

    m_session = new SessionController(m_db, {}, true, this);
    m_session->recoverAfterRestart();
    m_sounds = new Sounds(this);
    m_today = dayclock::today();
    m_jokers = m_db->setting("jokers_per_week", 2).toInt();
    m_resizable = m_db->setting("resizable", 1).toBool();
    m_numericDate = m_db->setting("date_format", "numeric").toString() == QLatin1String("numeric");
    m_sessionDay = dayclock::today();

    m_session->setWorkSec(m_db->setting("work_min", 25).toInt() * 60);
    m_session->setBreakSec(m_db->setting("break_min", 10).toInt() * 60);
    reloadFolders();
    qint64 lastFolder = m_db->setting("last_folder", 0).toLongLong();
    if (!m_tree.contains(lastFolder) && !m_tree.children(0).isEmpty()) lastFolder = m_tree.children(0).first();
    m_session->setFolderId(lastFolder);

    // ---- Página de grilla
    auto* gridPage = new QWidget(this);
    m_grid = new GridWidget(gridPage);
    m_grid->setProportional(m_db->setting("proportional", 0).toBool());
    m_toast = new QLabel(gridPage);
    m_toast->setProperty("role", "toast");
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->hide();
    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, m_toast, &QLabel::hide);

    m_liveStats = new LiveStats(gridPage);
    m_liveActivityScope = m_db->setting("live_scope", "activity").toString() == QLatin1String("activity");
    connect(m_liveStats, &LiveStats::scopeToggled, this, [this] {
        m_liveActivityScope = !m_liveActivityScope;
        m_db->setSetting("live_scope", m_liveActivityScope ? "activity" : "day");
        reloadGrid();
    });

    // Columna izquierda: grilla + stats en vivo. Derecha: el reproductor.
    auto* left = new QVBoxLayout;
    left->setContentsMargins(0, 0, 0, 0);
    left->setSpacing(6);
    left->addWidget(m_grid, 1);
    left->addWidget(m_toast);
    left->addWidget(m_liveStats);

    auto* body = new QHBoxLayout;
    body->setContentsMargins(10, 4, 10, 10);
    body->setSpacing(10);
    body->addLayout(left, 1);
    body->addWidget(buildDeck());

    auto* gridLay = new QVBoxLayout(gridPage);
    gridLay->setContentsMargins(0, 0, 0, 0);
    gridLay->setSpacing(0);
    gridLay->addWidget(buildDateBand());
    gridLay->addLayout(body, 1);

    // ---- Páginas superpuestas
    m_stats = new StatsWidget(m_db, this);
    m_stats->setJokersPerWeek(m_jokers);
    connect(m_stats, &StatsWidget::backRequested, this, [this] { showOverlay(Overlay::None); });

    m_activities = new ActivitiesWidget(m_db, this);
    connect(m_activities, &ActivitiesWidget::backRequested, this, [this] { showOverlay(Overlay::None); });
    connect(m_activities, &ActivitiesWidget::activitiesChanged, this, &MainWindow::reloadFolders);

    m_pages = new QStackedWidget(this);
    m_pages->insertWidget(PageGrid, gridPage);
    m_pages->insertWidget(PageStats, m_stats);
    m_pages->insertWidget(PageActivities, m_activities);
    // El tamaño de la ventana lo manejamos nosotros según la vista; que una
    // página más grande no imponga su mínimo en la vista día.
    for (QWidget* w : {gridPage, static_cast<QWidget*>(m_stats), static_cast<QWidget*>(m_activities)})
        w->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);
    root->addWidget(buildTitleBar());
    root->addWidget(m_pages, 1);

    m_edgeTab = new EdgeTab();
    connect(m_edgeTab, &EdgeTab::activated, this, &MainWindow::undock);
    buildTray();
    reloadFolders();   // ahora que existen la grilla y el menú de actividades

    // ---- Señales del motor
    connect(m_session, &SessionController::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_session, &SessionController::ticked, this, &MainWindow::onTick);
    connect(m_session, &SessionController::pomodoroStarted, this, [this] {
        reloadGrid();
    });
    connect(m_session, &SessionController::pomodoroSaved, this, &MainWindow::reloadGrid);
    connect(m_session, &SessionController::pomodoroChanged, this, [this] {
        rebuildFolderMenu();
        reloadGrid();
    });
    connect(m_session, &SessionController::studyRecorded, this, &MainWindow::onStudyRecorded);
    connect(m_session, &SessionController::pomodoroCompleted, this, [this] {
        m_sounds->playChime();
        bringToFront();
    });
    connect(m_session, &SessionController::breakFinished, this, [this] {
        m_sounds->startAlarm();
        bringToFront();
    });

    connect(m_grid, &GridWidget::dayActivated, this, [this](const QDate& d) {
        m_grid->setAnchor(d);
        setZoom(GridWidget::Zoom::Day);
    });
    connect(m_grid, &GridWidget::pomodoroMenuRequested, this, &MainWindow::showPomodoroMenu);

    // ---- Atajos
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(space, &QShortcut::activated, this, &MainWindow::togglePlay);
    auto* zoomOut = new QShortcut(QKeySequence::ZoomOut, this);
    connect(zoomOut, &QShortcut::activated, this, [this] { setZoom(GridWidget::Zoom(qMin(2, int(m_grid->zoom()) + 1))); });
    auto* zoomIn = new QShortcut(QKeySequence::ZoomIn, this);
    connect(zoomIn, &QShortcut::activated, this, [this] { setZoom(GridWidget::Zoom(qMax(0, int(m_grid->zoom()) - 1))); });

    // ---- Redimensión desde los bordes
    m_saveSizeTimer.setSingleShot(true);
    m_saveSizeTimer.setInterval(500);
    connect(&m_saveSizeTimer, &QTimer::timeout, this, [this] {
        m_db->setSetting("size/" + viewKey(), QStringLiteral("%1,%2").arg(width()).arg(height()));
    });
    qApp->installEventFilter(this);

    // ---- Cambio de día con la app abierta
    auto* midnight = new QTimer(this);
    midnight->setInterval(30 * 1000);
    connect(midnight, &QTimer::timeout, this, &MainWindow::checkDayRollover);
    midnight->start();

    // ---- Estado inicial: siempre abre en el día de hoy
    resize(viewSize("day"));
    const QPoint savedPos = m_db->setting("window_pos").toPoint();
    if (!savedPos.isNull() && QGuiApplication::screenAt(savedPos + QPoint(40, 40)))
        move(savedPos);
    m_grid->setAnchor(m_today);
    m_grid->setToday(m_today);
    setZoom(GridWidget::Zoom::Day);
    onStateChanged(m_session->state());
}

MainWindow::~MainWindow()
{
    qApp->removeEventFilter(this);
    if (m_cursorOverridden) QApplication::restoreOverrideCursor();
}

// ===========================================================================
// Construcción
// ===========================================================================
QWidget* MainWindow::buildTitleBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName("TitleBar");
    bar->setFixedHeight(36);

    // Click derecho en la barra: volver al tamaño original.
    bar->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(bar, &QWidget::customContextMenuRequested, this, [this, bar](const QPoint& pos) {
        QMenu menu(this);
        menu.addAction(QStringLiteral("Restablecer tamaño de esta vista"), this, [this] { resetViewSize(false); });
        menu.addAction(QStringLiteral("Restablecer tamaño de todas las vistas"), this, [this] { resetViewSize(true); });
        menu.exec(bar->mapToGlobal(pos));
    });

    auto* logo = new QLabel(bar);
    logo->setPixmap(icons::get(icons::Id::AppLogo).pixmap(16, 16));
    auto* name = dimLabel(bar, QStringLiteral("UltimateTracker"));

    m_btnProportional = iconButton(bar, icons::Id::Proportional,
                                   QStringLiteral("Tamaño proporcional: cada cuadradito según cuánto duró"));
    m_btnProportional->setCheckable(true);
    m_btnProportional->setChecked(m_db->setting("proportional", 0).toBool());
    connect(m_btnProportional, &QToolButton::toggled, this, [this](bool on) {
        m_grid->setProportional(on);
        m_db->setSetting("proportional", on ? 1 : 0);
    });

    m_btnZoomIn = iconButton(bar, icons::Id::ZoomIn, QStringLiteral("Acercar: mes → semana → día (Ctrl +)"));
    m_btnZoomOut = iconButton(bar, icons::Id::ZoomOut, QStringLiteral("Alejar: día → semana → mes (Ctrl −)"));
    connect(m_btnZoomIn, &QToolButton::clicked, this, [this] { setZoom(GridWidget::Zoom(qMax(0, int(m_grid->zoom()) - 1))); });
    connect(m_btnZoomOut, &QToolButton::clicked, this, [this] { setZoom(GridWidget::Zoom(qMin(2, int(m_grid->zoom()) + 1))); });

    auto* stats = iconButton(bar, icons::Id::Stats, QStringLiteral("Estadísticas"));
    connect(stats, &QToolButton::clicked, this, [this] {
        showOverlay(m_overlay == Overlay::Stats ? Overlay::None : Overlay::Stats);
    });

    auto* menuBtn = iconButton(bar, icons::Id::Menu, QStringLiteral("Más: actividades, ajustes, salir"));
    auto* menu = new QMenu(menuBtn);
    menu->addAction(QStringLiteral("Actividades…"), this, [this] { showOverlay(Overlay::Activities); });
    menu->addAction(QStringLiteral("Ajustes…"), this, [this] {
        SettingsDialog dlg(m_db->setting("work_min", 25).toInt(), m_db->setting("break_min", 10).toInt(),
                           m_jokers, m_resizable, m_numericDate, this);
        win::applyChrome(&dlg);
        if (dlg.exec() != QDialog::Accepted) return;
        m_db->setSetting("work_min", dlg.workMinutes());
        m_db->setSetting("break_min", dlg.breakMinutes());
        m_db->setSetting("jokers_per_week", dlg.jokersPerWeek());
        m_db->setSetting("resizable", dlg.resizable() ? 1 : 0);
        m_db->setSetting("date_format", dlg.numericDate() ? "numeric" : "long");
        m_numericDate = dlg.numericDate();
        refreshBand();
        m_jokers = dlg.jokersPerWeek();
        m_resizable = dlg.resizable();
        if (!m_resizable) setEdgeCursor({});
        m_stats->setJokersPerWeek(m_jokers);
        if (!m_session->inProgress()) {
            m_session->setWorkSec(dlg.workMinutes() * 60);
            m_session->setBreakSec(dlg.breakMinutes() * 60);
            refreshDeck();
        }
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Salir"), this, &MainWindow::quitApp);
    connect(menuBtn, &QToolButton::clicked, this, [menuBtn, menu] {
        menu->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    m_btnPin = iconButton(bar, icons::Id::Pin, QStringLiteral("Siempre visible (por encima de las demás ventanas)"));
    m_btnPin->setCheckable(true);
    connect(m_btnPin, &QToolButton::toggled, this, &MainWindow::togglePin);

    auto* dock = iconButton(bar, icons::Id::Dock,
                            QStringLiteral("Esconder en el borde (también: soltar la ventana contra un borde)"));
    connect(dock, &QToolButton::clicked, this, &MainWindow::dockToNearestEdge);

    auto* minimize = iconButton(bar, icons::Id::Minimize, QStringLiteral("Minimizar"));
    connect(minimize, &QToolButton::clicked, this, &QWidget::showMinimized);

    auto* close = iconButton(bar, icons::Id::Close, QStringLiteral("Cerrar"));
    close->setProperty("role", "close");
    connect(close, &QToolButton::clicked, this, &QWidget::close);

    auto* lay = new QHBoxLayout(bar);
    lay->setContentsMargins(12, 0, 4, 0);
    lay->setSpacing(2);
    lay->addWidget(logo);
    lay->addSpacing(6);
    lay->addWidget(name);
    lay->addStretch();
    lay->addWidget(m_btnProportional);
    lay->addWidget(m_btnZoomIn);
    lay->addWidget(m_btnZoomOut);
    lay->addWidget(stats);
    lay->addWidget(menuBtn);
    lay->addSpacing(6);
    lay->addWidget(m_btnPin);
    lay->addWidget(dock);
    lay->addWidget(minimize);
    lay->addWidget(close);
    return bar;
}

QWidget* MainWindow::buildDateBand()
{
    auto* outer = new QWidget(this);
    auto* outerLay = new QVBoxLayout(outer);
    outerLay->setContentsMargins(10, 2, 10, 4);

    m_band = new QWidget(outer);
    m_band->setObjectName("DateBand");
    m_band->setAttribute(Qt::WA_StyledBackground);
    m_band->setFixedHeight(40);

    auto* prev = iconButton(m_band, icons::Id::ChevronLeft, QStringLiteral("Anterior"));
    auto* next = iconButton(m_band, icons::Id::ChevronRight, QStringLiteral("Siguiente"));
    connect(prev, &QToolButton::clicked, this, [this] { navigate(-1); });
    connect(next, &QToolButton::clicked, this, [this] { navigate(+1); });

    m_dateLabel = new QLabel(m_band);
    m_dateLabel->setProperty("role", "date");

    m_titleEdit = new QLineEdit(m_band);
    m_titleEdit->setPlaceholderText(QStringLiteral("Tarea del día (opcional)"));
    m_titleEdit->setProperty("role", "dayTitle");
    m_titleEdit->setFocusPolicy(Qt::ClickFocus);
    connect(m_titleEdit, &QLineEdit::editingFinished, this, [this] {
        m_db->setDayTitle(m_grid->anchor(), m_titleEdit->text());
        m_titleEdit->clearFocus();
    });

    m_btnToday = textButton(m_band, QStringLiteral("Hoy"), "chip");
    connect(m_btnToday, &QPushButton::clicked, this, &MainWindow::goToday);

    // Tarea a la izquierda, fecha a la derecha.
    auto* lay = new QHBoxLayout(m_band);
    lay->setContentsMargins(6, 0, 4, 0);
    lay->setSpacing(4);
    lay->addWidget(m_titleEdit, 1);
    lay->addStretch(0);
    lay->addWidget(m_btnToday);
    lay->addSpacing(4);
    lay->addWidget(prev);
    lay->addWidget(m_dateLabel);
    lay->addWidget(next);

    outerLay->addWidget(m_band);
    return outer;
}

QWidget* MainWindow::buildDeck()
{
    m_deck = new TimerDeck(this);
    m_deck->setFixedWidth(282);

    m_folderMenu = new FolderMenu(this);
    connect(m_folderMenu, &FolderMenu::folderChosen, this, &MainWindow::chooseFolder);
    connect(m_folderMenu, &FolderMenu::manageRequested, this, [this] { showOverlay(Overlay::Activities); });
    m_deck->setActivityMenu(m_folderMenu);

    connect(m_deck, &TimerDeck::playClicked, this, [this] {
        if (m_session->state() == SessionController::State::Paused) m_session->resume();
        else m_session->start();
    });
    connect(m_deck, &TimerDeck::pauseClicked, m_session, &SessionController::pause);
    connect(m_deck, &TimerDeck::endPomodoroClicked, m_session, &SessionController::endPomodoro);
    connect(m_deck, &TimerDeck::startBreakClicked, m_session, &SessionController::startBreak);
    connect(m_deck, &TimerDeck::skipBreakClicked, m_session, &SessionController::skipBreak);
    connect(m_deck, &TimerDeck::endBreakClicked, m_session, &SessionController::endBreakEarly);
    connect(m_deck, &TimerDeck::silenceClicked, m_session, &SessionController::silenceAlarm);
    connect(m_deck, &TimerDeck::goTodayClicked, this, &MainWindow::goToday);
    connect(m_deck, &TimerDeck::durationChanged, this, [this](int min) {
        // El − / + ajusta lo que está por arrancar: el pomodoro o el recreo.
        if (m_session->state() == SessionController::State::WorkDone) m_session->setBreakSec(min * 60);
        else m_session->setWorkSec(min * 60);
        refreshDeck();
    });
    return m_deck;
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(icons::get(icons::Id::AppLogo), this);
    m_tray->setToolTip(QStringLiteral("UltimateTracker"));
    auto* menu = new QMenu(this);
    menu->addAction(QStringLiteral("Mostrar"), this, &MainWindow::bringToFront);
    menu->addSeparator();
    menu->addAction(QStringLiteral("Salir"), this, &MainWindow::quitApp);
    m_tray->setContextMenu(menu);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason r) {
        if (r == QSystemTrayIcon::Trigger || r == QSystemTrayIcon::DoubleClick) bringToFront();
    });
    m_tray->show();
}

// ===========================================================================
// Tamaño de la ventana
// ===========================================================================
int MainWindow::level() const
{
    return m_overlay != Overlay::None ? 3 : int(m_grid->zoom());
}

QString MainWindow::viewKey() const
{
    switch (m_overlay) {
    case Overlay::Stats:      return QStringLiteral("stats");
    case Overlay::Activities: return QStringLiteral("activities");
    case Overlay::None:       break;
    }
    switch (m_grid->zoom()) {
    case GridWidget::Zoom::Week:  return QStringLiteral("week");
    case GridWidget::Zoom::Month: return QStringLiteral("month");
    default:                      return QStringLiteral("day");
    }
}

// Tamaño recordado de una vista (si lo cambiaste a mano) o el original.
QSize MainWindow::viewSize(const QString& key) const
{
    const QStringList parts = m_db->setting("size/" + key).toString().split(',');
    if (parts.size() == 2) {
        const QSize s(parts[0].toInt(), parts[1].toInt());
        if (s.width() >= minimumWidth() && s.height() >= minimumHeight()) return s;
    }
    return defaultSize(key);
}

void MainWindow::applyViewSize()
{
    resizeAnimated(viewSize(viewKey()), level());
}

void MainWindow::resetViewSize(bool allViews)
{
    const QStringList keys = allViews
        ? QStringList{"day", "week", "month", "stats", "activities"}
        : QStringList{viewKey()};
    for (const QString& k : keys) m_db->setSetting("size/" + k, QString());
    applyViewSize();
}

// Niveles de "tamaño": día < semana < mes < páginas superpuestas. Al agrandar se
// guarda dónde estaba la ventana; al achicar vuelve exactamente a ese lugar.
void MainWindow::resizeAnimated(const QSize& wanted, int newLevel)
{
    QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QRect avail = scr->availableGeometry();
    const QSize size = wanted.boundedTo(avail.size() - QSize(16, 16));
    const QRect cur = (m_anim && m_anim->state() == QAbstractAnimation::Running)
                          ? m_anim->endValue().toRect() : geometry();

    const int oldLevel = m_level;
    m_level = newLevel;
    if (newLevel > oldLevel) {
        m_savedRects[oldLevel] = cur;
    } else if (newLevel < oldLevel && m_savedRects.contains(newLevel)) {
        QRect saved = m_savedRects.value(newLevel);
        for (int l = newLevel; l <= 3; ++l) m_savedRects.remove(l);
        if (avail.intersects(saved)) {
            saved.setSize(size);
            animateGeometry(cur, saved);
            return;
        }
    }

    // Crece hacia el lado donde hay lugar: si la ventana está a la derecha de la
    // pantalla, se agranda hacia la izquierda (y lo mismo en vertical).
    QRect target(QPoint(), size);
    if (cur.center().x() > avail.center().x()) target.moveRight(cur.right());
    else                                      target.moveLeft(cur.left());
    if (cur.center().y() > avail.center().y()) target.moveBottom(cur.bottom());
    else                                      target.moveTop(cur.top());
    if (target.left() < avail.left() + 8) target.moveLeft(avail.left() + 8);
    if (target.right() > avail.right() - 8) target.moveRight(avail.right() - 8);
    if (target.top() < avail.top() + 8) target.moveTop(avail.top() + 8);
    if (target.bottom() > avail.bottom() - 8) target.moveBottom(avail.bottom() - 8);

    animateGeometry(cur, target);
}

void MainWindow::animateGeometry(const QRect& from, const QRect& to)
{
    m_userResizing = false;   // los cambios que hace la app no se guardan como "tu tamaño"
    m_saveSizeTimer.stop();
    if (!isVisible()) {
        setGeometry(to);
        return;
    }
    if (m_anim) m_anim->stop();
    m_anim = new QPropertyAnimation(this, "geometry", this);
    m_anim->setDuration(160);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    m_anim->setStartValue(from);
    m_anim->setEndValue(to);
    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

Qt::Edges MainWindow::edgesAt(const QPoint& p) const
{
    Qt::Edges e;
    if (p.x() < kResizeMargin)            e |= Qt::LeftEdge;
    if (p.x() >= width() - kResizeMargin) e |= Qt::RightEdge;
    if (p.y() < kResizeMargin)            e |= Qt::TopEdge;
    if (p.y() >= height() - kResizeMargin) e |= Qt::BottomEdge;
    return e;
}

void MainWindow::setEdgeCursor(Qt::Edges e)
{
    if (e == m_cursorEdges && (m_cursorOverridden == bool(e))) return;
    m_cursorEdges = e;
    if (!e) {
        if (m_cursorOverridden) QApplication::restoreOverrideCursor();
        m_cursorOverridden = false;
        return;
    }
    Qt::CursorShape shape = Qt::SizeHorCursor;
    const bool h = e & (Qt::LeftEdge | Qt::RightEdge);
    const bool v = e & (Qt::TopEdge | Qt::BottomEdge);
    if (h && v) {
        const bool mainDiag = (e & Qt::LeftEdge && e & Qt::TopEdge) || (e & Qt::RightEdge && e & Qt::BottomEdge);
        shape = mainDiag ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor;
    } else if (v) {
        shape = Qt::SizeVerCursor;
    }
    if (m_cursorOverridden) QApplication::changeOverrideCursor(shape);
    else QApplication::setOverrideCursor(shape);
    m_cursorOverridden = true;
}

// Detecta el mouse sobre los bordes de la ventana (aunque esté sobre un hijo)
// para mostrar el cursor de redimensión y arrancar el resize nativo de Windows.
bool MainWindow::eventFilter(QObject* obj, QEvent* ev)
{
    const QEvent::Type t = ev->type();
    if (t != QEvent::MouseMove && t != QEvent::MouseButtonPress && t != QEvent::Leave)
        return QWidget::eventFilter(obj, ev);
    auto* w = qobject_cast<QWidget*>(obj);
    if (!w || w->window() != this) return QWidget::eventFilter(obj, ev);

    if (!m_resizable || m_docked || isMinimized() || m_dragging) {
        setEdgeCursor({});
        return QWidget::eventFilter(obj, ev);
    }
    if (t == QEvent::Leave) {
        if (obj == this) setEdgeCursor({});
        return QWidget::eventFilter(obj, ev);
    }
    auto* me = static_cast<QMouseEvent*>(ev);
    const Qt::Edges edges = edgesAt(mapFromGlobal(me->globalPosition().toPoint()));
    if (t == QEvent::MouseMove && me->buttons() == Qt::NoButton) {
        setEdgeCursor(edges);
    } else if (t == QEvent::MouseButtonPress && me->button() == Qt::LeftButton && edges) {
        if (windowHandle() && windowHandle()->startSystemResize(edges)) {
            if (m_anim) m_anim->stop();
            m_userResizing = true;
            return true;
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void MainWindow::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    // Tamaño que dejaste a mano: se recuerda para siempre para esta vista.
    if (m_userResizing) m_saveSizeTimer.start();
}

// ===========================================================================
// Vista
// ===========================================================================
void MainWindow::setZoom(GridWidget::Zoom z)
{
    if (m_overlay != Overlay::None) {
        m_overlay = Overlay::None;
        m_pages->setCurrentIndex(PageGrid);
    }
    m_grid->setZoom(z);
    m_btnZoomIn->setEnabled(z != GridWidget::Zoom::Day);
    m_btnZoomOut->setEnabled(z != GridWidget::Zoom::Month);
    refreshBand();
    reloadGrid();
    applyViewSize();
}

void MainWindow::showOverlay(Overlay o)
{
    if (o == m_overlay) return;
    m_overlay = o;
    switch (o) {
    case Overlay::Stats:
        m_stats->resetToToday();
        m_stats->refresh();
        m_pages->setCurrentIndex(PageStats);
        break;
    case Overlay::Activities:
        m_activities->refresh();
        m_pages->setCurrentIndex(PageActivities);
        break;
    case Overlay::None:
        m_pages->setCurrentIndex(PageGrid);
        break;
    }
    applyViewSize();
}

void MainWindow::navigate(int dir)
{
    QDate a = m_grid->anchor();
    switch (m_grid->zoom()) {
    case GridWidget::Zoom::Day:   a = a.addDays(dir); break;
    case GridWidget::Zoom::Week:  a = a.addDays(7 * dir); break;
    case GridWidget::Zoom::Month: a = a.addMonths(dir); break;
    }
    m_grid->setAnchor(a);
    refreshBand();
    reloadGrid();   // también refresca el reproductor (solo se arranca hoy)
}

void MainWindow::goToday()
{
    m_grid->setAnchor(m_today);
    refreshBand();
    reloadGrid();
}

void MainWindow::refreshBand()
{
    const QDate a = m_grid->anchor();
    bool showingToday = false;
    switch (m_grid->zoom()) {
    case GridWidget::Zoom::Day:
        m_dateLabel->setText(m_numericDate ? fmt::numericDate(a) : fmt::longDate(a));
        showingToday = a == m_today;
        break;
    case GridWidget::Zoom::Week:
        m_dateLabel->setText(m_numericDate ? fmt::numericWeekRange(fmt::mondayOf(a))
                                           : fmt::weekRange(fmt::mondayOf(a)));
        showingToday = fmt::mondayOf(a) == fmt::mondayOf(m_today);
        break;
    case GridWidget::Zoom::Month:
        m_dateLabel->setText(m_numericDate ? fmt::numericMonth(a) : fmt::monthYear(a));
        showingToday = a.year() == m_today.year() && a.month() == m_today.month();
        break;
    }
    const bool day = m_grid->zoom() == GridWidget::Zoom::Day;
    m_titleEdit->setVisible(day);
    if (day) {
        QSignalBlocker b(m_titleEdit);
        m_titleEdit->setText(m_db->dayTitle(a));
    }
    m_btnToday->setVisible(!showingToday);
}

void MainWindow::reloadGrid()
{
    // "Ver esta actividad" filtra los cuadraditos (y su numeración); "todo el día" muestra todo.
    const QSet<qint64> scope = m_tree.subtree(m_session->folderId());
    QVector<Pomodoro> pomos;
    for (const Pomodoro& p : m_db->pomodorosBetween(m_grid->rangeStart(), m_grid->rangeEnd()))
        if (!m_liveActivityScope || scope.contains(p.folderId)) pomos.append(p);
    m_grid->setData(pomos, m_db->dayTitles(m_grid->rangeStart(), m_grid->rangeEnd()));
    m_grid->setLive(m_session->currentPomodoro(), m_session->progress());

    // Pomodoros de hoy ya guardados (para numerar y para las stats en vivo).
    const auto running = m_session->currentPomodoro();
    m_todayPomos.clear();
    for (const Pomodoro& p : m_db->pomodorosBetween(m_today, m_today))
        if (!running || p.id != running->id) m_todayPomos.append(p);
    refreshDeck();
    refreshLiveStats();
}

void MainWindow::reloadFolders()
{
    m_tree = FolderTree(m_db->folders());
    QHash<qint64, QString> paths;
    for (const Folder& f : m_tree.all()) paths.insert(f.id, m_tree.path(f.id));
    if (m_grid) m_grid->setFolderPaths(paths);
    if (m_session->folderId() && !m_tree.contains(m_session->folderId()))
        m_session->setFolderId(m_tree.children(0).isEmpty() ? 0 : m_tree.children(0).first());
    if (m_folderMenu) rebuildFolderMenu();
    if (m_stats && m_overlay == Overlay::Stats) m_stats->refresh();
    if (m_deck) {
        refreshDeck();
        refreshLiveStats();
    }
}

void MainWindow::chooseFolder(qint64 id)
{
    if (m_session->switchWouldPark(id)) {
        const auto cur = m_session->currentPomodoro();
        QMessageBox box(this);
        box.setIcon(QMessageBox::Question);
        box.setWindowTitle(QStringLiteral("Cambiar de actividad"));
        box.setText(QStringLiteral("Tu pomodoro de «%1» (lleva %2) queda en pausa.")
                        .arg(m_tree.find(cur->folderId) ? m_tree.find(cur->folderId)->name : QStringLiteral("Sin actividad"),
                             fmt::duration(cur->studiedSec)));
        box.setInformativeText(QStringLiteral("Lo retomás cuando vuelvas a esa actividad. Si no lo retomás hoy, "
                                              "se cierra solo con el tiempo que lleve."));
        QPushButton* ok = box.addButton(QStringLiteral("Cambiar a «%1»").arg(m_tree.find(id) ? m_tree.find(id)->name : QString()),
                                        QMessageBox::AcceptRole);
        box.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
        win::applyChrome(&box);
        box.exec();
        if (box.clickedButton() != ok) {
            rebuildFolderMenu();   // vuelve a marcar la actividad actual
            return;
        }
    }
    m_session->setFolderId(id);
    m_db->setSetting("last_folder", id);
    rebuildFolderMenu();
    reloadGrid();
}

// Menú de actividades con un ⏸ en las que tienen un pomodoro en pausa.
void MainWindow::rebuildFolderMenu()
{
    QHash<qint64, QString> badges;
    const auto parked = m_session->parkedSeconds();
    for (auto it = parked.cbegin(); it != parked.cend(); ++it)
        badges.insert(it.key(), QStringLiteral("⏸ %1").arg(fmt::duration(it.value())));
    m_folderMenu->rebuild(m_tree, m_session->folderId(), badges);
}

// Pomodoros ya terminados hoy dentro de lo que se está mostrando (actividad o día).
int MainWindow::todayCount() const
{
    const QSet<qint64> scope = m_tree.subtree(m_session->folderId());
    int n = 0;
    for (const Pomodoro& p : m_todayPomos)
        if ((p.status == PomoStatus::Completed || p.status == PomoStatus::Final) &&
            (!m_liveActivityScope || scope.contains(p.folderId)))
            ++n;
    return n;
}

void MainWindow::refreshDeck()
{
    using S = SessionController::State;
    using M = TimerDeck::Mode;
    TimerDeck::Model m;
    m.activity = m_tree.path(m_session->folderId());
    const auto cur = m_session->currentPomodoro();
    switch (m_session->state()) {
    case S::Idle:
    case S::Ready:     m.mode = M::Ready; break;
    case S::Running:   m.mode = M::Running; break;
    case S::Paused:    m.mode = M::Paused; break;
    case S::WorkDone:  m.mode = M::BreakReady; break;
    case S::Break:     m.mode = M::Break; break;
    case S::BreakOver: m.mode = M::BreakOver; break;
    }
    // Número: el que corre, o el próximo.
    m.number = todayCount() + 1;
    if (m.mode == M::BreakReady || m.mode == M::Break) m.number = todayCount();
    m.remainingSec = m_session->remainingSec();
    m.totalSec = cur ? cur->plannedSec : (m.mode == M::Break ? m_session->breakSec() : m_session->workSec());
    m.durationMin = (m.mode == M::BreakReady ? m_session->breakSec() : m_session->workSec()) / 60;
    // Los pomodoros se registran solo hoy: viendo otro día, el reproductor ofrece volver.
    m.canStart = m_grid->zoom() != GridWidget::Zoom::Day || m_grid->anchor() == m_today;
    m_deck->setModel(m);
}

void MainWindow::refreshLiveStats()
{
    const auto cur = m_session->currentPomodoro();
    const qint64 folder = m_session->folderId();
    const QSet<qint64> scope = m_tree.subtree(folder);

    LiveStats::Data d;
    d.activityScope = m_liveActivityScope;
    d.scopeName = m_tree.find(folder) ? m_tree.find(folder)->name : QStringLiteral("Sin actividad");
    QSet<qint64> activities;
    for (const Pomodoro& p : m_todayPomos) {
        if (m_liveActivityScope && !scope.contains(p.folderId)) continue;
        if (p.status == PomoStatus::Completed || p.status == PomoStatus::Final) ++d.pomos;
        d.seconds += p.studiedSec;          // los que están en pausa suman minutos
        activities.insert(p.folderId);
    }
    if (cur && (!m_liveActivityScope || scope.contains(cur->folderId))) {
        d.seconds += cur->studiedSec;       // el que corre suma minutos en vivo
        activities.insert(cur->folderId);
    }
    d.activities = int(activities.size());
    if (m_liveActivityScope) {
        // Total histórico de la actividad (lo que corre ya está guardado cada 5 s).
        d.activityTotalSec = m_tree.rollUp(m_db->studyByFolder()).value(folder);
    }
    m_liveStats->setData(d);
}

void MainWindow::onTick()
{
    m_grid->setLive(m_session->currentPomodoro(), m_session->progress());
    refreshDeck();
    // Las stats en vivo cambian de a minutos: alcanza con refrescarlas cada 5 s.
    static int n = 0;
    if (++n % 20 == 0) refreshLiveStats();
}

// ===========================================================================
// Jornada
// ===========================================================================
void MainWindow::onStateChanged(SessionController::State s)
{
    using S = SessionController::State;
    if (s != S::BreakOver) m_sounds->stopAlarm();
    // Mientras hay algo en marcha, el último pomodoro de hoy no es el final del día.
    m_grid->setTodayActive(s == S::Running || s == S::Paused || s == S::Break);
    // Al arrancar un pomodoro, la grilla vuelve a mostrar hoy.
    if (s == S::Running && m_grid->zoom() == GridWidget::Zoom::Day && m_grid->anchor() != m_today) goToday();
    reloadGrid();
}

void MainWindow::togglePlay()
{
    if (m_titleEdit->hasFocus() || m_overlay != Overlay::None) return;
    using S = SessionController::State;
    switch (m_session->state()) {
    case S::Idle:
    case S::Ready:
        if (m_grid->zoom() == GridWidget::Zoom::Day && m_grid->anchor() != m_today) goToday();
        else m_session->start();
        break;
    case S::BreakOver: m_session->start(); break;
    case S::Running:   m_session->pause(); break;
    case S::Paused:    m_session->resume(); break;
    case S::WorkDone:  m_session->startBreak(); break;
    case S::Break:     break;
    }
}

void MainWindow::onStudyRecorded(qint64 folderId, qint64 seconds)
{
    if (seconds <= 0) return;
    const auto totals = m_tree.rollUp(m_db->studyByFolder());
    for (qint64 id : m_tree.ancestorsAndSelf(folderId)) {
        const qint64 after = totals.value(id);
        for (int h : stats::milestonesCrossed(after - seconds, after)) {
            if (!m_db->markMilestone(id, h)) continue;
            const QString name = m_tree.find(id)->name;
            const QString msg = h <= 10
                ? QStringLiteral("%1: ¡llegaste a %2 %3!").arg(name).arg(h).arg(h == 1 ? "hora" : "horas")
                : QStringLiteral("%1: ¡%2 horas! (de 10.000)").arg(name).arg(h);
            toast(msg);
            if (m_tray) m_tray->showMessage(QStringLiteral("UltimateTracker"), msg, QSystemTrayIcon::Information, 5000);
        }
    }
}

void MainWindow::showPomodoroMenu(qint64 id, const QPoint& globalPos)
{
    const auto p = m_db->pomodoro(id);
    if (!p) return;
    const bool running = p->status == PomoStatus::Running;
    const bool parked = p->status == PomoStatus::Parked;

    QMenu menu(this);
    auto* move = new FolderMenu(&menu);
    move->setTitle(QStringLiteral("Mover a actividad"));
    move->rebuild(m_tree, p->folderId);
    connect(move, &FolderMenu::folderChosen, this, [this, id, running, parked](qint64 folder) {
        if (running) {
            m_session->moveCurrentTo(folder);  // el actual (corriendo o pausado): se corrige su actividad
            m_db->setSetting("last_folder", folder);
            rebuildFolderMenu();
            reloadGrid();
        } else if (parked) {
            if (!m_session->moveParked(id, folder))
                toast(QStringLiteral("%1 ya tiene un pomodoro en pausa").arg(m_tree.path(folder)));
            else
                toast(QStringLiteral("Pomodoro en pausa movido a %1").arg(m_tree.path(folder)));
        } else {
            m_db->setPomodoroFolder(id, folder);
            reloadGrid();
            toast(QStringLiteral("Pomodoro movido a %1").arg(m_tree.path(folder)));
        }
    });
    connect(move, &FolderMenu::manageRequested, this, [this] { showOverlay(Overlay::Activities); });
    menu.addMenu(move);
    menu.addSeparator();
    QAction* del = menu.addAction(QStringLiteral("Borrar pomodoro…"));
    connect(del, &QAction::triggered, this, [this, id] { deletePomodoro(id); });
    menu.exec(globalPos);
}

void MainWindow::deletePomodoro(qint64 id)
{
    const auto p = m_db->pomodoro(id);
    if (!p) return;
    const bool running = p->status == PomoStatus::Running;
    const bool parked = p->status == PomoStatus::Parked;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Borrar pomodoro"));
    box.setText(QStringLiteral("¿Borrar este pomodoro?"));
    box.setInformativeText(
        QStringLiteral("%1 · %2, empezado el %3 a las %4.\n\n"
                       "Se descuenta de tus horas y de las estadísticas. No se puede deshacer.")
            .arg(m_tree.path(p->folderId), fmt::duration(p->studiedSec), fmt::shortDate(p->day),
                 QDateTime::fromSecsSinceEpoch(p->startedAt).time().toString("HH:mm")));
    QPushButton* del = box.addButton(QStringLiteral("Borrar"), QMessageBox::DestructiveRole);
    box.addButton(QStringLiteral("Cancelar"), QMessageBox::RejectRole);
    win::applyChrome(&box);
    box.exec();
    if (box.clickedButton() != del) return;

    if (running) m_session->discardCurrent();        // el actual (corriendo o pausado)
    else if (parked) m_session->deleteParked(id);     // en pausa por cambio de actividad
    else m_db->deletePomodoro(id);
    reloadGrid();
    toast(QStringLiteral("Pomodoro borrado"));
}

// La jornada termina con el día: pasada la medianoche, cuando no hay nada en
// marcha, se cierra sola y lo que quedó pendiente pasa a ser tiempo inactivo.
void MainWindow::checkDayRollover()
{
    const QDate now = dayclock::today();
    // Si hay algo en marcha se espera a que termine (el pomodoro cuenta para el día en que empezó).
    if (m_sessionDay < now && !m_session->inProgress()) {
        m_session->closeDay(true);
        m_sessionDay = now;
        rebuildFolderMenu();
    }
    if (now != m_today) {
        const bool wasOnToday = m_grid->anchor() == m_today;
        m_today = now;
        m_grid->setToday(now);
        if (wasOnToday) m_grid->setAnchor(now);
        refreshBand();
        reloadGrid();
    }
    maybeShowDaySummary();
}

// Al terminar el día te cuenta cómo te fue (y cuántos recreos indebidos tomaste).
// Aparece la próxima vez que mirás la ventana, una sola vez por día.
void MainWindow::maybeShowDaySummary()
{
    if (!isVisible() || m_docked || isMinimized()) return;
    QDate last;
    for (const QDate& d : m_db->studiedDays())
        if (d < m_today && (!last.isValid() || d > last)) last = d;
    if (!last.isValid()) return;
    const QDate shown = QDate::fromString(m_db->setting("summary/last_shown").toString(), Qt::ISODate);
    if (shown.isValid() && shown >= last) return;
    m_db->setSetting("summary/last_shown", last.toString(Qt::ISODate));

    const auto t = stats::totals(*m_db, last, last, nullptr, dayclock::dayStart(last).addSecs(86399));
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Resumen del día"));
    box.setText(QStringLiteral("<b>Resumen del %1</b>").arg(fmt::longDate(last)));
    box.setInformativeText(QStringLiteral("Estudiaste %1\nRecreos: %2\nRecreos indebidos: %3")
                               .arg(fmt::hours(t.study), fmt::hours(t.breaks), fmt::hours(t.undue)));
    box.setStandardButtons(QMessageBox::Ok);
    win::applyChrome(&box);
    box.exec();
}

// ===========================================================================
// Ventana: arrastre, borde, bandeja
// ===========================================================================
void MainWindow::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton) {
        m_dragging = true;
        m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
    }
}

void MainWindow::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) move(e->globalPosition().toPoint() - m_dragOffset);
}

void MainWindow::mouseReleaseEvent(QMouseEvent* e)
{
    if (!m_dragging) return;
    m_dragging = false;
    m_db->setSetting("window_pos", pos());

    // Si soltaste la ventana contra un borde de la pantalla, se esconde ahí.
    QScreen* scr = QGuiApplication::screenAt(e->globalPosition().toPoint());
    if (!scr) return;
    const QRect s = scr->geometry();
    const QPoint c = e->globalPosition().toPoint();
    constexpr int kEdge = 3;
    if (c.x() <= s.left() + kEdge)        dockTo(EdgeTab::Edge::Left);
    else if (c.x() >= s.right() - kEdge)  dockTo(EdgeTab::Edge::Right);
    else if (c.y() <= s.top() + kEdge)    dockTo(EdgeTab::Edge::Top);
    else if (c.y() >= s.bottom() - kEdge) dockTo(EdgeTab::Edge::Bottom);
}

void MainWindow::wheelEvent(QWheelEvent* e)
{
    if (!(e->modifiers() & Qt::ControlModifier) || m_overlay != Overlay::None) return;
    const int dir = e->angleDelta().y() < 0 ? 1 : -1;
    setZoom(GridWidget::Zoom(qBound(0, int(m_grid->zoom()) + dir, 2)));
}

void MainWindow::dockToNearestEdge()
{
    QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QRect s = scr->availableGeometry();
    const QRect g = frameGeometry();
    const int dl = g.left() - s.left(), dr = s.right() - g.right();
    const int dt = g.top() - s.top(), db = s.bottom() - g.bottom();
    const int m = qMin(qMin(dl, dr), qMin(dt, db));
    if (m == dl)      dockTo(EdgeTab::Edge::Left);
    else if (m == dr) dockTo(EdgeTab::Edge::Right);
    else if (m == dt) dockTo(EdgeTab::Edge::Top);
    else              dockTo(EdgeTab::Edge::Bottom);
}

void MainWindow::dockTo(EdgeTab::Edge edge)
{
    QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QRect s = scr->geometry();
    const QPoint c = frameGeometry().center();
    const bool vertical = edge == EdgeTab::Edge::Left || edge == EdgeTab::Edge::Right;
    m_edgeTab->attach(edge, s, vertical ? c.y() : c.x());
    m_docked = true;
    setEdgeCursor({});

    // Se desliza hacia el borde mientras se desvanece; recién ahí aparece la pestañita.
    if (m_anim) m_anim->stop();
    if (m_dockAnim) m_dockAnim->stop();
    const QRect from = geometry();
    auto* group = new QParallelAnimationGroup(this);
    auto* slide = new QPropertyAnimation(this, "geometry", group);
    slide->setDuration(190);
    slide->setEasingCurve(QEasingCurve::InCubic);
    slide->setStartValue(from);
    slide->setEndValue(offscreenRect(from, edge, s));
    auto* fade = new QPropertyAnimation(this, "windowOpacity", group);
    fade->setDuration(190);
    fade->setStartValue(1.0);
    fade->setEndValue(0.0);
    group->addAnimation(slide);
    group->addAnimation(fade);
    connect(group, &QAbstractAnimation::finished, this, [this, from] {
        hide();
        setWindowOpacity(1.0);
        setGeometry(from);
        m_edgeTab->show();
        m_edgeTab->raise();
    });
    m_dockAnim = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

QRect MainWindow::offscreenRect(const QRect& r, EdgeTab::Edge edge, const QRect& screen) const
{
    QRect out = r;
    switch (edge) {
    case EdgeTab::Edge::Left:   out.moveRight(screen.left() - 1); break;
    case EdgeTab::Edge::Right:  out.moveLeft(screen.right() + 1); break;
    case EdgeTab::Edge::Top:    out.moveBottom(screen.top() - 1); break;
    case EdgeTab::Edge::Bottom: out.moveTop(screen.bottom() + 1); break;
    }
    return out;
}

void MainWindow::undock()
{
    if (!m_docked) return;
    m_docked = false;
    m_edgeTab->hide();

    QScreen* scr = m_edgeTab->screen() ? m_edgeTab->screen() : QGuiApplication::primaryScreen();
    const QRect s = scr->availableGeometry();
    const QPoint tab = m_edgeTab->geometry().center();
    QRect g(QPoint(), size());
    constexpr int m = 8;
    switch (m_edgeTab->edge()) {
    case EdgeTab::Edge::Left:   g.moveLeft(s.left() + m);    g.moveTop(tab.y() - g.height() / 2); break;
    case EdgeTab::Edge::Right:  g.moveRight(s.right() - m);  g.moveTop(tab.y() - g.height() / 2); break;
    case EdgeTab::Edge::Top:    g.moveTop(s.top() + m);      g.moveLeft(tab.x() - g.width() / 2); break;
    case EdgeTab::Edge::Bottom: g.moveBottom(s.bottom() - m); g.moveLeft(tab.x() - g.width() / 2); break;
    }
    g.moveLeft(qBound(s.left() + m, g.left(), s.right() - g.width() - m));
    g.moveTop(qBound(s.top() + m, g.top(), s.bottom() - g.height() - m));

    // Sale desde el borde, apareciendo.
    if (m_dockAnim) m_dockAnim->stop();
    const QRect from = offscreenRect(g, m_edgeTab->edge(), scr->geometry());
    setWindowOpacity(0.0);
    setGeometry(from);
    show();
    win::forceForeground(this, m_btnPin->isChecked());
    auto* group = new QParallelAnimationGroup(this);
    auto* slide = new QPropertyAnimation(this, "geometry", group);
    slide->setDuration(230);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    slide->setStartValue(from);
    slide->setEndValue(g);
    auto* fade = new QPropertyAnimation(this, "windowOpacity", group);
    fade->setDuration(200);
    fade->setStartValue(0.0);
    fade->setEndValue(1.0);
    group->addAnimation(slide);
    group->addAnimation(fade);
    connect(group, &QAbstractAnimation::finished, this, [this, g] {
        setWindowOpacity(1.0);
        setGeometry(g);
        maybeShowDaySummary();
    });
    m_dockAnim = group;
    group->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::bringToFront()
{
    if (m_docked) {
        undock();
        return;
    }
    if (isMinimized()) showNormal();
    show();
    win::forceForeground(this, m_btnPin->isChecked());
    QApplication::alert(this);
}

void MainWindow::togglePin(bool on)
{
    win::setTopmost(this, on);
    m_db->setSetting("pinned", on ? 1 : 0);
}

void MainWindow::toast(const QString& text)
{
    m_toast->setText(text);
    m_toast->show();
    m_toastTimer.start(8000);
}

void MainWindow::showEvent(QShowEvent* e)
{
    QWidget::showEvent(e);
    static bool first = true;
    if (first) {
        first = false;
        win::applyChrome(this);
        win::enableMinimize(this);
        if (m_db->setting("pinned", 0).toBool()) m_btnPin->setChecked(true);
        QTimer::singleShot(400, this, &MainWindow::maybeShowDaySummary);
    }
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    // Con un pomodoro, pausa o recreo en marcha, cerrar la ventana la manda a la
    // bandeja: el tiempo sigue corriendo. Si no, cierra la app.
    if (m_session->inProgress() && m_tray) {
        e->ignore();
        hide();
        if (!m_trayHintShown) {
            m_trayHintShown = true;
            m_tray->showMessage(QStringLiteral("UltimateTracker"),
                                QStringLiteral("Sigo contando en segundo plano. Te aviso cuando termine el timer."),
                                QSystemTrayIcon::Information, 4000);
        }
        return;
    }
    m_db->setSetting("window_pos", pos());
    e->accept();
    quitApp();
}

void MainWindow::quitApp()
{
    // Si había un pomodoro corriendo, se guarda hasta donde llegó. Los que están
    // en pausa por actividad quedan guardados para retomarlos hoy.
    m_session->closeDay(false);
    m_sounds->stopAlarm();
    if (m_tray) m_tray->hide();
    m_edgeTab->close();
    qApp->quit();
}
