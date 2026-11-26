#include "ui/RoutineDialog.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDateEdit>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QTimeEdit>
#include <QVBoxLayout>

namespace routinedlg {

QColor colorFor(const QString& category)
{
    static const QColor pal[] = {QColor(0x6f, 0x9c, 0xd6), QColor(0xd0, 0x8f, 0x6a), QColor(0x7f, 0xb8, 0x8a), QColor(0xc2, 0x7d, 0xa6),
                                 QColor(0xb8, 0xa9, 0x5e), QColor(0x8f, 0x8c, 0xd6), QColor(0x6c, 0xb8, 0xb4)};
    const QStringList def = routine::defaultCategories();
    int i = def.indexOf(category);
    if (i < 0) i = int(qHash(category) % 7);
    return pal[i % 7];
}

bool edit(Database* db, qint64 id, const QDate& day, QWidget* parent)
{
    routine::Event e;
    for (const routine::Event& x : routine::events(*db)) if (x.id == id) e = x;
    QDialog dlg(parent);
    dlg.setWindowTitle(id ? QStringLiteral("Editar evento de la rutina") : QStringLiteral("Nuevo evento de la rutina"));
    dlg.setMinimumWidth(380);

    auto* title = new QLineEdit(e.title, &dlg);
    title->setPlaceholderText(QStringLiteral("Agregar título"));
    QFont tf = title->font();
    tf.setPointSizeF(tf.pointSizeF() + 5);
    title->setFont(tf);
    title->setStyleSheet(QStringLiteral("QLineEdit { border: none; border-bottom: 2px solid %1; background: transparent; padding: 4px 0; }").arg(theme::textDim.name()));

    auto* cat = new QComboBox(&dlg);
    cat->setEditable(true);
    cat->addItems(routine::defaultCategories());
    cat->setCurrentText(e.category.isEmpty() ? routine::defaultCategories().first() : e.category);
    auto* dot = new QLabel(&dlg);
    auto paintDot = [dot, cat] {
        dot->setFixedSize(12, 12);
        dot->setStyleSheet(QStringLiteral("background:%1; border-radius:6px;").arg(colorFor(cat->currentText().trimmed()).name()));
    };
    paintDot();
    QObject::connect(cat, &QComboBox::currentTextChanged, &dlg, paintDot);
    auto* catRow = new QHBoxLayout;
    catRow->addWidget(dot);
    catRow->addWidget(cat, 1);

    const QDate base = e.day.isValid() ? e.day : (day.isValid() ? day : QDate::currentDate());
    auto* from = new QTimeEdit(QTime(0, 0).addSecs((id ? e.startMin : 9 * 60) * 60), &dlg);
    auto* to = new QTimeEdit(QTime(0, 0).addSecs((id ? e.endMin : 10 * 60) * 60), &dlg);
    from->setDisplayFormat(QStringLiteral("HH:mm"));
    to->setDisplayFormat(QStringLiteral("HH:mm"));
    auto* when = new QHBoxLayout;
    when->addWidget(from);
    when->addWidget(new QLabel(QStringLiteral("–"), &dlg));
    when->addWidget(to);
    when->addStretch();

    // Repetición, como Google Calendar.
    auto* rep = new QComboBox(&dlg);
    rep->addItem(QStringLiteral("No se repite (solo %1)").arg(base.toString(QStringLiteral("dd/MM"))), 0);
    rep->addItem(QStringLiteral("Todos los días"), 1);
    rep->addItem(QStringLiteral("Lunes a viernes"), 2);
    rep->addItem(QStringLiteral("Cada semana el %1").arg(QLocale().dayName(base.dayOfWeek())), 3);
    rep->addItem(QStringLiteral("Personalizar…"), 4);
    auto* daysBox = new QWidget(&dlg);
    auto* days = new QHBoxLayout(daysBox);
    days->setContentsMargins(0, 0, 0, 0);
    QVector<QCheckBox*> dayBoxes;
    static const char* dn[] = {"L", "M", "X", "J", "V", "S", "D"};
    for (int i = 0; i < 7; ++i) {
        auto* c = new QCheckBox(QString::fromUtf8(dn[i]), daysBox);
        dayBoxes << c;
        days->addWidget(c);
    }
    auto* until = new QCheckBox(QStringLiteral("Termina el"), &dlg);
    auto* untilDate = new QDateEdit(e.to.isValid() ? e.to : base.addMonths(3), &dlg);
    untilDate->setCalendarPopup(true);
    until->setChecked(e.to.isValid());
    untilDate->setEnabled(until->isChecked());
    QObject::connect(until, &QCheckBox::toggled, untilDate, &QWidget::setEnabled);

    auto setDays = [&](int mask) { for (int i = 0; i < 7; ++i) dayBoxes[i]->setChecked(mask & (1 << i)); };
    auto sync = [&] {
        const int k = rep->currentData().toInt();
        daysBox->setVisible(k == 4);
        until->setVisible(k != 0);
        untilDate->setVisible(k != 0);
    };
    if (id && e.weekdays) {
        setDays(e.weekdays);
        const int m = e.weekdays & 127;
        rep->setCurrentIndex(m == 127 ? 1 : (m == 31 ? 2 : (m == (1 << (base.dayOfWeek() - 1)) ? 3 : 4)));
    } else if (id) rep->setCurrentIndex(0);
    QObject::connect(rep, &QComboBox::currentIndexChanged, &dlg, sync);
    sync();

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Categoría"), catRow);
    form->addRow(QStringLiteral("Horario"), when);
    form->addRow(QStringLiteral("Repetir"), rep);
    form->addRow(QString(), daysBox);
    auto* untilRow = new QHBoxLayout;
    untilRow->addWidget(until);
    untilRow->addWidget(untilDate);
    untilRow->addStretch();
    form->addRow(QString(), untilRow);

    auto* hint = new QLabel(QStringLiteral("Si «hasta» es antes que «desde», cruza la medianoche. No avisa nada: sirve para explicar tus horas inactivas."), &dlg);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:%1; font-size:11px;").arg(theme::textFaint.name()));

    auto* bb = new QDialogButtonBox(&dlg);
    auto* save = bb->addButton(QStringLiteral("Guardar"), QDialogButtonBox::AcceptRole);
    bb->addButton(QStringLiteral("Cancelar"), QDialogButtonBox::RejectRole);
    QPushButton* del = nullptr;
    bool deleted = false;
    if (id) {
        del = bb->addButton(QStringLiteral("Borrar"), QDialogButtonBox::DestructiveRole);
        QObject::connect(del, &QPushButton::clicked, &dlg, [&] { deleted = true; dlg.accept(); });
    }
    save->setDefault(true);
    QObject::connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* v = new QVBoxLayout(&dlg);
    v->addWidget(title);
    v->addSpacing(6);
    v->addLayout(form);
    v->addWidget(hint);
    v->addWidget(bb);
    title->setFocus();

    if (dlg.exec() != QDialog::Accepted) return false;
    if (deleted) { routine::remove(*db, id); return true; }
    e.title = title->text().trimmed();
    e.category = cat->currentText().trimmed();
    if (e.title.isEmpty() && e.category.isEmpty()) return false;
    e.startMin = from->time().hour() * 60 + from->time().minute();
    e.endMin = to->time().hour() * 60 + to->time().minute();
    const int k = rep->currentData().toInt();
    e.from = QDate();
    e.to = (k != 0 && until->isChecked()) ? untilDate->date() : QDate();
    switch (k) {
    case 0: e.weekdays = 0; e.day = base; break;
    case 1: e.weekdays = 127; e.day = QDate(); break;
    case 2: e.weekdays = 31; e.day = QDate(); break;
    case 3: e.weekdays = 1 << (base.dayOfWeek() - 1); e.day = QDate(); break;
    default:
        e.weekdays = 0;
        for (int i = 0; i < 7; ++i) if (dayBoxes[i]->isChecked()) e.weekdays |= 1 << i;
        e.day = QDate();
        if (e.weekdays == 0) return false;
    }
    if (k != 0 && !id) e.from = base;   // lo que se repite empieza ese día
    routine::save(*db, e);
    return true;
}

} // namespace routinedlg
