#include "ui/NotePad.h"

#include "core/Format.h"
#include "ui/MoodBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>

NotePad::NotePad(QWidget* parent) : QFrame(parent)
{
    setObjectName("NotePad");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);   // aleta: ventanita pegada a la principal
    setAttribute(Qt::WA_ShowWithoutActivating, false);
    auto* title = new QLabel(QStringLiteral("Anotador"), this);
    title->setObjectName("NotePadTitle");
    m_date = new QLabel(this);
    m_date->setProperty("dim", true);
    auto* close = new QPushButton(QStringLiteral("✕"), this);
    close->setObjectName("NotePadClose");
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(QStringLiteral("Cerrar el anotador"));
    connect(close, &QPushButton::clicked, this, &NotePad::closeRequested);

    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(title);
    titles->addWidget(m_date);
    auto* head = new QHBoxLayout;
    head->addLayout(titles, 1);
    head->addWidget(close, 0, Qt::AlignTop);

    m_note = new QPlainTextEdit(this);
    m_note->setObjectName("NotePadText");
    m_note->setPlaceholderText(QStringLiteral("Lo que querés hacer hoy, cómo salió, lo que se te ocurra…"));
    m_note->setTabChangesFocus(true);

    m_moodLabel = new QLabel(this);
    m_mood = new MoodBar(this);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 14, 12);
    lay->setSpacing(8);
    lay->addLayout(head);
    lay->addWidget(m_note, 1);
    lay->addWidget(m_moodLabel);
    lay->addWidget(m_mood);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    m_timer->setInterval(700);
    connect(m_note, &QPlainTextEdit::textChanged, this, [this] { m_dirty = true; m_timer->start(); });
    connect(m_timer, &QTimer::timeout, this, &NotePad::flush);
    connect(m_mood, &MoodBar::moodChanged, this, [this](int v) { emit moodChanged(m_day, v); });
}

void NotePad::setDay(const QDate& day, const QString& note, int mood, bool isToday)
{
    if (m_day != day) flush();   // lo escrito en el día anterior se guarda antes de cambiar
    m_day = day;
    m_date->setText(isToday ? QStringLiteral("Hoy · %1").arg(fmt::shortDate(day)) : fmt::shortDate(day));
    m_moodLabel->setText(isToday ? QStringLiteral("¿Cómo te va hoy?") : QStringLiteral("¿Cómo te fue?"));
    m_timer->stop();
    m_dirty = false;
    QSignalBlocker b(m_note);
    if (m_note->toPlainText() != note) m_note->setPlainText(note);
    m_mood->setMood(mood);
}

void NotePad::flush()
{
    m_timer->stop();
    if (!m_dirty) return;
    m_dirty = false;
    emit noteEdited(m_day, m_note->toPlainText().trimmed());
}

void NotePad::focusNote() { m_note->setFocus(); }
