#pragma once

#include <QWidget>

class QPushButton;

// Mejoras 4.0 · 10: "¿cómo te fue?" del 1 al 5 (mal, flojo, normal, bien, excelente).
// Tocar el que está marcado lo desmarca (0 = sin decir).
class MoodBar : public QWidget {
    Q_OBJECT
public:
    explicit MoodBar(QWidget* parent = nullptr);
    void setMood(int mood);
    int  mood() const { return m_mood; }
    static QString word(int mood);

signals:
    void moodChanged(int mood);

private:
    QPushButton* m_btn[5];
    int m_mood = 0;
};
