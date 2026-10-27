#include "ui/SettingsDialog.h"
#include "ui/Theme.h"
#include "ui/Tiles.h"
#include "ui/DevMode.h"
#include <QMessageBox>
#include <QApplication>

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

void SettingsDialog::loadTileStyle(Database* db)
{
    tiles::Style& s = tiles::style();
    s.shape = tiles::Shape(qBound(0, db->setting("tile_shape", 0).toInt(), 3));
    s.mark = tiles::Mark(qBound(0, db->setting("tile_mark", 0).toInt(), 4));
    s.fill = tiles::Fill(qBound(0, db->setting("tile_fill", 0).toInt(), 3));
    s.complete = tiles::Complete(qBound(0, db->setting("tile_complete", 0).toInt(), 3));
    s.inverted = db->setting("tile_inverted", 1).toBool();
    s.stripe = db->setting("tile_stripe", 1).toBool();
    s.caption = db->setting("tile_caption", 0).toBool();
    s.border = qBound(0, db->setting("tile_border", 1).toInt(), 2);
    s.palette = qBound(0, db->setting("tile_palette", 0).toInt(), 5);
}

QString SettingsDialog::themeKey(Database* db)
{
    theme::setStrongAccents(db->setting("accent_strong", 0).toBool());   // M22
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
                             QStringLiteral("Apariencia"), QStringLiteral("Cuadraditos"), QStringLiteral("Sistema")})
        m_nav->addItem(s);

    m_pages = new QStackedWidget(this);
    m_pages->addWidget(pageGeneral());
    m_pages->addWidget(pageTimer());
    m_pages->addWidget(pageAppearance());
    m_pages->addWidget(pageTiles());
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
    m_goalInput = new QComboBox(w);
    m_goalInput->addItem(QStringLiteral("Botones (cantidad × minutos)"), "buttons");
    m_goalInput->addItem(QStringLiteral("Texto (3x30 + 2x25)"), "text");
    m_goalInput->setCurrentIndex(m_db->setting("goal_input", "buttons").toString() == QLatin1String("text") ? 1 : 0);
    form->addRow(QStringLiteral("Objetivo por pomodoros"), m_goalInput);

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
    m_strong = new QCheckBox(QStringLiteral("Acentos fuertes (colores más potentes)"), w);
    m_strong->setChecked(m_db->setting("accent_strong", 0).toBool());
    m_originalStrong = m_strong->isChecked();
    connect(m_strong, &QCheckBox::toggled, this, [this](bool on) {
        theme::setStrongAccents(on);
        theme::apply(theme::keyFor(m_themeColor, m_themeMode));
    });
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

    m_groupStyle = new QComboBox(w);
    m_groupStyle->addItem(QStringLiteral("Todos juntos, sin agrupar"));
    m_groupStyle->addItem(QStringLiteral("Por actividad, con títulos"));
    m_groupStyle->addItem(QStringLiteral("Por actividad, en línea y recuadrados"));
    m_groupStyle->setCurrentIndex(m_db->setting("group_style", m_db->setting("group_by_activity", 1).toBool() ? 1 : 0).toInt());
    m_celebrations = new QCheckBox(QStringLiteral("Animación al terminar un pomodoro, cumplir el objetivo o llegar a un hito"), w);
    m_celebrations->setChecked(m_db->setting("celebrations", 1).toBool());

    // M16: tamaño de los cuadraditos
    const int mask = m_db->setting("size_modes", 7).toInt();
    const char* sizeNames[] = {"Todos iguales", "Proporcional solo a lo ancho", "Proporcional completo"};
    auto* sizeRow = new QHBoxLayout;
    for (int i = 0; i < 3; ++i) {
        m_sizeModes[i] = new QCheckBox(QString::fromUtf8(sizeNames[i]), w);
        m_sizeModes[i]->setChecked(mask & (1 << i));
        sizeRow->addWidget(m_sizeModes[i]);
    }
    sizeRow->addStretch();
    m_sizeButton = new QCheckBox(QStringLiteral("Mostrar el botón para alternar (si no, queda fijo el de por defecto)"), w);
    m_sizeButton->setChecked(m_db->setting("size_button", 1).toBool());
    m_sizeDefault = new QComboBox(w);
    for (const char* n : sizeNames) m_sizeDefault->addItem(QString::fromUtf8(n));
    m_sizeDefault->setCurrentIndex(m_db->setting("size_default", 0).toInt());

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Tamaño de la ventana"), m_size);
    form->addRow(QStringLiteral("Cuadraditos por defecto"), m_sizeDefault);
    form->addRow(QStringLiteral("Agrupar"), m_groupStyle);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Apariencia"), w));
    lay->addWidget(note(QStringLiteral("Tema de colores"), w));
    lay->addLayout(modeRow);
    lay->addLayout(grid);
    lay->addWidget(m_strong);
    lay->addSpacing(8);
    lay->addLayout(form);
    lay->addWidget(note(QStringLiteral("El botón de tamaño de la barra cicla entre:"), w));
    lay->addLayout(sizeRow);
    lay->addWidget(m_sizeButton);
    lay->addWidget(m_resizable);
    lay->addWidget(m_celebrations);
    lay->addWidget(note(QStringLiteral("Cambiar el tamaño vuelve todas las vistas a su tamaño original."), w));
    lay->addStretch();
    return w;
}

// M18: estilo de los cuadraditos, con muestra en vivo.
QWidget* SettingsDialog::pageTiles()
{
    auto* w = new QWidget(this);
    m_originalTiles = tiles::style();
    auto combo = [w](const QStringList& names, int current) {
        auto* c = new QComboBox(w);
        for (const QString& n : names) c->addItem(n);
        c->setCurrentIndex(current);
        return c;
    };
    const tiles::Style st = tiles::style();
    m_tileShape = combo(tiles::shapeNames(), int(st.shape));
    m_tileMark = combo(tiles::markNames(), int(st.mark));
    m_tileFill = combo(tiles::fillNames(), int(st.fill));
    m_tileComplete = combo(tiles::completeNames(), int(st.complete));
    m_tilePalette = combo(tiles::paletteNames(), st.palette);
    m_tileBorder = combo(tiles::borderNames(), st.border);
    m_tileInverted = new QCheckBox(QStringLiteral("Invertidos: fondo gris que se va llenando del color de terminado"), w);
    m_tileInverted->setChecked(st.inverted);
    m_tileStripe = new QCheckBox(QStringLiteral("Rayita del color de la actividad abajo"), w);
    m_tileStripe->setChecked(st.stripe);
    m_tileCaption = new QCheckBox(QStringLiteral("Mostrar la actividad dentro del cuadradito"), w);
    m_tileCaption->setChecked(st.caption);
    m_tileSize = new QSpinBox(w);
    m_tileSize->setRange(60, 170);
    m_tileSize->setSingleStep(10);
    m_tileSize->setSuffix(QStringLiteral(" %"));
    m_tileSize->setValue(m_db->setting("tile_size", 100).toInt());
    m_originalTileSize = m_tileSize->value();
    auto* preview = new TilePreview(w);
    auto live = [this, preview] {
        tiles::Style& cur = tiles::style();
        cur.shape = tiles::Shape(m_tileShape->currentIndex());
        cur.mark = tiles::Mark(m_tileMark->currentIndex());
        cur.fill = tiles::Fill(m_tileFill->currentIndex());
        cur.complete = tiles::Complete(m_tileComplete->currentIndex());
        cur.palette = m_tilePalette->currentIndex();
        cur.border = m_tileBorder->currentIndex();
        cur.inverted = m_tileInverted->isChecked();
        cur.stripe = m_tileStripe->isChecked();
        cur.caption = m_tileCaption->isChecked();
        emit tileSizePreview(m_tileSize->value());
        preview->update();
        if (parentWidget()) parentWidget()->update();
        for (QWidget* x : QApplication::allWidgets()) x->update();
    };
    for (QComboBox* c : {m_tileShape, m_tileMark, m_tileFill, m_tileComplete, m_tilePalette, m_tileBorder})
        connect(c, &QComboBox::currentIndexChanged, this, live);
    for (QCheckBox* c : {m_tileInverted, m_tileStripe, m_tileCaption})
        connect(c, &QCheckBox::toggled, this, live);
    connect(m_tileSize, &QSpinBox::valueChanged, this, live);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Forma"), m_tileShape);
    form->addRow(QStringLiteral("Marca de hecho"), m_tileMark);
    form->addRow(QStringLiteral("Llenado (el que corre)"), m_tileFill);
    form->addRow(QStringLiteral("Al completarse"), m_tileComplete);
    form->addRow(QStringLiteral("Colores"), m_tilePalette);
    form->addRow(QStringLiteral("Borde"), m_tileBorder);
    form->addRow(QStringLiteral("Tamaño base"), m_tileSize);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Cuadraditos"), w));
    lay->addWidget(preview);
    lay->addSpacing(6);
    lay->addLayout(form);
    lay->addWidget(m_tileInverted);
    lay->addWidget(m_tileStripe);
    lay->addWidget(m_tileCaption);
    lay->addWidget(note(QStringLiteral("La muestra repite el ciclo: el tercero se llena y se completa. "
                                       "Inclinado = romboide apoyado sobre su base; Tarjeta = vertical, con la actividad "
                                       "arriba y el tiempo abajo; Biselado = esquinas cortadas."), w));
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

    m_devMode = new QCheckBox(QStringLiteral("Modo desarrollador: probar sobre una copia de tus datos (nada queda registrado)"), w);
    m_devMode->setChecked(devmode::enabledFlag(qApp->property("dataDir").toString()));

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
    lay->addSpacing(10);
    lay->addWidget(m_devMode);
    lay->addWidget(note(QStringLiteral("Con el modo desarrollador aparece el botón 🧪 en la barra (completar ya, "
                                       "acelerar el tiempo, ver festejos…). Se aplica al reiniciar."), w));
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
    m_db->setSetting("goal_input", m_goalInput->currentData().toString());

    m_db->setSetting("timer_wheel", m_wheel->isChecked() ? 1 : 0);
    m_db->setSetting("timer_wheel_step", m_wheelStep->value());
    m_db->setSetting("timer_type", m_type->isChecked() ? 1 : 0);
    m_db->setSetting("timer_buttons", m_extraButtons->isChecked() ? 1 : 0);
    m_db->setSetting("timer_step_small", m_stepSmall->value());
    m_db->setSetting("timer_step_big", m_stepBig->value());

    m_db->setSetting("accent_strong", m_strong->isChecked() ? 1 : 0);
    m_db->setSetting("theme_color", m_themeColor);
    m_db->setSetting("theme_mode", m_themeMode);
    m_db->setSetting("window_size", m_size->currentData().toString());
    m_db->setSetting("resizable", m_resizable->isChecked() ? 1 : 0);
    {
        int mask = 0;
        for (int i = 0; i < 3; ++i) if (m_sizeModes[i]->isChecked()) mask |= 1 << i;
        m_db->setSetting("size_modes", mask ? mask : 1);
        m_db->setSetting("size_button", m_sizeButton->isChecked() ? 1 : 0);
        m_db->setSetting("size_default", m_sizeDefault->currentIndex());
        if (!(mask & (1 << m_db->setting("size_mode", 0).toInt()))) m_db->setSetting("size_mode", m_sizeDefault->currentIndex());
    }
    m_db->setSetting("group_style", m_groupStyle->currentIndex());
    m_db->setSetting("celebrations", m_celebrations->isChecked() ? 1 : 0);

    m_db->setSetting("tile_shape", m_tileShape->currentIndex());
    m_db->setSetting("tile_mark", m_tileMark->currentIndex());
    m_db->setSetting("tile_fill", m_tileFill->currentIndex());
    m_db->setSetting("tile_complete", m_tileComplete->currentIndex());
    m_db->setSetting("tile_palette", m_tilePalette->currentIndex());
    m_db->setSetting("tile_border", m_tileBorder->currentIndex());
    m_db->setSetting("tile_inverted", m_tileInverted->isChecked() ? 1 : 0);
    m_db->setSetting("tile_stripe", m_tileStripe->isChecked() ? 1 : 0);
    m_db->setSetting("tile_caption", m_tileCaption->isChecked() ? 1 : 0);
    m_db->setSetting("tile_size", m_tileSize->value());
    m_db->setSetting("autostart", m_autostart->isChecked() ? 1 : 0);
    m_db->setSetting("greeting", m_greeting->isChecked() ? 1 : 0);
    m_db->setSetting("hotkey", m_hotkey->isChecked() ? 1 : 0);
    m_db->setSetting("quiet_fullscreen", m_quietFullscreen->isChecked() ? 1 : 0);
    m_db->setSetting("backup", m_backup->isChecked() ? 1 : 0);
    m_db->setSetting("backup_dir", m_backupDir->text().trimmed());
    // M20: el interruptor va en un archivo (no en la base) y pide reiniciar.
    const QString dataDir = qApp->property("dataDir").toString();
    if (m_devMode->isChecked() != devmode::enabledFlag(dataDir)) {
        devmode::setEnabledFlag(dataDir, m_devMode->isChecked());
        m_restartRequested = QMessageBox::question(
            this, QStringLiteral("Modo desarrollador"),
            m_devMode->isChecked() ? QStringLiteral("Para entrar al modo de pruebas hay que reiniciar la app. ¿Reiniciar ahora?")
                                   : QStringLiteral("Para volver a tus datos reales hay que reiniciar la app. ¿Reiniciar ahora?"),
            QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
    }
    QDialog::accept();
}

void SettingsDialog::reject()
{
    theme::setStrongAccents(m_originalStrong);
    theme::apply(m_originalTheme);   // deshace la vista previa
    tiles::style() = m_originalTiles;
    emit tileSizePreview(m_originalTileSize);
    QDialog::reject();
}
