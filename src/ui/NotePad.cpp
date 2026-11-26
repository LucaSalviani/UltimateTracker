#include "ui/NotePad.h"

#include "core/Format.h"
#include "ui/MoodBar.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include "ui/Libreta.h"
#include <QTimer>
#include <QVBoxLayout>

NotePad::NotePad(Database* db, QWidget* parent) : QFrame(parent)
{
    setObjectName("NotePad");
    setWindowFlags(Qt::Tool | Qt::FramelessWindowHint);   // aleta: ventanita pegada a la principal
    setAttribute(Qt::WA_ShowWithoutActivating, false);

    // Mejoras 6.0: dos solapas. Diario = la hoja del día; Libreta = páginas permanentes.
    auto tabBtn = [this](const QString& t) {
        auto* b = new QPushButton(t, this);
        b->setObjectName("NotePadTab");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        return b;
    };
    m_tabDiary = tabBtn(QStringLiteral("Diario"));
    m_tabBook = tabBtn(QStringLiteral("Libreta"));
    connect(m_tabDiary, &QPushButton::clicked, this, [this] { showTab(0); });
    connect(m_tabBook, &QPushButton::clicked, this, [this] { showTab(1); });
    auto* close = new QPushButton(QStringLiteral("✕"), this);
    close->setObjectName("NotePadClose");
    close->setCursor(Qt::PointingHandCursor);
    close->setToolTip(QStringLiteral("Cerrar"));
    connect(close, &QPushButton::clicked, this, &NotePad::closeRequested);
    auto* head = new QHBoxLayout;
    head->setSpacing(4);
    head->addWidget(m_tabDiary);
    head->addWidget(m_tabBook);
    head->addStretch(1);
    head->addWidget(close, 0, Qt::AlignTop);

    // Página 0: el diario del día
    auto* diary = new QWidget(this);
    m_date = new QLabel(diary);
    m_date->setProperty("dim", true);
    m_note = new QPlainTextEdit(diary);
    m_note->setObjectName("NotePadText");
    m_note->setPlaceholderText(QStringLiteral("Lo que querés hacer hoy, cómo salió, lo que se te ocurra…"));
    m_note->setTabChangesFocus(true);
    m_moodLabel = new QLabel(diary);
    m_mood = new MoodBar(diary);
    auto* dl = new QVBoxLayout(diary);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(8);
    dl->addWidget(m_date);
    dl->addWidget(m_note, 1);
    dl->addWidget(m_moodLabel);
    dl->addWidget(m_mood);

    // Página 1: la libreta
    m_libreta = new Libreta(db, this);

    m_stack = new QStackedWidget(this);
    m_stack->addWidget(diary);
    m_stack->addWidget(m_libreta);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(16, 12, 14, 12);
    lay->setSpacing(8);
    lay->addLayout(head);
    lay->addWidget(m_stack, 1);
    showTab(0);

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
    if (m_libreta) m_libreta->flush();
    m_timer->stop();
    if (!m_dirty) return;
    m_dirty = false;
    emit noteEdited(m_day, m_note->toPlainText().trimmed());
}

void NotePad::focusNote() { m_note->setFocus(); }

void NotePad::showTab(int tab)
{
    if (m_tab != tab) { flush(); m_libreta->flush(); }
    m_tab = tab;
    m_stack->setCurrentIndex(tab);
    m_tabDiary->setChecked(tab == 0);
    m_tabBook->setChecked(tab == 1);
    if (tab == 1) m_libreta->reload();
}

void NotePad::focusCurrent() { if (m_tab == 1) m_libreta->focusPage(); else focusNote(); }
