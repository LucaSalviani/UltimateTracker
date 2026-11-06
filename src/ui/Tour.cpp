#include "ui/Tour.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QVBoxLayout>

Tour::Tour(const QVector<Step>& steps, QWidget* parent)
    : QWidget(parent)
    , m_steps(steps)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setFocusPolicy(Qt::StrongFocus);

    m_bubble = new QWidget(this);
    m_bubble->setObjectName("TourBubble");
    m_bubble->setAttribute(Qt::WA_StyledBackground);
    m_count = new QLabel(m_bubble);
    m_count->setObjectName("TourCount");
    m_title = new QLabel(m_bubble);
    m_title->setObjectName("TourTitle");
    m_title->setWordWrap(true);
    m_text = new QLabel(m_bubble);
    m_text->setObjectName("TourText");
    m_text->setWordWrap(true);
    m_skip = new QPushButton(QStringLiteral("Saltar"), m_bubble);
    m_skip->setObjectName("GreetLater");
    m_prev = new QPushButton(QStringLiteral("Anterior"), m_bubble);
    m_prev->setObjectName("GreetLater");
    m_next = new QPushButton(m_bubble);
    m_next->setObjectName("GreetGo");
    for (QPushButton* b : {m_skip, m_prev, m_next}) b->setCursor(Qt::PointingHandCursor);
    connect(m_skip, &QPushButton::clicked, this, [this] { close(); emit finished(); });
    connect(m_prev, &QPushButton::clicked, this, [this] { go(m_index - 1); });
    connect(m_next, &QPushButton::clicked, this, [this] {
        if (m_index + 1 >= m_steps.size()) { close(); emit finished(); }
        else go(m_index + 1);
    });

    auto* buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addWidget(m_skip);
    buttons->addStretch();
    buttons->addWidget(m_prev);
    buttons->addWidget(m_next);
    auto* lay = new QVBoxLayout(m_bubble);
    lay->setContentsMargins(18, 14, 18, 14);
    lay->setSpacing(6);
    lay->addWidget(m_count);
    lay->addWidget(m_title);
    lay->addWidget(m_text);
    lay->addSpacing(6);
    lay->addLayout(buttons);
}

void Tour::start()
{
    if (parentWidget()) setGeometry(parentWidget()->rect());
    show();
    raise();
    setFocus();
    go(0);
}

void Tour::go(int index)
{
    m_index = qBound(0, index, int(m_steps.size()) - 1);
    const Step& s = m_steps[m_index];
    m_count->setText(QStringLiteral("%1 de %2").arg(m_index + 1).arg(m_steps.size()));
    m_title->setText(s.title);
    m_text->setText(s.text);
    m_prev->setVisible(m_index > 0);
    m_next->setText(m_index + 1 >= m_steps.size() ? QStringLiteral("Listo") : QStringLiteral("Siguiente"));
    m_next->setFocus();
    place();
    update();
}

QRect Tour::hole() const
{
    const Step& s = m_steps[m_index];
    if (!s.target || !s.target->isVisible()) return {};
    const QRect r(s.target->mapTo(parentWidget(), QPoint(0, 0)), s.target->size());
    return r.adjusted(-6, -6, 6, 6);
}

void Tour::place()
{
    const int w = qMin(width() - 24, 340);
    m_bubble->setFixedWidth(w);
    m_bubble->adjustSize();
    const QSize b = m_bubble->size();
    const QRect h = hole();
    QPoint pos;
    if (h.isNull()) {
        pos = QPoint((width() - b.width()) / 2, (height() - b.height()) / 2);
    } else {
        // Abajo del foco si entra; si no, arriba; si no, al costado.
        if (h.bottom() + 12 + b.height() <= height() - 8) pos = QPoint(h.center().x() - b.width() / 2, h.bottom() + 12);
        else if (h.top() - 12 - b.height() >= 8) pos = QPoint(h.center().x() - b.width() / 2, h.top() - 12 - b.height());
        else if (h.left() - 12 - b.width() >= 8) pos = QPoint(h.left() - 12 - b.width(), h.center().y() - b.height() / 2);
        else pos = QPoint(h.right() + 12, h.center().y() - b.height() / 2);
    }
    pos.setX(qBound(12, pos.x(), qMax(12, width() - b.width() - 12)));
    pos.setY(qBound(12, pos.y(), qMax(12, height() - b.height() - 12)));
    m_bubble->move(pos);
}

void Tour::resizeEvent(QResizeEvent*) { if (!m_steps.isEmpty()) place(); }

void Tour::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath all;
    all.addRect(rect());
    const QRect h = hole();
    if (!h.isNull()) {
        QPainterPath hp;
        hp.addRoundedRect(h, 10, 10);
        all = all.subtracted(hp);
    }
    QColor dim = theme::bg;
    dim.setAlphaF(0.80);
    p.fillPath(all, dim);
    if (!h.isNull()) {
        p.setPen(QPen(theme::pomoAccent, 2));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(h, 10, 10);
    }
    // El globito
    const QRectF r = QRectF(m_bubble->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(theme::mix(theme::bg, theme::text, 0.18), 1));
    p.setBrush(theme::mix(theme::bg, theme::text, 0.05));
    p.drawRoundedRect(r, 12, 12);
}

void Tour::keyPressEvent(QKeyEvent* e)
{
    switch (e->key()) {
    case Qt::Key_Escape: close(); emit finished(); return;
    case Qt::Key_Right:
    case Qt::Key_Return:
    case Qt::Key_Enter: m_next->click(); return;
    case Qt::Key_Left: if (m_index > 0) go(m_index - 1); return;
    default: QWidget::keyPressEvent(e);
    }
}

void Tour::mousePressEvent(QMouseEvent* e) { e->accept(); }   // no se toca la app de abajo mientras dura
