#include "ui/SettingsDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(int workMin, int breakMin, int jokers, QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Ajustes"));

    m_work = new QSpinBox(this);
    m_work->setRange(1, 240);
    m_work->setSuffix(QStringLiteral(" min"));
    m_work->setValue(workMin);

    m_break = new QSpinBox(this);
    m_break->setRange(1, 120);
    m_break->setSuffix(QStringLiteral(" min"));
    m_break->setValue(breakMin);

    m_jokers = new QSpinBox(this);
    m_jokers->setRange(0, 3);
    m_jokers->setValue(jokers);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Pomodoro por defecto"), m_work);
    form->addRow(QStringLiteral("Recreo por defecto"), m_break);
    form->addRow(QStringLiteral("Comodines por semana"), m_jokers);

    auto* note = new QLabel(QStringLiteral("Los comodines son días sin estudiar que no cortan la racha."), this);
    note->setWordWrap(true);
    note->setProperty("dim", true);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Guardar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* lay = new QVBoxLayout(this);
    lay->addLayout(form);
    lay->addWidget(note);
    lay->addWidget(buttons);
}

int SettingsDialog::workMinutes() const { return m_work->value(); }
int SettingsDialog::breakMinutes() const { return m_break->value(); }
int SettingsDialog::jokersPerWeek() const { return m_jokers->value(); }
