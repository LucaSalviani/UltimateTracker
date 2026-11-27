#include "ui/SettingsDialog.h"
#include "core/AppUsage.h"
#include "core/DayClock.h"
#include "core/Stats.h"
#include "core/Routine.h"
#include "ui/RoutineDialog.h"
#include "ui/FocusGuard.h"
#include "ui/Theme.h"
#include "ui/Tiles.h"
#include "ui/Celebration.h"
#include "ui/Sounds.h"
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
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPainter>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QDateEdit>
#include <QTimeEdit>
#include <QScrollArea>
#include <cmath>
#include <functional>
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

// Título chico de grupo dentro de una página (en mayúsculas, como en el panel del objetivo).
QLabel* subheading(const QString& text, QWidget* parent)
{
    auto* l = new QLabel(text.toUpper(), parent);
    l->setProperty("role", "caption");
    return l;
}

// Mejoras 4.1: grupo plegable ("Avanzado") para bajar el ruido de cada página.
QWidget* collapsible(const QString& title, QWidget* parent, const std::function<void(QVBoxLayout*)>& fill, bool open = false)
{
    auto* box = new QWidget(parent);
    auto* outer = new QVBoxLayout(box);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(4);
    auto* head = new QToolButton(box);
    head->setObjectName("SettingsFold");
    head->setCursor(Qt::PointingHandCursor);
    head->setToolButtonStyle(Qt::ToolButtonTextOnly);
    auto* body = new QWidget(box);
    auto* bl = new QVBoxLayout(body);
    bl->setContentsMargins(0, 0, 0, 0);
    fill(bl);
    auto sync = [=](bool on) { head->setText((on ? QStringLiteral("▾  ") : QStringLiteral("▸  ")) + title); body->setVisible(on); };
    head->setCheckable(true);
    head->setChecked(open);
    QObject::connect(head, &QToolButton::toggled, box, sync);
    sync(open);
    outer->addWidget(head, 0, Qt::AlignLeft);
    outer->addWidget(body);
    return box;
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
    {   // M26/M31: festejo por cosa (si no hay, el general de antes)
        const int legacy = qBound(0, db->setting("celebration_style", 0).toInt(), 4);
        const char* keys[] = {"celebration_pomodoro", "celebration_goal", "celebration_milestone"};
        for (int i = 0; i < 3; ++i)
            Celebration::setStyleFor(Celebration::Kind(i), Celebration::Style(qBound(0, db->setting(keys[i], legacy).toInt(), 4)));
    }
    tiles::Style& s = tiles::style();
    s.shape = tiles::Shape(qBound(0, db->setting("tile_shape", 0).toInt(), 3));
    s.mark = tiles::Mark(qBound(0, db->setting("tile_mark", 2).toInt(), 4));
    s.fill = tiles::Fill(qBound(0, db->setting("tile_fill", 0).toInt(), 3));
    s.complete = tiles::Complete(qBound(0, db->setting("tile_complete", 2).toInt(), 3));
    s.inverted = db->setting("tile_inverted", 1).toBool();
    s.stripe = db->setting("tile_stripe", 0).toBool();
    s.caption = db->setting("tile_caption", 0).toBool();
    s.border = qBound(0, db->setting("tile_border", 0).toInt(), 2);
    s.palette = qBound(0, db->setting("tile_palette", 0).toInt(), 5);
    s.numberPos = qBound(0, db->setting("tile_number_pos", 1).toInt(), 2);
    s.numberPrefix = db->setting("tile_number_prefix", 1).toBool();
    s.highlight = qBound(0, db->setting("tile_highlight", 1).toInt(), 2);
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
    resize(660, 560);

    m_nav = new QListWidget(this);
    m_nav->setObjectName("SettingsNav");
    m_nav->setFixedWidth(150);
    for (const QString& s : {QStringLiteral("General"), QStringLiteral("Cronómetro"), QStringLiteral("Apariencia"),
                             QStringLiteral("Cuadraditos"), QStringLiteral("Festejos y sonidos"), QStringLiteral("Hitos"),
                             QStringLiteral("Sistema"), QStringLiteral("Rutina")})
        m_nav->addItem(s);

    m_pages = new QStackedWidget(this);
    // Cada página con scroll (Cuadraditos y Apariencia ya no entran enteras).
    auto wrap = [this](QWidget* page) {
        auto* sc = new QScrollArea(this);
        sc->setObjectName("SettingsScroll");
        sc->setWidgetResizable(true);
        sc->setFrameShape(QFrame::NoFrame);
        sc->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        page->setObjectName("SettingsPage");
        sc->setWidget(page);
        return sc;
    };
    m_pages->addWidget(wrap(pageGeneral()));
    m_pages->addWidget(wrap(pageTimer()));
    m_pages->addWidget(wrap(pageAppearance()));
    m_pages->addWidget(wrap(pageTiles()));
    m_pages->addWidget(wrap(pageCelebrations()));
    m_pages->addWidget(wrap(pageMilestones()));
    m_pages->addWidget(wrap(pageSystem()));
    m_pages->addWidget(wrap(pageRoutine()));
    connect(m_nav, &QListWidget::currentRowChanged, m_pages, &QStackedWidget::setCurrentIndex);
    m_nav->setCurrentRow(0);

    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Guardar"));
    buttons->button(QDialogButtonBox::Ok)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Ok)->setDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setAutoDefault(false);
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("Cancelar"));
    // M38: vuelve la sección que estás viendo a los valores por defecto (se guarda con Guardar).
    auto* reset = buttons->addButton(QStringLiteral("Restablecer esta sección"), QDialogButtonBox::ResetRole);
    reset->setAutoDefault(false);
    reset->setToolTip(QStringLiteral("Pone los valores por defecto en esta sección. No se guarda hasta que toques Guardar."));
    connect(reset, &QPushButton::clicked, this, [this] { resetSection(m_pages->currentIndex()); });
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
    m_sessionGap = spin(10, 240, m_db->setting("session_gap_min", 60).toInt(), QStringLiteral(" min"), w);   // Mejoras 5.1

    m_goalInput = new QComboBox(w);
    m_goalInput->addItem(QStringLiteral("Botones (cantidad × minutos)"), "buttons");
    m_goalInput->addItem(QStringLiteral("Texto (3x30 + 2x25)"), "text");
    m_goalStyle = new QComboBox(w);   // Mejoras 7.0
    m_goalStyle->addItem(QStringLiteral("Por pomodoros (ej. 3×25 de SO)"), QStringLiteral("pomodoros"));
    m_goalStyle->addItem(QStringLiteral("Por tiempo (ej. 3h30)"), QStringLiteral("time"));
    m_goalStyle->setCurrentIndex(m_db->setting("goal_style", "pomodoros").toString() == QLatin1String("time") ? 1 : 0);
    m_goalInput->setCurrentIndex(m_db->setting("goal_input", "buttons").toString() == QLatin1String("text") ? 1 : 0);
    // B2-1: qué pasa con el fantasma del objetivo cuando arrancás ese pomodoro.
    m_goalRunning = new QComboBox(w);
    m_goalRunning->addItem(QStringLiteral("Reemplaza al fantasma"));
    m_goalRunning->addItem(QStringLiteral("Se funde con el fantasma al terminar"));
    m_goalRunning->setCurrentIndex(qBound(0, m_db->setting("goal_running", 0).toInt(), 1));
    // Mejoras 2.0: con qué recreo se calcula "≈ X con recreos" en el objetivo.
    m_goalBreak = new QComboBox(w);
    m_goalBreak->addItem(QStringLiteral("Mi duración de recreo"));
    m_goalBreak->addItem(QStringLiteral("Mi promedio real (últimas 4 semanas)"));
    m_goalBreak->setCurrentIndex(qBound(0, m_db->setting("goal_break_source", 0).toInt(), 1));
    // Mejoras 4.1: anotador desplegable y ánimo al terminar la sesión.
    m_notepad = new QCheckBox(QStringLiteral("Anotador desplegable junto al objetivo"), w);
    m_notepad->setChecked(m_db->setting("notepad_on", 1).toBool());
    m_notepad->setToolTip(QStringLiteral("Apagado, el diario (ánimo y nota) queda dentro del panel del objetivo, como antes."));
    m_moodOffer = new QCheckBox(QStringLiteral("Preguntarme cómo me fue al terminar la sesión (Me voy)"), w);
    m_moodOffer->setChecked(m_db->setting("mood_offer", 1).toBool());
    m_startOffer = new QCheckBox(QStringLiteral("Proponerme el arranque de mañana (hora y tarea mínima) al terminar la sesión"), w);
    m_startOffer->setChecked(m_db->setting("startgoal_offer", 1).toBool());

    auto* times = new QFormLayout;
    times->addRow(QStringLiteral("Pomodoro por defecto"), m_work);
    times->addRow(QStringLiteral("Recreo por defecto"), m_break);
    times->addRow(QStringLiteral("El día empieza a las"), m_dayStart);
    times->addRow(QStringLiteral("Una pausa más larga corta la sesión"), m_sessionGap);
    times->addRow(QStringLiteral("Comodines por semana"), m_jokers);
    m_monthMode = new QComboBox(w);
    m_monthMode->addItem(QStringLiteral("Mes del calendario"), "calendar");
    m_monthMode->addItem(QStringLiteral("Últimos 30 días"), "rolling");
    m_monthMode->setCurrentIndex(m_db->setting("month_mode", "calendar").toString() == QLatin1String("rolling") ? 1 : 0);
    times->addRow(QStringLiteral("Mes en Estadísticas"), m_monthMode);
    auto* goalForm = new QFormLayout;
    goalForm->addRow(QStringLiteral("Estilo del objetivo"), m_goalStyle);
    goalForm->addRow(QStringLiteral("Cargar por pomodoros con"), m_goalInput);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("General"), w));
    lay->addWidget(subheading(QStringLiteral("Tiempos y racha"), w));
    lay->addLayout(times);
    lay->addWidget(note(QStringLiteral("Los comodines son días sin estudiar que no cortan la racha."), w));
    lay->addSpacing(8);
    lay->addWidget(subheading(QStringLiteral("Objetivo y diario"), w));
    lay->addLayout(goalForm);
    lay->addWidget(m_notepad);
    lay->addWidget(m_moodOffer);
    lay->addWidget(m_startOffer);
    lay->addSpacing(8);
    lay->addWidget(collapsible(QStringLiteral("Avanzado"), w, [&](QVBoxLayout* l) {
        auto* f = new QFormLayout;
        f->addRow(QStringLiteral("Formato de la fecha"), m_dateFormat);
        f->addRow(QStringLiteral("Pomodoro del objetivo"), m_goalRunning);
        f->addRow(QStringLiteral("Recreos del objetivo"), m_goalBreak);
        l->addLayout(f);
        l->addWidget(note(QStringLiteral("El cambio del inicio del día se aplica al reiniciar la app."), w));
    }));
    {
        auto* act = new QPushButton(QStringLiteral("Abrir actividades…"), w);
        act->setCursor(Qt::PointingHandCursor);
        connect(act, &QPushButton::clicked, this, [this] { m_openActivities = true; accept(); });
        lay->addSpacing(6);
        lay->addWidget(subheading(QStringLiteral("Actividades"), w));
        lay->addWidget(note(QStringLiteral("Crear, renombrar, ordenar, colorear y borrar actividades (también con la tecla A)."), w));
        lay->addWidget(act, 0, Qt::AlignLeft);
    }
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
    m_extraButtons->setChecked(m_db->setting("timer_buttons", 0).toBool());
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
    lay->addWidget(subheading(QStringLiteral("Ajustar la duración"), w));
    lay->addWidget(m_wheel);
    lay->addWidget(m_type);
    lay->addWidget(m_extraButtons);
    lay->addWidget(collapsible(QStringLiteral("Pasos y sensibilidad"), w, [&](QVBoxLayout* l) {
        l->addLayout(form);
        l->addWidget(note(QStringLiteral("Sin los botones de paso fino, el − / + va de a 5 min arriba de 10 y de a 1 por debajo."), w));
    }));

    // Mejoras 3.0: distracciones (Instagram, WhatsApp…) con un pomodoro corriendo
    lay->addSpacing(10);
    lay->addWidget(subheading(QStringLiteral("Distracciones"), w));
    m_guardOn = new QCheckBox(QStringLiteral("Avisarme si me voy a una distracción con un pomodoro corriendo"), w);
    m_guardOn->setChecked(m_db->setting("guard_on", 0).toBool());
    m_guardPause = new QCheckBox(QStringLiteral("Pausar el pomodoro (el rato cuenta como recreo indebido)"), w);
    m_guardPause->setChecked(m_db->setting("guard_pause", 1).toBool());
    m_guardGrace = spin(0, 300, m_db->setting("guard_grace", 20).toInt(), QStringLiteral(" s"), w);
    m_guardWords = new QLineEdit(m_db->setting("guard_words", FocusGuard::defaultWords().join(QStringLiteral(", "))).toString(), w);
    m_guardWords->setPlaceholderText(QStringLiteral("Instagram, WhatsApp, YouTube…"));
    auto* gform = new QFormLayout;
    gform->addRow(QStringLiteral("Tiempo de gracia"), m_guardGrace);
    gform->addRow(QStringLiteral("Qué cuenta como distracción"), m_guardWords);
    lay->addWidget(m_guardOn);
    lay->addWidget(m_guardPause);
    lay->addLayout(gform);
    lay->addWidget(note(QStringLiteral("Mira solo el programa y el título de la ventana al frente; todo queda en tu PC."), w));
    for (QWidget* x : {static_cast<QWidget*>(m_guardPause), static_cast<QWidget*>(m_guardGrace), static_cast<QWidget*>(m_guardWords)}) {
        x->setEnabled(m_guardOn->isChecked());
        connect(m_guardOn, &QCheckBox::toggled, x, &QWidget::setEnabled);
    }

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
    m_resizable = new QCheckBox(QStringLiteral("Cambiar el tamaño desde los bordes"), w);
    m_resizable->setChecked(m_db->setting("resizable", 1).toBool());
    m_visorStyle = new QComboBox(w);
    m_visorStyle->addItem(QStringLiteral("Del color del acento"));
    m_visorStyle->addItem(QStringLiteral("Oscuro siempre"));
    m_visorStyle->addItem(QStringLiteral("Sigue al tema (claro u oscuro)"));
    m_visorStyle->setCurrentIndex(m_db->setting("visor_style", 0).toInt());
    m_originalVisor = m_visorStyle->currentIndex();
    // M34: vista previa en vivo (si cancelás, vuelve al de antes).
    connect(m_visorStyle, &QComboBox::currentIndexChanged, this, &SettingsDialog::visorPreview);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Visor del cronómetro"), m_visorStyle);
    form->addRow(QStringLiteral("Tamaño de la ventana"), m_size);

    // M32: Apariencia = tema, visor y ventana. Lo de los cuadraditos y los festejos
    // tiene su propia sección.
    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Apariencia"), w));
    lay->addWidget(subheading(QStringLiteral("Tema de colores"), w));
    lay->addLayout(modeRow);
    lay->addLayout(grid);
    lay->addWidget(m_strong);
    lay->addSpacing(8);
    lay->addWidget(subheading(QStringLiteral("Cronómetro y ventana"), w));
    lay->addLayout(form);
    lay->addWidget(m_resizable);
    lay->addWidget(note(QStringLiteral("Cambiar el tamaño de la ventana vuelve todas las vistas a su tamaño original."), w));
    lay->addStretch();
    return w;
}

// M32: festejos y sonidos, juntos.
QWidget* SettingsDialog::pageCelebrations()
{
    auto* w = new QWidget(this);
    m_celebrations = new QCheckBox(QStringLiteral("Festejar pomodoros, objetivos e hitos"), w);
    m_celebrations->setChecked(m_db->setting("celebrations", 1).toBool());
    const int legacy = m_db->setting("celebration_style", 0).toInt();
    const char* keys[] = {"celebration_pomodoro", "celebration_goal", "celebration_milestone"};
    for (int i = 0; i < 3; ++i) {
        m_celebrationStyles[i] = new QComboBox(w);
        for (const QString& n : Celebration::styleNames()) m_celebrationStyles[i]->addItem(n);
        m_celebrationStyles[i]->setCurrentIndex(m_db->setting(keys[i], legacy).toInt());
        connect(m_celebrations, &QCheckBox::toggled, m_celebrationStyles[i], &QWidget::setEnabled);
        m_celebrationStyles[i]->setEnabled(m_celebrations->isChecked());
    }
    auto* cform = new QFormLayout;
    cform->addRow(QStringLiteral("Pomodoro terminado"), m_celebrationStyles[0]);
    cform->addRow(QStringLiteral("Objetivo cumplido"), m_celebrationStyles[1]);
    cform->addRow(QStringLiteral("Hito de horas"), m_celebrationStyles[2]);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Festejos y sonidos"), w));
    lay->addWidget(subheading(QStringLiteral("Festejos"), w));
    lay->addWidget(m_celebrations);
    lay->addLayout(cform);
    lay->addWidget(note(QStringLiteral("Visor tapa la ventana un par de segundos; Línea, Borde, Tilde y Cifra son "
                                       "minimalistas y no bloquean nada. En modo desarrollador se prueban con 🧪."), w));
    lay->addSpacing(8);
    // M27: sonidos, con botón para escucharlos
    lay->addWidget(subheading(QStringLiteral("Sonidos"), w));
    m_soundVolume = spin(0, 100, m_db->setting("sound_volume", 100).toInt(), QStringLiteral(" %"), w);
    m_soundVolume->setSingleStep(10);
    auto soundRow = [this, w](const QStringList& names, int current, int which) {
        auto* combo = new QComboBox(w);
        combo->addItems(names);
        combo->setCurrentIndex(current);
        auto* play = new QPushButton(QStringLiteral("▶"), w);
        play->setFixedWidth(36);
        play->setToolTip(QStringLiteral("Escuchar"));
        connect(play, &QPushButton::clicked, this, [this, combo, which] {
            emit soundPreview(which, combo->currentIndex(), m_soundVolume->value());
        });
        auto* row = new QHBoxLayout;
        row->addWidget(combo, 1);
        row->addWidget(play);
        return std::make_pair(combo, row);
    };
    auto [chime, chimeRow] = soundRow(Sounds::chimeNames(), m_db->setting("sound_chime", 0).toInt(), 0);
    auto [ach, achRow] = soundRow(Sounds::achievementNames(), m_db->setting("sound_achievement", 0).toInt(), 1);
    auto [alarm, alarmRow] = soundRow(Sounds::alarmNames(), m_db->setting("sound_alarm", 0).toInt(), 2);
    m_soundChime = chime;
    m_soundAchievement = ach;
    m_soundAlarm = alarm;
    auto* sform = new QFormLayout;
    sform->addRow(QStringLiteral("Fin de pomodoro"), chimeRow);
    sform->addRow(QStringLiteral("Logro (objetivo, hito)"), achRow);
    sform->addRow(QStringLiteral("Alarma del recreo"), alarmRow);
    sform->addRow(QStringLiteral("Volumen"), m_soundVolume);
    lay->addLayout(sform);
    // Mejoras 2.0: al probar un sonido dice si salió y por qué salida.
    m_soundResult = new QLabel(QStringLiteral("Tocá ▶ para probar: acá aparece por qué salida suena."), w);
    m_soundResult->setObjectName("Hint");
    m_soundResult->setWordWrap(true);
    lay->addWidget(m_soundResult);
    m_soundToast = new QCheckBox(QStringLiteral("Avisar también con una notificación de Windows al terminar el pomodoro o el recreo"), w);
    m_soundToast->setChecked(m_db->setting("notify_toast", 1).toBool());
    m_soundToast->setToolTip(QStringLiteral("Tiene su propio sonido: sirve de respaldo si el de la app no se escucha."));
    lay->addWidget(m_soundToast);
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
    m_tileNumberPos = combo({QStringLiteral("Adentro, arriba"), QStringLiteral("Afuera, arriba"), QStringLiteral("Sin número")}, st.numberPos);
    m_tileHighlight = combo({QStringLiteral("Nada"), QStringLiteral("El que está corriendo (color del pomodoro)"),
                             QStringLiteral("El último del día (rojo, como antes)")}, st.highlight);
    m_tilePrefix = new QCheckBox(QStringLiteral("Número con \"Nº\" (Nº3)"), w);
    m_tilePrefix->setChecked(st.numberPrefix);
    m_tileBorder = combo(tiles::borderNames(), st.border);
    m_tileInverted = new QCheckBox(QStringLiteral("Invertidos: fondo claro que se llena oscuro"), w);
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
    // Tamaños (M16) y agrupado / colores (M24, M25): viven acá con el resto de los cuadraditos.
    const int mask = m_db->setting("size_modes", 7).toInt();
    const char* sizeNames[] = {"Todos iguales", "Proporcional solo a lo ancho", "Proporcional completo"};
    auto* sizeRow = new QHBoxLayout;
    for (int i = 0; i < 3; ++i) {
        m_sizeModes[i] = new QCheckBox(QString::fromUtf8(sizeNames[i]), w);
        m_sizeModes[i]->setChecked(mask & (1 << i));
        sizeRow->addWidget(m_sizeModes[i]);
    }
    sizeRow->addStretch();
    m_sizeButton = new QCheckBox(QStringLiteral("Mostrar el botón de la barra para alternar"), w);
    m_sizeButton->setChecked(m_db->setting("size_button", 1).toBool());
    m_sizeDefault = combo({QString::fromUtf8(sizeNames[0]), QString::fromUtf8(sizeNames[1]), QString::fromUtf8(sizeNames[2])},
                          m_db->setting("size_default", 0).toInt());
    m_groupStyle = combo({QStringLiteral("Todos juntos, sin agrupar"), QStringLiteral("Por actividad, con títulos"),
                          QStringLiteral("Por actividad, en línea y recuadrados")},
                         m_db->setting("group_style", m_db->setting("group_by_activity", 1).toBool() ? 1 : 0).toInt());
    m_originalGroup = m_groupStyle->currentIndex();
    connect(m_groupStyle, &QComboBox::currentIndexChanged, this, &SettingsDialog::groupPreview);   // en vivo
    m_activityColors = new QCheckBox(QStringLiteral("Colores por actividad"), w);
    m_activityColors->setChecked(m_db->setting("activity_colors", 1).toBool());

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
        cur.numberPos = m_tileNumberPos->currentIndex();
        cur.numberPrefix = m_tilePrefix->isChecked();
        cur.highlight = m_tileHighlight->currentIndex();
        emit tileSizePreview(m_tileSize->value());
        preview->update();
        if (parentWidget()) parentWidget()->update();
        for (QWidget* x : QApplication::allWidgets()) x->update();
    };
    for (QComboBox* c : {m_tileShape, m_tileMark, m_tileFill, m_tileComplete, m_tilePalette, m_tileBorder,
                         m_tileNumberPos, m_tileHighlight})
        connect(c, &QComboBox::currentIndexChanged, this, live);
    for (QCheckBox* c : {m_tileInverted, m_tileStripe, m_tileCaption, m_tilePrefix})
        connect(c, &QCheckBox::toggled, this, live);
    connect(m_tileSize, &QSpinBox::valueChanged, this, live);

    auto* form = new QFormLayout;
    form->addRow(QStringLiteral("Forma"), m_tileShape);
    form->addRow(QStringLiteral("Colores"), m_tilePalette);
    form->addRow(QStringLiteral("Borde"), m_tileBorder);
    form->addRow(QStringLiteral("Marca de hecho"), m_tileMark);
    form->addRow(QStringLiteral("Llenado (el que corre)"), m_tileFill);
    form->addRow(QStringLiteral("Al completarse"), m_tileComplete);
    form->addRow(QStringLiteral("Resaltar"), m_tileHighlight);
    auto* nform = new QFormLayout;
    nform->addRow(QStringLiteral("Número"), m_tileNumberPos);
    auto* zform = new QFormLayout;
    zform->addRow(QStringLiteral("Tamaño base"), m_tileSize);
    m_sizeDefault->hide();   // Mejoras 5.0: el tamaño se elige desde ⋯ y se recuerda
    auto* gform = new QFormLayout;
    gform->addRow(QStringLiteral("Agrupar"), m_groupStyle);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Cuadraditos"), w));
    lay->addWidget(preview);
    lay->addWidget(subheading(QStringLiteral("Estilo"), w));
    lay->addLayout(form);
    lay->addWidget(m_tileInverted);
    lay->addWidget(m_tileStripe);
    lay->addSpacing(6);
    lay->addWidget(subheading(QStringLiteral("Número y actividad"), w));
    lay->addLayout(nform);
    lay->addWidget(m_tilePrefix);
    lay->addWidget(m_tileCaption);
    lay->addSpacing(6);
    lay->addWidget(collapsible(QStringLiteral("Tamaño"), w, [&](QVBoxLayout* l) {
        l->addLayout(zform);
        l->addWidget(note(QStringLiteral("Los tamaños que aparecen en ⋯ › Tamaño de los cuadraditos:"), w));
        l->addLayout(sizeRow);
        m_sizeButton->hide();
    }));
    lay->addSpacing(6);
    lay->addWidget(subheading(QStringLiteral("Agrupado y colores"), w));
    lay->addLayout(gform);
    lay->addWidget(m_activityColors);
    lay->addWidget(note(QStringLiteral("El color de cada actividad se elige en Actividades → click derecho → Color."), w));
    lay->addStretch();
    return w;
}

// M44: hitos (se prenden, se apagan y se ajustan solo desde acá).
namespace {
const char* kMsKeys[6] = {"ms_day", "ms_streaks", "ms_weekly", "ms_hours", "ms_stages", "ms_month"};
}

QWidget* SettingsDialog::pageMilestones()
{
    auto* w = new QWidget(this);
    const char* labels[6] = {"Marca del día (bronce, plata, oro)", "Rachas y récords", "Meta semanal",
                             "Horas por actividad (1, 5, 10, 25, 50, 100, 150… h)",
                             "Etapas hacia las 10.000 h (por actividad y nivel global)", "Resumen del mes"};
    for (int i = 0; i < 6; ++i) {
        m_msToggle[i] = new QCheckBox(QString::fromUtf8(labels[i]), w);
        m_msToggle[i]->setChecked(m_db->setting(kMsKeys[i], 1).toBool());
    }
    const char* markKeys[3] = {"ms_bronze_min", "ms_silver_min", "ms_gold_min"};
    const int markDef[3] = {120, 240, 360};
    for (int i = 0; i < 3; ++i) {
        m_markH[i] = new QDoubleSpinBox(w);
        m_markH[i]->setRange(0.5, 16);
        m_markH[i]->setSingleStep(0.5);
        m_markH[i]->setDecimals(1);
        m_markH[i]->setSuffix(QStringLiteral(" h"));
        m_markH[i]->setValue(m_db->setting(markKeys[i], markDef[i]).toInt() / 60.0);
        connect(m_msToggle[0], &QCheckBox::toggled, m_markH[i], &QWidget::setEnabled);
        m_markH[i]->setEnabled(m_msToggle[0]->isChecked());
    }
    m_weeklyH = spin(1, 100, m_db->setting("ms_weekly_min", 20 * 60).toInt() / 60, QStringLiteral(" h por semana"), w);
    m_dailyPomos = spin(0, 50, m_db->setting("ms_daily_pomos", 0).toInt(), QStringLiteral(" pomodoros por día"), w);   // Mejoras 7.0
    m_dailyPomos->setSpecialValueText(QStringLiteral("sin mínimo"));
    connect(m_msToggle[2], &QCheckBox::toggled, m_weeklyH, &QWidget::setEnabled);
    m_weeklyH->setEnabled(m_msToggle[2]->isChecked());

    auto* mform = new QFormLayout;
    mform->addRow(QStringLiteral("Bronce"), m_markH[0]);
    mform->addRow(QStringLiteral("Plata"), m_markH[1]);
    mform->addRow(QStringLiteral("Oro"), m_markH[2]);
    auto* wform = new QFormLayout;
    wform->addRow(QStringLiteral("Meta"), m_weeklyH);
    wform->addRow(QStringLiteral("Mínimo diario"), m_dailyPomos);

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Hitos"), w));
    lay->addWidget(subheading(QStringLiteral("Corto plazo"), w));
    lay->addWidget(m_msToggle[0]);
    lay->addLayout(mform);
    lay->addWidget(m_msToggle[1]);
    lay->addWidget(m_msToggle[2]);
    lay->addLayout(wform);
    lay->addSpacing(6);
    lay->addWidget(subheading(QStringLiteral("Mediano y largo plazo"), w));
    lay->addWidget(m_msToggle[3]);
    lay->addWidget(m_msToggle[4]);
    lay->addWidget(note(QStringLiteral("De Novato a Maestro pasando por 8 etapas, con tres subniveles cada una."), w));
    lay->addWidget(m_msToggle[5]);
    lay->addStretch();
    return w;
}

void SettingsDialog::showPage(int page)
{
    m_nav->setCurrentRow(qBound(0, page, m_nav->count() - 1));
}

// Mejoras 5.1: rutina. Los cambios se guardan al instante (no esperan a "Guardar").
QWidget* SettingsDialog::pageRoutine()
{
    auto* w = new QWidget(this);
    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Rutina"), w));
    lay->addWidget(note(QStringLiteral("Lo que hacés por fuera de la app: facultad, trabajo, gimnasio, médico. No avisa nada: sirve para explicar las "
                                       "horas inactivas («9 h inactivo: 4 h facultad, 1 h gimnasio, 4 h sin motivo») en Estadísticas. "
                                       "Los cambios se guardan al instante."), w));
    m_routineList = new QListWidget(w);
    m_routineList->setMinimumHeight(220);
    connect(m_routineList, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* it) { editRoutine(it->data(Qt::UserRole).toLongLong()); });
    lay->addWidget(m_routineList);
    auto* row = new QHBoxLayout;
    auto* add = new QPushButton(QStringLiteral("Agregar…"), w);
    auto* edit = new QPushButton(QStringLiteral("Editar…"), w);
    auto* del = new QPushButton(QStringLiteral("Borrar"), w);
    for (QPushButton* b : {add, edit, del}) { b->setAutoDefault(false); row->addWidget(b); }
    row->addStretch();
    lay->addLayout(row);
    connect(add, &QPushButton::clicked, this, [this] { editRoutine(0); });
    connect(edit, &QPushButton::clicked, this, [this] {
        if (auto* it = m_routineList->currentItem()) editRoutine(it->data(Qt::UserRole).toLongLong());
    });
    connect(del, &QPushButton::clicked, this, [this] {
        if (auto* it = m_routineList->currentItem()) { routine::remove(*m_db, it->data(Qt::UserRole).toLongLong()); reloadRoutine(); }
    });
    lay->addStretch();
    reloadRoutine();
    return w;
}

void SettingsDialog::reloadRoutine()
{
    m_routineList->clear();
    auto evs = routine::events(*m_db);
    for (const routine::Event& e : evs) {
        const QString when = e.weekdays ? routine::weekdaysText(e.weekdays) : e.day.toString(QStringLiteral("dd/MM/yy"));
        const QString time = QStringLiteral("%1–%2").arg(QTime(0, 0).addSecs(e.startMin * 60).toString(QStringLiteral("HH:mm")),
                                                         QTime(0, 0).addSecs(e.endMin * 60).toString(QStringLiteral("HH:mm")));
        auto* it = new QListWidgetItem(QStringLiteral("%1 · %2 · %3  [%4]").arg(e.title.isEmpty() ? e.category : e.title, when, time, e.category));
        it->setData(Qt::UserRole, e.id);
        m_routineList->addItem(it);
    }
    if (evs.isEmpty()) m_routineList->addItem(QStringLiteral("Todavía no cargaste nada. Probá con «Agregar…» (ej. Facultad, lunes a viernes, 8:00–12:00)."));
}

void SettingsDialog::editRoutine(qint64 id)
{
    if (routinedlg::edit(m_db, id, QDate::currentDate(), this)) reloadRoutine();
}

QWidget* SettingsDialog::pageSystem()
{
    auto* w = new QWidget(this);
    m_autostart = new QCheckBox(QStringLiteral("Iniciar con Windows"), w);
    m_autostart->setChecked(m_db->setting("autostart", 0).toBool());
    m_greeting = new QCheckBox(QStringLiteral("Saludarme la primera vez que uso la PC en el día"), w);
    m_greeting->setChecked(m_db->setting("greeting", 0).toBool());
    m_greeting->setToolTip(QStringLiteral("Al prender la PC, despertarla o desbloquearla."));
    m_closeToTray = new QCheckBox(QStringLiteral("Al cerrar la ventana, seguir abierta en segundo plano (⋯ › Salir la cierra del todo)"), w);
    m_closeToTray->setChecked(m_db->setting("close_to_tray", 1).toBool());
    m_appTrack = new QCheckBox(QStringLiteral("Anotar qué programas uso (solo con la app abierta, aunque esté en segundo plano)"), w);
    m_appTrack->setChecked(m_db->setting("apptrack_on", 0).toBool());
    auto* appNote = new QLabel(QStringLiteral("Cada 5 segundos anota el programa y el título de la ventana al frente, y no cuenta si pasan 3 minutos sin tocar teclado ni mouse. Gasta casi nada de procesador. Todo queda solo en esta PC (no se sincroniza ni se exporta). Se ve en ⋯ › Uso de apps."), w);
    appNote->setWordWrap(true);
    appNote->setStyleSheet(QStringLiteral("color:%1").arg(theme::textDim.name()));
    auto* wipe = new QPushButton(QStringLiteral("Borrar los datos de uso de apps…"), w);
    wipe->setAutoDefault(false);
    connect(wipe, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, QStringLiteral("Borrar uso de apps"), QStringLiteral("¿Borrar todo el historial de uso de programas? No toca tus pomodoros.")) == QMessageBox::Yes) {
            appusage::clear(*m_db);
        }
    });
    m_hotkey = new QCheckBox(QStringLiteral("Atajo global Ctrl + Alt + P (pausar / seguir)"), w);
    m_hotkey->setChecked(m_db->setting("hotkey", 1).toBool());
    m_quietFullscreen = new QCheckBox(QStringLiteral("No saltar al frente en pantalla completa (solo aviso)"), w);
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

    m_devMode = new QCheckBox(QStringLiteral("Modo desarrollador (pruebas sobre una copia de tus datos)"), w);
    m_devMode->setChecked(devmode::enabledFlag(qApp->property("dataDir").toString()));

    auto* lay = new QVBoxLayout(w);
    lay->addWidget(heading(QStringLiteral("Sistema"), w));
    lay->addWidget(subheading(QStringLiteral("Inicio"), w));
    lay->addWidget(m_autostart);
    lay->addWidget(m_greeting);
    lay->addWidget(m_closeToTray);
    lay->addSpacing(6);
    lay->addWidget(subheading(QStringLiteral("Uso de apps"), w));
    lay->addWidget(m_appTrack);
    lay->addWidget(appNote);
    lay->addWidget(wipe, 0, Qt::AlignLeft);
    lay->addSpacing(6);
    lay->addWidget(subheading(QStringLiteral("Atajos y avisos"), w));
    lay->addWidget(m_hotkey);
    lay->addWidget(m_quietFullscreen);
    lay->addSpacing(6);
    lay->addWidget(subheading(QStringLiteral("Copia de seguridad"), w));
    lay->addWidget(m_backup);
    lay->addLayout(dirRow);
    lay->addWidget(note(QStringLiteral("Con una carpeta de OneDrive o Drive, las copias también te salvan si se rompe el disco."), w));
    lay->addSpacing(6);
    lay->addWidget(collapsible(QStringLiteral("Avanzado"), w, [&](QVBoxLayout* l) {
        l->addWidget(m_devMode);
        l->addWidget(note(QStringLiteral("Aparece el botón 🧪 en la barra. Al guardar, la app se reinicia sola."), w));
    }));
    lay->addStretch();
    return w;
}

// ---------------------------------------------------------------------------
void SettingsDialog::accept()
{
    m_db->setSetting("work_min", m_work->value());
    m_db->setSetting("break_min", m_break->value());
    stats::setJokers(*m_db, m_jokers->value(), dayclock::today());   // Mejoras 3.0: desde hoy, no hacia atrás
    m_db->setSetting("date_format", m_dateFormat->currentIndex() == 0 ? "numeric" : "long");
    m_db->setSetting("day_start_hour", m_dayStart->value());
    m_db->setSetting("session_gap_min", m_sessionGap->value());
    m_db->setSetting("goal_input", m_goalInput->currentData().toString());
    m_db->setSetting("goal_style", m_goalStyle->currentData().toString());
    m_db->setSetting("notepad_on", m_notepad->isChecked() ? 1 : 0);
    m_db->setSetting("month_mode", m_monthMode->currentData().toString());
    m_db->setSetting("mood_offer", m_moodOffer->isChecked() ? 1 : 0);
    m_db->setSetting("startgoal_offer", m_startOffer->isChecked() ? 1 : 0);
    m_db->setSetting("goal_running", m_goalRunning->currentIndex());
    m_db->setSetting("goal_break_source", m_goalBreak->currentIndex());

    m_db->setSetting("timer_wheel", m_wheel->isChecked() ? 1 : 0);
    m_db->setSetting("timer_wheel_step", m_wheelStep->value());
    m_db->setSetting("timer_type", m_type->isChecked() ? 1 : 0);
    m_db->setSetting("timer_buttons", m_extraButtons->isChecked() ? 1 : 0);
    m_db->setSetting("timer_step_small", m_stepSmall->value());
    m_db->setSetting("timer_step_big", m_stepBig->value());
    m_db->setSetting("sound_chime", m_soundChime->currentIndex());
    m_db->setSetting("sound_achievement", m_soundAchievement->currentIndex());
    m_db->setSetting("sound_alarm", m_soundAlarm->currentIndex());
    m_db->setSetting("sound_volume", m_soundVolume->value());
    if (m_soundToast) m_db->setSetting("notify_toast", m_soundToast->isChecked() ? 1 : 0);
    m_db->setSetting("guard_on", m_guardOn->isChecked() ? 1 : 0);
    m_db->setSetting("guard_pause", m_guardPause->isChecked() ? 1 : 0);
    m_db->setSetting("guard_grace", m_guardGrace->value());
    m_db->setSetting("guard_words", m_guardWords->text());

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
    for (int i = 0; i < 6; ++i) m_db->setSetting(kMsKeys[i], m_msToggle[i]->isChecked() ? 1 : 0);   // M44
    m_db->setSetting("ms_bronze_min", int(std::lround(m_markH[0]->value() * 60)));
    m_db->setSetting("ms_silver_min", int(std::lround(qMax(m_markH[0]->value(), m_markH[1]->value()) * 60)));
    m_db->setSetting("ms_gold_min", int(std::lround(qMax(m_markH[1]->value(), m_markH[2]->value()) * 60)));
    m_db->setSetting("ms_weekly_min", m_weeklyH->value() * 60);
    m_db->setSetting("ms_daily_pomos", m_dailyPomos->value());
    m_db->setSetting("activity_colors", m_activityColors->isChecked() ? 1 : 0);
    {
        const char* keys[] = {"celebration_pomodoro", "celebration_goal", "celebration_milestone"};
        for (int i = 0; i < 3; ++i) {
            m_db->setSetting(keys[i], m_celebrationStyles[i]->currentIndex());
            Celebration::setStyleFor(Celebration::Kind(i), Celebration::Style(m_celebrationStyles[i]->currentIndex()));
        }
    }
    m_db->setSetting("visor_style", m_visorStyle->currentIndex());

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
    m_db->setSetting("tile_number_pos", m_tileNumberPos->currentIndex());
    m_db->setSetting("tile_number_prefix", m_tilePrefix->isChecked() ? 1 : 0);
    m_db->setSetting("tile_highlight", m_tileHighlight->currentIndex());
    m_db->setSetting("autostart", m_autostart->isChecked() ? 1 : 0);
    m_db->setSetting("apptrack_on", m_appTrack->isChecked() ? 1 : 0);
    m_db->setSetting("close_to_tray", m_closeToTray->isChecked() ? 1 : 0);
    m_db->setSetting("greeting", m_greeting->isChecked() ? 1 : 0);
    m_db->setSetting("hotkey", m_hotkey->isChecked() ? 1 : 0);
    m_db->setSetting("quiet_fullscreen", m_quietFullscreen->isChecked() ? 1 : 0);
    m_db->setSetting("backup", m_backup->isChecked() ? 1 : 0);
    m_db->setSetting("backup_dir", m_backupDir->text().trimmed());
    // M20: el interruptor va en un archivo (no en la base) y pide reiniciar.
    const QString dataDir = qApp->property("dataDir").toString();
    if (m_devMode->isChecked() != devmode::enabledFlag(dataDir)) {
        devmode::setEnabledFlag(dataDir, m_devMode->isChecked());
        m_restartRequested = true;   // M28: se reinicia de una
    }
    QDialog::accept();
}

// M28: Enter no cierra Ajustes (se usa para confirmar un número en un campo).
void SettingsDialog::keyPressEvent(QKeyEvent* e)
{
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) {
        if (QWidget* f = focusWidget()) f->clearFocus();
        return;
    }
    QDialog::keyPressEvent(e);
}

void SettingsDialog::reject()
{
    theme::setStrongAccents(m_originalStrong);
    theme::apply(m_originalTheme);   // deshace la vista previa
    tiles::style() = m_originalTiles;
    emit visorPreview(m_originalVisor);
    emit groupPreview(m_originalGroup);
    emit tileSizePreview(m_originalTileSize);
    QDialog::reject();
}

// M38: los valores por defecto son los mismos que usa la app cuando no hay nada
// guardado (o sea, los tuyos de M33). El modo de pruebas no se toca.
void SettingsDialog::resetSection(int page)
{
    auto pick = [](QComboBox* c, int i) { c->setCurrentIndex(i); };
    switch (page) {
    case 0:   // General
        m_work->setValue(25);
        m_break->setValue(10);
        m_jokers->setValue(2);
        pick(m_dateFormat, 0);
        m_dayStart->setValue(4);
        m_sessionGap->setValue(60);
        pick(m_goalInput, 0);
        pick(m_goalRunning, 0);
        m_notepad->setChecked(true);
        m_monthMode->setCurrentIndex(0);
        m_moodOffer->setChecked(true);
        m_startOffer->setChecked(true);
        break;
    case 1:   // Cronómetro
        m_wheel->setChecked(true);
        m_wheelStep->setValue(1);
        m_type->setChecked(true);
        m_extraButtons->setChecked(false);
        m_stepSmall->setValue(1);
        m_stepBig->setValue(5);
        break;
    case 2: { // Apariencia: gris oscuro, sin acentos fuertes
        m_strong->setChecked(false);
        for (QAbstractButton* b : m_modes->buttons())
            if (b->property("mode") == "dark") b->click();
        for (QAbstractButton* b : m_themes->buttons())
            if (b->property("color") == "gray") b->click();
        pick(m_visorStyle, 0);
        pick(m_size, qMax(0, m_size->findData("medium")));
        m_resizable->setChecked(true);
        break;
    }
    case 3:   // Cuadraditos
        pick(m_tileShape, 0);
        pick(m_tilePalette, 0);
        pick(m_tileBorder, 0);
        pick(m_tileMark, 2);
        pick(m_tileFill, 0);
        pick(m_tileComplete, 2);
        pick(m_tileHighlight, 1);
        pick(m_tileNumberPos, 1);
        m_tilePrefix->setChecked(true);
        m_tileInverted->setChecked(true);
        m_tileStripe->setChecked(false);
        m_tileCaption->setChecked(false);
        m_tileSize->setValue(100);
        for (QCheckBox* c : m_sizeModes) c->setChecked(true);
        m_sizeButton->setChecked(true);
        pick(m_sizeDefault, 0);
        pick(m_groupStyle, 1);
        m_activityColors->setChecked(true);
        break;
    case 4:   // Festejos y sonidos
        m_celebrations->setChecked(true);
        for (QComboBox* c : m_celebrationStyles) pick(c, 0);
        pick(m_soundChime, 0);
        pick(m_soundAchievement, 0);
        pick(m_soundAlarm, 0);
        m_soundVolume->setValue(100);
        break;
    case 6:   // Sistema
        m_autostart->setChecked(false);
        m_appTrack->setChecked(false);
        m_closeToTray->setChecked(true);
        m_greeting->setChecked(false);
        m_hotkey->setChecked(true);
        m_quietFullscreen->setChecked(true);
        m_backup->setChecked(true);
        m_backupDir->clear();
        break;
    case 5:   // Hitos
        for (QCheckBox* c : m_msToggle) c->setChecked(true);
        m_markH[0]->setValue(2);
        m_markH[1]->setValue(4);
        m_markH[2]->setValue(6);
        m_weeklyH->setValue(20);
        break;
    default: break;
    }
}

void SettingsDialog::showSoundResult(bool ok, const QString& detail)
{
    if (!m_soundResult) return;
    m_soundResult->setText((ok ? QStringLiteral("✓ ") : QStringLiteral("✗ ")) + detail);
}
