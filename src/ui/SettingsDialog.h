#pragma once

#include <QDialog>

class QCheckBox;
class QComboBox;
class QSpinBox;

class SettingsDialog : public QDialog {
    Q_OBJECT
public:
    SettingsDialog(int workMin, int breakMin, int jokers, bool resizable, bool numericDate,
                   QWidget* parent = nullptr);
    bool resizable() const;
    bool numericDate() const;
    int workMinutes() const;
    int breakMinutes() const;
    int jokersPerWeek() const;

private:
    QSpinBox* m_work;
    QSpinBox* m_break;
    QSpinBox* m_jokers;
    QCheckBox* m_resizable;
    QComboBox* m_dateFormat;
};
