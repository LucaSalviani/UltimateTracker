#include "ui/MoodBar.h"

#include <QHBoxLayout>
#include <QPushButton>

QString MoodBar::word(int mood)
{
    static const char* w[] = {"", "mal", "flojo", "normal", "bien", "excelente"};
    return QString::fromUtf8(w[qBound(0, mood, 5)]);
}

MoodBar::MoodBar(QWidget* parent) : QWidget(parent)
{
    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(6);
    for (int i = 0; i < 5; ++i) {
        auto* b = new QPushButton(QString::number(i + 1), this);
        b->setObjectName("MoodBtn");
        b->setCheckable(true);
        b->setFixedSize(32, 32);
        b->setCursor(Qt::PointingHandCursor);
        b->setFocusPolicy(Qt::NoFocus);
        b->setToolTip(word(i + 1));
        connect(b, &QPushButton::clicked, this, [this, i] {
            const int v = m_mood == i + 1 ? 0 : i + 1;
            setMood(v);
            emit moodChanged(v);
        });
        m_btn[i] = b;
        lay->addWidget(b);
    }
    lay->addStretch();
}

void MoodBar::setMood(int mood)
{
    m_mood = qBound(0, mood, 5);
    for (int i = 0; i < 5; ++i) m_btn[i]->setChecked(i + 1 == m_mood);
}
