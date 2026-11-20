#pragma once

#include <QDate>
#include <QFrame>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class MoodBar;

// Mejoras 4.1: el anotador. Una hoja libre por día (lo que querés hacer, cómo salió,
// lo que sea) con el "¿cómo te va?" abajo. Se despliega al lado del panel del objetivo.
// No toca la base: avisa con señales y MainWindow guarda.
class NotePad : public QFrame {
    Q_OBJECT
public:
    explicit NotePad(QWidget* parent = nullptr);
    void setDay(const QDate& day, const QString& note, int mood, bool isToday);
    QDate day() const { return m_day; }
    void flush();   // manda lo escrito que todavía no se guardó
    void focusNote();

signals:
    void noteEdited(const QDate& day, const QString& note);
    void moodChanged(const QDate& day, int mood);
    void closeRequested();

private:
    QDate           m_day;
    QLabel*         m_date;
    QLabel*         m_moodLabel;
    QPlainTextEdit* m_note;
    MoodBar*        m_mood;
    QTimer*         m_timer;
    bool            m_dirty = false;
};
