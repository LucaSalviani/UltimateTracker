#pragma once

#include <QDate>
#include <QFrame>

class QLabel;
class QPlainTextEdit;
class QPushButton;
class QTimer;
class MoodBar;
class Libreta;
class Database;
class QStackedWidget;

// Mejoras 4.1: el anotador. Una hoja libre por día (lo que querés hacer, cómo salió,
// lo que sea) con el "¿cómo te va?" abajo. Se despliega al lado del panel del objetivo.
// No toca la base: avisa con señales y MainWindow guarda.
class NotePad : public QFrame {
    Q_OBJECT
public:
    explicit NotePad(Database* db, QWidget* parent = nullptr);
    void showTab(int tab);   // 0 = Diario (el día), 1 = Libreta (permanente)
    int  tab() const { return m_tab; }
    void setDay(const QDate& day, const QString& note, int mood, bool isToday);
    QDate day() const { return m_day; }
    void flush();   // manda lo escrito que todavía no se guardó
    void focusNote();
    void focusCurrent();
    bool expanded() const { return m_expanded; }   // Mejoras 7.0: modo bloc de notas (más grande)
    void setExpanded(bool on);

signals:
    void noteEdited(const QDate& day, const QString& note);
    void moodChanged(const QDate& day, int mood);
    void closeRequested();
    void expandedChanged(bool expanded);

private:
    int             m_tab = 0;
    QStackedWidget* m_stack = nullptr;
    QPushButton*    m_tabDiary = nullptr;
    QPushButton*    m_tabBook = nullptr;
    Libreta*        m_libreta = nullptr;
    QDate           m_day;
    QLabel*         m_date;
    QLabel*         m_moodLabel;
    QPlainTextEdit* m_note;
    MoodBar*        m_mood;
    QTimer*         m_timer;
    bool            m_dirty = false;
    bool            m_expanded = false;
    QPushButton*    m_btnExpand = nullptr;
};
