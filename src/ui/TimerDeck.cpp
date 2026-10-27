#include "ui/TimerDeck.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QIntValidator>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QRegularExpressionValidator>
#include <QWheelEvent>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>
#include <QtMath>

namespace {
using Mode = TimerDeck::Mode;

bool isBreakMode(Mode m) { return m == Mode::BreakReady || m == Mode::Break || m == Mode::BreakOver; }

QColor accentFor(Mode m)
{
    return isBreakMode(m) && m != Mode::BreakOver ? theme::breakAccent : theme::pomoAccent;
}

// Fuente de "contador de casetera": Bahnschrift viene con Windows 10/11.
QFont deckFont(qreal px, int weight = QFont::DemiBold)
{
    QFont f;
    f.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("DIN Alternate"), QStringLiteral("Inter"),
                   QStringLiteral("Segoe UI"), QStringLiteral("sans-serif")});
    f.setPixelSize(qMax(6, int(px)));
    f.setWeight(QFont::Weight(weight));
    f.setStretch(QFont::SemiCondensed);
    return f;
}

void repolish(QWidget* w)
{
    w->style()->unpolish(w);
    w->style()->polish(w);
}
} // namespace

// ===========================================================================
// TimerDeck
// ===========================================================================
TimerDeck::TimerDeck(QWidget* parent)
    : QWidget(parent)
{
    setObjectName("Deck");
    setAttribute(Qt::WA_StyledBackground);

    m_activity = new QToolButton(this);
    m_activity->setObjectName("DeckActivity");
    m_activity->setIcon(icons::get(icons::Id::Folder, icons::Ink::Dim));
    m_activity->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    m_activity->setPopupMode(QToolButton::InstantPopup);
    m_activity->setFocusPolicy(Qt::NoFocus);
    m_activity->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_activity->setToolTip(QStringLiteral("Actividad (se puede cambiar en cualquier momento, también con el pomodoro corriendo)"));

    m_display = new DeckDisplay(this);

    // [−1] [−5]  25 min  [+5] [+1]   (los de afuera son opcionales)
    m_stepper = new QWidget(this);
    m_minus = new QToolButton(m_stepper);
    m_plus = new QToolButton(m_stepper);
    m_minusFine = new QToolButton(m_stepper);
    m_plusFine = new QToolButton(m_stepper);
    m_minus->setText(QStringLiteral("−"));
    m_plus->setText(QStringLiteral("+"));
    for (QToolButton* b : {m_minus, m_plus, m_minusFine, m_plusFine}) {
        b->setObjectName("DeckStep");
        b->setFocusPolicy(Qt::NoFocus);
        b->setAutoRepeat(true);
        b->setAutoRepeatDelay(350);
        b->setAutoRepeatInterval(90);
        b->setCursor(Qt::PointingHandCursor);
    }
    m_minusFine->setProperty("fine", true);
    m_plusFine->setProperty("fine", true);
    m_stepLabel = new QLabel(m_stepper);
    m_stepLabel->setObjectName("DeckStepLabel");
    m_stepLabel->setAlignment(Qt::AlignCenter);
    m_stepLabel->setMinimumWidth(74);
    auto* stepLay = new QHBoxLayout(m_stepper);
    stepLay->setContentsMargins(0, 0, 0, 0);
    stepLay->setSpacing(6);
    stepLay->addStretch();
    stepLay->addWidget(m_minusFine);
    stepLay->addWidget(m_minus);
    stepLay->addWidget(m_stepLabel);
    stepLay->addWidget(m_plus);
    stepLay->addWidget(m_plusFine);
    stepLay->addStretch();
    connect(m_minus, &QToolButton::clicked, this, [this] {
        const int v = m_model.durationMin;
        if (m_adjust.buttons) { nudge(-m_adjust.stepBig); return; }
        // Clásico: de a 5 minutos arriba de 10, de a 1 por debajo.
        emit durationChanged(qMax(1, v > 10 ? ((v - 1) / 5) * 5 : v - 1));
    });
    connect(m_plus, &QToolButton::clicked, this, [this] {
        const int v = m_model.durationMin;
        if (m_adjust.buttons) { nudge(m_adjust.stepBig); return; }
        emit durationChanged(qMin(240, v >= 10 ? (v / 5 + 1) * 5 : v + 1));
    });
    connect(m_minusFine, &QToolButton::clicked, this, [this] { nudge(-m_adjust.stepSmall); });
    connect(m_plusFine, &QToolButton::clicked, this, [this] { nudge(m_adjust.stepSmall); });

    // Rueda y click sobre el visor
    connect(m_display, &DeckDisplay::wheelTurned, this, [this](int delta) {
        if (!editable() || !m_adjust.wheel) return;
        m_wheelAccum += delta;                 // touchpads mandan pasos chiquitos
        const int notches = m_wheelAccum / 120;
        if (notches == 0) return;
        m_wheelAccum -= notches * 120;
        nudge(notches * m_adjust.wheelStep);
    });
    connect(m_display, &DeckDisplay::digitsClicked, this, [this] {
        if (editable() && m_adjust.type) beginTyping();
    });

    // Editor para escribir los minutos: "45", "90" o "1:30"
    m_editor = new QLineEdit(m_display);
    m_editor->setObjectName("DeckEditor");
    m_editor->setAlignment(Qt::AlignCenter);
    m_editor->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("^\\d{0,3}(:\\d{0,2})?$")), m_editor));
    m_editor->setToolTip(QStringLiteral("Minutos (o h:mm). Enter para confirmar, Esc para cancelar"));
    m_editor->hide();
    m_editor->installEventFilter(this);
    connect(m_editor, &QLineEdit::returnPressed, this, [this] { finishTyping(true); });
    connect(m_editor, &QLineEdit::editingFinished, this, [this] { if (m_editor->isVisible()) finishTyping(true); });

    // ¿En qué trabajaste? (aparece al terminar el pomodoro)
    m_note = new QLineEdit(this);
    m_note->setObjectName("DeckNote");
    m_note->setPlaceholderText(QStringLiteral("¿En qué trabajaste? (opcional)"));
    m_note->setMaxLength(200);
    m_note->setClearButtonEnabled(true);
    m_note->hide();
    auto commitNote = [this] {
        const QString t = m_note->text().trimmed();
        if (t == m_noteSaved) return;
        m_noteSaved = t;
        emit noteEdited(t);
    };
    connect(m_note, &QLineEdit::returnPressed, this, [this, commitNote] { commitNote(); m_note->clearFocus(); });
    connect(m_note, &QLineEdit::editingFinished, this, commitNote);

    // Transporte: acción secundaria "hefty" + botón grande
    m_secondary = new QPushButton(this);
    m_secondary->setObjectName("DeckAction");
    m_secondary->setFocusPolicy(Qt::NoFocus);
    m_secondary->setCursor(Qt::PointingHandCursor);
    m_secondary->setIconSize(QSize(16, 16));

    m_primary = new QToolButton(this);
    m_primary->setObjectName("DeckPlay");
    m_primary->setFixedSize(58, 58);
    m_primary->setIconSize(QSize(26, 26));
    m_primary->setFocusPolicy(Qt::NoFocus);
    m_primary->setCursor(Qt::PointingHandCursor);

    connect(m_primary, &QToolButton::clicked, this, [this] {
        switch (m_model.mode) {
        case Mode::Ready:
        case Mode::Paused:
        case Mode::BreakOver:  emit playClicked(); break;
        case Mode::Running:    emit pauseClicked(); break;
        case Mode::BreakReady: emit startBreakClicked(); break;
        case Mode::Break:      break;
        }
    });
    connect(m_secondary, &QPushButton::clicked, this, [this] {
        switch (m_model.mode) {
        case Mode::Running:
        case Mode::Paused:     emit endPomodoroClicked(); break;
        case Mode::BreakReady: emit skipBreakClicked(); break;
        case Mode::Break:      emit endBreakClicked(); break;
        case Mode::BreakOver:  emit silenceClicked(); break;
        case Mode::Ready:      if (!m_model.canStart) emit goTodayClicked(); break;
        }
    });

    auto* transport = new QHBoxLayout;
    transport->setSpacing(12);
    transport->addStretch();
    transport->addWidget(m_secondary);
    transport->addWidget(m_primary);
    transport->addStretch();

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(12, 10, 12, 12);
    lay->setSpacing(8);
    lay->addWidget(m_activity);
    lay->addWidget(m_display, 1);
    lay->addWidget(m_note);
    lay->addWidget(m_stepper);
    lay->addLayout(transport);

    applyMode();
}

void TimerDeck::setAdjustOptions(const AdjustOptions& o)
{
    m_adjust = o;
    m_minusFine->setText(QStringLiteral("−%1").arg(o.stepSmall));
    m_plusFine->setText(QStringLiteral("+%1").arg(o.stepSmall));
    m_minus->setText(o.buttons ? QStringLiteral("−%1").arg(o.stepBig) : QStringLiteral("−"));
    m_plus->setText(o.buttons ? QStringLiteral("+%1").arg(o.stepBig) : QStringLiteral("+"));
    m_minus->setProperty("labeled", o.buttons);
    m_plus->setProperty("labeled", o.buttons);
    for (QWidget* w : {static_cast<QWidget*>(m_minus), static_cast<QWidget*>(m_plus)}) repolish(w);
    m_minusFine->setVisible(o.buttons);
    m_plusFine->setVisible(o.buttons);
    applyMode();
}

void TimerDeck::setNoteEnabled(bool on)
{
    m_noteEnabled = on;
    applyMode();
}

void TimerDeck::clearNote(const QString& placeholderHint)
{
    m_note->clear();
    m_noteSaved.clear();
    m_note->setPlaceholderText(placeholderHint.isEmpty()
                                   ? QStringLiteral("¿En qué trabajaste? (opcional)")
                                   : QStringLiteral("¿En qué trabajaste? (antes: %1)").arg(placeholderHint));
}

bool TimerDeck::editable() const
{
    return (m_model.mode == Mode::Ready && m_model.canStart) || m_model.mode == Mode::BreakReady;
}

void TimerDeck::nudge(int minutes)
{
    const int v = qBound(1, m_model.durationMin + minutes, 240);
    if (v != m_model.durationMin) emit durationChanged(v);
}

void TimerDeck::beginTyping()
{
    QFont f = m_editor->font();
    const QRectF r = m_display->digitsRect();
    f.setFamilies({QStringLiteral("Bahnschrift"), QStringLiteral("Inter"), QStringLiteral("Segoe UI")});
    f.setPixelSize(qMax(14, int(qMin(r.height() * 0.62, r.width() * 0.26))));
    f.setWeight(QFont::DemiBold);
    m_editor->setFont(f);
    const int w = int(r.width() * 0.62), h = int(f.pixelSize() * 1.35);
    m_editor->setGeometry(int(r.center().x() - w / 2), int(r.center().y() - h / 2), w, h);
    m_editor->setText(QString::number(m_model.durationMin));
    m_editor->show();
    m_editor->setFocus();
    m_editor->selectAll();
}

void TimerDeck::finishTyping(bool commit)
{
    if (!m_editor->isVisible()) return;
    const QString t = m_editor->text().trimmed();
    m_editor->hide();
    if (!commit || t.isEmpty()) return;
    int minutes = 0;
    if (t.contains(':')) {
        const QStringList hm = t.split(':');
        minutes = hm.value(0).toInt() * 60 + hm.value(1).toInt();
    } else {
        minutes = t.toInt();
    }
    if (minutes > 0) emit durationChanged(qBound(1, minutes, 240));
}

bool TimerDeck::eventFilter(QObject* o, QEvent* e)
{
    if (o == m_editor && e->type() == QEvent::KeyPress &&
        static_cast<QKeyEvent*>(e)->key() == Qt::Key_Escape) {
        finishTyping(false);
        return true;
    }
    return QWidget::eventFilter(o, e);
}

void TimerDeck::setActivityMenu(QMenu* menu)
{
    m_activity->setMenu(menu);
}

void TimerDeck::setModel(const Model& m)
{
    const bool modeChanged = m_first || m.mode != m_model.mode || m.canStart != m_model.canStart;
    m_first = false;
    m_model = m;
    m_display->setModel(m);
    const QString act = m.activity.isEmpty() ? QStringLiteral("Sin actividad") : m.activity;
    m_activity->setText(QFontMetrics(m_activity->font()).elidedText(act, Qt::ElideLeft, qMax(80, width() - 70)));
    m_stepLabel->setText(QStringLiteral("%1 min").arg(m.durationMin));
    if (modeChanged) applyMode();
}

void TimerDeck::applyMode()
{
    const Mode m = m_model.mode;
    const bool brk = isBreakMode(m) && m != Mode::BreakOver;
    const QColor onAccent(0xff, 0xff, 0xff);

    // Botón grande
    m_primary->setVisible(m != Mode::Break);
    m_primary->setProperty("accent", brk ? "break" : "pomo");
    switch (m) {
    case Mode::Ready:
        m_primary->setIcon(icons::get(icons::Id::Play, onAccent));
        m_primary->setToolTip(QStringLiteral("Arrancar pomodoro (Espacio)"));
        break;
    case Mode::Running:
        m_primary->setIcon(icons::get(icons::Id::Pause, onAccent));
        m_primary->setToolTip(QStringLiteral("Pausar (Espacio)"));
        break;
    case Mode::Paused:
        m_primary->setIcon(icons::get(icons::Id::Play, onAccent));
        m_primary->setToolTip(QStringLiteral("Seguir (Espacio)"));
        break;
    case Mode::BreakReady:
        m_primary->setIcon(icons::get(icons::Id::Play, onAccent));
        m_primary->setToolTip(QStringLiteral("Iniciar recreo (Espacio)"));
        break;
    case Mode::BreakOver:
        m_primary->setIcon(icons::get(icons::Id::Play, onAccent));
        m_primary->setToolTip(QStringLiteral("Siguiente pomodoro (Espacio)"));
        break;
    case Mode::Break:
        break;
    }

    // Viendo otro día no se arranca nada: solo "Ir a hoy".
    const bool blocked = m == Mode::Ready && !m_model.canStart;
    if (blocked) m_primary->setVisible(false);

    // Acción secundaria
    const icons::Ink ink = icons::Ink::Text;
    m_secondary->setVisible(m != Mode::Ready || blocked);
    m_secondary->setProperty("accent", brk ? "break" : "pomo");
    switch (m) {
    case Mode::Running:
    case Mode::Paused:
        m_secondary->setText(QStringLiteral("Terminar pomodoro"));
        m_secondary->setIcon(icons::get(icons::Id::Stop, ink));
        m_secondary->setToolTip(QStringLiteral("Lo corta acá y guarda lo que llevás (si pasó 1 minuto)"));
        break;
    case Mode::BreakReady:
        m_secondary->setText(QStringLiteral("Saltear recreo"));
        m_secondary->setIcon(icons::get(icons::Id::Skip, ink));
        m_secondary->setToolTip(QStringLiteral("Arranca el siguiente pomodoro ya, pegado a este"));
        break;
    case Mode::Break:
        m_secondary->setText(QStringLiteral("Terminar recreo"));
        m_secondary->setIcon(icons::get(icons::Id::Skip, ink));
        m_secondary->setToolTip(QStringLiteral("Cortar el recreo y volver"));
        break;
    case Mode::BreakOver:
        m_secondary->setText(QStringLiteral("Silenciar"));
        m_secondary->setIcon(icons::get(icons::Id::Mute, ink));
        m_secondary->setToolTip(QStringLiteral("Apagar la alarma"));
        break;
    case Mode::Ready:
        if (blocked) {
            m_secondary->setText(QStringLiteral("Ir a hoy"));
            m_secondary->setIcon(icons::get(icons::Id::ChevronRight, ink));
            m_secondary->setToolTip(QStringLiteral("Los pomodoros se registran solo en el día de hoy"));
        }
        break;
    }

    // − / + solo cuando se está por arrancar algo
    m_stepper->setVisible((m == Mode::Ready && !blocked) || m == Mode::BreakReady);
    m_stepper->setToolTip(m == Mode::Ready ? QStringLiteral("Duración del pomodoro")
                                           : QStringLiteral("Duración del recreo"));
    if (!editable()) finishTyping(false);
    // Nota: visible mientras se decide el recreo; al salir se guarda lo escrito.
    const bool showNote = m_noteEnabled && m == Mode::BreakReady;
    if (!showNote && m_note->isVisible()) {
        const QString t = m_note->text().trimmed();
        if (t != m_noteSaved) { m_noteSaved = t; emit noteEdited(t); }
    }
    m_note->setVisible(showNote);
    QStringList hints;
    if (editable() && m_adjust.wheel) hints << QStringLiteral("rueda para ajustar");
    if (editable() && m_adjust.type) hints << QStringLiteral("click para escribir los minutos");
    m_display->setToolTip(hints.isEmpty() ? QString() : QStringLiteral("Duración: ") + hints.join(QStringLiteral(" · ")));
    m_display->setCursor(editable() && m_adjust.type ? Qt::IBeamCursor : Qt::ArrowCursor);

    for (QWidget* w : {static_cast<QWidget*>(m_primary), static_cast<QWidget*>(m_secondary)}) repolish(w);
}

// ===========================================================================
// DeckDisplay
// ===========================================================================
DeckDisplay::DeckDisplay(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(120);
    m_anim.setInterval(40);
    connect(&m_anim, &QTimer::timeout, this, [this] {
        m_phase += 0.04;
        update();
    });
}

void DeckDisplay::setModel(const TimerDeck::Model& m)
{
    m_model = m;
    const bool animate = m.mode == Mode::Running || m.mode == Mode::Break ||
                         m.mode == Mode::Paused || m.mode == Mode::BreakOver;
    if (animate && !m_anim.isActive()) m_anim.start();
    if (!animate && m_anim.isActive()) m_anim.stop();
    update();
}

QRectF DeckDisplay::digitsRect() const
{
    const QRectF R = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal pad = qMax(10.0, R.width() * 0.05);
    return QRectF(R.left() + pad, R.top() + pad + 20, R.width() - 2 * pad, R.height() - 2 * pad - 20 - 34);
}

void DeckDisplay::wheelEvent(QWheelEvent* e)
{
    emit wheelTurned(e->angleDelta().y());
    e->accept();
}

void DeckDisplay::mousePressEvent(QMouseEvent* e)
{
    if (e->button() == Qt::LeftButton && digitsRect().contains(e->position())) emit digitsClicked();
    QWidget::mousePressEvent(e);
}

void DeckDisplay::paintReel(QPainter& p, const QPointF& c, qreal r, bool spinning) const
{
    p.save();
    p.translate(c);
    p.setPen(QPen(QColor(0x4a, 0x4a, 0x4a), 1.4));
    p.setBrush(QColor(0x12, 0x12, 0x12));
    p.drawEllipse(QPointF(0, 0), r, r);
    p.rotate(spinning ? m_phase * 140.0 : 0.0);
    p.setPen(QPen(QColor(0x6a, 0x6a, 0x6a), 1.6, Qt::SolidLine, Qt::RoundCap));
    for (int i = 0; i < 3; ++i) {
        p.drawLine(QPointF(0, r * 0.28), QPointF(0, r * 0.78));
        p.rotate(120);
    }
    p.setBrush(QColor(0x2a, 0x2a, 0x2a));
    p.setPen(Qt::NoPen);
    p.drawEllipse(QPointF(0, 0), r * 0.22, r * 0.22);
    p.restore();
}

void DeckDisplay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF R = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const Mode mode = m_model.mode;
    const QColor accent = accentFor(mode);
    const bool blinkOn = std::fmod(m_phase, 1.0) < 0.6;

    // Pantalla: negro profundo con un brillo sutil arriba, como un visor de casetera.
    QLinearGradient bg(R.topLeft(), R.bottomLeft());
    bg.setColorAt(0, QColor(0x14, 0x14, 0x14));
    bg.setColorAt(1, QColor(0x0c, 0x0c, 0x0c));
    p.setPen(QPen(QColor(0x2c, 0x2c, 0x2c), 1));
    p.setBrush(bg);
    p.drawRoundedRect(R, 10, 10);

    const qreal W = R.width(), H = R.height();
    const qreal pad = qMax(10.0, W * 0.05);

    // ---- Cabecera: LED + modo
    QString label;
    switch (mode) {
    case Mode::Ready:      label = m_model.canStart ? QStringLiteral("POMODORO %1").arg(m_model.number, 2, 10, QChar('0'))
                                                        : QStringLiteral("SOLO HOY"); break;
    case Mode::Running:    label = QStringLiteral("POMODORO %1").arg(m_model.number, 2, 10, QChar('0')); break;
    case Mode::Paused:     label = QStringLiteral("POMODORO %1").arg(m_model.number, 2, 10, QChar('0')); break;
    case Mode::BreakReady: label = QStringLiteral("RECREO"); break;
    case Mode::Break:      label = QStringLiteral("RECREO"); break;
    case Mode::BreakOver:  label = QStringLiteral("FIN DEL RECREO"); break;
    }
    const bool ledLit = mode == Mode::Running || mode == Mode::Break ||
                        ((mode == Mode::Paused || mode == Mode::BreakOver) && blinkOn);
    QColor led = accent;
    if (!ledLit) led = accent.darker(260);
    const QPointF ledC(R.left() + pad + 4, R.top() + pad + 6);
    if (ledLit) {
        QRadialGradient glow(ledC, 10);
        QColor g = accent;
        g.setAlphaF(0.35);
        glow.setColorAt(0, g);
        glow.setColorAt(1, Qt::transparent);
        p.setPen(Qt::NoPen);
        p.setBrush(glow);
        p.drawEllipse(ledC, 10, 10);
    }
    p.setPen(Qt::NoPen);
    p.setBrush(led);
    p.drawEllipse(ledC, 3.6, 3.6);

    QFont hf = deckFont(qBound(10.0, H * 0.075, 14.0));
    hf.setLetterSpacing(QFont::AbsoluteSpacing, 1.6);
    p.setFont(hf);
    p.setPen(QColor(0x9a, 0x9a, 0x9a));
    p.drawText(QRectF(ledC.x() + 12, R.top() + pad - 2, W, 18), Qt::AlignLeft | Qt::AlignVCenter, label);

    // Indicador a la derecha: ▶ / ❚❚ / ■ en chiquito, como en las caseteras.
    {
        const QRectF ind(R.right() - pad - 12, R.top() + pad, 12, 12);
        p.setPen(Qt::NoPen);
        p.setBrush(ledLit ? accent : QColor(0x55, 0x55, 0x55));
        if (mode == Mode::Running || mode == Mode::Break) {
            QPainterPath tri;
            tri.moveTo(ind.left() + 2, ind.top());
            tri.lineTo(ind.right(), ind.center().y());
            tri.lineTo(ind.left() + 2, ind.bottom());
            tri.closeSubpath();
            p.drawPath(tri);
        } else if (mode == Mode::Paused) {
            p.drawRect(QRectF(ind.left() + 1, ind.top(), 3.5, 12));
            p.drawRect(QRectF(ind.left() + 7.5, ind.top(), 3.5, 12));
        } else {
            p.drawRect(ind.adjusted(1, 1, -1, -1));
        }
    }

    // ---- Zona central
    const QRectF center = digitsRect();
    const int shown = (mode == Mode::Ready || mode == Mode::BreakReady)
                          ? m_model.durationMin * 60 : m_model.remainingSec;

    if (mode == Mode::Paused) {
        // Pausa "hefty": ícono grande + PAUSA
        const qreal s = qMin(center.height() * 0.62, center.width() * 0.22);
        QFont pf = deckFont(s * 0.78, QFont::Bold);
        pf.setLetterSpacing(QFont::AbsoluteSpacing, 2);
        const QString txt = QStringLiteral("PAUSA");
        const qreal tw = QFontMetricsF(pf).horizontalAdvance(txt);
        const qreal total = s * 0.9 + s * 0.35 + tw;
        qreal x = center.center().x() - total / 2;
        const qreal y = center.center().y() - s / 2;
        QColor c(0xe8, 0xe8, 0xe8);   // el visor es siempre oscuro
        if (!blinkOn) c.setAlphaF(0.55);
        p.setPen(Qt::NoPen);
        p.setBrush(c);
        p.drawRoundedRect(QRectF(x, y, s * 0.32, s), 2.5, 2.5);
        p.drawRoundedRect(QRectF(x + s * 0.55, y, s * 0.32, s), 2.5, 2.5);
        x += s * 0.9 + s * 0.35;
        p.setFont(pf);
        p.setPen(c);
        p.drawText(QRectF(x, center.top(), tw + 4, center.height()), Qt::AlignLeft | Qt::AlignVCenter, txt);
    } else {
        // Dígitos grandes de contador
        QString digits = (mode == Mode::Ready && !m_model.canStart) ? QStringLiteral("--:--") : fmt::clock(shown);
        QFont df = deckFont(qMin(center.height() * 0.80, center.width() * 0.34), QFont::DemiBold);
        QFontMetricsF fm(df);
        while (fm.horizontalAdvance(digits) > center.width() * 0.96 && df.pixelSize() > 12) {
            df.setPixelSize(df.pixelSize() - 2);
            fm = QFontMetricsF(df);
        }
        // "fantasma" de LCD detrás de los dígitos
        p.setFont(df);
        QString ghost = digits;
        for (QChar& ch : ghost) if (ch.isDigit()) ch = QChar('8');
        QColor gc = accent;
        gc.setAlphaF(0.07);
        p.setPen(gc);
        p.drawText(center, Qt::AlignCenter, ghost);

        QColor dc = (mode == Mode::Ready || mode == Mode::BreakReady)
                        ? QColor(0xd8, 0xd8, 0xd8)
                        : accent.lighter(135);
        if (mode == Mode::BreakOver && !blinkOn) dc.setAlphaF(0.25);
        p.setPen(dc);
        p.drawText(center, Qt::AlignCenter, digits);
    }

    // ---- Abajo: carrete · barra de progreso con tiempos · carrete
    const qreal reelR = 11;
    const qreal by = R.bottom() - pad - reelR + 2;
    const bool spinning = mode == Mode::Running || mode == Mode::Break;
    paintReel(p, QPointF(R.left() + pad + reelR, by), reelR, spinning);
    paintReel(p, QPointF(R.right() - pad - reelR, by), reelR, spinning);

    const qreal bx0 = R.left() + pad + 2 * reelR + 10;
    const qreal bx1 = R.right() - pad - 2 * reelR - 10;
    const qreal total = qMax(1, m_model.totalSec);
    qreal frac = 0;
    if (mode == Mode::Running || mode == Mode::Paused || mode == Mode::Break)
        frac = qBound(0.0, 1.0 - m_model.remainingSec / total, 1.0);
    else if (mode == Mode::BreakOver)
        frac = 1.0;

    const QRectF track(bx0, by - 6, bx1 - bx0, 4);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0x2e, 0x2e, 0x2e));
    p.drawRoundedRect(track, 2, 2);
    if (frac > 0) {
        p.setBrush(accent);
        p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * frac, track.height()), 2, 2);
        p.setBrush(QColor(0xe8, 0xe8, 0xe8));
        p.drawEllipse(QPointF(track.left() + track.width() * frac, track.center().y()), 4.5, 4.5);
    }
    // Tiempos como en Spotify: transcurrido a la izquierda, total a la derecha
    QFont tf = deckFont(11, QFont::Medium);
    p.setFont(tf);
    p.setPen(QColor(0x8a, 0x8a, 0x8a));
    const int elapsed = (mode == Mode::Ready || mode == Mode::BreakReady) ? 0 : int(total - m_model.remainingSec);
    const int totalShown = (mode == Mode::Ready || mode == Mode::BreakReady) ? m_model.durationMin * 60 : int(total);
    p.drawText(QRectF(bx0, by, 80, 14), Qt::AlignLeft | Qt::AlignVCenter, fmt::clock(qMax(0, elapsed)));
    p.drawText(QRectF(bx1 - 80, by, 80, 14), Qt::AlignRight | Qt::AlignVCenter, fmt::clock(totalShown));
}
