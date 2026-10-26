#pragma once

#include <QDialog>

class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(int workMin, int breakMin, int jokers, QWidget* parent = nullptr);
    int workMinutes() const;
    int breakMinutes() const;
    int jokersPerWeek() const;

private:
    QSpinBox* m_work;
    QSpinBox* m_break;
    QSpinBox* m_jokers;
};
