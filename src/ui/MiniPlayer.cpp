#include "ui/MiniPlayer.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
using Mode = TimerDeck::Mode;
bool isBreak(Mode m) { return m == Mode::BreakReady || m == Mode::Break; }
} // namespace

MiniPlayer::MiniPlayer(QWidget* parent)
    : QWidget(parent, Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setWindowTitle(QStringLiteral("UltimateTracker · mini"));
    setFixedSize(300, 96);

    m_primary = new QToolButton(this);
    m_primary->setObjectName("MiniPlay");
    m_primary->setFixedSize(36, 36);
    m_primary->setIconSize(QSize(16, 16));
    m_primary->setCursor(Qt::PointingHandCursor);
    m_primary->setFocusPolicy(Qt::NoFocus);

    m_secondary = new QToolButton(this);
    m_secondary->setObjectName("MiniAction");
    m_secondary->setFixedSize(28, 28);
    m_secondary->setIconSize(QSize(14, 14));
    m_secondary->setCursor(Qt::PointingHandCursor);
    m_secondary->setFocusPolicy(Qt::NoFocus);

    m_close = new QToolButton(this);
    m_close->setObjectName("MiniClose");
    m_close->setIcon(icons::get(icons::Id::Close, QColor(0x90, 0x90, 0x90)));
    m_close->setIconSize(QSize(10, 10));
    m_close->setFixedSize(18, 18);
    m_close->setToolTip(QStringLiteral("Cerrar el mini reproductor"));
    m_close->setCursor(Qt::PointingHandCursor);
    m_close->setFocusPolicy(Qt::NoFocus);
    connect(m_close, &QToolButton::clicked, this, [this] {
        hide();
        emit closed();
    });

    connect(m_primary, &QToolButton::clicked, this, [this] {
        switch (m_model.mode) {
        case Mode::Ready:
        case Mode::Paused:
        case Mode::BreakOver:  emit playClicked(); break;
        case Mode::Running:    emit pauseClicked(); break;
        case Mode::BreakReady: emit startBreakClicked(); break;
        case Mode::Break:      emit endBreakClicked(); break;
        }
    });
    connect(m_secondary, &QToolButton::clicked, this, [this] {
        switch (m_model.mode) {
        case Mode::Running:
        case Mode::Paused:     emit endPomodoroClicked(); break;
        case Mode::BreakReady: emit skipBreakClicked(); break;
        case Mode::BreakOver:  emit silenceClicked(); break;
        default: break;
        }
    });

    // Botones a la derecha; el resto se pinta a mano.
    m_close->move(width() - 24, 6);
    m_secondary->move(width() - 80, 40);
    m_primary->move(width() - 46, 36);
    applyMode();
}

void MiniPlayer::setModel(const TimerDeck::Model& m)
{
    const bool changed = m_first || m.mode != m_model.mode || m.canStart != m_model.canStart;
    m_first = false;
    m_model = m;
    if (changed) applyMode();
    update();
}

void MiniPlayer::applyMode()
{
    const Mode m = m_model.mode;
    const QColor white(255, 255, 255);
    m_primary->setProperty("accent", isBreak(m) ? "break" : "pomo");
    switch (m) {
    case Mode::Running:
        m_primary->setIcon(icons::get(icons::Id::Pause, white));
        m_primary->setToolTip(QStringLiteral("Pausar"));
        break;
    case Mode::Break:
        m_primary->setIcon(icons::get(icons::Id::Skip, white));
        m_primary->setToolTip(QStringLiteral("Terminar recreo"));
        break;
    case Mode::BreakReady:
        m_primary->setIcon(icons::get(icons::Id::Play, white));
        m_primary->setToolTip(QStringLiteral("Iniciar recreo"));
        break;
    default:
        m_primary->setIcon(icons::get(icons::Id::Play, white));
        m_primary->setToolTip(m == Mode::Paused ? QStringLiteral("Seguir") : QStringLiteral("Arrancar pomodoro"));
        break;
    }
    m_primary->setVisible(m_model.canStart || m != Mode::Ready);

    // Secundario: terminar (pomodoro), saltear (recreo por arrancar), silenciar (alarma).
    const QColor ink(0xe0, 0xe0, 0xe0);   // el mini es siempre oscuro
    m_secondary->setVisible(m == Mode::Running || m == Mode::Paused || m == Mode::BreakReady || m == Mode::BreakOver);
    if (m == Mode::Running || m == Mode::Paused) {
        m_secondary->setIcon(icons::get(icons::Id::Stop, ink));
        m_secondary->setToolTip(QStringLiteral("Terminar pomodoro"));
    } else if (m == Mode::BreakReady) {
        m_secondary->setIcon(icons::get(icons::Id::Skip, ink));
        m_secondary->setToolTip(QStringLiteral("Saltear recreo"));
    } else if (m == Mode::BreakOver) {
        m_secondary->setIcon(icons::get(icons::Id::Mute, ink));
        m_secondary->setToolTip(QStringLiteral("Silenciar"));
    }
    for (QWidget* w : {static_cast<QWidget*>(m_primary), static_cast<QWidget*>(m_secondary)}) {
        w->style()->unpolish(w);
        w->style()->polish(w);
    }
}

void MiniPlayer::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF R = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    // Cuerpo: el mismo visor oscuro del reproductor grande.
    QLinearGradient bg(R.topLeft(), R.bottomLeft());
    bg.setColorAt(0, QColor(0x17, 0x17, 0x17));
    bg.setColorAt(1, QColor(0x0d, 0x0d, 0x0d));
    p.setPen(QPen(QColor(0x34, 0x34, 0x34), 1));
    p.setBrush(bg);
    p.drawRoundedRect(R, 12, 12);

    const Mode mode = m_model.mode;
    const bool brk = isBreak(mode);
    const QColor accent = brk ? theme::breakAccent : theme::pomoAccent;
    const bool live = mode == Mode::Running || mode == Mode::Break;

    // LED + etiqueta
    p.setPen(Qt::NoPen);
    p.setBrush(live ? accent : accent.darker(240));
    p.drawEllipse(QPointF(16, 17), 3.2, 3.2);
    QFont hf = theme::uiFont(7.5, QFont::DemiBold);
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.4);
    p.setFont(hf);
    p.setPen(QColor(0x9a, 0x9a, 0x9a));
    const QString label = brk || mode == Mode::BreakOver ? QStringLiteral("RECREO")
                        : mode == Mode::Paused          ? QStringLiteral("POMODORO %1 · PAUSA").arg(m_model.number, 2, 10, QChar('0'))
                                                        : QStringLiteral("POMODORO %1").arg(m_model.number, 2, 10, QChar('0'));
    p.drawText(QRectF(26, 9, width() - 60, 16), Qt::AlignLeft | Qt::AlignVCenter, label);

    // Actividad (chiquito)
    QFont af = theme::uiFont(8);
    p.setFont(af);
    p.setPen(QColor(0x80, 0x80, 0x80));
    const QString act = m_model.activity.isEmpty() ? QStringLiteral("Sin actividad") : m_model.activity;
    p.drawText(QRectF(12, 70, width() - 100, 16), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(af).elidedText(act, Qt::ElideLeft, width() - 100));

    // Tiempo
    const int shown = (mode == Mode::Ready || mode == Mode::BreakReady) ? m_model.durationMin * 60 : m_model.remainingSec;
    QFont df;
    df.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("Inter"), QStringLiteral("Segoe UI")});
    df.setPixelSize(34);
    df.setWeight(QFont::DemiBold);
    df.setStretch(QFont::SemiCondensed);
    p.setFont(df);
    p.setPen(live ? accent.lighter(135) : QColor(0xd8, 0xd8, 0xd8));
    p.drawText(QRectF(11, 24, 160, 44), Qt::AlignLeft | Qt::AlignVCenter,
               (mode == Mode::Ready && !m_model.canStart) ? QStringLiteral("--:--") : fmt::clock(shown));

    // Progreso abajo, de borde a borde
    double frac = 0;
    if (m_model.totalSec > 0 && (mode == Mode::Running || mode == Mode::Paused || mode == Mode::Break))
        frac = qBound(0.0, 1.0 - double(m_model.remainingSec) / m_model.totalSec, 1.0);
    if (mode == Mode::BreakOver) frac = 1;
    const QRectF track(12, height() - 7, width() - 24, 2.5);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2e, 0x2e, 0x2e));
    p.drawRoundedRect(track, 1.2, 1.2);
    if (frac > 0) {
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * frac, track.height()), 1.2, 1.2);
    }
}

void MiniPlayer::mousePressEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    m_dragging = true;
    m_dragOffset = e->globalPosition().toPoint() - frameGeometry().topLeft();
}

void MiniPlayer::mouseMoveEvent(QMouseEvent* e)
{
    if (m_dragging) move(e->globalPosition().toPoint() - m_dragOffset);
}

void MiniPlayer::mouseReleaseEvent(QMouseEvent*)
{
    if (m_dragging) emit moved(pos());
    m_dragging = false;
}

void MiniPlayer::mouseDoubleClickEvent(QMouseEvent*)
{
    emit openAppClicked();   // doble click: abre la app
}
