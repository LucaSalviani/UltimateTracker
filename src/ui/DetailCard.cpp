#include "ui/DetailCard.h"
#include "ui/Theme.h"

#include <QEvent>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QVBoxLayout>

DetailCard::DetailCard(QWidget* parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    hide();
    parent->installEventFilter(this);

    m_card = new QWidget(this);
    m_card->setObjectName("GreetCard");
    m_card->setAttribute(Qt::WA_StyledBackground);
    m_title = new QLabel(m_card);
    m_title->setObjectName("GreetTitle");
    m_title->setWordWrap(true);
    m_body = new QLabel;
    m_body->setObjectName("DetailBody");
    m_body->setWordWrap(true);
    m_body->setTextFormat(Qt::RichText);
    m_body->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_scroll = new QScrollArea(m_card);
    m_scroll->setWidget(m_body);
    m_scroll->setWidgetResizable(true);
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setStyleSheet(QStringLiteral("QScrollArea, QScrollArea > QWidget > QWidget { background: transparent; }"));
    m_action = new QPushButton(m_card);
    m_action->setObjectName("GreetLater");
    m_action->setCursor(Qt::PointingHandCursor);
    auto* close = new QPushButton(QStringLiteral("Cerrar"), m_card);
    close->setObjectName("GreetGo");
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QPushButton::clicked, this, &DetailCard::closeCard);
    connect(m_action, &QPushButton::clicked, this, [this] { if (m_onAction) m_onAction(); });

    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_action);
    buttons->addStretch();
    buttons->addWidget(close);
    auto* lay = new QVBoxLayout(m_card);
    lay->setContentsMargins(22, 18, 22, 16);
    lay->setSpacing(10);
    lay->addWidget(m_title);
    lay->addWidget(m_scroll, 1);
    lay->addLayout(buttons);
}

void DetailCard::showDetail(const QString& title, const QString& html, const QString& actionText, std::function<void()> action)
{
    m_title->setText(title);
    m_body->setText(html);
    m_onAction = std::move(action);
    m_action->setVisible(!actionText.isEmpty());
    m_action->setText(actionText);
    setGeometry(parentWidget()->rect());
    place();
    show();
    raise();
    setFocus();
}

void DetailCard::closeCard() { hide(); }

void DetailCard::place()
{
    const int w = qMin(width() - 28, 470);
    m_body->setFixedWidth(qMax(200, w - 44 - 14));
    const int want = m_body->heightForWidth(m_body->width()) + m_title->sizeHint().height() + 110;
    const int h = qBound(160, want, qMax(160, height() - 28));
    m_card->setGeometry((width() - w) / 2, qMax(14, (height() - h) / 2), w, h);
}

void DetailCard::resizeEvent(QResizeEvent*) { place(); }

bool DetailCard::eventFilter(QObject* o, QEvent* e)
{
    if (o == parentWidget() && e->type() == QEvent::Resize && isVisible()) setGeometry(parentWidget()->rect());
    return QWidget::eventFilter(o, e);
}

void DetailCard::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QColor dim = theme::bg;
    dim.setAlphaF(0.80);
    p.fillRect(rect(), dim);
    const QRectF r = QRectF(m_card->geometry()).adjusted(0.5, 0.5, -0.5, -0.5);
    p.setPen(QPen(theme::mix(theme::bg, theme::text, 0.16), 1));
    p.setBrush(theme::mix(theme::bg, theme::text, 0.04));
    p.drawRoundedRect(r, 12, 12);
}

void DetailCard::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Escape || e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) { closeCard(); return; }
    QWidget::keyPressEvent(e);
}

void DetailCard::mousePressEvent(QMouseEvent* e)
{
    if (!m_card->geometry().contains(e->position().toPoint())) closeCard();
    e->accept();
}
