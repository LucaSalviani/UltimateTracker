#include "ui/GoalPanel.h"
#include "core/Format.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSignalBlocker>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSpinBox>
#include <QWheelEvent>
#include <QStackedWidget>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>
#include <functional>

namespace {
const char* kDayLetters[] = {"L", "M", "X", "J", "V", "S", "D"};

QLabel* caption(const QString& t, QWidget* parent)
{
    auto* l = new QLabel(t, parent);
    l->setProperty("role", "caption");
    return l;
}
} // namespace

GoalPanel::GoalPanel(QWidget* parent)
    : QFrame(parent)
{
    setObjectName("GoalPanel");
    setAttribute(Qt::WA_StyledBackground);

    // ---- Cabecera
    auto* title = new QLabel(QStringLiteral("Objetivo del día"), this);
    title->setProperty("heading", true);
    m_date = new QLabel(this);
    m_date->setProperty("dim", true);
    auto* close = new QToolButton(this);
    close->setObjectName("GoalClose");
    close->setIcon(icons::get(icons::Id::Close));
    close->setToolTip(QStringLiteral("Cerrar (Esc)"));
    close->setCursor(Qt::PointingHandCursor);
    connect(close, &QToolButton::clicked, this, &GoalPanel::closeRequested);
    auto* head = new QHBoxLayout;
    auto* titles = new QVBoxLayout;
    titles->setSpacing(0);
    titles->addWidget(title);
    titles->addWidget(m_date);
    head->addLayout(titles, 1);
    head->addWidget(close, 0, Qt::AlignTop);

    // ---- Tarea del día (el mismo texto que la banda de arriba)
    m_dayTitle = new QLineEdit(this);
    m_dayTitle->setObjectName("GoalField");
    m_dayTitle->setPlaceholderText(QStringLiteral("¿Qué querés lograr hoy?"));
    connect(m_dayTitle, &QLineEdit::editingFinished, this, [this] {
        emit dayTitleEdited(m_day, m_dayTitle->text().trimmed());
    });

    // ---- Modo: por tiempo / por pomodoros
    auto* byTime = new QPushButton(QStringLiteral("Por tiempo"), this);
    auto* byPomos = new QPushButton(QStringLiteral("Por pomodoros"), this);
    m_mode = new QButtonGroup(this);
    int idx = 0;
    for (QPushButton* b : {byTime, byPomos}) {
        b->setObjectName("GoalSeg");
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_mode->addButton(b, idx++);
    }
    byTime->setChecked(true);
    auto* seg = new QHBoxLayout;
    seg->setSpacing(0);
    seg->addWidget(byTime);
    seg->addWidget(byPomos);

    m_modePages = new QStackedWidget(this);
    // Por tiempo
    auto* timePage = new QWidget(m_modePages);
    // Mejoras 2.0: el número es el campo: click y escribís ("3h30", "3:30", "210").
    // Con el foco puesto, la rueda suma o resta de a 15 minutos.
    m_time = new QLineEdit(timePage);
    m_time->setObjectName("GoalField");
    m_time->setPlaceholderText(QStringLiteral("Ej.: 3h30, 3:30 o 210"));
    m_time->installEventFilter(this);
    m_timeHint = new QLabel(QStringLiteral("Ej.: 3h30, 3:30, 4 (horas) o 210 (min)"), timePage);
    m_timeHint->setProperty("dim", true);
    auto* tl = new QVBoxLayout(timePage);
    tl->setContentsMargins(0, 0, 0, 0);
    tl->setSpacing(4);
    tl->addWidget(m_time);
    tl->addWidget(m_timeHint);
    // Por pomodoros
    auto* comboPage = new QWidget(m_modePages);
    m_combo = new QLineEdit(comboPage);
    m_combo->setObjectName("GoalField");
    m_combo->setPlaceholderText(QStringLiteral("3x30 + 2x25 + 2x60"));
    m_comboHint = new QLabel(QStringLiteral("Cantidad x minutos, separados con +"), comboPage);
    m_comboHint->setProperty("dim", true);
    auto* cl = new QVBoxLayout(comboPage);
    cl->setContentsMargins(0, 0, 0, 0);
    cl->setSpacing(4);
    m_rows = new ComboRows(comboPage);
    connect(m_rows, &ComboRows::changed, this, [this] {
        // Las filas escriben en el mismo spec de texto (así el resto no cambia).
        m_combo->setText(m_rows->spec());
        // Con más filas la página crece (antes quedaba con la altura vieja y se encimaba con lo de abajo).
        if (m_mode->checkedId() == 1) QTimer::singleShot(0, this, [this] { showModePage(1); });
        if (m_mode->checkedId() == 1) showModePage(1);
    });
    cl->addWidget(m_rows);
    cl->addWidget(m_combo);
    cl->addWidget(m_comboHint);
    m_modePages->addWidget(timePage);
    m_modePages->addWidget(comboPage);
    connect(m_mode, &QButtonGroup::idClicked, this, [this](int id) {
        showModePage(id);
        // Al pasar de tiempo a pomodoros, se propone el equivalente.
        if (id == 1 && m_combo->text().trimmed().isEmpty()) {
            const auto p = goals::parse(QStringLiteral("t:%1").arg(qMax(0, goals::parseDuration(m_time->text()))), m_pomoMin);
            if (p.ok) m_combo->setText(QStringLiteral("%1x%2").arg(p.blocks.size()).arg(m_pomoMin));
            m_rows->setSpec(goals::normalizeCombo(m_combo->text()), m_pomoMin);
        }
        refreshPreview();
    });
    connect(m_time, &QLineEdit::textChanged, this, &GoalPanel::refreshPreview);
    connect(m_time, &QLineEdit::editingFinished, this, [this] {   // se prolija: "330" → "5h30"
        const int m = goals::parseDuration(m_time->text());
        if (m > 0) { m_time->blockSignals(true); m_time->setText(goals::formatDuration(m)); m_time->blockSignals(false); }
    });
    connect(m_combo, &QLineEdit::textChanged, this, &GoalPanel::refreshPreview);

    m_preview = new GoalPreview(this);
    m_summary = new QLabel(this);
    m_summary->setProperty("dim", true);
    m_summary->setWordWrap(true);

    // ---- Repetir (como en Google Calendar)
    m_repeat = new QComboBox(this);
    m_repeat->addItem(QStringLiteral("Solo este día"), 0);
    m_repeat->addItem(QStringLiteral("Todos los días"), goals::kEveryDay);
    m_repeat->addItem(QStringLiteral("De lunes a viernes"), goals::kWeekdays);
    m_repeat->addItem(QStringLiteral("Días elegidos…"), -1);
    m_daysRow = new QWidget(this);
    m_days = new QButtonGroup(this);
    m_days->setExclusive(false);
    auto* dl = new QHBoxLayout(m_daysRow);
    dl->setContentsMargins(0, 0, 0, 0);
    dl->setSpacing(4);
    for (int i = 0; i < 7; ++i) {
        auto* b = new QToolButton(m_daysRow);
        b->setObjectName("GoalDay");
        b->setText(QString::fromLatin1(kDayLetters[i]));
        b->setCheckable(true);
        b->setCursor(Qt::PointingHandCursor);
        m_days->addButton(b, i);
        dl->addWidget(b);
    }
    dl->addStretch();
    m_daysRow->hide();
    connect(m_repeat, &QComboBox::currentIndexChanged, this, [this] {
        m_daysRow->setVisible(m_repeat->currentData().toInt() == -1);
    });

    // ---- Estado y botones
    m_status = new QLabel(this);
    m_status->setObjectName("GoalStatus");
    m_status->setWordWrap(true);

    m_save = new QPushButton(QStringLiteral("Guardar objetivo"), this);
    m_save->setObjectName("GoalSave");
    m_save->setCursor(Qt::PointingHandCursor);
    connect(m_save, &QPushButton::clicked, this, [this] {
        const QString spec = currentSpec();
        if (spec.isEmpty()) {
            m_summary->setText(QStringLiteral("Poné un tiempo o una combinación de pomodoros."));
            return;
        }
        emit saveRequested(m_day, m_dayTitle->text().trimmed(), spec, currentWeekdays());
    });
    m_remove = new QPushButton(QStringLiteral("Quitar"), this);
    m_remove->setObjectName("GoalRemove");
    m_remove->setCursor(Qt::PointingHandCursor);
    connect(m_remove, &QPushButton::clicked, this, [this] {
        if (!m_recurring) { emit removeDayRequested(m_day); return; }
        QMenu menu(this);
        menu.addAction(QStringLiteral("Solo este día"), this, [this] { emit removeDayRequested(m_day); });
        menu.addAction(QStringLiteral("Desde este día en adelante"), this, [this] { emit removeFromDayRequested(m_day); });
        menu.exec(m_remove->mapToGlobal(QPoint(0, m_remove->height())));
    });
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(m_remove);
    buttons->addStretch();
    buttons->addWidget(m_save);

    auto* hint = new QLabel(QStringLiteral("Tip: desde la semana o el mes, click derecho en un día para planear su objetivo."), this);
    hint->setProperty("dim", true);
    hint->setWordWrap(true);

    // Todo va en un área con scroll: en la ventana chica no entra entero.
    auto* scroll = new QScrollArea(this);
    scroll->setObjectName("GoalScroll");
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* content = new QWidget(scroll);
    content->setObjectName("GoalContent");
    scroll->setWidget(content);
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);

    auto* lay = new QVBoxLayout(content);
    lay->setContentsMargins(16, 12, 12, 12);
    lay->setSpacing(6);
    lay->addLayout(head);
    lay->addSpacing(4);
    lay->addWidget(m_dayTitle);
    lay->addSpacing(6);
    showModePage(0);
    lay->addWidget(caption(QStringLiteral("CUÁNTO"), this));
    lay->addLayout(seg);
    lay->addWidget(m_modePages);
    lay->addWidget(m_preview);
    lay->addWidget(m_summary);
    lay->addSpacing(4);
    lay->addWidget(caption(QStringLiteral("REPETIR"), this));
    lay->addWidget(m_repeat);
    lay->addWidget(m_daysRow);
    lay->addSpacing(4);
    lay->addWidget(m_status);
    lay->addStretch();
    lay->addLayout(buttons);
    lay->addWidget(hint);
}

void GoalPanel::load(const QDate& day, const std::optional<GoalRule>& rule, const QString& dayTitle,
                     int pomodoroMin, bool isToday)
{
    m_day = day;
    m_pomoMin = pomodoroMin;
    m_date->setText(isToday ? QStringLiteral("Hoy · %1").arg(fmt::shortDate(day)) : fmt::shortDate(day));
    m_dayTitle->setText(dayTitle);
    m_dayTitle->setPlaceholderText(isToday ? QStringLiteral("¿Qué querés lograr hoy?")
                                           : QStringLiteral("¿Qué querés lograr este día?"));

    const bool has = rule && !rule->spec.isEmpty();
    m_recurring = rule && rule->weekdays != 0;
    const QString spec = has ? rule->spec : QString();
    const bool timeMode = !has || spec.startsWith(QLatin1String("t:"));
    m_mode->button(timeMode ? 0 : 1)->setChecked(true);
    showModePage(timeMode ? 0 : 1);
    const int total = has && timeMode ? spec.mid(2).toInt() : (has ? 0 : 0);
    m_time->blockSignals(true);
    m_time->setText(goals::formatDuration(total));
    m_time->blockSignals(false);
    m_combo->blockSignals(true);
    m_combo->setText(has && !timeMode ? goals::describe(spec).replace(QChar(0x00d7), 'x') : QString());
    m_combo->blockSignals(false);
    {
        QSignalBlocker b(m_rows);
        m_rows->setSpec(has && !timeMode ? spec : QString(), pomodoroMin);
    }
    showModePage(timeMode ? 0 : 1);   // con las filas ya cargadas, la página mide lo que tiene que medir

    const int wd = rule ? rule->weekdays : 0;
    int repeatIdx = 0;
    if (wd == goals::kEveryDay) repeatIdx = 1;
    else if (wd == goals::kWeekdays) repeatIdx = 2;
    else if (wd != 0) repeatIdx = 3;
    m_repeat->setCurrentIndex(repeatIdx);
    for (int i = 0; i < 7; ++i) m_days->button(i)->setChecked(wd ? ((wd >> i) & 1) : i == day.dayOfWeek() - 1);
    m_daysRow->setVisible(repeatIdx == 3);

    m_remove->setVisible(has);
    m_save->setText(has ? QStringLiteral("Guardar cambios") : QStringLiteral("Guardar objetivo"));
    m_offDay = rule && rule->weekdays == 0 && rule->spec.isEmpty();
    refreshPreview();
}

void GoalPanel::setActivities(const FolderTree& tree, const QHash<qint64, QColor>& colors)
{
    m_tree = tree;
    m_colors = colors;
    m_rows->setActivities(tree, colors);
    refreshPreview();
}

void GoalPanel::setProgress(const Progress& p)
{
    m_progress = p;
    refreshPreview();
}

void GoalPanel::setComboStyle(const QString& style)
{
    m_buttonsStyle = style != QLatin1String("text");
    m_rows->setVisible(m_buttonsStyle);
    m_combo->setVisible(!m_buttonsStyle);
    m_comboHint->setVisible(!m_buttonsStyle);
    if (m_mode->checkedId() == 1) showModePage(1);
}

// La pila mide lo que mide la página visible (no la más alta de las dos).
void GoalPanel::showModePage(int id)
{
    m_modePages->setCurrentIndex(id);
    for (int i = 0; i < m_modePages->count(); ++i)
        m_modePages->widget(i)->setSizePolicy(QSizePolicy::Preferred, i == id ? QSizePolicy::Preferred : QSizePolicy::Ignored);
    m_modePages->widget(id)->adjustSize();
    m_modePages->setFixedHeight(m_modePages->widget(id)->sizeHint().height());
}

QString GoalPanel::currentSpec() const
{
    if (m_mode->checkedId() == 0) {
        const int total = qMin(24 * 60, qMax(0, goals::parseDuration(m_time->text())));
        return total > 0 ? QStringLiteral("t:%1").arg(total) : QString();
    }
    return m_buttonsStyle ? m_rows->spec() : goals::normalizeCombo(m_combo->text());
}

int GoalPanel::currentWeekdays() const
{
    const int v = m_repeat->currentData().toInt();
    if (v != -1) return v;
    int mask = 0;
    for (int i = 0; i < 7; ++i)
        if (m_days->button(i)->isChecked()) mask |= 1 << i;
    return mask ? mask : (1 << (m_day.dayOfWeek() - 1));
}

void GoalPanel::refreshPreview()
{
    const QString spec = currentSpec();
    const auto p = goals::parse(spec, m_pomoMin);
    const bool comboTyped = m_mode->checkedId() == 1 && !m_combo->text().trimmed().isEmpty();
    m_comboHint->setText(comboTyped && !p.ok ? QStringLiteral("No entiendo eso. Ej: 3x30 + 2x25")
                                             : QStringLiteral("Cantidad x minutos, separados con +"));
    const bool timeTyped = m_mode->checkedId() == 0 && !m_time->text().trimmed().isEmpty();
    m_timeHint->setText(timeTyped && goals::parseDuration(m_time->text()) < 0
                            ? QStringLiteral("No entiendo eso. Ej: 3h30, 3:30 o 210")
                            : QStringLiteral("Ej.: 3h30, 3:30, 4 (horas) o 210 (min)"));
    if (!p.ok) {
        m_preview->setBlocks({}, {});
        m_summary->setText(QString());
        m_status->setText(m_offDay ? QStringLiteral("Este día está marcado sin objetivo.") : QString());
        return;
    }
    // M21 → B2: por tiempo acumulado y, si las filas tienen actividad, por actividad.
    QHash<qint64, qint64> by = m_progress.byFolder;
    if (by.isEmpty() && m_progress.studiedSec > 0) by.insert(0, m_progress.studiedSec);
    const QVector<double> fills = goals::fillByActivity(p, by, m_tree);
    QVector<QColor> colors;
    for (qint64 f : p.folders) colors.append(f > 0 ? m_colors.value(f) : QColor());
    m_preview->setBlocks(p.blocks, fills, colors);
    const bool done = goals::allDone(fills);
    // Mejoras 2.0: además del estudio, cuánto es con los recreos en el medio.
    const qint64 withBreaks = goals::withBreaksSec(p, m_breakSec);
    const QString breaks = p.blocks.size() > 1
        ? QStringLiteral("\n%1 de estudio · ≈ %2 con recreos").arg(fmt::duration(p.totalMin * 60), fmt::duration(int(withBreaks)))
        : QString();
    m_summary->setText((p.timeMode
        ? QStringLiteral("≈ %1 %2 de %3").arg(p.blocks.size()).arg(p.blocks.size() == 1 ? "pomodoro" : "pomodoros")
              .arg(fmt::duration(m_pomoMin * 60))
        : QStringLiteral("%1 %2 · %3").arg(p.blocks.size()).arg(p.blocks.size() == 1 ? "pomodoro" : "pomodoros")
              .arg(fmt::duration(p.totalMin * 60))) + breaks);
    m_summary->setToolTip(QStringLiteral("Con un recreo de %1 entre bloque y bloque (%2; se cambia en Ajustes › General).")
                              .arg(fmt::duration(m_breakSec),
                                   m_breakFromAverage ? QStringLiteral("tu promedio real") : QStringLiteral("tu duración de recreo")));

    const qint64 target = qint64(p.totalMin) * 60;
    const int pct = target > 0 ? int(qMin<qint64>(100, m_progress.studiedSec * 100 / target)) : 0;
    if (done && target > 0)
        m_status->setText(QStringLiteral("✓ Objetivo cumplido · %1").arg(fmt::duration(int(m_progress.studiedSec))));
    else
        m_status->setText(QStringLiteral("%1 de %2 · %3 %")
                              .arg(fmt::duration(int(m_progress.studiedSec)), fmt::duration(int(target)))
                              .arg(pct));
}

// ===========================================================================
GoalPreview::GoalPreview(QWidget* parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void GoalPreview::setBlocks(const QVector<int>& blocks, const QVector<double>& fills, const QVector<QColor>& colors)
{
    m_blocks = blocks;
    m_fills = fills;
    m_colors = colors;
    setFixedHeight(heightForWidth(width() > 60 ? width() : 260));
    update();
}

void GoalPreview::resizeEvent(QResizeEvent* e)
{
    QWidget::resizeEvent(e);
    const int h = heightForWidth(width());
    if (h != height()) setFixedHeight(h);
}

QSize GoalPreview::sizeHint() const { return {240, heightForWidth(240)}; }

int GoalPreview::heightForWidth(int w) const
{
    if (m_blocks.isEmpty()) return 0;
    const int side = 22, gap = 4;
    const int perRow = qMax(1, (w + gap) / (side + gap));
    const int rows = (int(m_blocks.size()) + perRow - 1) / perRow;
    return rows * (side + gap);
}

void GoalPreview::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int side = 22, gap = 4;
    const int perRow = qMax(1, (width() + gap) / (side + gap));
    for (int i = 0; i < m_blocks.size(); ++i) {
        const QRectF r((i % perRow) * (side + gap) + 0.5, (i / perRow) * (side + gap) + 0.5, side - 1, side - 1);
        const double fill = m_fills.value(i);
        const bool full = fill >= 0.999;
        const QColor act = m_colors.value(i);
        if (full) {
            p.setPen(QPen(act.isValid() ? act : theme::squareDoneEdge, 1));
            p.setBrush(theme::squareDone);
        } else {
            QPen pen(act.isValid() ? theme::mix(theme::textFaint, act, 0.75) : theme::textFaint, 1, Qt::DashLine);
            pen.setDashPattern({3, 3});
            p.setPen(pen);
            p.setBrush(Qt::NoBrush);
        }
        p.drawRoundedRect(r, 3.5, 3.5);
        if (!full && fill > 0) {   // el que va a medias
            p.save();
            QPainterPath clip;
            clip.addRoundedRect(r, 3.5, 3.5);
            p.setClipPath(clip);
            QColor pf = act.isValid() ? act : theme::textFaint;
            pf.setAlphaF(0.4);
            p.setPen(Qt::NoPen);
            p.setBrush(pf);
            p.drawRect(QRectF(r.left(), r.top(), r.width() * fill, r.height()));
            p.restore();
        }
        QFont f = theme::uiFont(7, QFont::DemiBold);
        f.setPixelSize(9);
        p.setFont(f);
        p.setPen(full ? theme::textDim : theme::textFaint);
        p.drawText(r, Qt::AlignCenter, QString::number(m_blocks[i]));
    }
}

// ===========================================================================
// ComboRows (M17)
// ===========================================================================
namespace {
QToolButton* stepButton(const QString& text, QWidget* parent)
{
    auto* b = new QToolButton(parent);
    b->setObjectName("GoalStep");
    b->setText(text);
    b->setAutoRepeat(true);
    b->setAutoRepeatDelay(350);
    b->setAutoRepeatInterval(90);
    b->setCursor(Qt::PointingHandCursor);
    b->setFocusPolicy(Qt::NoFocus);
    return b;
}
} // namespace

ComboRows::ComboRows(QWidget* parent)
    : QWidget(parent)
{
    m_scroll = new QScrollArea(this);
    m_scroll->setObjectName("GoalScroll");
    m_scroll->setFrameShape(QFrame::NoFrame);
    m_scroll->setWidgetResizable(true);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto* inner = new QWidget(m_scroll);
    inner->setObjectName("GoalContent");
    m_list = new QVBoxLayout(inner);
    m_list->setContentsMargins(0, 0, 0, 0);
    m_list->setSpacing(4);
    m_list->addStretch();
    m_scroll->setWidget(inner);

    auto* add = new QPushButton(QStringLiteral("+  Agregar pomodoros"), this);
    add->setObjectName("GoalAdd");
    add->setCursor(Qt::PointingHandCursor);
    connect(add, &QPushButton::clicked, this, [this] {
        // La fila nueva arranca igual a la última (lo más común es repetir).
        const int min = m_rows.isEmpty() ? m_defaultMin : m_rows.last().minutes;
        addRow(1, min, m_rows.isEmpty() ? 0 : m_rows.last().folder);
        relayout();
        emit changed();
    });

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(4);
    lay->addWidget(m_scroll);
    lay->addWidget(add, 0, Qt::AlignLeft);
}

void ComboRows::setSpec(const QString& spec, int defaultMin)
{
    m_defaultMin = defaultMin;
    // M21: sacarlas del layout YA (deleteLater sola dejaba la fila vieja a la vista
    // hasta la próxima vuelta: se veían dos filas y los minutos "no cambiaban").
    for (const Row& r : m_rows) {
        m_list->removeWidget(r.w);
        r.w->hide();
        r.w->deleteLater();
    }
    m_rows.clear();
    for (const goals::Part& pt : goals::parts(spec))
        if (pt.count > 0 && pt.minutes > 0) addRow(pt.count, pt.minutes, pt.folder);
    if (m_rows.isEmpty()) addRow(4, defaultMin);   // un punto de partida razonable
    relayout();
    emit changed();
}

QString ComboRows::spec() const
{
    QVector<goals::Part> ps;
    for (const Row& r : m_rows) ps.append({r.count, r.minutes, r.folder});
    return goals::joinParts(ps);
}

// B2: el botón de actividad de cada fila: un puntito de color y el nombre corto.
void ComboRows::refreshActivityButton(Row& r)
{
    if (!r.act) return;
    const Folder* f = r.folder > 0 ? m_tree.find(r.folder) : nullptr;
    if (!f) r.folder = 0;
    QPixmap dot(20, 20);
    dot.setDevicePixelRatio(2);
    dot.fill(Qt::transparent);
    {
        QPainter g(&dot);
        g.setRenderHint(QPainter::Antialiasing);
        if (f) { g.setPen(Qt::NoPen); g.setBrush(m_colors.value(r.folder, theme::textDim)); }
        else { g.setPen(QPen(theme::textFaint, 1, Qt::DashLine)); g.setBrush(Qt::NoBrush); }
        g.drawEllipse(QRectF(1.5, 1.5, 7, 7));
    }
    r.act->setIcon(QIcon(dot));
    const QString name = f ? f->name : QStringLiteral("Cualquiera");
    r.act->setText(QFontMetrics(r.act->font()).elidedText(name, Qt::ElideRight, 170));
    r.act->setToolTip(f ? QStringLiteral("Actividad: %1").arg(m_tree.path(r.folder)) : QStringLiteral("Cualquier actividad"));
}

void ComboRows::setActivities(const FolderTree& tree, const QHash<qint64, QColor>& colors)
{
    m_tree = tree;
    m_colors = colors;
    for (Row& r : m_rows) refreshActivityButton(r);
}

void ComboRows::addRow(int count, int minutes, qint64 folder)
{
    auto* w = new QWidget(this);
    w->setObjectName("GoalRow");
    auto* cm = stepButton(QStringLiteral("−"), w);
    auto* cp = stepButton(QStringLiteral("+"), w);
    auto* cnt = new QLabel(w);
    cnt->setObjectName("GoalRowValue");
    cnt->setAlignment(Qt::AlignCenter);
    cnt->setMinimumWidth(18);
    auto* x = new QLabel(QStringLiteral("×"), w);
    x->setProperty("dim", true);
    auto* mm = stepButton(QStringLiteral("−"), w);
    auto* mp = stepButton(QStringLiteral("+"), w);
    auto* min = new QLabel(w);
    min->setObjectName("GoalRowValue");
    min->setAlignment(Qt::AlignCenter);
    min->setMinimumWidth(40);
    auto* act = new QToolButton(w);
    act->setObjectName("GoalRowActivity");
    act->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    act->setIconSize(QSize(10, 10));
    act->setCursor(Qt::PointingHandCursor);
    act->setFocusPolicy(Qt::NoFocus);
    act->setAutoRaise(true);
    auto* del = new QToolButton(w);
    del->setObjectName("GoalRowDelete");
    del->setText(QStringLiteral("✕"));
    del->setToolTip(QStringLiteral("Quitar esta fila"));
    del->setCursor(Qt::PointingHandCursor);
    del->setFocusPolicy(Qt::NoFocus);

    // B2: dos renglones: cantidad × minutos arriba, la actividad abajo (no entra todo en uno).
    auto* v = new QVBoxLayout(w);
    v->setContentsMargins(0, 0, 0, 2);
    v->setSpacing(2);
    auto* l = new QHBoxLayout;
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(3);
    v->addLayout(l);
    auto* l2 = new QHBoxLayout;
    l2->setContentsMargins(2, 0, 0, 0);
    v->addLayout(l2);
    l->addWidget(cm);
    l->addWidget(cnt);
    l->addWidget(cp);
    l->addSpacing(4);
    l->addWidget(x);
    l->addSpacing(4);
    l->addWidget(mm);
    l->addWidget(min);
    l->addWidget(mp);
    l->addStretch();
    l->addWidget(del);
    l2->addWidget(act);
    l2->addStretch();

    m_rows.append({w, count, minutes, cnt, min, folder, act});
    refreshActivityButton(m_rows.last());
    // B2: elegir la actividad de la fila (o "cualquiera").
    connect(act, &QToolButton::clicked, this, [this, w, act] {
        QMenu menu(this);
        auto pick = [this, w](qint64 id) {
            for (Row& r : m_rows)
                if (r.w == w) { r.folder = id; refreshActivityButton(r); }
            emit changed();
        };
        menu.addAction(QStringLiteral("Cualquier actividad"), this, [pick] { pick(0); });
        menu.addSeparator();
        std::function<void(qint64, int)> add = [&](qint64 parent, int depth) {
            for (qint64 id : m_tree.children(parent)) {
                QPixmap dot(20, 20);
                dot.setDevicePixelRatio(2);
                dot.fill(Qt::transparent);
                QPainter g(&dot);
                g.setRenderHint(QPainter::Antialiasing);
                g.setPen(Qt::NoPen);
                g.setBrush(m_colors.value(id, theme::textDim));
                g.drawEllipse(QRectF(1.5, 1.5, 7, 7));
                g.end();
                menu.addAction(QIcon(dot), QString(depth * 3, QChar(' ')) + m_tree.find(id)->name, this, [pick, id] { pick(id); });
                add(id, depth + 1);
            }
        };
        add(0, 0);
        menu.exec(act->mapToGlobal(QPoint(0, act->height())));
    });
    auto refresh = [cnt, min, this, w] {
        for (const Row& r : m_rows)
            if (r.w == w) {
                cnt->setText(QString::number(r.count));
                min->setText(QStringLiteral("%1m").arg(r.minutes));
            }
    };
    refresh();
    auto edit = [this, w, refresh](int dc, int dm) {
        for (Row& r : m_rows)
            if (r.w == w) {
                r.count = qBound(1, r.count + dc, 40);
                // De a 5 minutos arriba de 10, de a 1 por debajo (como el reproductor).
                if (dm > 0) r.minutes = qMin(240, r.minutes >= 10 ? (r.minutes / 5 + 1) * 5 : r.minutes + 1);
                if (dm < 0) r.minutes = qMax(1, r.minutes > 10 ? ((r.minutes - 1) / 5) * 5 : r.minutes - 1);
            }
        refresh();
        emit changed();
    };
    connect(cm, &QToolButton::clicked, this, [edit] { edit(-1, 0); });
    connect(cp, &QToolButton::clicked, this, [edit] { edit(1, 0); });
    connect(mm, &QToolButton::clicked, this, [edit] { edit(0, -1); });
    connect(mp, &QToolButton::clicked, this, [edit] { edit(0, 1); });
    connect(del, &QToolButton::clicked, this, [this, w] {
        if (m_rows.size() <= 1) return;   // siempre queda al menos una
        removeRow(w);
        relayout();
        emit changed();
    });
    m_list->insertWidget(m_list->count() - 1, w);
}

void ComboRows::removeRow(QWidget* w)
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows[i].w == w) { m_rows.removeAt(i); break; }
    m_list->removeWidget(w);
    w->hide();
    w->deleteLater();
}

// Hasta 3 filas se ven enteras; con más, la lista scrollea en ese espacio.
void ComboRows::relayout()
{
    const int rowH = m_rows.isEmpty() ? 28 : qMax(28, m_rows.first().w->sizeHint().height());
    const int visible = qMin(3, int(m_rows.size()));
    m_scroll->setFixedHeight(visible * rowH + qMax(0, visible - 1) * m_list->spacing() + 4);
    updateGeometry();
}

void GoalPanel::setBreakSec(int sec, bool fromAverage)
{
    m_breakSec = sec;
    m_breakFromAverage = fromAverage;
    refreshPreview();
}

bool GoalPanel::eventFilter(QObject* obj, QEvent* e)
{
    // Rueda sobre el tiempo (con el foco puesto): de a 15 minutos.
    if (obj == m_time && e->type() == QEvent::Wheel && m_time->hasFocus()) {
        const auto* we = static_cast<QWheelEvent*>(e);
        const int cur = qMax(0, goals::parseDuration(m_time->text()));
        const int next = qBound(0, (cur / 15 + (we->angleDelta().y() > 0 ? 1 : (cur % 15 ? 0 : -1))) * 15, 16 * 60);
        m_time->setText(goals::formatDuration(next));
        return true;
    }
    return QWidget::eventFilter(obj, e);
}
