#include "ui/MainWindow.h"
#include "core/Format.h"
#include "core/Stats.h"
#include "ui/FolderMenu.h"
#include "ui/FoldersDialog.h"
#include "ui/Icons.h"
#include "ui/SettingsDialog.h"
#include "ui/Sounds.h"
#include "ui/StatsWidget.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCloseEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QScreen>
#include <QShortcut>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

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

enum ControlPage { PageIdle, PageRunning, PagePaused, PageWorkDone, PageBreak, PageBreakOver };
} // namespace

// ===========================================================================
MainWindow::MainWindow(Database* db, QWidget* parent)
    : QWidget(parent, Qt::Window | Qt::FramelessWindowHint)
    , m_db(db)
{
    setObjectName("Root");
    setWindowTitle(QStringLiteral("Constancia"));
    setWindowIcon(icons::get(icons::Id::AppLogo));
    setAttribute(Qt::WA_StyledBackground);

    m_session = new SessionController(m_db, {}, true, this);
    m_session->recoverAfterRestart();
    m_sounds = new Sounds(this);
    m_today = QDate::currentDate();
    m_jokers = m_db->setting("jokers_per_week", 2).toInt();

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
    m_grid->setFolderPaths({});
    m_toast = new QLabel(gridPage);
    m_toast->setProperty("role", "toast");
    m_toast->setAlignment(Qt::AlignCenter);
    m_toast->hide();
    m_toastTimer.setSingleShot(true);
    connect(&m_toastTimer, &QTimer::timeout, m_toast, &QLabel::hide);

    auto* gridLay = new QVBoxLayout(gridPage);
    gridLay->setContentsMargins(0, 0, 0, 0);
    gridLay->setSpacing(0);
    gridLay->addWidget(buildDateBand());
    gridLay->addWidget(m_grid, 1);
    gridLay->addWidget(m_toast);
    gridLay->addWidget(buildControls());

    // ---- Página de estadísticas
    m_stats = new StatsWidget(m_db, this);
    m_stats->setJokersPerWeek(m_jokers);
    connect(m_stats, &StatsWidget::backRequested, this, [this] { showStats(false); });

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(gridPage);
    m_pages->addWidget(m_stats);
    // El tamaño de la ventana lo manejamos nosotros según la vista; que la
    // página de estadísticas (más ancha) no imponga su mínimo en la vista día.
    gridPage->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_stats->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(1, 1, 1, 1);
    root->setSpacing(0);
    root->addWidget(buildTitleBar());
    root->addWidget(m_pages, 1);

    m_edgeTab = new EdgeTab();
    connect(m_edgeTab, &EdgeTab::activated, this, &MainWindow::undock);
    buildTray();
    reloadFolders();   // ahora que existen la grilla y el menú de carpetas

    // ---- Señales del motor
    connect(m_session, &SessionController::stateChanged, this, &MainWindow::onStateChanged);
    connect(m_session, &SessionController::ticked, this, &MainWindow::onTick);
    connect(m_session, &SessionController::pomodoroStarted, this, &MainWindow::reloadGrid);
    connect(m_session, &SessionController::pomodoroFinalized, this, &MainWindow::reloadGrid);
    connect(m_session, &SessionController::studyRecorded, this, &MainWindow::onStudyRecorded);
    connect(m_session, &SessionController::sessionEnded, this, &MainWindow::onSessionEnded);
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

    // ---- Atajos
    auto* space = new QShortcut(QKeySequence(Qt::Key_Space), this);
    connect(space, &QShortcut::activated, this, &MainWindow::togglePlay);
    auto* zoomOut = new QShortcut(QKeySequence::ZoomOut, this);
    connect(zoomOut, &QShortcut::activated, this, [this] { setZoom(GridWidget::Zoom(qMin(2, int(m_grid->zoom()) + 1))); });
    auto* zoomIn = new QShortcut(QKeySequence::ZoomIn, this);
    connect(zoomIn, &QShortcut::activated, this, [this] { setZoom(GridWidget::Zoom(qMax(0, int(m_grid->zoom()) - 1))); });

    // Cambio de día con la app abierta.
    auto* midnight = new QTimer(this);
    midnight->setInterval(30 * 1000);
    connect(midnight, &QTimer::timeout, this, [this] {
        const QDate now = QDate::currentDate();
        if (now == m_today) return;
        const bool wasOnToday = m_grid->anchor() == m_today;
        m_today = now;
        m_grid->setToday(now);
        if (wasOnToday) m_grid->setAnchor(now);
        reloadGrid();
    });
    midnight->start();

    // ---- Estado inicial: siempre abre en el día de hoy
    resize(sizeFor(GridWidget::Zoom::Day));
    const QPoint savedPos = m_db->setting("window_pos").toPoint();
    if (!savedPos.isNull() && QGuiApplication::screenAt(savedPos + QPoint(40, 40)))
        move(savedPos);
    m_grid->setAnchor(m_today);
    m_grid->setToday(m_today);
    setZoom(GridWidget::Zoom::Day);
    onStateChanged(m_session->state());
}

// ===========================================================================
// Construcción
// ===========================================================================
QWidget* MainWindow::buildTitleBar()
{
    auto* bar = new QWidget(this);
    bar->setObjectName("TitleBar");
    bar->setFixedHeight(36);

    auto* logo = new QLabel(bar);
    logo->setPixmap(icons::get(icons::Id::AppLogo).pixmap(16, 16));
    auto* name = dimLabel(bar, QStringLiteral("Constancia"));

    m_btnProportional = iconButton(bar, icons::Id::Proportional, QStringLiteral("Tamaño proporcional al tiempo"));
    m_btnProportional->setCheckable(true);
    m_btnProportional->setChecked(m_db->setting("proportional", 0).toBool());
    connect(m_btnProportional, &QToolButton::toggled, this, [this](bool on) {
        m_grid->setProportional(on);
        m_db->setSetting("proportional", on ? 1 : 0);
    });

    m_btnZoomIn = iconButton(bar, icons::Id::ZoomIn, QStringLiteral("Acercar (Ctrl +)"));
    m_btnZoomOut = iconButton(bar, icons::Id::ZoomOut, QStringLiteral("Alejar: semana, mes (Ctrl −)"));
    connect(m_btnZoomIn, &QToolButton::clicked, this, [this] { setZoom(GridWidget::Zoom(qMax(0, int(m_grid->zoom()) - 1))); });
    connect(m_btnZoomOut, &QToolButton::clicked, this, [this] { setZoom(GridWidget::Zoom(qMin(2, int(m_grid->zoom()) + 1))); });

    auto* stats = iconButton(bar, icons::Id::Stats, QStringLiteral("Estadísticas"));
    connect(stats, &QToolButton::clicked, this, [this] { showStats(!m_inStats); });

    auto* menuBtn = iconButton(bar, icons::Id::Menu, QStringLiteral("Más"));
    auto* menu = new QMenu(menuBtn);
    menu->addAction(QStringLiteral("Carpetas…"), this, [this] {
        FoldersDialog dlg(m_db, this);
        win::applyChrome(&dlg);
        connect(&dlg, &FoldersDialog::foldersChanged, this, &MainWindow::reloadFolders);
        dlg.exec();
    });
    menu->addAction(QStringLiteral("Ajustes…"), this, [this] {
        SettingsDialog dlg(m_db->setting("work_min", 25).toInt(), m_db->setting("break_min", 10).toInt(), m_jokers, this);
        win::applyChrome(&dlg);
        if (dlg.exec() != QDialog::Accepted) return;
        m_db->setSetting("work_min", dlg.workMinutes());
        m_db->setSetting("break_min", dlg.breakMinutes());
        m_db->setSetting("jokers_per_week", dlg.jokersPerWeek());
        m_jokers = dlg.jokersPerWeek();
        m_stats->setJokersPerWeek(m_jokers);
        if (!m_session->sessionOpen()) {
            m_session->setWorkSec(dlg.workMinutes() * 60);
            m_session->setBreakSec(dlg.breakMinutes() * 60);
            refreshControls();
        }
    });
    menu->addSeparator();
    menu->addAction(QStringLiteral("Salir"), this, &MainWindow::quitApp);
    connect(menuBtn, &QToolButton::clicked, this, [menuBtn, menu] {
        menu->popup(menuBtn->mapToGlobal(QPoint(0, menuBtn->height())));
    });

    m_btnPin = iconButton(bar, icons::Id::Pin, QStringLiteral("Siempre visible"));
    m_btnPin->setCheckable(true);
    connect(m_btnPin, &QToolButton::toggled, this, &MainWindow::togglePin);

    auto* dock = iconButton(bar, icons::Id::Dock, QStringLiteral("Esconder en el borde (o arrastrá la ventana contra un borde)"));
    connect(dock, &QToolButton::clicked, this, &MainWindow::dockToNearestEdge);

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

    auto* lay = new QHBoxLayout(m_band);
    lay->setContentsMargins(4, 0, 8, 0);
    lay->setSpacing(4);
    lay->addWidget(prev);
    lay->addWidget(m_dateLabel);
    lay->addWidget(next);
    lay->addSpacing(8);
    lay->addWidget(m_titleEdit, 1);
    lay->addWidget(m_btnToday);

    outerLay->addWidget(m_band);
    return outer;
}

QWidget* MainWindow::buildControls()
{
    m_controls = new QStackedWidget(this);
    m_controls->setObjectName("Controls");
    m_controls->setFixedHeight(58);

    auto page = [this]() {
        auto* w = new QWidget(m_controls);
        auto* lay = new QHBoxLayout(w);
        lay->setContentsMargins(12, 6, 12, 10);
        lay->setSpacing(8);
        m_controls->addWidget(w);
        return lay;
    };

    // --- Listo para arrancar (sin sesión o entre pomodoros)
    {
        auto* lay = page();
        m_folderMenu = new FolderMenu(this);
        connect(m_folderMenu, &FolderMenu::folderChosen, this, &MainWindow::chooseFolder);
        connect(m_folderMenu, &FolderMenu::manageRequested, this, [this] {
            FoldersDialog dlg(m_db, this);
            win::applyChrome(&dlg);
            connect(&dlg, &FoldersDialog::foldersChanged, this, &MainWindow::reloadFolders);
            dlg.exec();
        });
        m_folderBtn = new QToolButton;
        m_folderBtn->setProperty("role", "folder");
        m_folderBtn->setIcon(icons::get(icons::Id::Folder, theme::textDim));
        m_folderBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        m_folderBtn->setPopupMode(QToolButton::InstantPopup);
        m_folderBtn->setMenu(m_folderMenu);
        m_folderBtn->setFocusPolicy(Qt::NoFocus);
        m_folderBtn->setMaximumWidth(220);

        m_workSpin = new QSpinBox;
        m_workSpin->setRange(1, 240);
        m_workSpin->setSuffix(QStringLiteral(" min"));
        m_workSpin->setToolTip(QStringLiteral("Duración del pomodoro"));
        m_workSpin->setValue(m_session->workSec() / 60);
        connect(m_workSpin, &QSpinBox::valueChanged, this, [this](int v) { m_session->setWorkSec(v * 60); });

        m_todayLabel = dimLabel(nullptr);
        auto* play = roundButton(nullptr, icons::Id::Play, QStringLiteral("Arrancar (Espacio)"));
        connect(play, &QToolButton::clicked, m_session, &SessionController::start);

        lay->addWidget(m_folderBtn);
        lay->addWidget(m_workSpin);
        lay->addStretch();
        lay->addWidget(m_todayLabel);
        lay->addWidget(play);
    }
    // --- Corriendo
    {
        auto* lay = page();
        m_runFolder = dimLabel(nullptr);
        m_runClock = new QLabel;
        m_runClock->setProperty("role", "clock");
        auto* end = textButton(nullptr, QStringLiteral("Terminar sesión"), "link");
        connect(end, &QPushButton::clicked, m_session, &SessionController::endSession);
        auto* pause = roundButton(nullptr, icons::Id::Pause, QStringLiteral("Pausar (Espacio)"));
        connect(pause, &QToolButton::clicked, m_session, &SessionController::pause);
        lay->addWidget(m_runFolder, 1);
        lay->addWidget(m_runClock);
        lay->addSpacing(6);
        lay->addWidget(end);
        lay->addWidget(pause);
    }
    // --- En pausa
    {
        auto* lay = page();
        m_pausedLabel = new QLabel;
        m_pausedLabel->setProperty("role", "warn");
        auto* end = textButton(nullptr, QStringLiteral("Terminar sesión"), "link");
        connect(end, &QPushButton::clicked, m_session, &SessionController::endSession);
        auto* resume = roundButton(nullptr, icons::Id::Play, QStringLiteral("Seguir (Espacio)"));
        connect(resume, &QToolButton::clicked, m_session, &SessionController::resume);
        lay->addWidget(m_pausedLabel, 1);
        lay->addWidget(end);
        lay->addWidget(resume);
    }
    // --- Terminó el pomodoro
    {
        auto* lay = page();
        auto* startBreak = textButton(nullptr, QStringLiteral("Iniciar recreo"), "primary");
        connect(startBreak, &QPushButton::clicked, m_session, &SessionController::startBreak);
        m_breakSpin = new QSpinBox;
        m_breakSpin->setRange(1, 120);
        m_breakSpin->setSuffix(QStringLiteral(" min"));
        m_breakSpin->setValue(m_session->breakSec() / 60);
        connect(m_breakSpin, &QSpinBox::valueChanged, this, [this](int v) { m_session->setBreakSec(v * 60); });
        m_doneLabel = dimLabel(nullptr);
        auto* skip = textButton(nullptr, QStringLiteral("Saltear recreo"), "link");
        connect(skip, &QPushButton::clicked, m_session, &SessionController::skipBreak);
        auto* end = textButton(nullptr, QStringLiteral("Terminar sesión"), "link");
        connect(end, &QPushButton::clicked, m_session, &SessionController::endSession);
        lay->addWidget(startBreak);
        lay->addWidget(m_breakSpin);
        lay->addWidget(m_doneLabel, 1);
        lay->addWidget(skip);
        lay->addWidget(end);
    }
    // --- Recreo
    {
        auto* lay = page();
        auto* label = dimLabel(nullptr, QStringLiteral("Recreo"));
        m_breakClock = new QLabel;
        m_breakClock->setProperty("role", "clock");
        auto* endBreak = textButton(nullptr, QStringLiteral("Terminar recreo"), "link");
        connect(endBreak, &QPushButton::clicked, m_session, &SessionController::endBreakEarly);
        auto* end = textButton(nullptr, QStringLiteral("Terminar sesión"), "link");
        connect(end, &QPushButton::clicked, m_session, &SessionController::endSession);
        lay->addWidget(label);
        lay->addWidget(m_breakClock);
        lay->addStretch();
        lay->addWidget(endBreak);
        lay->addWidget(end);
    }
    // --- Se terminó el recreo (alarma sonando)
    {
        auto* lay = page();
        m_overLabel = new QLabel;
        m_overLabel->setProperty("role", "warn");
        auto* silence = textButton(nullptr, QStringLiteral("Silenciar"), "link");
        connect(silence, &QPushButton::clicked, m_session, &SessionController::silenceAlarm);
        auto* end = textButton(nullptr, QStringLiteral("Terminar sesión"), "link");
        connect(end, &QPushButton::clicked, m_session, &SessionController::endSession);
        auto* next = roundButton(nullptr, icons::Id::Play, QStringLiteral("Siguiente pomodoro (Espacio)"));
        connect(next, &QToolButton::clicked, m_session, &SessionController::start);
        lay->addWidget(m_overLabel, 1);
        lay->addWidget(silence);
        lay->addWidget(end);
        lay->addWidget(next);
    }
    return m_controls;
}

void MainWindow::buildTray()
{
    if (!QSystemTrayIcon::isSystemTrayAvailable()) return;
    m_tray = new QSystemTrayIcon(icons::get(icons::Id::AppLogo), this);
    m_tray->setToolTip(QStringLiteral("Constancia"));
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
// Vista
// ===========================================================================
QSize MainWindow::sizeFor(GridWidget::Zoom z) const
{
    // Día: del tamaño de la calculadora pero apaisada.
    switch (z) {
    case GridWidget::Zoom::Day:   return {540, 340};
    case GridWidget::Zoom::Week:  return {980, 560};
    case GridWidget::Zoom::Month: return {1140, 800};
    }
    return {540, 340};
}

void MainWindow::resizeAnimated(const QSize& wanted)
{
    QScreen* scr = screen() ? screen() : QGuiApplication::primaryScreen();
    const QRect avail = scr->availableGeometry();
    const QSize size = wanted.boundedTo(avail.size() - QSize(16, 16));
    const QRect cur = geometry();

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

    if (!isVisible()) {
        setGeometry(target);
        return;
    }
    if (m_anim) m_anim->stop();
    m_anim = new QPropertyAnimation(this, "geometry", this);
    m_anim->setDuration(160);
    m_anim->setEasingCurve(QEasingCurve::OutCubic);
    m_anim->setStartValue(cur);
    m_anim->setEndValue(target);
    m_anim->start(QAbstractAnimation::DeleteWhenStopped);
}

void MainWindow::setZoom(GridWidget::Zoom z)
{
    if (m_inStats) showStats(false);
    m_grid->setZoom(z);
    m_btnZoomIn->setEnabled(z != GridWidget::Zoom::Day);
    m_btnZoomOut->setEnabled(z != GridWidget::Zoom::Month);
    refreshBand();
    reloadGrid();
    resizeAnimated(sizeFor(z));
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
    reloadGrid();
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
        m_dateLabel->setText(fmt::longDate(a));
        showingToday = a == m_today;
        break;
    case GridWidget::Zoom::Week:
        m_dateLabel->setText(fmt::weekRange(fmt::mondayOf(a)));
        showingToday = fmt::mondayOf(a) == fmt::mondayOf(m_today);
        break;
    case GridWidget::Zoom::Month:
        m_dateLabel->setText(fmt::monthYear(a));
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
    const QVector<Pomodoro> pomos = m_db->pomodorosBetween(m_grid->rangeStart(), m_grid->rangeEnd());
    m_grid->setData(pomos, m_db->dayTitles(m_grid->rangeStart(), m_grid->rangeEnd()));
    m_grid->setLive(m_session->currentPomodoro(), m_session->progress());

    // Estudio de hoy (para el contador de la barra de controles).
    const auto running = m_session->currentPomodoro();
    m_todayBaseSec = 0;
    for (const Pomodoro& p : m_db->pomodorosBetween(m_today, m_today))
        if (!running || p.id != running->id) m_todayBaseSec += p.studiedSec;
    refreshControls();
}

void MainWindow::reloadFolders()
{
    m_tree = FolderTree(m_db->folders());
    QHash<qint64, QString> paths;
    for (const Folder& f : m_tree.all()) paths.insert(f.id, m_tree.path(f.id));
    if (m_grid) m_grid->setFolderPaths(paths);
    if (m_session->folderId() && !m_tree.contains(m_session->folderId()))
        m_session->setFolderId(m_tree.children(0).isEmpty() ? 0 : m_tree.children(0).first());
    if (m_folderMenu) m_folderMenu->rebuild(m_tree, m_session->folderId());
    if (m_stats && m_inStats) m_stats->refresh();
    if (m_controls) refreshControls();
}

void MainWindow::chooseFolder(qint64 id)
{
    m_session->setFolderId(id);
    m_db->setSetting("last_folder", id);
    m_folderMenu->rebuild(m_tree, id);
    refreshControls();
    reloadGrid();
}

void MainWindow::showStats(bool on)
{
    if (on == m_inStats) return;
    m_inStats = on;
    if (on) {
        m_stats->resetToToday();
        m_stats->refresh();
        m_pages->setCurrentWidget(m_stats);
        resizeAnimated({940, 640});
    } else {
        m_pages->setCurrentIndex(0);
        resizeAnimated(sizeFor(m_grid->zoom()));
    }
}

void MainWindow::refreshControls()
{
    using S = SessionController::State;
    const S s = m_session->state();

    QString folderText = m_tree.path(m_session->folderId());
    m_folderBtn->setText(QFontMetrics(m_folderBtn->font()).elidedText(folderText, Qt::ElideLeft, 170));
    m_folderBtn->setToolTip(folderText);

    const auto running = m_session->currentPomodoro();
    const qint64 todaySec = m_todayBaseSec + (running ? running->studiedSec : 0);
    m_todayLabel->setText(todaySec > 0 ? QStringLiteral("Hoy %1").arg(fmt::duration(todaySec)) : QString());

    switch (s) {
    case S::Closed:
    case S::Ready:
        if (m_workSpin->value() != m_session->workSec() / 60) {
            QSignalBlocker b(m_workSpin);
            m_workSpin->setValue(m_session->workSec() / 60);
        }
        if (s == S::Ready && m_session->stateElapsedSec() >= 5)
            m_todayLabel->setText(QStringLiteral("indebido %1   ·   %2")
                                      .arg(fmt::clock(m_session->stateElapsedSec()),
                                           QStringLiteral("Hoy %1").arg(fmt::duration(todaySec))));
        break;
    case S::Running:
        m_runFolder->setText(QFontMetrics(m_runFolder->font()).elidedText(folderText, Qt::ElideLeft, 200));
        m_runClock->setText(fmt::clock(m_session->remainingSec()));
        break;
    case S::Paused:
        m_pausedLabel->setText(QStringLiteral("En pausa · %1 de recreo indebido")
                                   .arg(fmt::clock(m_session->stateElapsedSec())));
        break;
    case S::WorkDone:
        m_doneLabel->setText(m_session->stateElapsedSec() >= 5
                                 ? QStringLiteral("indebido %1").arg(fmt::clock(m_session->stateElapsedSec()))
                                 : QString());
        break;
    case S::Break:
        m_breakClock->setText(fmt::clock(m_session->remainingSec()));
        break;
    case S::BreakOver:
        m_overLabel->setText(QStringLiteral("Se terminó el recreo · %1 indebido")
                                 .arg(fmt::clock(m_session->stateElapsedSec())));
        break;
    }
}

void MainWindow::onTick()
{
    m_grid->setLive(m_session->currentPomodoro(), m_session->progress());
    refreshControls();
}

// ===========================================================================
// Sesión
// ===========================================================================
void MainWindow::onStateChanged(SessionController::State s)
{
    using S = SessionController::State;
    int page = PageIdle;
    switch (s) {
    case S::Closed:
    case S::Ready:     page = PageIdle; break;
    case S::Running:   page = PageRunning; break;
    case S::Paused:    page = PagePaused; break;
    case S::WorkDone:  page = PageWorkDone; break;
    case S::Break:     page = PageBreak; break;
    case S::BreakOver: page = PageBreakOver; break;
    }
    m_controls->setCurrentIndex(page);
    if (s != S::BreakOver) m_sounds->stopAlarm();
    if (s == S::WorkDone) {
        QSignalBlocker b(m_breakSpin);
        m_breakSpin->setValue(m_session->breakSec() / 60);
    }
    // Al arrancar un pomodoro, la grilla vuelve a mostrar hoy.
    if (s == S::Running && m_grid->zoom() == GridWidget::Zoom::Day && m_grid->anchor() != m_today) goToday();
    reloadGrid();
}

void MainWindow::togglePlay()
{
    if (m_titleEdit->hasFocus()) return;
    using S = SessionController::State;
    switch (m_session->state()) {
    case S::Closed:
    case S::Ready:
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
            if (m_tray) m_tray->showMessage(QStringLiteral("Constancia"), msg, QSystemTrayIcon::Information, 5000);
        }
    }
}

void MainWindow::onSessionEnded(qint64 study, qint64 brk, qint64 undue)
{
    reloadGrid();
    QMessageBox box(this);
    box.setWindowTitle(QStringLiteral("Sesión terminada"));
    box.setText(QStringLiteral("<b>Sesión terminada</b>"));
    box.setInformativeText(QStringLiteral("Estudiaste %1\nRecreos: %2\nRecreos indebidos: %3")
                               .arg(fmt::hours(study), fmt::hours(brk), fmt::hours(undue)));
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
    if (!(e->modifiers() & Qt::ControlModifier) || m_inStats) return;
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
    hide();
    m_edgeTab->show();
    m_edgeTab->raise();
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
    setGeometry(g);
    show();
    win::forceForeground(this, m_btnPin->isChecked());
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
        if (m_db->setting("pinned", 0).toBool()) m_btnPin->setChecked(true);
    }
}

void MainWindow::closeEvent(QCloseEvent* e)
{
    // Con una sesión abierta, cerrar la ventana la manda a la bandeja: el tiempo
    // sigue corriendo. Sin sesión, cierra la app.
    if (m_session->sessionOpen() && m_tray) {
        e->ignore();
        hide();
        if (!m_trayHintShown) {
            m_trayHintShown = true;
            m_tray->showMessage(QStringLiteral("Constancia"),
                                QStringLiteral("La sesión sigue abierta. Te aviso cuando termine el timer."),
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
    if (m_session->sessionOpen()) {
        // Se cierra prolijo: el último pomodoro queda como final.
        disconnect(m_session, &SessionController::sessionEnded, this, &MainWindow::onSessionEnded);
        m_session->endSession();
    }
    m_sounds->stopAlarm();
    if (m_tray) m_tray->hide();
    m_edgeTab->close();
    qApp->quit();
}
