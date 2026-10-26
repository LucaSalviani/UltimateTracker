#include "ui/SettingsDialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QVBoxLayout>

SettingsDialog::SettingsDialog(int workMin, int breakMin, int jokers, bool resizable, bool numericDate,
                               QWidget* parent)
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

    m_resizable = new QCheckBox(QStringLiteral("Permitir cambiar el tamaño de la ventana desde los bordes"), this);
    m_resizable->setChecked(resizable);

    m_dateFormat = new QComboBox(this);
    const QDate sample = QDate::currentDate();
    m_dateFormat->addItem(sample.toString(QStringLiteral("dd/MM/yy")) + QStringLiteral("  (solo números)"));
    m_dateFormat->addItem(QStringLiteral("Escrita (lunes 26 de octubre)"));
    m_dateFormat->setCurrentIndex(numericDate ? 0 : 1);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Pomodoro por defecto"), m_work);
    form->addRow(QStringLiteral("Recreo por defecto"), m_break);
    form->addRow(QStringLiteral("Comodines por semana"), m_jokers);
    form->addRow(QStringLiteral("Formato de la fecha"), m_dateFormat);

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
    lay->addSpacing(6);
    lay->addWidget(m_resizable);
    lay->addWidget(buttons);
}

int SettingsDialog::workMinutes() const { return m_work->value(); }
int SettingsDialog::breakMinutes() const { return m_break->value(); }
int SettingsDialog::jokersPerWeek() const { return m_jokers->value(); }
bool SettingsDialog::resizable() const { return m_resizable->isChecked(); }
bool SettingsDialog::numericDate() const { return m_dateFormat->currentIndex() == 0; }
