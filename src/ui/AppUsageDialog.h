#pragma once

#include "core/AppUsage.h"

#include <QDate>
#include <QDialog>

class Database;
class QLabel;
class QPushButton;

// Mejoras 7.0: «Uso de apps». Dos tortas (con un pomodoro corriendo / fuera de un pomodoro) para el
// día, la semana o el mes, con leyenda (programa, tiempo, %). Click en un programa: sus ventanas más usadas.
class AppUsageDialog : public QDialog {
    Q_OBJECT
public:
    AppUsageDialog(Database* db, const QDate& today, QWidget* parent);

private:
    class Pie;
    void reload();
    QPair<QDate, QDate> range() const;
    Database* m_db;
    QDate     m_today, m_anchor;
    int       m_mode = 0;   // 0 día, 1 semana, 2 mes
    QLabel*   m_title = nullptr;
    QLabel*   m_note = nullptr;
    Pie*      m_pomo = nullptr;
    Pie*      m_out = nullptr;
    QVector<QPushButton*> m_modeBtns;
};
