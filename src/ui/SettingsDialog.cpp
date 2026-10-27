#include "ui/SettingsDialog.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
QLabel* note(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setWordWrap(true);
    l->setProperty("dim", true);
    return l;
}

QLabel* heading(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text, parent);
    l->setProperty("heading", true);
    return l;
}

QSpinBox* spin(int min, int max, int value, const QString& suffix, QWidget* parent)
{
    auto* s = new QSpinBox(parent);
    s->setRange(min, max);
    s->setValue(value);
    s->setSuffix(suffix);
    return s;
}

// Muestra de colores de una paleta, para elegir tema.
QIcon swatch(const theme::Palette& p)
{
    QPixmap pm(56, 34);
    pm.fill(Qt::transparent);
    QPainter g(&pm);
    g.setRenderHint(QPainter::Antialiasing);
    g.setPen(QPen(theme::mix(p.bg, p.ink, 0.25), 1));
    g.setBrush(p.bg);
    g.drawRoundedRect(QRectF(0.5, 0.5, 55, 33), 7, 7);
    const QColor cells[4] = {p.squareDone, p.squareDone, p.squareFill, p.accent};
    for (int i = 0; i < 4; ++i) {
        g.setPen(Qt::NoPen);
        g.setBrush(cells[i]);
        g.drawRoundedRect(QRectF(6 + i * 11.5, 11, 9, 12), 2, 2);
    }
    return QIcon(pm);
}
} // namespace

void SettingsDialog::currentTheme(Database* db, QString* color, QString* mode)
{
    *color = db->setting("theme_color").toString();
    *mode = db->setting("theme_mode").toString();
    if (color->isEmpty()) {   // ajuste viejo de un solo valor
        theme::migrateSetting(color, mode, db->setting("theme").toString());
        db->setSetting("theme_color", *color);
        db->setSetting("theme_mode", *mode);
    }
}

QString SettingsDialog::themeKey(Database* db)
{
    QString c, m;
    currentTheme(db, &c, &m);
    return theme::keyFor(c, m);
}

SettingsDialog::SettingsDialog(Database* db, QWidget* parent)
    : QDialog(parent)
    , m_db(db)
    , m_originalTheme(themeKey(db))
{
    setWindowTitle(QStringLiteral("Ajustes"));
    resize(620, 470);

    m_nav = new QListWidget(this);
    m_nav->setObjectName("SettingsNav");
    m_nav->setFixedWidth(150);
    for (const QString& s : {QStringLiteral("General"), QStringLiteral("Cronómetro"),
                             QStringLiteral("Apariencia"), QStringLiteral("Sistema")})
        m_nav->addItem(s);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(pageGeneral());
    m_pages->addWidget(pageTimer());
    m_pages->addWidget(pageAppearance());
    m_pages->addWidget(pageSystem());
    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Guardar"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto* body = new QHBoxLayout;
    body->setSpacing(14);
    body->addWidget(m_nav);
    body->addWidget(m_pages, 1);

    auto* lay = new QVBoxLayout(this);
    lay->addLayout(body, 1);
    lay->addWidget(buttons);
}

// ---------------------------------------------------------------------------
QWidget* SettingsDialog::pageGeneral()
{
    auto* w = new QWidget(this);
    m_work = spin(1, 240, m_db->setting("work_min", 25).toInt(), QStringLiteral(" min"), w);
    m_break = spin(1, 120, m_db->setting("break_min", 10).toInt(), QStringLiteral(" min"), w);
    m_jokers = spin(0, 3, m_db->setting("jokers_per_week", 2).toInt(), QString(), w);
    m_dateFormat = new QComboBox(w);
    m_dateFormat->addItem(QDate::currentDate().toString(QStringLiteral("dd/MM/yy")) + QStringLiteral("  (solo números)"));
    m_dateFormat->addItem(QStringLiteral("Escrita (lunes 26 de octubre)"));
    m_dateFormat->setCurrentIndex(m_db->setting("date_format", "numeric").toString() == QLatin1String("numeric") ? 0 : 1);
    m_dayStart = spin(0, 6, m_db->setting("day_start_hour", 4).toInt(), QStringLiteral(":00 h"), w);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Pomodoro por defecto"), m_work);
    form->addRow(QStringLiteral("Recreo por defecto"), m_break);
    form->addRow(QStringLiteral("Comodines por semana"), m_jokers);
    form->addRow(QStringLiteral("Formato de la fecha"), m_dateFormat);
    form->addRow(QStringLiteral("El día empieza a las"), m_dayStart);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("General"), w));
    lay->addLayout(form);
    lay->addWidget(note(QStringLiteral("Los comodines son días sin estudiar que no cortan la racha. "
                                       "Si el día empieza a las 4, lo que estudies a las 2 AM cuenta para el día anterior "
                                       "(se aplica al reiniciar la app)."), w));
    lay->addStretch();
    return w;
}

QWidget* SettingsDialog::pageTimer()
{
    auto* w = new QWidget(this);
    m_wheel = new QCheckBox(QStringLiteral("Cambiar la duración con la rueda del mouse sobre el tiempo"), w);
    m_wheel->setChecked(m_db->setting("timer_wheel", 1).toBool());
    m_wheelStep = spin(1, 10, m_db->setting("timer_wheel_step", 1).toInt(), QStringLiteral(" min por paso"), w);
    m_type = new QCheckBox(QStringLiteral("Click en el tiempo para escribir los minutos"), w);
    m_type->setChecked(m_db->setting("timer_type", 1).toBool());
    m_extraButtons = new QCheckBox(QStringLiteral("Botones de paso fino a los costados (− − / + +)"), w);
    m_extraButtons->setChecked(m_db->setting("timer_buttons", 1).toBool());
    m_stepSmall = spin(1, 30, m_db->setting("timer_step_small", 1).toInt(), QStringLiteral(" min"), w);
    m_stepBig = spin(1, 60, m_db->setting("timer_step_big", 5).toInt(), QStringLiteral(" min"), w);
    connect(m_wheel, &QCheckBox::toggled, m_wheelStep, &QWidget::setEnabled);
    m_wheelStep->setEnabled(m_wheel->isChecked());

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Sensibilidad de la rueda"), m_wheelStep);
    form->addRow(QStringLiteral("Botones de afuera"), m_stepSmall);
    form->addRow(QStringLiteral("Botones de adentro"), m_stepBig);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Cronómetro"), w));
    lay->addWidget(m_wheel);
    lay->addWidget(m_type);
    lay->addWidget(m_extraButtons);
    lay->addSpacing(6);
    lay->addLayout(form);
    lay->addWidget(note(QStringLiteral("Sin los botones de paso fino, el − / + de siempre va de a 5 minutos "
                                       "arriba de 10 y de a 1 por debajo."), w));
    lay->addStretch();
    return w;
}

QWidget* SettingsDialog::pageAppearance()
{
    auto* w = new QWidget(this);
    // M15: color + modo (Oscuro / Claro / Seguir a Windows)
    {
        QString c, m;
        currentTheme(m_db, &c, &m);
        m_themeColor = c;
        m_themeMode = m;
    }
    m_themes = new QButtonGroup(w);
    auto* grid = new QGridLayout;
    grid->setSpacing(8);
    const QStringList colorKeys = theme::colors();
    for (int i = 0; i < colorKeys.size(); ++i) {
        auto* b = new QToolButton(w);
        b->setObjectName("ThemeChoice");
        b->setCheckable(true);
        b->setIconSize(QSize(56, 34));
        b->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
        b->setProperty("color", colorKeys[i]);
        b->setChecked(colorKeys[i] == m_themeColor);
        b->setMinimumWidth(118);
        m_themes->addButton(b, i);
        grid->addWidget(b, i / 3, i % 3);
    }
    m_modes = new QButtonGroup(w);
    auto* modeRow = new QHBoxLayout;
    modeRow->setSpacing(0);
    const QList<QPair<QString, QString>> modes = {{"dark", QStringLiteral("Oscuro")},
                                                  {"light", QStringLiteral("Claro")},
                                                  {"system", QStringLiteral("Seguir a Windows")}};
    for (int i = 0; i < modes.size(); ++i) {
        auto* b = new QPushButton(modes[i].second, w);
        b->setObjectName("GoalSeg");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        b->setProperty("mode", modes[i].first);
        b->setChecked(modes[i].first == m_themeMode);
        m_modes->addButton(b, i);
        modeRow->addWidget(b);
    }
    modeRow->addStretch();
    auto refreshSwatches = [this] {
        for (QAbstractButton* b : m_themes->buttons()) {
            const QString key = theme::keyFor(b->property("color").toString(), m_themeMode);
            for (const theme::Palette& p : theme::palettes())
                if (p.key == key) {
                    b->setIcon(swatch(p));
                    b->setText(p.name + (b->property("color") == "gray" ? QStringLiteral(" (por defecto)") : QString()));
                }
        }
    };
    refreshSwatches();
    // Vista previa al toque; si cancelás, vuelve al tema anterior.
    connect(m_themes, &QButtonGroup::idClicked, this, [this](int id) {
        m_themeColor = m_themes->button(id)->property("color").toString();
        theme::apply(theme::keyFor(m_themeColor, m_themeMode));
    });
    connect(m_modes, &QButtonGroup::idClicked, this, [this, refreshSwatches](int id) {
        m_themeMode = m_modes->button(id)->property("mode").toString();
        refreshSwatches();
        theme::apply(theme::keyFor(m_themeColor, m_themeMode));
    });

    m_size = new QComboBox(w);
    m_size->addItem(QStringLiteral("Chica"), "small");
    m_size->addItem(QStringLiteral("Mediana (por defecto)"), "medium");
    m_size->addItem(QStringLiteral("Grande"), "large");
    m_size->setCurrentIndex(qMax(0, m_size->findData(m_db->setting("window_size", "medium").toString())));
    m_resizable = new QCheckBox(QStringLiteral("Permitir cambiar el tamaño de la ventana desde los bordes"), w);
    m_resizable->setChecked(m_db->setting("resizable", 1).toBool());

    m_groupByActivity = new QCheckBox(QStringLiteral("Agrupar los cuadraditos por actividad"), w);
    m_groupByActivity->setChecked(m_db->setting("group_by_activity", 1).toBool());
    m_celebrations = new QCheckBox(QStringLiteral("Animación al terminar un pomodoro, cumplir el objetivo o llegar a un hito"), w);
    m_celebrations->setChecked(m_db->setting("celebrations", 1).toBool());

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Tamaño de la ventana"), m_size);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Apariencia"), w));
    lay->addWidget(note(QStringLiteral("Tema de colores"), w));
    lay->addLayout(modeRow);
    lay->addLayout(grid);
    lay->addSpacing(8);
    lay->addLayout(form);
    lay->addWidget(m_resizable);
    lay->addWidget(m_groupByActivity);
    lay->addWidget(m_celebrations);
    lay->addWidget(note(QStringLiteral("Cambiar el tamaño vuelve todas las vistas a su tamaño original."), w));
    lay->addStretch();
    return w;
}

QWidget* SettingsDialog::pageSystem()
{
    auto* w = new QWidget(this);
    m_autostart = new QCheckBox(QStringLiteral("Iniciar con Windows"), w);
    m_autostart->setChecked(m_db->setting("autostart", 0).toBool());
    m_greeting = new QCheckBox(QStringLiteral("Saludarme al iniciar con Windows (una vez por día)"), w);
    m_greeting->setChecked(m_db->setting("greeting", 1).toBool());
    m_hotkey = new QCheckBox(QStringLiteral("Atajo global Ctrl + Alt + P para pausar / seguir desde cualquier lado"), w);
    m_hotkey->setChecked(m_db->setting("hotkey", 1).toBool());
    m_quietFullscreen = new QCheckBox(QStringLiteral("No saltar al frente si estás en pantalla completa (solo aviso y sonido)"), w);
    m_quietFullscreen->setChecked(m_db->setting("quiet_fullscreen", 1).toBool());
    m_backup = new QCheckBox(QStringLiteral("Copia de seguridad diaria (guarda las últimas 7)"), w);
    m_backup->setChecked(m_db->setting("backup", 1).toBool());
    m_backupDir = new QLineEdit(m_db->setting("backup_dir").toString(), w);
    m_backupDir->setPlaceholderText(QStringLiteral("Carpeta (vacío = junto a los datos de la app)"));
    auto* pick = new QPushButton(QStringLiteral("Elegir…"), w);
    connect(pick, &QPushButton::clicked, this, [this] {
        const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Carpeta para las copias"), m_backupDir->text());
        if (!dir.isEmpty()) m_backupDir->setText(dir);
    });
    auto* dirRow = new QHBoxLayout;
    dirRow->addWidget(m_backupDir, 1);
    dirRow->addWidget(pick);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Sistema"), w));
    lay->addWidget(m_autostart);
    lay->addWidget(m_greeting);
    lay->addWidget(m_hotkey);
    lay->addWidget(m_quietFullscreen);
    lay->addSpacing(6);
    lay->addWidget(m_backup);
    lay->addLayout(dirRow);
    lay->addWidget(note(QStringLiteral("Tip: si elegís una carpeta de OneDrive o Google Drive, las copias también "
                                       "te salvan si se rompe el disco."), w));
    lay->addStretch();
    return w;
}

// ---------------------------------------------------------------------------
void SettingsDialog::accept()
{
    m_db->setSetting("work_min", m_work->value());
    m_db->setSetting("break_min", m_break->value());
    m_db->setSetting("jokers_per_week", m_jokers->value());
    m_db->setSetting("date_format", m_dateFormat->currentIndex() == 0 ? "numeric" : "long");
    m_db->setSetting("day_start_hour", m_dayStart->value());

    m_db->setSetting("timer_wheel", m_wheel->isChecked() ? 1 : 0);
    m_db->setSetting("timer_wheel_step", m_wheelStep->value());
    m_db->setSetting("timer_type", m_type->isChecked() ? 1 : 0);
    m_db->setSetting("timer_buttons", m_extraButtons->isChecked() ? 1 : 0);
    m_db->setSetting("timer_step_small", m_stepSmall->value());
    m_db->setSetting("timer_step_big", m_stepBig->value());

    m_db->setSetting("theme_color", m_themeColor);
    m_db->setSetting("theme_mode", m_themeMode);
    m_db->setSetting("window_size", m_size->currentData().toString());
    m_db->setSetting("resizable", m_resizable->isChecked() ? 1 : 0);
    m_db->setSetting("group_by_activity", m_groupByActivity->isChecked() ? 1 : 0);
    m_db->setSetting("celebrations", m_celebrations->isChecked() ? 1 : 0);

    m_db->setSetting("autostart", m_autostart->isChecked() ? 1 : 0);
    m_db->setSetting("greeting", m_greeting->isChecked() ? 1 : 0);
    m_db->setSetting("hotkey", m_hotkey->isChecked() ? 1 : 0);
    m_db->setSetting("quiet_fullscreen", m_quietFullscreen->isChecked() ? 1 : 0);
    m_db->setSetting("backup", m_backup->isChecked() ? 1 : 0);
    m_db->setSetting("backup_dir", m_backupDir->text().trimmed());
    QDialog::accept();
}

void SettingsDialog::reject()
{
    theme::apply(m_originalTheme);   // deshace la vista previa
    QDialog::reject();
}
