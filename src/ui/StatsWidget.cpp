#include "ui/StatsWidget.h"
#include "core/Routine.h"
#include "core/Kinds.h"
#include "ui/DetailCard.h"
#include "ui/EvolutionView.h"
#include "core/FolderTree.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Export.h"
#include "core/Report.h"
#include "core/Milestones.h"
#include "core/Goals.h"
#include "core/Insights.h"
#include "core/StartGoal.h"
#include "ui/ActivitiesWidget.h"
#include "ui/Fade.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QUrl>
#include <QDir>
#include <QDesktopServices>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QScrollBar>
#include <QTimer>
#include <array>
#include <QToolTip>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <cmath>
#include <functional>

// ===========================================================================
// Lienzo: todo lo que se dibuja en la página de estadísticas
// ===========================================================================
// M35 → M41: color de las estadísticas = el del visor del cronómetro (el rojo
// suave en el tema gris, el acento en los temas de color).
static QColor statsInk() { return theme::pomoAccent; }

// M44: horas redondas para los cortes de etapa ("233 h", no "233h20").
static QString roundHours(qint64 sec) { return QStringLiteral("%1 h").arg(QLocale().toString(qint64(std::llround(sec / 3600.0)))); }

struct StatsData {
    QString studyLabel = QStringLiteral("Estudio");   // Mejoras 3.0: según el tipo de actividad
    StatsWidget::Period period = StatsWidget::Period::Week;
    QDate from, to, today;
    stats::Totals totals;
    QMap<QDate, qint64> byDay;
    QVector<insights::Session> sessions;           // Mejoras 5.1
    QMap<QDate, insights::DayStat> daily;          // Mejoras 4.0: por día (estudio, pomodoros, indebido, inactivo)
    QHash<QDate, Database::DayDiary> diary;         // nota y ánimo
    QString heatMetric = QStringLiteral("hours");   // qué colorea el mapa de calor
    bool    simple = true;                           // Mejoras 4.1: vista simple (por defecto) o avanzada
    QVector<Pomodoro> pomos;       // para la línea del día
    QVector<Interval> intervals;   // idem
    const QSet<qint64>* filter = nullptr;
    QSet<qint64> filterSet;
    stats::Streak streak;
    int jokers = 2;
    struct Row { QString name; int depth = 0; qint64 period = 0; qint64 total = 0; qint64 id = 0; };
    QVector<Row> rows;
    // B2-3: objetivo de cada día (hasta hoy): meta, lo hecho, si se cumplió y cuánto faltó
    struct GoalDay { qint64 target = 0; qint64 studied = 0; qint64 missing = 0; bool met = false; };
    QMap<QDate, GoalDay> goals;
    // M44: hitos
    ms::Config ms;
    qint64 todaySec = 0, weekSec = 0, allSec = 0;
    int startMet = 0, startCounted = 0;                       // Mejoras 7.0: objetivos de arranque (últimos 30 días)
    QVector<QPair<QDate, QString>> startRows;                 // fecha + resultado, de más nuevo a más viejo
    int pomosToday = 0, weekMinMet = 0, weekMinCounted = 0;   // Mejoras 7.0: mínimo diario de pomodoros
    ms::Records records;   // incluye hoy
    int bestStreak = 0;
    // Mejoras 5.2: paneles de la pantalla (orden, visibilidad y tamaño 0/1/2 = S/M/L), editables en la vista avanzada
    struct Panel { QString id; bool visible = true; int size = 1; };   // solo para migrar el formato viejo
    // Mejoras 7.0: grilla con imán. Cada tarjeta o sección tiene un ancho en columnas (de 12) y un alto en filas.
    struct Item { QString id; int cols = 4; int rows = 2; int x = -1, y = -1; };   // x (en doceavos) e y: lugar elegido a mano; -1 = automático
    QVector<Item> grid;
    QSet<QString> cards;   // Mejoras 6.0: tarjetas sueltas que se ven (cada vista, simple o avanzada, tiene las suyas)
    bool has(const QString& id) const { return cards.contains(id); }
    bool editing = false;
    QSet<qint64> hiddenRows;   // Mejoras 5.0: actividades que el usuario sacó de la lista
    QHash<qint64, QColor> colors;   // M35: color de cada actividad (vacío si están apagados)
    QColor colorOf(qint64 id) const { return colors.value(id, statsInk()); }
};

// Mejoras 6.0: tarjetas sueltas. Cada vista (simple / avanzada) guarda las suyas.
static const QStringList& allCards()
{
    static const QStringList l = {"streak", "study", "focus", "breaks", "undue", "paused", "inactive", "sessions", "goals", "mark", "startgoal", "weekly", "records", "level", "chart", "folders"};
    return l;
}
static QString cardName(const QString& id)
{
    static const QMap<QString, QString> m = {{"streak", "Racha"}, {"study", "Estudio"}, {"focus", "Foco %"}, {"breaks", "Recreos"},
        {"undue", "Tiempo muerto"}, {"paused", "Recreo indebido"}, {"inactive", "Horas inactivas"}, {"sessions", "Sesiones"}, {"goals", "Objetivos"},
        {"mark", "Marca de hoy"}, {"startgoal", "Arranque"}, {"weekly", "Meta semanal"}, {"records", "Tus récords"}, {"level", "Nivel global"}, {"chart", "Gráfico"}, {"folders", "Actividades"}};
    return m.value(id, id);
}
static QSet<QString> defaultCards(bool simple)
{
    QSet<QString> s;
    for (const QString& id : allCards()) {
        const bool advancedOnly = id == QLatin1String("breaks") || id == QLatin1String("undue") || id == QLatin1String("paused") || id == QLatin1String("inactive") ||
                                  id == QLatin1String("sessions") || id == QLatin1String("level");
        if (!simple || !advancedOnly) s.insert(id);
    }
    return s;
}
static QSet<QString> parseCards(const QString& s, bool simple)
{
    if (s.isEmpty()) return defaultCards(simple);
    QSet<QString> out;   // «none» = todas sacadas
    for (const QString& id : s.split(',', Qt::SkipEmptyParts)) if (allCards().contains(id)) out.insert(id);
    return out;
}
static QString cardsKey(bool simple) { return simple ? QStringLiteral("stats_cards_simple") : QStringLiteral("stats_cards_adv"); }
static QString layoutKey(bool simple) { return simple ? QStringLiteral("stats_layout_simple") : QStringLiteral("stats_layout_adv"); }

// Mejoras 5.2: orden, visibilidad y tamaño de los paneles ("hero:1:1,chips:1:1,…" = id:visible:tamaño 0/1/2).
static QVector<StatsData::Panel> parseLayout(const QString& s)
{
    static const QStringList all = {QStringLiteral("hero"), QStringLiteral("chips"), QStringLiteral("chart"), QStringLiteral("folders")};
    QVector<StatsData::Panel> out;
    for (const QString& part : s.split(',', Qt::SkipEmptyParts)) {
        const QStringList f = part.split(':');
        if (f.size() < 3 || !all.contains(f[0])) continue;
        bool dup = false;
        for (const auto& o : out) if (o.id == f[0]) dup = true;
        if (dup) continue;
        out.append({f[0], f[1] != QLatin1String("0"), qBound(0, f[2].toInt(), 2)});
    }
    for (const QString& id : all) {   // lo que falte vuelve con el valor por defecto
        bool has = false;
        for (const auto& o : out) if (o.id == id) has = true;
        if (!has) out.append({id, true, 1});
    }
    return out;
}
static QString serializeLayout(const QVector<StatsData::Panel>& v)
{
    QStringList l;
    for (const auto& p : v) l << QStringLiteral("%1:%2:%3").arg(p.id).arg(p.visible ? 1 : 0).arg(p.size);
    return l.join(',');
}

// Mejoras 7.0: orden y tamaño de las tarjetas ("id:columnas:filas,…"; las que falten vuelven con su tamaño por defecto, al final).
static QVector<StatsData::Item> defaultGrid()
{
    static const struct { const char* id; int c, r; } d[] = {
        {"streak", 4, 3}, {"study", 3, 3}, {"records", 5, 3},
        {"breaks", 3, 2}, {"undue", 3, 2}, {"paused", 3, 2}, {"inactive", 3, 2}, {"sessions", 3, 2},
        {"goals", 4, 2}, {"mark", 4, 2}, {"startgoal", 4, 2}, {"weekly", 4, 3}, {"focus", 4, 2}, {"level", 4, 2},
        {"chart", 12, 7}, {"folders", 12, 9}};
    QVector<StatsData::Item> v;
    for (const auto& x : d) v.append({QString::fromLatin1(x.id), x.c, x.r});
    return v;
}
static QVector<StatsData::Item> parseGrid(const QString& s)
{
    const QVector<StatsData::Item> def = defaultGrid();
    QVector<StatsData::Item> out;
    for (const QString& part : s.split(',', Qt::SkipEmptyParts)) {
        const QStringList f = part.split(':');
        if (f.size() < 3 || !allCards().contains(f[0])) continue;
        bool dup = false;
        for (const auto& o : out) if (o.id == f[0]) dup = true;
        if (dup) continue;
        StatsData::Item it{f[0], qBound(2, f[1].toInt(), 12), qBound(2, f[2].toInt(), 24)};
        if (f.size() >= 5) { it.x = qBound(0, f[3].toInt(), 11); it.y = qMax(0, f[4].toInt()); }
        out.append(it);
    }
    for (const auto& d : def) {
        bool has = false;
        for (const auto& o : out) if (o.id == d.id) has = true;
        if (!has) out.append(d);
    }
    return out;
}
static QString serializeGrid(const QVector<StatsData::Item>& v)
{
    QStringList l;
    for (const auto& it : v) {
        if (it.x >= 0 && it.y >= 0) l << QStringLiteral("%1:%2:%3:%4:%5").arg(it.id).arg(it.cols).arg(it.rows).arg(it.x).arg(it.y);
        else l << QStringLiteral("%1:%2:%3").arg(it.id).arg(it.cols).arg(it.rows);
    }
    return l.join(',');
}
static QString gridKey(bool simple) { return simple ? QStringLiteral("stats_grid_simple") : QStringLiteral("stats_grid_adv"); }

class StatsCanvas : public QWidget {
public:
    explicit StatsCanvas(QWidget* parent) : QWidget(parent)
    {
        setMouseTracking(true);
        m_animTimer.setInterval(16);
        connect(&m_animTimer, &QTimer::timeout, this, [this] { if (!stepSpring()) m_animTimer.stop(); update(); });
    }
    StatsData data;
    // Mejoras 3.0: cada dato es una zona: al pasar el mouse muestra más detalle
    // y al hacer click abre su mini ventana.
    struct Zone { QRectF rect; QString key; QString tip; };
    QVector<Zone> zones;
    std::function<void(const QString&)> onZone;
    QRectF shownRect(const QString& id) const { return m_shown.value(id); }
    std::function<void(const QVector<StatsData::Item>&)> onGrid;
    QScrollArea* scrollArea = nullptr;
    std::function<QString(const QString&)> tipFor;
    bool hoverTips = false;   // Mejoras 4.1: el botón ⓘ vuelve a activar la info al pasar el mouse

    void relayout()
    {
        setMinimumHeight(contentHeight(width()));
        update();
    }

protected:
    void resizeEvent(QResizeEvent*) override { setMinimumHeight(contentHeight(width())); }

    const Zone* zoneAt(const QPointF& pos) const
    {
        for (const Zone& z : zones) if (z.rect.contains(pos)) return &z;
        return nullptr;
    }
    void zone(const QRectF& r, const QString& key, QPainter& p)
    {
        zones.append({r, key, QString()});
        if (key == m_hoverKey) {   // se nota que se puede tocar
            p.save();
            p.setPen(QPen(theme::textFaint, 1));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
            p.restore();
        }
    }
    QString m_hoverKey;

    // ---- Mejoras 7.0: grilla con imán. Tarjetas y secciones se arrastran y se estiran como ventanas;
    // al soltar se acomodan con un resorte (rebote corto) en el casillero que les toca.
    void mousePressEvent(QMouseEvent* e) override
    {
        if (!data.editing || e->button() != Qt::LeftButton) { QWidget::mousePressEvent(e); return; }
        for (const Zone& z : zones)
            if (z.rect.contains(e->position()) && (z.key.startsWith(QLatin1String("card:")) || z.key.startsWith(QLatin1String("lay:")))) return;   // botones: los atiende el release
        const QString id = itemAt(e->position());
        if (id.isEmpty()) return;
        m_pressId = id;
        m_pressPos = e->position();
        m_pressRect = m_shown.value(id);
        m_grab = edgeAt(id, e->position());
    }
    void mouseMoveEvent(QMouseEvent* e) override
    {
        if (data.editing && !m_pressId.isEmpty() && (e->buttons() & Qt::LeftButton)) { dragStep(e->position()); return; }
        const Zone* z = zoneAt(e->position());
        QString id;
        if (data.editing && !z) id = itemAt(e->position());
        if (!id.isEmpty()) {
            const int g = edgeAt(id, e->position());
            setCursor(g == 3 ? Qt::SizeFDiagCursor : g == 1 ? Qt::SizeHorCursor : g == 2 ? Qt::SizeVerCursor : Qt::OpenHandCursor);
        } else setCursor(z ? Qt::PointingHandCursor : Qt::ArrowCursor);
        const QString key = z ? z->key : QString();
        if (key != m_hoverKey) { m_hoverKey = key; update(); }
        if (hoverTips && z && tipFor) QToolTip::showText(e->globalPosition().toPoint(), tipFor(z->key), this, z->rect.toRect());
        else QToolTip::hideText();   // por defecto el detalle sale solo con click (menos estímulo)
    }
    void leaveEvent(QEvent*) override { if (!m_hoverKey.isEmpty()) { m_hoverKey.clear(); update(); } }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (!m_dragId.isEmpty() || !m_resId.isEmpty()) {
            const QString who = !m_dragId.isEmpty() ? m_dragId : m_resId;
            for (auto& v : m_vel) v = {0, 0, 0, 0};
            (void)who;
            if (!m_preview.isEmpty()) {
                data.grid = m_preview;
                pinPositions(data.grid, flow(data.grid, m_W, presentSet(), QString()));   // suelta: todo se apoya sin huecos
            }
            m_preview.clear();
            m_dragId.clear();
            m_resId.clear();
            m_pressId.clear();
            unsetCursor();
            setMinimumHeight(contentHeight(width()));
            if (onGrid) onGrid(data.grid);
            update();   // el resto lo hace el resorte
            return;
        }
        m_pressId.clear();
        if (const Zone* z = zoneAt(e->position()); z && onZone) onZone(z->key);
    }

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), theme::bg);
        const double L = 16, W = width() - 32;
        double y0 = 8;
        zones.clear();
        pendingX.clear();
        if (!data.editing && (!m_preview.isEmpty() || !m_dragId.isEmpty() || !m_resId.isEmpty())) { m_preview.clear(); m_dragId.clear(); m_resId.clear(); m_pressId.clear(); }

        if (data.editing) y0 = paintEditHeader(p, L, y0, W) + 10;
        m_L = L; m_y0 = y0; m_W = W;
        const QSet<QString> pres = presentSet();
        const auto placed = flow(currentItems(), W, pres, !m_dragId.isEmpty() ? m_dragId : m_resId);
        QSet<QString> alive;
        bool needAnim = false;
        for (const Placed& pl : placed) {
            const QRectF t = placedRect(pl);
            alive.insert(pl.id);
            m_target[pl.id] = t;
            if (!m_shown.contains(pl.id) || !data.editing) { m_shown[pl.id] = t; m_vel[pl.id] = {0, 0, 0, 0}; }
            else if (pl.id != m_dragId && pl.id != m_resId) {
                const QRectF c = m_shown[pl.id];
                if (qAbs(c.x() - t.x()) > 0.3 || qAbs(c.y() - t.y()) > 0.3 || qAbs(c.width() - t.width()) > 0.3 || qAbs(c.height() - t.height()) > 0.3) needAnim = true;
            }
        }
        for (auto it = m_target.begin(); it != m_target.end();) {
            if (!alive.contains(it.key())) { m_shown.remove(it.key()); m_vel.remove(it.key()); it = m_target.erase(it); }
            else ++it;
        }
        const bool interacting = !m_dragId.isEmpty() || !m_resId.isEmpty();
        if (interacting) paintGridDots(p, placed);
        for (const Placed& pl : placed)
            if (pl.id != m_dragId && pl.id != m_resId) paintOne(p, pl.id, m_shown.value(pl.id));
        auto outline = [&](const QRectF& r, const QColor& c) {
            p.save();
            p.setPen(QPen(c, 1.5, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(0.75, 0.75, -0.75, -0.75), 10, 10);
            p.restore();
        };
        const QString top = !m_dragId.isEmpty() ? m_dragId : m_resId;
        if (!top.isEmpty() && m_target.contains(top)) {
            outline(m_target.value(top), theme::textDim);   // el casillero al que va a quedar pegada
            p.save();
            p.setOpacity(0.92);
            paintOne(p, top, m_shown.value(top));
            p.restore();
        }
        if (!pendingX.isEmpty()) zones = pendingX + zones;   // las ✕ van adelante
        if (needAnim && !m_animTimer.isActive()) m_animTimer.start();
    }

private:
    double m_foldAvail = 6 * 26.0;   // Mejoras 7.0: alto disponible para la lista (más que eso → scroll adentro)
    static constexpr double kRowH = 26;

    bool goalChip() const { return data.period != StatsWidget::Period::Day && !data.goals.isEmpty(); }

    // M44: tarjetitas con los hitos de corto y largo plazo.
    // Mejoras 3.0: si no entran en una fila (ventana chica) bajan a dos o más, así el
    // texto se lee entero; cada una abre su detalle.
    struct Chip { QString key, label, value; double frac = -1; int dots = -1; QString sub; };
    QVector<Chip> buildChips() const
    {
        QVector<Chip> chips;
        if (goalChip()) {   // B2-3
            int met = 0, counted = 0;
            qint64 missing = 0;
            for (auto it = data.goals.cbegin(); it != data.goals.cend(); ++it) {
                if (it.key() == data.today && !it->met) continue;   // hoy todavía no terminó
                ++counted;
                if (it->met) ++met; else missing += it->missing;
            }
            if (counted > 0)
                chips.append({QStringLiteral("goals"), QStringLiteral("Objetivos"),
                              missing > 0 ? QStringLiteral("%1 de %2 · faltaron %3").arg(met).arg(counted).arg(fmt::hours(missing))
                                          : QStringLiteral("%1 de %2 cumplidos").arg(met).arg(counted),
                              double(met) / counted});
        }
        if (data.ms.dayMark) {
            const int lvl = ms::dayMark(data.todaySec, data.ms.markMin);
            const QString next = lvl < 3 ? QStringLiteral(" · %1 a las %2").arg(ms::dayMarkName(lvl + 1),
                                                                                 fmt::hours(qint64(data.ms.markMin[lvl]) * 60))
                                         : QString();
            chips.append({QStringLiteral("mark"), QStringLiteral("Marca de hoy"),
                          (lvl ? ms::dayMarkName(lvl).left(1).toUpper() + ms::dayMarkName(lvl).mid(1) : QStringLiteral("Todavía no"))
                              + next, -1, lvl});
        }
        if (data.startCounted > 0 || !data.startRows.isEmpty())
            chips.append({QStringLiteral("startgoal"), QStringLiteral("Arranque"),
                          data.startCounted > 0 ? QStringLiteral("%1 de %2 cumplidos").arg(data.startMet).arg(data.startCounted) : QStringLiteral("Todavía sin días"),
                          data.startCounted > 0 ? double(data.startMet) / data.startCounted : -1.0});
        const bool wk = data.ms.weekly && data.ms.weeklyMin > 0, dm = data.ms.dailyPomos > 0;
        if (wk || dm) {
            const qint64 goal = qint64(data.ms.weeklyMin) * 60;
            Chip c{QStringLiteral("weekly"), QStringLiteral("Meta semanal"),
                   wk ? QStringLiteral("%1 de %2").arg(fmt::hours(data.weekSec), fmt::hours(goal)) : QStringLiteral("%1 esta semana").arg(fmt::hours(data.weekSec)),
                   wk ? double(data.weekSec) / goal : -1.0};
            if (dm)   // Mejoras 7.0: el mínimo diario de pomodoros va debajo, en el mismo bloque
                c.sub = QStringLiteral("Mín. %1 pom/día · hoy %2/%1 · semana %3/%4")
                            .arg(data.ms.dailyPomos).arg(data.pomosToday).arg(data.weekMinMet).arg(data.weekMinCounted);
            chips.append(c);
        }
        {   // Mejoras 7.0: el Foco pasó acá (los récords ocupan su lugar arriba)
            const int fp = focusPct();
            chips.append({QStringLiteral("focus"), QStringLiteral("Foco"), fp >= 0 ? QStringLiteral("%1 % del tiempo de sesión").arg(fp) : QStringLiteral("—"),
                          fp >= 0 ? fp / 100.0 : -1});
        }
        if (data.ms.stages) {
            const ms::Stage st = ms::stageFor(data.allSec);
            chips.append({QStringLiteral("level"), QStringLiteral("Nivel global"),
                          st.toSec ? QStringLiteral("%1 · próx. %2").arg(st.label, roundHours(st.toSec)) : st.label,
                          st.toSec ? st.progress : 1.0});
        }
        QVector<Chip> keep;   // Mejoras 6.0: solo las tarjetas que esa vista tiene activadas
        for (const Chip& ch : chips) if (data.has(ch.key)) keep << ch;
        return keep;
    }
    void paintChip(QPainter& p, const QRectF& r, const Chip& c)
    {
        static const QColor medal[3] = {QColor(0xc0, 0x8a, 0x5a), QColor(0xb8, 0xbc, 0xc4), QColor(0xe0, 0xb8, 0x4a)};
        p.setPen(QPen(theme::line, 1));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
        p.setFont(theme::uiFont(8));
        p.setPen(theme::textDim);
        p.drawText(r.adjusted(12, 8, -8, 0), Qt::AlignLeft | Qt::AlignTop, c.label);
        if (c.dots >= 0) {   // las medallas van arriba a la derecha: el texto tiene todo el ancho
            for (int k = 0; k < 3; ++k) {
                const QRectF dot(r.right() - 12 - (3 - k) * 10, r.top() + 10, 7, 7);
                if (k < c.dots) { p.setPen(Qt::NoPen); p.setBrush(medal[k]); }
                else { p.setPen(QPen(theme::textFaint, 1)); p.setBrush(Qt::NoBrush); }
                p.drawEllipse(dot);
            }
        }
        const QRectF vr(r.left() + 12, r.top() + 22, r.width() - 22, 22);
        // Si no entra, primero se achica la letra; recién después se corta.
        double pt = 9.5;
        p.setFont(theme::uiFont(pt, QFont::DemiBold));
        while (pt > 7.6 && QFontMetricsF(p.font()).horizontalAdvance(c.value) > vr.width()) {
            pt -= 0.5;
            p.setFont(theme::uiFont(pt, QFont::DemiBold));
        }
        p.setPen(theme::text);
        p.drawText(vr, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(c.value, Qt::ElideRight, int(vr.width())));
        if (!c.sub.isEmpty()) {
            p.setFont(theme::uiFont(8));
            p.setPen(theme::textDim);
            p.drawText(QRectF(r.left() + 12, r.top() + 42, r.width() - 22, 14), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(c.sub, Qt::ElideRight, int(r.width() - 22)));
        }
        if (c.frac >= 0 && (c.sub.isEmpty() || r.height() >= 66)) {
            const QRectF track(r.left() + 12, r.bottom() - 9, r.width() - 24, 3);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::surface);
            p.drawRoundedRect(track, 1.5, 1.5);
            p.setBrush(c.frac >= 1 ? theme::breakAccent : statsInk());
            p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * qMin(1.0, c.frac), 3), 1.5, 1.5);
        }
        zone(r, c.key, p);
        xButton(p, r, c.key);
    }

    double chartHeight(double W) const
    {
        switch (data.period) {
        case StatsWidget::Period::Day:  return 24 + 66;
        case StatsWidget::Period::Year: return 24 + 30 + 7 * (heatCell(W) + 3) + 18 + 24;
        default:                        return 24 + plotH() + 40;
        }
    }
    double m_plotH = 128;   // Mejoras 7.0: sale del alto de la tarjeta
    double plotH() const { return m_plotH; }
    static double heatCell(double W) { return qBound(6.0, (W - 30) / 53.0 - 3, 14.0); }

    // ---- Mejoras 7.0: motor de la grilla ----------------------------------------------------
    static constexpr double kCellH = 22, kGridGap = 10, kEdgeGrab = 8, kSpringK = 420, kSpringC = 28;
    int gridCols(double W) const { return W < 520 ? 6 : 12; }
    struct Placed { QString id; int col = 0, row = 0, cols = 1, rows = 1; };
    struct Limits { int minC, maxC, minR, maxR; };   // en columnas de 12; minR = 0: el alto no se toca

    const QVector<StatsData::Item>& currentItems() const { return m_preview.isEmpty() ? data.grid : m_preview; }
    bool isBarsChart() const { return data.period != StatsWidget::Period::Day && data.period != StatsWidget::Period::Year; }
    Limits limitsFor(const QString& id) const
    {
        if (id == QLatin1String("chart")) return {6, 12, isBarsChart() ? 5 : 0, isBarsChart() ? 16 : 0};
        if (id == QLatin1String("folders")) return {6, 12, 4, 24};
        return {2, 12, 2, 6};
    }
    // Qué tarjetas tienen algo para mostrar hoy (las prendidas que tienen datos).
    QSet<QString> presentSet() const
    {
        QSet<QString> s;
        if (data.has("streak")) s.insert("streak");
        if (data.has("study")) s.insert("study");
        if (recordsCard()) s.insert("records");
        if (data.has("chart")) s.insert("chart");
        if (data.has("folders")) s.insert("folders");
        for (const Small& sm : smallCards()) if (data.has(sm.id)) s.insert(sm.id);
        for (const Chip& c : buildChips()) s.insert(c.key);
        return s;
    }
    static double cellW(double W, int n) { return (W - (n - 1) * kGridGap) / n; }
    static double rowsPx(int r) { return r * kCellH + (r - 1) * kGridGap; }
    static int rowsFor(double h) { return qMax(2, int(std::ceil((h + kGridGap) / (kCellH + kGridGap) - 1e-9))); }
    static int itemCols(const StatsData::Item& it, int n) { return qBound(1, int(std::lround(it.cols * n / 12.0)), n); }
    int itemRows(const StatsData::Item& it, double W, int cols, int n) const
    {
        if (it.id == QLatin1String("chart") && !isBarsChart())   // día y año tienen alto propio
            return rowsFor(chartHeight(cols * cellW(W, n) + (cols - 1) * kGridGap));
        return it.rows;
    }
    // Reparte las tarjetas: las que tienen lugar elegido van ahí (si chocan, la de abajo se corre hacia abajo),
    // después todas «suben» hasta apoyarse (sin huecos de más), y las automáticas van al primer hueco libre.
    // `priority` (la que se arrastra o estira) se acomoda primero y no sube.
    QVector<Placed> flow(const QVector<StatsData::Item>& items, double W, const QSet<QString>& pres, const QString& priority = QString()) const
    {
        const int n = gridCols(W);
        QVector<QVector<char>> occ;
        auto isFree = [&](int r, int c, int w, int h) {
            for (int y = r; y < r + h && y < occ.size(); ++y)
                for (int x = c; x < c + w; ++x) if (occ[y][x]) return false;
            return true;
        };
        auto mark = [&](int r, int c, int w, int h, char v) {
            while (occ.size() < r + h) occ.append(QVector<char>(n, 0));
            for (int y = r; y < r + h; ++y) for (int x = c; x < c + w; ++x) occ[y][x] = v;
        };
        QVector<Placed> fixed, autos;
        QVector<int> order;   // índices de items con lugar propio
        for (int i = 0; i < items.size(); ++i) if (pres.contains(items[i].id) && items[i].x >= 0 && items[i].y >= 0) order.append(i);
        std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
            const bool pa = items[a].id == priority, pb = items[b].id == priority;
            if (pa != pb) return pa;
            return items[a].y != items[b].y ? items[a].y < items[b].y : items[a].x < items[b].x;
        });
        QVector<Placed> out;
        for (int i : order) {
            const auto& it = items[i];
            const int c = itemCols(it, n), r = itemRows(it, W, c, n);
            int col = qBound(0, int(std::lround(it.x * n / 12.0)), n - c), row = it.y;
            while (!isFree(row, col, c, r)) ++row;
            mark(row, col, c, r, 1);
            out.append({it.id, col, row, c, r});
        }
        // gravedad: cada una sube hasta apoyarse (la que se está moviendo se queda donde está)
        std::stable_sort(out.begin(), out.end(), [&](const Placed& a, const Placed& b) { return a.row != b.row ? a.row < b.row : a.col < b.col; });
        for (Placed& pl : out) {
            if (pl.id == priority) continue;
            mark(pl.row, pl.col, pl.cols, pl.rows, 0);
            while (pl.row > 0 && isFree(pl.row - 1, pl.col, pl.cols, pl.rows)) --pl.row;
            mark(pl.row, pl.col, pl.cols, pl.rows, 1);
        }
        // automáticas: primer hueco libre, en orden
        for (const auto& it : items) {
            if (!pres.contains(it.id) || (it.x >= 0 && it.y >= 0)) continue;
            const int c = itemCols(it, n), r = itemRows(it, W, c, n);
            for (int row = 0;; ++row) {
                bool done = false;
                for (int col = 0; col + c <= n; ++col) {
                    if (!isFree(row, col, c, r)) continue;
                    mark(row, col, c, r, 1);
                    out.append({it.id, col, row, c, r});
                    done = true;
                    break;
                }
                if (done) break;
            }
        }
        return out;
    }
    // Fija el lugar de cada tarjeta visible (al empezar a mover algo y al soltar), para que el resto no se corra solo.
    void pinPositions(QVector<StatsData::Item>& items, const QVector<Placed>& placed) const
    {
        const int n = gridCols(m_W);
        for (auto& it : items)
            for (const Placed& pl : placed)
                if (pl.id == it.id) { it.x = qBound(0, int(std::lround(pl.col * 12.0 / n)), 11); it.y = pl.row; break; }
    }
    QRectF placedRect(const Placed& pl) const
    {
        const int n = gridCols(m_W);
        const double cw = cellW(m_W, n);
        return QRectF(m_L + pl.col * (cw + kGridGap), m_y0 + pl.row * (kCellH + kGridGap), pl.cols * cw + (pl.cols - 1) * kGridGap, rowsPx(pl.rows));
    }
    int contentHeight(int w) const
    {
        const double W = w - 32;
        double y0 = 8 + (data.editing ? 34 + 10 : 0);
        int maxRow = 0;
        for (const Placed& pl : flow(currentItems(), W, presentSet(), !m_dragId.isEmpty() ? m_dragId : m_resId)) maxRow = qMax(maxRow, pl.row + pl.rows);
        return int(y0 + (maxRow > 0 ? maxRow * (kCellH + kGridGap) - kGridGap : 0) + 6);
    }

    QString itemAt(const QPointF& pos) const
    {
        for (auto it = m_shown.cbegin(); it != m_shown.cend(); ++it) if (it.value().contains(pos)) return it.key();
        return {};
    }
    int edgeAt(const QString& id, const QPointF& pos) const   // 1 = borde derecho, 2 = inferior, 3 = esquina
    {
        const QRectF r = m_shown.value(id);
        const Limits lim = limitsFor(id);
        int g = 0;
        if (pos.x() >= r.right() - kEdgeGrab) g |= 1;
        if (lim.minR > 0 && pos.y() >= r.bottom() - kEdgeGrab) g |= 2;
        return g;
    }
    void paintGridDots(QPainter& p, const QVector<Placed>& placed) const
    {
        const int n = gridCols(m_W);
        const double cw = cellW(m_W, n);
        int rows = 4;
        for (const Placed& pl : placed) rows = qMax(rows, pl.row + pl.rows + 3);
        p.setPen(Qt::NoPen);
        QColor c = theme::textFaint;
        c.setAlpha(110);
        p.setBrush(c);
        for (int r = 0; r < rows; ++r)
            for (int k = 0; k <= n; ++k)
                p.drawEllipse(QPointF(m_L + k * (cw + kGridGap) - kGridGap / 2, m_y0 + r * (kCellH + kGridGap) - kGridGap / 2), 1.2, 1.2);
    }

    void dragStep(const QPointF& pos)
    {
        const QPointF d = pos - m_pressPos;
        if (m_dragId.isEmpty() && m_resId.isEmpty()) {
            if (d.x() * d.x() + d.y() * d.y() < 25) return;
            m_preview = data.grid;
            pinPositions(m_preview, flow(data.grid, m_W, presentSet()));
            if (m_grab) m_resId = m_pressId;
            else { m_dragId = m_pressId; m_dragOff = m_pressPos - m_pressRect.topLeft(); setCursor(Qt::ClosedHandCursor); }
        }
        if (!m_resId.isEmpty()) resizeStep(d); else dragTo(pos);
        if (scrollArea) {   // arrastrando cerca del borde, la página sigue
            QScrollBar* sb = scrollArea->verticalScrollBar();
            const int vy = mapTo(scrollArea->viewport(), pos.toPoint()).y();
            if (vy < 36) sb->setValue(sb->value() - 14);
            else if (vy > scrollArea->viewport()->height() - 36) sb->setValue(sb->value() + 14);
        }
        setMinimumHeight(contentHeight(width()));
        update();
    }
    void dragTo(const QPointF& pos)
    {
        const QRectF cur = m_shown.value(m_dragId);
        const QPointF tl = pos - m_dragOff;
        m_shown[m_dragId] = QRectF(tl, cur.size());
        const int n = gridCols(m_W);
        const double cw = cellW(m_W, n);
        for (auto& it : m_preview) {
            if (it.id != m_dragId) continue;
            const int c = itemCols(it, n);
            const int col = qBound(0, int(std::lround((tl.x() - m_L) / (cw + kGridGap))), n - c);
            it.x = qBound(0, int(std::lround(col * 12.0 / n)), 11);
            it.y = qMax(0, int(std::lround((tl.y() - m_y0) / (kCellH + kGridGap))));
        }
    }
    void resizeStep(const QPointF& d)
    {
        const int n = gridCols(m_W);
        const double cw = cellW(m_W, n);
        const Limits lim = limitsFor(m_resId);
        double nw = m_pressRect.width() + ((m_grab & 1) ? d.x() : 0);
        double nh = m_pressRect.height() + ((m_grab & 2) ? d.y() : 0);
        for (auto& it : m_preview) {
            if (it.id != m_resId) continue;
            if (m_grab & 1) {
                const int effMin = qMax(1, int(std::lround(lim.minC * n / 12.0)));
                const int c = qBound(effMin, int(std::lround((nw + kGridGap) / (cw + kGridGap))), n);
                it.cols = qBound(2, int(std::lround(c * 12.0 / n)), 12);
            }
            if ((m_grab & 2) && lim.minR > 0) it.rows = qBound(lim.minR, int(std::lround((nh + kGridGap) / (kCellH + kGridGap))), lim.maxR);
        }
        m_shown[m_resId] = QRectF(m_pressRect.topLeft(), QSizeF(qMax(60.0, nw), qMax(40.0, nh)));   // sigue al mouse; el casillero marca dónde va a quedar
    }
    bool stepSpring()
    {
        constexpr double dt = 0.016;
        bool moving = false;
        for (auto it = m_target.cbegin(); it != m_target.cend(); ++it) {
            if (it.key() == m_dragId || it.key() == m_resId) continue;
            QRectF& cur = m_shown[it.key()];
            auto& v = m_vel[it.key()];
            double c[4] = {cur.x(), cur.y(), cur.width(), cur.height()};
            const QRectF t = it.value();
            const double tv[4] = {t.x(), t.y(), t.width(), t.height()};
            for (int i = 0; i < 4; ++i) {
                v[i] += (kSpringK * (tv[i] - c[i]) - kSpringC * v[i]) * dt;
                c[i] += v[i] * dt;
                if (qAbs(tv[i] - c[i]) > 0.3 || qAbs(v[i]) > 6) moving = true;
                else { c[i] = tv[i]; v[i] = 0; }
            }
            cur = QRectF(c[0], c[1], qMax(1.0, c[2]), qMax(1.0, c[3]));
        }
        return moving;
    }

    void paintOne(QPainter& p, const QString& id, const QRectF& r)
    {
        if (id == QLatin1String("streak")) paintStreak(p, r);
        else if (id == QLatin1String("study")) card(p, r, data.studyLabel, fmt::hours(data.totals.study), QString(), 20, theme::text, id);
        else if (id == QLatin1String("records")) paintRecords(p, r);
        else if (id == QLatin1String("chart")) {
            m_plotH = qMax(50.0, r.height() - 24 - 40);
            paintChart(p, r.left(), r.top(), r.width());
            xButton(p, r, id);
        } else if (id == QLatin1String("folders")) {
            m_foldAvail = qMax(kRowH, r.height() - 24 - 12);
            paintFolders(p, r.left(), r.top(), r.width());
            xButton(p, r, id);
        } else {
            bool done = false;
            for (const Small& s : smallCards())
                if (s.id == id) { card(p, r, s.label, s.value, s.sub, 13, s.col, s.id); done = true; break; }
            if (!done) for (const Chip& c : buildChips()) if (c.key == id) { paintChip(p, r, c); break; }
        }
        if (data.editing) {   // borde punteado y la manijita para estirar
            p.save();
            QColor c = theme::textFaint;
            c.setAlpha(150);
            p.setPen(QPen(c, 1, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
            // flechitas para agrandar o achicar: borde derecho, borde inferior y esquina
            QColor ac = theme::text;
            ac.setAlpha(190);
            p.setPen(QPen(ac, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
            auto arrow = [&](QPointF c, QPointF d, double len) {   // flecha de dos puntas, d = dirección unitaria
                const QPointF a = c - d * (len / 2), b = c + d * (len / 2), nrm(-d.y(), d.x());
                p.drawLine(a, b);
                for (const auto& e : {QPair<QPointF, double>(a, 1.0), QPair<QPointF, double>(b, -1.0)}) {
                    p.drawLine(e.first, e.first + (d * e.second + nrm * 0.7) * 3.6);
                    p.drawLine(e.first, e.first + (d * e.second - nrm * 0.7) * 3.6);
                }
            };
            const Limits lim = limitsFor(id);
            arrow(QPointF(r.right() - 8, r.center().y()), QPointF(1, 0), 11);
            if (lim.minR > 0) {
                arrow(QPointF(r.center().x(), r.bottom() - 5), QPointF(0, 1), 9);
                arrow(QPointF(r.right() - 11, r.bottom() - 11), QPointF(0.7071, 0.7071), 11);
            }
            p.restore();
        }
    }

    // Botoncito de la barra de edición (con su zona de click).
    QRectF editPill(QPainter& p, double right, double y, const QString& t, const QString& key, bool on = false)
    {
        p.setFont(theme::uiFont(8, on ? QFont::DemiBold : QFont::Normal));
        const double w = qMax(22.0, QFontMetricsF(p.font()).horizontalAdvance(t) + 16);
        const QRectF r(right - w, y + 3, w, 22);
        p.setPen(Qt::NoPen);
        p.setBrush(on ? theme::mix(theme::surface, statsInk(), 0.45) : theme::surface);
        p.drawRoundedRect(r, 11, 11);
        p.setPen(on ? theme::text : theme::textDim);
        p.drawText(r, Qt::AlignCenter, t);
        zone(r, key, p);
        return r;
    }
    double paintEditHeader(QPainter& p, double L, double y, double W)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(QRectF(L, y, W, 34), 10, 10);
        p.setFont(theme::uiFont(9));
        p.setPen(theme::textDim);
        p.drawText(QRectF(L + 14, y, W - 300, 34), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(QStringLiteral("Arrastrá para mover · estirá de los bordes · ✕ la saca"), Qt::ElideRight, int(W - 310)));
        QRectF r = editPill(p, L + W - 8, y + 3, QStringLiteral("Listo"), QStringLiteral("lay:done"), true);
        r = editPill(p, r.left() - 6, y + 3, QStringLiteral("Restablecer"), QStringLiteral("lay:reset"));
        editPill(p, r.left() - 6, y + 3, QStringLiteral("＋ Tarjetas"), QStringLiteral("lay:add"));
        return y + 34;
    }

    // estado de la grilla
    double m_L = 16, m_y0 = 8, m_W = 600;
    QHash<QString, QRectF> m_shown, m_target;
    QHash<QString, std::array<double, 4>> m_vel;
    QVector<StatsData::Item> m_preview;
    QString m_dragId, m_resId, m_pressId;
    QPointF m_dragOff, m_pressPos;
    QRectF m_pressRect;
    int m_grab = 0;
    QTimer m_animTimer;

    void sectionTitle(QPainter& p, double x, double y, const QString& t) const
    {
        p.setFont(theme::uiFont(9, QFont::DemiBold));
        p.setPen(theme::textDim);
        p.drawText(QPointF(x, y + 13), t.toUpper());
    }

    // Mejoras 5.0: arriba de todo, tres datos con jerarquía: la racha (protagonista), lo estudiado y el foco.
    // En la vista avanzada, abajo van los datos finos (recreos, indebidos, inactivas).
    bool recordsCard() const { return data.has("records") && data.ms.streaks; }
    // En edición, cada tarjeta lleva una ✕ (zona delante de las demás: se agrega al final de paintEvent).
    QVector<Zone> pendingX;
    void xButton(QPainter& p, const QRectF& r, const QString& id)
    {
        if (!data.editing) return;
        const QRectF b(r.right() - 24, r.top() + 6, 18, 18);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::mix(theme::surface, theme::text, 0.18));
        p.drawEllipse(b);
        p.setPen(theme::text);
        p.setFont(theme::uiFont(8, QFont::DemiBold));
        p.drawText(b, Qt::AlignCenter, QStringLiteral("✕"));
        pendingX.append({b, QStringLiteral("card:hide:") + id, QString()});
    }
    int focusPct() const
    {
        const qint64 den = data.totals.study + data.totals.breaks + data.totals.undue + data.totals.paused;
        return den > 0 ? int(std::lround(100.0 * data.totals.study / den)) : -1;
    }
    void card(QPainter& p, const QRectF& r, const QString& label, const QString& value, const QString& sub,
              double valuePt, const QColor& accent, const QString& key)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(r, 10, 10);
        p.setFont(theme::uiFont(8.5));
        p.setPen(theme::textDim);
        p.drawText(r.adjusted(14, 10, -8, 0), Qt::AlignLeft | Qt::AlignTop,
                   QFontMetrics(p.font()).elidedText(label, Qt::ElideRight, int(r.width() - 22)));
        p.setFont(theme::uiFont(valuePt, QFont::DemiBold));
        p.setPen(accent);
        p.drawText(r.adjusted(14, 0, -8, sub.isEmpty() ? -10 : -26), Qt::AlignLeft | Qt::AlignBottom, value);
        if (!sub.isEmpty()) {
            p.setFont(theme::uiFont(8));
            p.setPen(theme::textFaint);
            p.drawText(r.adjusted(14, 0, -8, -9), Qt::AlignLeft | Qt::AlignBottom,
                       QFontMetrics(p.font()).elidedText(sub, Qt::ElideRight, int(r.width() - 22)));
        }
        zone(r, key, p);
        xButton(p, r, key);
    }

    void paintStreak(QPainter& p, const QRectF& sr)
    {
        // ---- racha: el número grande
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(sr, 10, 10);
        p.setFont(theme::uiFont(8.5));
        p.setPen(theme::textDim);
        p.drawText(sr.adjusted(14, 10, -8, 0), Qt::AlignLeft | Qt::AlignTop, QStringLiteral("RACHA"));
        const QString num = QString::number(data.streak.days);
        p.setFont(theme::uiFont(34, QFont::DemiBold));
        p.setPen(data.streak.days > 0 ? theme::text : theme::textDim);
        p.drawText(sr.adjusted(14, 0, 0, -8), Qt::AlignLeft | Qt::AlignBottom, num);
        const double numW = QFontMetricsF(p.font()).horizontalAdvance(num);
        p.setFont(theme::uiFont(10));
        p.setPen(theme::textDim);
        p.drawText(QRectF(sr.left() + 14 + numW + 6, sr.top(), 60, sr.height() - 14), Qt::AlignLeft | Qt::AlignBottom,
                   data.streak.days == 1 ? QStringLiteral("día") : QStringLiteral("días"));
        // columna derecha: comodines, récord y lo que pasa hoy
        QStringList side;
        side << QStringLiteral("comodines %1 de %2").arg(data.streak.jokersThisWeek).arg(data.jokers);
        if (data.ms.streaks && data.bestStreak > data.streak.days) side << QStringLiteral("récord %1").arg(data.bestStreak);
        if (data.period == StatsWidget::Period::Day && data.goals.contains(data.from)) {   // B2-3
            const auto g = data.goals.value(data.from);
            if (g.met) side << QStringLiteral("objetivo %1 cumplido").arg(fmt::hours(g.target));
            else side << QStringLiteral("objetivo %1: %2 %3").arg(fmt::hours(g.target), data.from == data.today ? QStringLiteral("faltan") : QStringLiteral("faltaron"), fmt::hours(g.missing));
        }
        if (!data.streak.todayDone) side << QStringLiteral("hoy todavía no arrancaste");
        p.setFont(theme::uiFont(8.5));
        p.setPen(theme::textDim);
        const double sx = sr.left() + 14 + numW + 62;
        const double sideW = sr.right() - 10 - sx;
        if (sideW > 70) {
            const QFontMetrics fm(p.font());
            double ty = sr.bottom() - 12 - side.size() * 15;
            for (const QString& s : side) {
                p.drawText(QRectF(sx, ty, sideW, 15), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(s, Qt::ElideRight, int(sideW)));
                ty += 15;
            }
        }
        zone(sr, QStringLiteral("streak"), p);
        xButton(p, sr, QStringLiteral("streak"));
    }
    void paintRecords(QPainter& p, const QRectF& rr)
    {
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(rr, 10, 10);
        p.setFont(theme::uiFont(8.5));
        p.setPen(theme::textDim);
        p.drawText(rr.adjusted(14, 10, -8, 0), Qt::AlignLeft | Qt::AlignTop, QStringLiteral("TUS RÉCORDS"));
        const QString dayV = data.records.bestDay > 0
            ? fmt::hours(data.records.bestDay) + (data.records.bestDayDate.isValid() ? QStringLiteral(" · ") + fmt::shortDate(data.records.bestDayDate) : QString())
            : QStringLiteral("—");
        const QString weekV = data.records.bestWeek > 0 ? fmt::hours(data.records.bestWeek) : QStringLiteral("—");
        const QString streakV = data.bestStreak > 0 ? QStringLiteral("%1 %2").arg(data.bestStreak).arg(data.bestStreak == 1 ? "día" : "días") : QStringLiteral("—");
        const QString labs[3] = {QStringLiteral("Más horas en un día"), QStringLiteral("Semana con más horas"), QStringLiteral("Racha más larga")};
        const QString vals[3] = {dayV, weekV, streakV};
        const double top = rr.top() + 30, lh = qMax(16.0, (rr.height() - 38) / 3.0);
        for (int i = 0; i < 3; ++i) {
            const QRectF row(rr.left() + 14, top + i * lh, rr.width() - 24, lh);
            p.setFont(theme::uiFont(9));
            p.setPen(theme::textDim);
            p.drawText(row, Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(labs[i], Qt::ElideRight, int(row.width() * 0.62)));
            p.setFont(theme::uiFont(10.5, QFont::DemiBold));
            p.setPen(theme::text);
            p.drawText(row, Qt::AlignRight | Qt::AlignVCenter, vals[i]);
        }
        zone(rr, QStringLiteral("records"), p);
        xButton(p, rr, QStringLiteral("records"));
    }
    struct Small { QString id, label, value, sub; QColor col; };
    QVector<Small> smallCards() const
    {
        qint64 sesSec = 0;
        for (const auto& s : data.sessions) sesSec += s.length();
        const QVector<Small> smalls = {
            {"breaks", QStringLiteral("Recreos"), fmt::hours(data.totals.breaks), QString(), theme::text},
            {"undue", QStringLiteral("Tiempo muerto"), fmt::hours(data.totals.undue), QString(), data.totals.undue > 0 ? theme::squareFinal.lighter(125) : theme::text},
            {"paused", QStringLiteral("Recreo indebido"), fmt::hours(data.totals.paused), QString(), data.totals.paused > 0 ? theme::squareFinal.lighter(125) : theme::text},
            {"inactive", QStringLiteral("Horas inactivas"), fmt::hours(data.totals.inactive), QString(), theme::textDim},
            {"sessions", QStringLiteral("Sesiones"),
             data.sessions.isEmpty() ? QStringLiteral("0") : QStringLiteral("%1 · prom. %2").arg(data.sessions.size()).arg(fmt::duration(int(sesSec / data.sessions.size()))), QString(), theme::text}};
        return smalls;
    }

    double paintChart(QPainter& p, double L, double y, double W)
    {
        switch (data.period) {
        case StatsWidget::Period::Day:  return paintTimeline(p, L, y, W);
        case StatsWidget::Period::Year: return paintHeatmap(p, L, y, W);
        default:                        return paintBars(p, L, y, W);
        }
    }

    // Día: línea de 0 a 24 h con estudio, recreos e indebidos.
    double paintTimeline(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Tu día"));
        y += 24;
        const QRectF bar(L, y, W, 26);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(bar, 5, 5);

        const qint64 dayStart = dayclock::dayStart(data.from).toSecsSinceEpoch();
        auto xAt = [&](qint64 t) { return bar.left() + qBound(0.0, double(t - dayStart) / 86400.0, 1.0) * bar.width(); };
        auto block = [&](qint64 a, qint64 b, const QColor& c) {
            const double x0 = xAt(a), x1 = qMax(x0 + 1.5, xAt(b));
            p.setBrush(c);
            p.drawRect(QRectF(x0, bar.top() + 3, x1 - x0, bar.height() - 6));
        };
        for (const Interval& i : data.intervals)
            block(i.startedAt, i.startedAt + i.seconds,
                  i.kind == IntervalKind::Break ? theme::timelineBreak : i.kind == IntervalKind::Pause ? theme::squareFinal : theme::squareFinal.darker(140));
        for (const Pomodoro& pm : data.pomos) {
            if (data.filter && !data.filter->contains(pm.folderId)) continue;
            block(pm.startedAt, qMax(pm.endedAt, pm.startedAt + pm.studiedSec), data.colorOf(pm.folderId));
        }

        p.setFont(theme::uiFont(8));
        p.setPen(theme::textFaint);
        for (int h = 0; h <= 24; h += 3) {
            const double x = bar.left() + bar.width() * h / 24.0;
            p.drawText(QRectF(x - 20, bar.bottom() + 4, 40, 14), Qt::AlignCenter,
                       QStringLiteral("%1h").arg((h + dayclock::startHour()) % 24));
        }
        // Leyenda
        const QPair<QString, QColor> legend[3] = {
            {data.colors.isEmpty() ? QStringLiteral("estudio") : QStringLiteral("estudio (color de la actividad)"), data.colors.isEmpty() ? statsInk() : theme::textDim},
            {QStringLiteral("recreo"), theme::timelineBreak},
            {QStringLiteral("indebido"), theme::squareFinal.darker(140)},
        };
        double lx = L;
        if (data.totals.study <= 0)
            emptyNote(p, bar, emptyText());
        const double ly = bar.bottom() + 24;
        for (const auto& [name, c] : legend) {
            p.setPen(Qt::NoPen);
            p.setBrush(c);
            p.drawRoundedRect(QRectF(lx, ly + 3, 9, 9), 2, 2);
            p.setPen(theme::textDim);
            p.drawText(QPointF(lx + 14, ly + 11), name);
            lx += 26 + QFontMetrics(p.font()).horizontalAdvance(name);
        }
        return ly + 16;
    }

    // Semana / mes: barras de horas por día.
    // Mejoras 4.0: letra del día debajo de cada barra, fines de semana más claros,
    // línea de promedio, y cada día es una zona (hover con detalle, click abre el día).
    double paintBars(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Horas por día"));
        y += 24;
        const int n = int(data.from.daysTo(data.to)) + 1;
        const double chartH = plotH();
        const double labelRoom = 18;   // espacio para el número arriba de la barra
        const double slot = W / n;
        const double bw = qMax(3.0, slot * 0.62);
        qint64 maxV = 3600;
        for (auto v : data.byDay) maxV = qMax(maxV, v);
        for (const auto& g : data.goals) maxV = qMax(maxV, g.target);   // B2-3: que la rayita del objetivo entre
        const QDate last = qMin(data.to, data.today);
        const int elapsed = qMax(1, int(data.from.daysTo(last)) + 1);
        qint64 sum = 0;
        for (auto it = data.byDay.cbegin(); it != data.byDay.cend(); ++it) if (it.key() <= last) sum += it.value();
        const qint64 avg = sum / elapsed;
        maxV = qMax(maxV, avg);
        p.setFont(theme::uiFont(7.5));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(L, y - 24, W, 16), Qt::AlignRight | Qt::AlignVCenter,
                   data.simple ? (data.goals.isEmpty() ? QStringLiteral("promedio %1 por día").arg(fmt::duration(int(avg)))
                                                       : QStringLiteral("—  objetivo del día     ●  cumplido     promedio %1 por día").arg(fmt::duration(int(avg))))
                   : data.goals.isEmpty() ? QStringLiteral("- - promedio por día")
                                          : QStringLiteral("—  objetivo del día     ●  cumplido     - -  promedio por día"));

        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(L, y + chartH), QPointF(L + W, y + chartH));

        static const char* letters = "LMMJVSD";
        for (int i = 0; i < n; ++i) {
            const QDate d = data.from.addDays(i);
            const qint64 v = data.byDay.value(d);
            const double h = (chartH - labelRoom) * double(v) / maxV;
            const QRectF r(L + i * slot + (slot - bw) / 2, y + chartH - h, bw, h);
            const bool weekend = d.dayOfWeek() >= 6;
            p.setPen(Qt::NoPen);
            // M35: barras en el color del tema; hoy, a pleno; fin de semana, más suave
            const double k = weekend ? 0.34 : 0.58;
            p.setBrush(d == data.today ? statsInk() : theme::mix(theme::surface, statsInk(), k));
            if (data.goals.value(d).met) p.setBrush(d == data.today ? theme::breakAccent : theme::mix(theme::surface, theme::breakAccent, weekend ? 0.5 : 0.78));
            if (v > 0) p.drawRoundedRect(r, 2, 2);
            if (data.goals.contains(d)) {   // B2-3: rayita a la altura del objetivo
                const double gy = y + chartH - (chartH - labelRoom) * double(data.goals.value(d).target) / maxV;
                p.setPen(QPen(theme::textDim, 1.5, Qt::SolidLine, Qt::FlatCap));
                p.drawLine(QPointF(r.left() - 3, gy), QPointF(r.right() + 3, gy));
                p.setPen(Qt::NoPen);
            }

            p.setFont(theme::uiFont(8));
            if (v > 0 && slot > 30) {
                p.setPen(theme::textDim);
                p.drawText(QRectF(r.left() - 10, r.top() - 16, bw + 20, 14), Qt::AlignCenter, fmt::duration(v));
            }
            // Abajo: la letra del día y, debajo, el número (todos si entran; si no, cada 5).
            const bool isToday = d == data.today;
            p.setPen(isToday ? theme::text : (weekend ? theme::textDim : theme::textFaint));
            const QRectF col(L + i * slot - 6, y + chartH + 3, slot + 12, 13);
            if (data.period == StatsWidget::Period::Week) {
                p.drawText(col, Qt::AlignCenter, fmt::shortDate(d));
            } else {
                if (slot >= 9) p.drawText(col, Qt::AlignCenter, QString(QLatin1Char(letters[d.dayOfWeek() - 1])));
                const bool number = slot >= 17 || d.day() == 1 || d.day() % 5 == 0;
                if (number) p.drawText(col.translated(0, 13), Qt::AlignCenter, QString::number(d.day()));
            }
            zone(QRectF(L + i * slot, y - 4, slot, chartH + 36), QStringLiteral("day:") + fmt::dayKey(d), p);
        }
        // Línea de promedio (punteada)
        if (avg > 0 && !data.simple) {   // Mejoras 4.1: la línea solo en la vista avanzada
            const double ay = y + chartH - (chartH - labelRoom) * double(avg) / maxV;
            p.setPen(QPen(theme::textDim, 1, Qt::DashLine));
            p.drawLine(QPointF(L, ay), QPointF(L + W, ay));
            p.setFont(theme::uiFont(7.5));
            p.setPen(theme::textDim);
            p.drawText(QRectF(L, ay - 14, 120, 12), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("prom. ") + fmt::duration(int(avg)));
        }
        if (data.totals.study <= 0)   // M36: estado vacío
            emptyNote(p, QRectF(L, y, W, chartH - 10), emptyText());
        return y + chartH + (data.period == StatsWidget::Period::Week ? 22 : 36);
    }

    // Mejoras 4.0: valor de un día para el mapa de calor, según lo que se esté mirando.
    // Devuelve <0 si ese día no cuenta (sin objetivo, en el caso de "objetivo").
    double heatValue(const QDate& d) const
    {
        const insights::DayStat s = data.daily.value(d);
        const QString m = data.heatMetric;
        if (m == QLatin1String("pomos"))     return s.pomos;
        if (m == QLatin1String("undue"))     return double(s.undue);
        if (m == QLatin1String("paused"))    return double(s.paused);
        if (m == QLatin1String("inactive"))  return double(s.inactive);
        if (m == QLatin1String("goal")) {
            if (!data.goals.contains(d)) return -1;
            return data.goals.value(d).met ? 1.0 : 0.0;
        }
        return double(data.byDay.value(d));   // hours
    }
    QColor heatInk() const
    {
        const QString m = data.heatMetric;
        if (m == QLatin1String("goal"))     return theme::breakAccent;
        if (m == QLatin1String("undue") || m == QLatin1String("paused")) return theme::squareFinal;
        if (m == QLatin1String("inactive")) return theme::mix(theme::surface, theme::text, 0.8);
        return statsInk();
    }
    static QString heatName(const QString& m)
    {
        if (m == QLatin1String("pomos")) return QStringLiteral("Pomodoros");
        if (m == QLatin1String("goal")) return QStringLiteral("Objetivo");
        if (m == QLatin1String("undue")) return QStringLiteral("Tiempo muerto");
        if (m == QLatin1String("paused")) return QStringLiteral("Recreo indebido");
        if (m == QLatin1String("inactive")) return QStringLiteral("Inactivo");
        return QStringLiteral("Horas");
    }

    // Año: mapa de calor, una celda por día (columnas = semanas).
    // Mejoras 4.0: la escala se reparte por cuartiles de tus propios días (los más
    // fuertes se notan), se elige qué mostrar (horas, pomodoros, objetivo, indebido,
    // inactivo), el objetivo cumplido es un puntito (no un recuadro verde) y cada día
    // tiene hover con sus datos y click para abrirlo.
    double paintHeatmap(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Mapa de calor"));
        // Selector de qué se colorea (a la derecha del título; solo en la vista avanzada)
        if (!data.simple) {
            static const char* metrics[] = {"hours", "pomos", "goal", "undue", "paused", "inactive"};
            p.setFont(theme::uiFont(8, QFont::DemiBold));
            double x = L + W;
            for (int k = 5; k >= 0; --k) {
                const QString name = heatName(QString::fromLatin1(metrics[k]));
                const double w = QFontMetricsF(p.font()).horizontalAdvance(name) + 18;
                x -= w;
                const QRectF pill(x, y - 1, w - 4, 20);
                const bool on = data.heatMetric == QLatin1String(metrics[k]);
                p.setPen(on ? Qt::NoPen : QPen(theme::line, 1));
                if (on) p.setBrush(theme::mix(theme::surface, heatInk(), 0.45)); else p.setBrush(Qt::NoBrush);
                p.drawRoundedRect(pill, 10, 10);
                p.setPen(on ? theme::text : theme::textDim);
                p.drawText(pill, Qt::AlignCenter, name);
                zone(pill, QStringLiteral("hm:") + QString::fromLatin1(metrics[k]), p);
            }
        }
        y += 30;
        const double cell = heatCell(W);
        const double step = cell + 3;
        const QDate start = fmt::mondayOf(data.from);
        const double left = L + 26;

        p.setFont(theme::uiFont(7.5));
        p.setPen(theme::textFaint);
        const char* rows[] = {"lun", "", "mié", "", "vie", "", ""};
        for (int r = 0; r < 7; ++r)
            p.drawText(QRectF(L, y + r * step, 24, cell), Qt::AlignLeft | Qt::AlignVCenter, QString::fromUtf8(rows[r]));

        // Cortes por cuartiles de los días que tienen algo (así siempre hay contraste).
        QVector<double> vals;
        const QDate last = qMin(data.to, data.today);
        for (QDate d = data.from; d <= last; d = d.addDays(1)) { const double v = heatValue(d); if (v > 0) vals << v; }
        std::sort(vals.begin(), vals.end());
        auto cut = [&](double q) { return vals.isEmpty() ? 0.0 : vals[qMin<int>(int(vals.size()) - 1, int(q * vals.size()))]; };
        const double c1 = cut(0.20), c2 = cut(0.40), c3 = cut(0.60), c4 = cut(0.80);
        const QColor ink = heatInk();
        const bool goalMode = data.heatMetric == QLatin1String("goal");
        auto level = [&](double v) { return v <= 0 ? -1 : v < c1 ? 0 : v < c2 ? 1 : v < c3 ? 2 : v < c4 ? 3 : 4; };
        auto shade = [&](int lv) { return theme::mix(theme::surface, ink, 0.28 + 0.18 * lv); };

        int lastMonth = -1;
        for (int w = 0; w < 54; ++w) {
            for (int r = 0; r < 7; ++r) {
                const QDate d = start.addDays(w * 7 + r);
                if (d < data.from || d > data.to) continue;
                const QRectF c(left + w * step, y + r * step, cell, cell);
                const double v = d > data.today ? -1 : heatValue(d);
                QColor fill;
                if (d > data.today) fill = theme::bg.lighter(115);
                else if (goalMode) fill = v < 0 ? theme::surface : (v >= 1 ? theme::mix(theme::surface, ink, 0.9) : theme::mix(theme::surface, theme::squareFinal, 0.28));
                else fill = v > 0 ? shade(level(v)) : theme::surface;
                p.setPen(d == data.today ? QPen(theme::text, 1.4) : Qt::NoPen);
                p.setBrush(fill);
                p.drawRoundedRect(c, 2, 2);
                // objetivo cumplido: puntito (en el modo "objetivo" ya es el color de la celda)
                if (!goalMode && cell >= 8 && d <= data.today && data.goals.value(d).met) {
                    p.setPen(Qt::NoPen);
                    p.setBrush(QColor(255, 255, 255, 215));
                    p.drawEllipse(c.center(), cell * 0.17, cell * 0.17);
                }
                if (d <= data.today) zone(c, QStringLiteral("day:") + fmt::dayKey(d), p);
                if (r == 0 || d.day() == 1) {
                    if (d.month() != lastMonth && d.day() <= 7) {
                        lastMonth = d.month();
                        p.setPen(theme::textFaint);
                        p.setFont(theme::uiFont(7.5));
                        p.drawText(QPointF(left + w * step, y + 7 * step + 12), fmt::monthYear(d).left(3));
                    }
                }
            }
        }
        // Leyenda: qué significa cada cosa
        const double ly = y + 7 * step + 16;
        p.setFont(theme::uiFont(7.5));
        p.setPen(theme::textFaint);
        if (goalMode) {
            p.drawText(QRectF(L, ly, W, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("color pleno = objetivo cumplido · rojizo = no cumplido · gris = ese día no había objetivo"));
        } else {
            bool any = false;
            for (auto it = data.goals.cbegin(); it != data.goals.cend(); ++it) any |= it->met;
            if (any) {
                p.setPen(Qt::NoPen);
                p.setBrush(QColor(255, 255, 255, 215));
                p.drawEllipse(QPointF(L + 4, ly + 7), 2.6, 2.6);
                p.setPen(theme::textFaint);
                p.drawText(QRectF(L + 12, ly, 150, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("objetivo cumplido"));
            }
            const double lw = 14, lg = 3;
            double x = L + W - (5 * (lw + lg) + 62);
            p.drawText(QRectF(x - 34, ly, 32, 14), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("menos"));
            for (int k = 0; k < 5; ++k) {
                p.setPen(Qt::NoPen);
                p.setBrush(shade(k));
                p.drawRoundedRect(QRectF(x + k * (lw + lg), ly + 1, lw, 12), 2, 2);
            }
            p.setPen(theme::textFaint);
            p.drawText(QRectF(x + 5 * (lw + lg) + 2, ly, 40, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("más"));
        }
        if (data.totals.study <= 0)
            emptyNote(p, QRectF(left, y, 53 * step, 7 * step), emptyText());
        return y + 7 * step + 32;
    }

    // M36: texto de "no hay nada" centrado sobre una pastilla, sin tapar el gráfico.
    QString emptyText() const
    {
        if (data.from > data.today) return QStringLiteral("Este período todavía no llegó");
        if (data.filter) return QStringLiteral("Sin horas de esta actividad en este período");
        switch (data.period) {
        case StatsWidget::Period::Day:  return data.from == data.today ? QStringLiteral("Hoy todavía no hay pomodoros") : QStringLiteral("Sin pomodoros este día");
        case StatsWidget::Period::Week: return QStringLiteral("Sin horas esta semana");
        case StatsWidget::Period::Month: return QStringLiteral("Sin horas este mes");
        default:                        return QStringLiteral("Sin horas este año");
        }
    }
    void emptyNote(QPainter& p, const QRectF& area, const QString& text)
    {
        p.setFont(theme::uiFont(9));
        const QFontMetricsF fm(p.font());
        const double w = fm.horizontalAdvance(text) + 28, h = 26;
        const QRectF pill(area.center().x() - w / 2, area.center().y() - h / 2, w, h);
        p.setPen(QPen(theme::line, 1));
        p.setBrush(theme::bg);
        p.drawRoundedRect(pill, h / 2, h / 2);
        p.setPen(theme::textDim);
        p.drawText(pill, Qt::AlignCenter, text);
    }

    // Mejoras 5.0: las actividades ocupan poco (6 filas) y el resto se recorre con scroll; se puede elegir cuáles se ven.
    QVector<const StatsData::Row*> visibleRows() const
    {
        QVector<const StatsData::Row*> v;
        int hideDepth = -1;   // si se saca una actividad, sus subactividades también
        for (const auto& r : data.rows) {
            if (hideDepth >= 0 && r.depth > hideDepth) continue;
            hideDepth = -1;
            if (r.id && data.hiddenRows.contains(r.id)) { hideDepth = r.depth; continue; }
            v << &r;
        }
        return v;
    }
    double foldersBodyH() const
    {
        const int n = visibleRows().size();
        return qMax(kRowH, qMin(n * kRowH, m_foldAvail));
    }
    double m_foldScroll = 0, m_foldMax = 0;
    QRectF m_foldRect;

    void wheelEvent(QWheelEvent* e) override
    {
        if (m_foldMax > 0 && m_foldRect.contains(e->position())) {
            const double next = qBound(0.0, m_foldScroll - e->angleDelta().y() / 120.0 * kRowH, m_foldMax);
            if (next != m_foldScroll) { m_foldScroll = next; update(); e->accept(); return; }
        }
        e->ignore();   // al borde, la rueda sigue con la página
    }

    void paintFolders(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Actividades"));
        {   // selector de cuáles se muestran
            const int hidden = data.hiddenRows.size();
            const QString t = hidden ? QStringLiteral("Mostrar: %1 de %2 ▾").arg(data.rows.size() - hidden).arg(data.rows.size()) : QStringLiteral("Mostrar ▾");
            p.setFont(theme::uiFont(8));
            const double tw = QFontMetricsF(p.font()).horizontalAdvance(t) + 16;
            const QRectF pill(L + W * 0.42 - tw / 2 - 60, y - 1, tw, 18);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::surface);
            p.drawRoundedRect(pill, 9, 9);
            p.setPen(theme::textDim);
            p.drawText(pill, Qt::AlignCenter, t);
            zone(pill, QStringLiteral("folders:pick"), p);
        }
        p.setFont(theme::uiFont(8));
        p.setPen(theme::textFaint);
        const double cPeriod = L + W * 0.42, cTotal = L + W * 0.56, cBar = L + W * 0.70;
        p.drawText(QRectF(cPeriod - 60, y, 60 + 40, 16), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("período"));
        p.drawText(QRectF(cTotal - 60, y, 60 + 40, 16), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("total"));
        p.drawText(QRectF(cBar + 10, y, W, 16), Qt::AlignLeft | Qt::AlignVCenter,
                   data.ms.stages ? QStringLiteral("etapa · camino a las 10.000 h") : QStringLiteral("camino a las 10.000 h"));
        y += 24;

        if (data.rows.isEmpty()) {
            p.setPen(theme::textFaint);
            p.setFont(theme::uiFont(9));
            p.drawText(QPointF(L, y + 14), QStringLiteral("Todavía no hay actividades: creá una con la tecla A o en Ajustes › General"));
            return;
        }
        const auto vis = visibleRows();
        if (vis.isEmpty()) {
            p.setPen(theme::textFaint);
            p.setFont(theme::uiFont(9));
            p.drawText(QPointF(L, y + 14), QStringLiteral("Sacaste todas las actividades de la lista (botón Mostrar)."));
            return;
        }
        const double bodyH = foldersBodyH();
        m_foldRect = QRectF(L, y, W, bodyH);
        m_foldMax = qMax(0.0, vis.size() * kRowH - bodyH);
        m_foldScroll = qBound(0.0, m_foldScroll, m_foldMax);
        p.save();
        p.setClipRect(m_foldRect);
        y -= m_foldScroll;
        for (const StatsData::Row* rp : vis) {
            const StatsData::Row& row = *rp;
            const QRectF r(L, y, W, kRowH);
            p.setFont(theme::uiFont(9.5, row.depth == 0 ? QFont::DemiBold : QFont::Normal));
            p.setPen(row.depth == 0 ? theme::text : theme::textDim);
            const double indent = row.depth * 16 + (data.colors.isEmpty() ? 0 : 14);
            if (!data.colors.isEmpty() && row.id) {   // M35: puntito de color
                p.setPen(Qt::NoPen);
                p.setBrush(data.colorOf(row.id));
                p.drawEllipse(QRectF(L + row.depth * 16 + 1, y + kRowH / 2 - 4, 8, 8));
                p.setPen(row.depth == 0 ? theme::text : theme::textDim);
            }
            p.drawText(QRectF(L + indent, y, cPeriod - 70 - L - indent, kRowH), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(row.name, Qt::ElideRight, int(cPeriod - 70 - L - indent)));
            p.setFont(theme::uiFont(9.5));
            p.setPen(row.period > 0 ? theme::text : theme::textFaint);
            p.drawText(QRectF(cPeriod - 60, y, 100, kRowH), Qt::AlignRight | Qt::AlignVCenter, fmt::hours(row.period));
            p.setPen(theme::textDim);
            p.drawText(QRectF(cTotal - 60, y, 100, kRowH), Qt::AlignRight | Qt::AlignVCenter, fmt::hours(row.total));

            // Barra hacia 10.000 h + próximo aviso (M44: o hacia el próximo subnivel de la etapa)
            const double textW = data.ms.stages ? 160 : 130;
            const double bx = cBar + 10, bw = L + W - bx - textW - 6;
            const ms::Stage stage = ms::stageFor(row.total);
            const double frac = data.ms.stages ? stage.progress : qBound(0.0, row.total / (10000.0 * 3600.0), 1.0);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::surface);
            p.drawRoundedRect(QRectF(bx, y + kRowH / 2 - 3, bw, 6), 3, 3);
            p.setBrush(row.id ? data.colorOf(row.id) : theme::squareFill);
            p.drawRoundedRect(QRectF(bx, y + kRowH / 2 - 3, qMax(frac > 0 ? 3.0 : 0.0, bw * frac), 6), 3, 3);

            p.setFont(theme::uiFont(8));
            p.setPen(theme::textFaint);
            if (data.ms.stages) {
                p.setPen(row.total > 0 ? theme::textDim : theme::textFaint);
                p.drawText(QRectF(bx + bw + 8, y, textW, kRowH), Qt::AlignLeft | Qt::AlignVCenter,
                           stage.toSec ? QStringLiteral("%1 · %2").arg(stage.label, roundHours(stage.toSec)) : stage.label);
            } else {
                const qint64 h = row.total / 3600;
                const auto marks = ms::hourMarks();
                qint64 next = marks.last();
                for (int m : marks) if (m > h) { next = m; break; }
                p.drawText(QRectF(bx + bw + 8, y, 130, kRowH), Qt::AlignLeft | Qt::AlignVCenter,
                           QStringLiteral("%1 %  · próx. %2 h").arg(QLocale().toString(frac * 100, 'f', frac < 0.1 ? 2 : 1)).arg(next));
            }
            y += kRowH;
        }
        p.restore();
        if (m_foldMax > 0) {   // barrita de scroll
            const double th = qMax(24.0, bodyH * bodyH / (vis.size() * kRowH));
            const double ty = m_foldRect.top() + (bodyH - th) * (m_foldScroll / m_foldMax);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::line);
            p.drawRoundedRect(QRectF(L + W + 6, ty, 3, th), 1.5, 1.5);
        }
    }
};

// ===========================================================================
// StatsWidget
// ===========================================================================
StatsWidget::StatsWidget(Database* db, QWidget* parent)
    : QWidget(parent)
    , m_db(db)
    , m_anchor(dayclock::today())
{
    auto* back = new QToolButton(this);
    back->setIcon(icons::get(icons::Id::Back));
    back->setToolTip(QStringLiteral("Volver a la grilla"));
    connect(back, &QToolButton::clicked, this, &StatsWidget::backRequested);

    auto* title = new QLabel(QStringLiteral("Estadísticas"), this);
    title->setProperty("heading", true);

    m_periodGroup = new QButtonGroup(this);
    auto* seg = new QHBoxLayout;
    seg->setSpacing(0);
    const QString names[] = {QStringLiteral("Día"), QStringLiteral("Semana"), QStringLiteral("Mes"), QStringLiteral("Año"), QStringLiteral("Evolución")};
    for (int i = 0; i < 5; ++i) {
        auto* b = new QPushButton(names[i], this);
        b->setCheckable(true);
        b->setMinimumWidth(b->fontMetrics().horizontalAdvance(names[i]) + 26);
        b->setProperty("segment", i == 0 ? "first" : (i == 4 ? "last" : "mid"));
        m_periodGroup->addButton(b, i);
        seg->addWidget(b);
    }
    m_periodGroup->button(int(m_period))->setChecked(true);
    connect(m_periodGroup, &QButtonGroup::idClicked, this, [this](int id) {
        fade::cross(m_canvas);   // M36
        m_period = Period(id);
        refresh();
    });

    auto* prev = m_prev = new QToolButton(this);
    prev->setIcon(icons::get(icons::Id::ChevronLeft));
    auto* next = m_next = new QToolButton(this);
    next->setIcon(icons::get(icons::Id::ChevronRight));
    connect(prev, &QToolButton::clicked, this, [this] { fade::cross(m_canvas, 150); shift(-1); });
    connect(next, &QToolButton::clicked, this, [this] { fade::cross(m_canvas, 150); shift(+1); });
    m_label = new QLabel(this);
    m_label->setMinimumWidth(120);
    m_label->setAlignment(Qt::AlignCenter);

    m_folder = new QComboBox(this);
    m_folder->setMinimumWidth(130);
    connect(m_folder, &QComboBox::currentIndexChanged, this, [this](int) { fade::cross(m_canvas, 150); refresh(); });

    auto* top = new QHBoxLayout;
    top->setContentsMargins(10, 8, 12, 4);
    top->addWidget(back);
    top->addWidget(title);
    top->addSpacing(14);
    top->addLayout(seg);
    top->addStretch();
    top->addWidget(prev);
    top->addWidget(m_label);
    top->addWidget(next);
    top->addSpacing(10);
    top->addWidget(m_folder);

    // Mejoras 7.0: exportar vive en el menú ⋯ de la barra principal.

    // Mejoras 4.1: vista simple / avanzada + ayuda de cómo ver el detalle (click, no hover)
    m_viewBtn = new QPushButton(this);
    m_viewBtn->setCursor(Qt::PointingHandCursor);
    m_viewBtn->setToolTip(QStringLiteral("Vista de estadísticas: simple o avanzada (click para cambiar)"));
    connect(m_viewBtn, &QPushButton::clicked, this, [this] {
        m_db->setSetting("stats_advanced", m_db->setting("stats_advanced", 0).toBool() ? 0 : 1);
        fade::cross(m_canvas, 150);
        refresh();
    });
    auto* info = new QToolButton(this);
    info->setText(QStringLiteral("ⓘ"));
    info->setCheckable(true);
    info->setChecked(m_db->setting("stats_hover", 0).toBool());
    info->setToolTip(QStringLiteral("Mostrar la info extra al pasar el mouse (apagado: solo con click)"));
    auto applyHover = [this](bool on) { m_canvas->hoverTips = on; if (m_evo) m_evo->setHoverTips(on); };
    connect(info, &QToolButton::toggled, this, [this, applyHover](bool on) {
        m_db->setSetting("stats_hover", on ? 1 : 0);
        applyHover(on);
        emit message(on ? QStringLiteral("Info al pasar el mouse: activada") : QStringLiteral("Info al pasar el mouse: apagada"));
    });
    // Mejoras 5.0: el mes puede ser el calendario o los últimos 30 días.
    m_monthBtn = new QPushButton(this);
    m_monthBtn->setCursor(Qt::PointingHandCursor);
    m_monthBtn->setToolTip(QStringLiteral("Mes del calendario o últimos 30 días"));
    connect(m_monthBtn, &QPushButton::clicked, this, [this] {
        m_db->setSetting("month_mode", rolling() ? QStringLiteral("calendar") : QStringLiteral("rolling"));
        if (rolling()) m_anchor = dayclock::today();   // los 30 días terminan hoy
        fade::cross(m_canvas, 150);
        refresh();
    });
    top->insertWidget(top->indexOf(m_folder), m_monthBtn);
    top->insertWidget(top->indexOf(m_folder) + 1, m_viewBtn);

    top->addWidget(info);

    m_canvas = new StatsCanvas(this);
    m_canvas->tipFor = [this](const QString& key) { return tipFor(key); };
    m_canvas->onZone = [this](const QString& key) { showDetail(key); };
    m_canvas->onGrid = [this](const QVector<StatsData::Item>& g) {
        m_db->setSetting(gridKey(!m_db->setting("stats_advanced", 0).toBool()), serializeGrid(g));
    };
    m_detail = new DetailCard(this);
    auto* scroll = m_scroll = new QScrollArea(this);
    scroll->setWidget(m_canvas);
    m_canvas->scrollArea = scroll;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);
    lay->addLayout(top);
    lay->addWidget(scroll, 1);
    // Mejoras 4.0: la vista Evolución, en su propio scroll
    m_evo = new EvolutionView(this);
    { const bool hv = m_db->setting("stats_hover", 0).toBool(); m_canvas->hoverTips = hv; m_evo->setHoverTips(hv); }
    m_evoScroll = new QScrollArea(this);
    m_evoScroll->setWidget(m_evo);
    m_evoScroll->setWidgetResizable(true);
    m_evoScroll->setFrameShape(QFrame::NoFrame);
    m_evoScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_evoScroll->hide();
    lay->addWidget(m_evoScroll, 1);
}

void StatsWidget::resetToToday()
{
    m_anchor = dayclock::today();
}

QDate StatsWidget::periodStart() const
{
    switch (m_period) {
    case Period::Day:   return m_anchor;
    case Period::Week:  return fmt::mondayOf(m_anchor);
    case Period::Month: return rolling() ? m_anchor.addDays(-29) : QDate(m_anchor.year(), m_anchor.month(), 1);
    case Period::Year:  return QDate(m_anchor.year(), 1, 1);
    case Period::Evolution: return m_anchor;
    }
    return m_anchor;
}

QDate StatsWidget::periodEnd() const
{
    switch (m_period) {
    case Period::Day:   return m_anchor;
    case Period::Week:  return fmt::mondayOf(m_anchor).addDays(6);
    case Period::Month: return rolling() ? m_anchor : QDate(m_anchor.year(), m_anchor.month(), m_anchor.daysInMonth());
    case Period::Year:  return QDate(m_anchor.year(), 12, 31);
    case Period::Evolution: return m_anchor;
    }
    return m_anchor;
}

QString StatsWidget::periodLabel() const
{
    switch (m_period) {
    case Period::Day:   return fmt::longDate(m_anchor);
    case Period::Week:  return fmt::weekRange(fmt::mondayOf(m_anchor));
    case Period::Month:
        if (rolling()) {   // Mejoras 5.0: últimos 30 días hasta la fecha elegida
            const QLocale es(QLocale::Spanish);
            return QStringLiteral("30 días · %1 – %2").arg(es.toString(m_anchor.addDays(-29), QStringLiteral("d MMM")).remove('.'), es.toString(m_anchor, QStringLiteral("d MMM")).remove('.'));
        }
        return fmt::monthYear(m_anchor);
    case Period::Year:  return QString::number(m_anchor.year());
    case Period::Evolution: return QStringLiteral("Evolución");
    }
    return {};
}

void StatsWidget::shift(int dir)
{
    switch (m_period) {
    case Period::Day:   m_anchor = m_anchor.addDays(dir); break;
    case Period::Week:  m_anchor = m_anchor.addDays(7 * dir); break;
    case Period::Month: m_anchor = rolling() ? m_anchor.addDays(30 * dir) : m_anchor.addMonths(dir); break;
    case Period::Year:  m_anchor = m_anchor.addYears(dir); break;
    case Period::Evolution: return;
    }
    refresh();
}

void StatsWidget::step(int dir)
{
    fade::cross(m_canvas, 150);
    shift(dir);
}

void StatsWidget::exportReport()
{
    const QString base = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString parent = QFileDialog::getExistingDirectory(this, QStringLiteral("¿Dónde guardo el informe? (se crea una carpeta)"), base);
    if (parent.isEmpty()) return;
    const QDateTime now = QDateTime::currentDateTime();
    const QString dir = QDir(parent).filePath(QStringLiteral("UltimateTracker informe %1").arg(now.toString(QStringLiteral("yyyy-MM-dd"))));
    const QString err = report::writeAll(*m_db, dir, now);
    if (!err.isEmpty()) { emit message(err); return; }
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir(dir).filePath(QStringLiteral("informe.html"))));
    emit message(QStringLiteral("Informe guardado en la carpeta «%1»").arg(QFileInfo(dir).fileName()));
}

void StatsWidget::exportCsv(bool everything)
{
    const FolderTree tree(m_db->folders());
    const QDate today = dayclock::logicalDate(QDateTime::currentDateTime());
    QDate from = periodStart(), to = qMin(periodEnd(), today);
    QSet<qint64> filter;
    const qint64 filterId = m_folder->currentData().toLongLong();
    if (everything) {
        from = QDate(2000, 1, 1);
        to = today;
    } else if (filterId) {
        filter = tree.subtree(filterId);
    }
    QString name;
    if (everything) name = QStringLiteral("historial");
    else switch (m_period) {
        case Period::Day:   name = from.toString(Qt::ISODate); break;
        case Period::Week:  name = QStringLiteral("semana %1").arg(from.toString(Qt::ISODate)); break;
        case Period::Month: name = from.toString(QStringLiteral("yyyy-MM")); break;
        case Period::Year:  name = from.toString(QStringLiteral("yyyy")); break;
    }
    if (!everything && filterId && tree.find(filterId)) name += QStringLiteral(" ") + tree.find(filterId)->name;
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Exportar a CSV"),
                                                      dir + QStringLiteral("/UltimateTracker %1.csv").arg(name),
                                                      QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return;
    const QString csv = csvexport::build(m_db->pomodorosBetween(from, to), m_db->intervalsBetween(from, to), tree,
                                         filter.isEmpty() ? nullptr : &filter, filter.isEmpty());
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit message(QStringLiteral("No pude guardar el archivo"));
        return;
    }
    f.write(csv.toUtf8());
    f.close();
    emit message(QStringLiteral("Exportado: %1").arg(QFileInfo(path).fileName()));
}

void StatsWidget::rebuildFolderCombo()
{
    const qint64 current = m_folder->currentData().toLongLong();
    const FolderTree tree(m_db->folders());
    QSignalBlocker block(m_folder);
    m_folder->clear();
    m_folder->addItem(QStringLiteral("Todas las actividades"), qint64(0));
    std::function<void(qint64, int)> add = [&](qint64 parent, int depth) {
        for (qint64 id : tree.children(parent)) {
            m_folder->addItem(QString(depth * 3, QChar(' ')) + tree.find(id)->name, id);
            add(id, depth + 1);
        }
    };
    add(0, 0);
    const int idx = m_folder->findData(current);
    m_folder->setCurrentIndex(idx >= 0 ? idx : 0);
}

void StatsWidget::refresh()
{
    rebuildFolderCombo();
    m_periodGroup->button(int(m_period))->setChecked(true);
    m_label->setText(periodLabel());

    const QDateTime now = QDateTime::currentDateTime();
    const FolderTree tree(m_db->folders());
    const qint64 filterId = m_folder->currentData().toLongLong();

    // Mejoras 4.0: la vista Evolución tiene su propio lienzo (sin flechas de período).
    m_monthBtn->setVisible(m_period == Period::Month);
    m_monthBtn->setText(rolling() ? QStringLiteral("30 días") : QStringLiteral("Calendario"));
    const bool advanced = m_db->setting("stats_advanced", 0).toBool();
    m_viewBtn->setText(advanced ? QStringLiteral("Avanzada") : QStringLiteral("Simple"));
    m_periodGroup->button(int(Period::Evolution))->setVisible(advanced);   // la Evolución es de la vista avanzada
    if (!advanced && m_period == Period::Evolution) { m_period = Period::Week; m_periodGroup->button(int(m_period))->setChecked(true); m_label->setText(periodLabel()); }
    const bool evo = m_period == Period::Evolution;
    m_scroll->setVisible(!evo);
    m_evoScroll->setVisible(evo);
    m_prev->setVisible(!evo);
    m_next->setVisible(!evo);
    if (evo) {
        QSet<qint64> sub;
        if (filterId) sub = tree.subtree(filterId);
        m_label->setText(QStringLiteral("Evolución en el tiempo"));
        m_evo->reload(m_db, filterId ? &sub : nullptr);
        return;
    }

    StatsData& d = m_canvas->data;
    d = StatsData{};
    d.period = m_period;
    d.from = periodStart();
    d.to = periodEnd();
    d.today = dayclock::logicalDate(now);
    d.jokers = m_jokers;
    if (filterId) {
        d.filterSet = tree.subtree(filterId);
        d.filter = &d.filterSet;
    }
    d.studyLabel = (d.filter ? kinds::of(tree, filterId) : kinds::general(tree)).name;
    d.totals = stats::totals(*m_db, d.from, d.to, d.filter, now);
    d.byDay = stats::studyByDay(*m_db, d.from, d.to, d.filter);
    d.sessions = insights::sessions(*m_db, d.from, qMin(d.to, now.date().addDays(1)), d.filter);
    d.simple = !m_db->setting("stats_advanced", 0).toBool();
    migrateGrid();
    d.grid = parseGrid(m_db->setting(gridKey(d.simple)).toString());
    if (!m_db->setting("stats_paused_migrated", 0).toBool()) {   // beta 19: la tarjeta nueva aparece junto a «Tiempo muerto»
        for (bool sm : {true, false}) {
            const QString v = m_db->setting(cardsKey(sm), "").toString();
            if (!v.isEmpty() && v != QLatin1String("none") && v.contains(QLatin1String("undue"))) m_db->setSetting(cardsKey(sm), v + QStringLiteral(",paused"));
        }
        m_db->setSetting("stats_paused_migrated", 1);
    }
    d.cards = parseCards(m_db->setting(cardsKey(d.simple), "").toString(), d.simple);
    d.editing = m_editing && m_period != Period::Evolution;
    d.heatMetric = d.simple ? QStringLiteral("hours") : m_db->setting("stats/heat", "hours").toString();
    if (m_period != Period::Day) {
        d.daily = insights::daily(*m_db, d.from, d.to, d.filter, now);
        d.diary = m_db->dayDiaries(d.from, d.to);
    }
    if (m_period == Period::Day) {
        d.pomos = m_db->pomodorosBetween(d.from, d.to);
        d.intervals = m_db->intervalsBetween(d.from, d.to);
    }
    d.colors = ActivitiesWidget::colorsFor(m_db, tree);
    // B2-3: objetivos del período (hasta hoy). Las reglas guardan el pasado tal cual
    // (cambiar una que se repite corta la vieja el día anterior), así que se resuelven acá.
    {
        const QVector<GoalRule> rules = m_db->goals();
        const int pomoMin = m_db->setting("work_min", 25).toInt();
        const QDate last = qMin(d.to, d.today);
        if (!rules.isEmpty() && d.from <= last) {
            QHash<QDate, QHash<qint64, qint64>> by;
            for (const Pomodoro& p : m_db->pomodorosBetween(d.from, last)) by[p.day][p.folderId] += p.studiedSec;
            for (QDate day = d.from; day <= last; day = day.addDays(1)) {
                const auto rule = goals::resolve(rules, day);
                if (!rule) continue;
                const auto parsed = goals::parse(rule->spec, pomoMin);
                if (!parsed.ok) continue;
                const auto fills = goals::fillByActivity(parsed, by.value(day), tree);
                StatsData::GoalDay g;
                g.target = qint64(parsed.totalMin) * 60;
                for (qint64 v : by.value(day)) g.studied += v;
                for (int i = 0; i < fills.size(); ++i) g.missing += qint64((1.0 - qMin(1.0, fills[i])) * parsed.blocks[i] * 60);
                if (parsed.timeMode) g.missing = qMax<qint64>(0, g.target - g.studied);
                g.met = parsed.timeMode ? g.studied >= g.target : goals::allDone(fills);
                d.goals.insert(day, g);
            }
        }
    }
    // M44: hitos (siempre sobre todo, sin el filtro de actividad)
    d.ms = ms::Config::load(*m_db);
    if (d.ms.dayMark || d.ms.weekly || d.ms.streaks || d.ms.dailyPomos > 0) {
        const auto all = stats::studyByDay(*m_db, QDate(2000, 1, 1), d.today, nullptr);
        d.todaySec = all.value(d.today);
        d.weekSec = ms::weekTotal(all, d.today);
        d.records = ms::records(all, d.today.addDays(1));
        if (d.weekSec > d.records.bestWeek) d.records.bestWeek = d.weekSec;   // la semana en curso también puede ser récord
        d.bestStreak = qMax(m_db->setting("ms/best_streak", 0).toInt(), 0);
    }
    if (d.ms.dailyPomos > 0) {   // Mejoras 7.0
        const QDate mon = fmt::mondayOf(d.today);
        const auto wd = insights::daily(*m_db, mon, d.today, nullptr, QDateTime::currentDateTime());
        d.pomosToday = wd.value(d.today).pomos;
        for (QDate x = mon; x <= d.today; x = x.addDays(1)) {
            if (x == d.today && wd.value(x).pomos < d.ms.dailyPomos) continue;   // hoy todavía puede cumplirse
            ++d.weekMinCounted;
            if (wd.value(x).pomos >= d.ms.dailyPomos) ++d.weekMinMet;
        }
    }
    {   // Mejoras 7.0: objetivos de arranque de los últimos 30 días
        const QDate from = d.today.addDays(-29);
        auto goalsList = startgoal::between(*m_db, from, d.today);
        for (int i = goalsList.size() - 1; i >= 0; --i) {
            const auto& g = goalsList[i];
            const auto r = startgoal::evaluate(*m_db, g, QDateTime::currentDateTime());
            QString how;
            if (r.met) how = QStringLiteral("cumplido");
            else if (r.pending) how = QStringLiteral("en curso");
            else if (!r.started) how = QStringLiteral("no arrancaste");
            else if (!r.onTime) how = QStringLiteral("%1 min tarde").arg(r.lateMin);
            else how = QStringLiteral("faltó la tarea (%1/%2)").arg(r.done).arg(g.pomos);
            d.startRows.append({g.day, QStringLiteral("%1:%2 · %3")
                                           .arg(g.minute / 60, 2, 10, QLatin1Char('0')).arg(g.minute % 60, 2, 10, QLatin1Char('0')).arg(how)});
            if (r.pending) continue;
            ++d.startCounted;
            if (r.met) ++d.startMet;
        }
    }
    if (d.ms.stages) for (qint64 v : m_db->studyByFolder()) d.allSec += v;
    d.streak = stats::streak(m_db->studiedDays(), d.today, stats::StreakRules::load(*m_db));
    d.bestStreak = qMax(d.bestStreak, d.streak.days);

    const auto periodTotals = tree.rollUp(m_db->studyByFolder(d.from, d.to));
    const auto allTotals = tree.rollUp(m_db->studyByFolder());
    std::function<void(qint64, int)> addRows = [&](qint64 parent, int depth) {
        for (qint64 id : tree.children(parent)) {
            d.rows.append({tree.find(id)->name, depth, periodTotals.value(id), allTotals.value(id), id});
            addRows(id, depth + 1);
        }
    };
    if (filterId) {
        d.rows.append({tree.find(filterId)->name, 0, periodTotals.value(filterId), allTotals.value(filterId), filterId});
        addRows(filterId, 1);
    } else {
        addRows(0, 0);
        const qint64 orphanAll = m_db->studyByFolder().value(0);
        if (orphanAll > 0)
            d.rows.append({QStringLiteral("Sin actividad"), 0, m_db->studyByFolder(d.from, d.to).value(0), orphanAll});
    }
    d.hiddenRows.clear();
    for (const QString& s : m_db->setting("stats_hidden_acts", "").toString().split(',', Qt::SkipEmptyParts)) d.hiddenRows.insert(s.toLongLong());
    m_canvas->relayout();
}

// ===========================================================================
// Mejoras 3.0: detalle de cada dato
// ===========================================================================
namespace {
QString row(const QString& a, const QString& b, const QString& c = QString())
{
    return QStringLiteral("<tr><td style='padding:3px 14px 3px 0; color:%1'>%2</td><td style='padding:3px 14px 3px 0'><b>%3</b></td>"
                          "<td style='padding:3px 0; color:%1'>%4</td></tr>")
        .arg(theme::textDim.name(), a, b, c);
}
QString table(const QStringList& rows) { return QStringLiteral("<table cellspacing='0' cellpadding='0'>%1</table>").arg(rows.join(QString())); }
QString dim(const QString& t) { return QStringLiteral("<span style='color:%1'>%2</span>").arg(theme::textDim.name(), t); }
QString para(const QString& t) { return QStringLiteral("<p style='margin:0 0 8px 0'>%1</p>").arg(t); }
QString clock(qint64 epoch) { return QDateTime::fromSecsSinceEpoch(epoch).toString(QStringLiteral("HH:mm")); }
QString cap(const QString& s) { return s.isEmpty() ? s : s.left(1).toUpper() + s.mid(1); }
} // namespace


// Mejoras 4.0: resumen de un día (hover y mini ventana).
static QString moodWord(int m)
{
    static const char* w[] = {"", "mal", "flojo", "normal", "bien", "excelente"};
    return QString::fromUtf8(w[qBound(0, m, 5)]);
}

// Mejoras 5.1: cuánto de lo inactivo explica la rutina, sumado de `from` a `to`.
static routine::Explain routineExplain(Database* db, const QDate& from, const QDate& to)
{
    routine::Explain sum;
    const auto evs = routine::events(*db);
    if (evs.isEmpty()) return sum;
    const QDateTime now = QDateTime::currentDateTime();
    const QDate last = qMin(to, dayclock::logicalDate(now));
    for (QDate d = from; d <= last; d = d.addDays(1)) {
        const routine::Explain e = routine::explain(*db, evs, d, now);
        sum.total += e.total;
        sum.unexplained += e.unexplained;
        for (auto it = e.byCategory.cbegin(); it != e.byCategory.cend(); ++it) sum.byCategory[it.key()] += it.value();
    }
    return sum;
}

QString StatsWidget::dayHtml(const QDate& day, bool brief) const
{
    const StatsData& d = m_canvas->data;
    const insights::DayStat s = d.daily.value(day);
    QStringList rows;
    rows << row(d.studyLabel, fmt::duration(int(s.study)), s.pomos > 0 ? QStringLiteral("%1 pomodoro%2").arg(s.pomos).arg(s.pomos == 1 ? "" : "s") : QString());
    rows << row(QStringLiteral("Recreos"), fmt::duration(int(s.breaks)));
    rows << row(QStringLiteral("Tiempo muerto"), fmt::duration(int(s.undue)));
    rows << row(QStringLiteral("Recreo indebido (pausas)"), fmt::duration(int(s.paused)));
    rows << row(QStringLiteral("Inactivo"), fmt::hours(s.inactive));
    if (!brief) {
        const routine::Explain ex = routineExplain(m_db, day, day);
        for (auto it = ex.byCategory.cbegin(); it != ex.byCategory.cend(); ++it) rows << row(QStringLiteral("   · ") + it.key().toHtmlEscaped(), fmt::hours(it.value()));
        if (!ex.byCategory.isEmpty()) rows << row(QStringLiteral("   · sin motivo"), fmt::hours(ex.unexplained));
    }
    {
        int ns = 0; qint64 sec = 0;
        for (const auto& se : d.sessions) if (se.day == day) { ++ns; sec += se.length(); }
        if (ns > 0) rows << row(QStringLiteral("Sesiones"), QString::number(ns), QStringLiteral("%1 en total").arg(fmt::duration(int(sec))));
    }
    if (d.goals.contains(day)) {
        const auto g = d.goals.value(day);
        rows << row(QStringLiteral("Objetivo"), g.met ? QStringLiteral("cumplido") : QStringLiteral("no cumplido"),
                    QStringLiteral("%1 de %2").arg(fmt::duration(int(g.studied)), fmt::duration(int(g.target))));
    }
    QString html = para(QStringLiteral("<b>%1</b>").arg(fmt::longDate(day))) + table(rows);
    const auto dy = d.diary.value(day);
    if (dy.mood > 0) html += para(QStringLiteral("<br>Cómo te fue: <b>%1</b> (%2/5)").arg(moodWord(dy.mood)).arg(dy.mood));
    if (!dy.note.isEmpty()) {
        QString n = dy.note.toHtmlEscaped();
        if (brief && n.size() > 140) n = n.left(140) + QStringLiteral("…");
        html += para(QStringLiteral("<br><i>%1</i>").arg(n.replace(QLatin1Char('\n'), QStringLiteral("<br>"))));
    }
    return html;
}

QString StatsWidget::tipFor(const QString& key) const
{
    const StatsData& d = m_canvas->data;
    const QString more = QStringLiteral("<br><span style='color:%1'>Click para ver el detalle</span>").arg(theme::textDim.name());
    const int days = int(d.from.daysTo(qMin(d.to, d.today))) + 1;
    if (key.startsWith(QLatin1String("day:"))) {
        const QDate day = fmt::fromDayKey(key.mid(4));
        return dayHtml(day, true) + QString(more).replace(QStringLiteral("Click para ver el detalle"), QStringLiteral("Click para abrir este día"));
    }
    if (key.startsWith(QLatin1String("hm:"))) return QStringLiteral("Colorear el mapa según: ") + key.mid(3);
    if (key == "study") {
        int withStudy = 0;
        qint64 best = 0;
        for (qint64 v : d.byDay) { if (v > 0) ++withStudy; best = qMax(best, v); }
        return QStringLiteral("<b>%1 en %2</b><br>%3 de %4 días · mejor día %5 · promedio %6 por día")
                   .arg(fmt::hours(d.totals.study), m_label->text().toLower()).arg(withStudy).arg(qMax(1, days))
                   .arg(fmt::hours(best), fmt::hours(d.totals.study / qMax(1, days))) + more;
    }
    if (key == "breaks") return QStringLiteral("<b>Recreos: %1</b><br>El tiempo de recreo dentro de lo pactado.").arg(fmt::hours(d.totals.breaks)) + more;
    if (key == "undue") return QStringLiteral("<b>Tiempo muerto: %1</b><br>Demoras en arrancar el recreo o el próximo pomodoro y recreos pasados de largo.").arg(fmt::hours(d.totals.undue)) + more;
    if (key == "paused") return QStringLiteral("<b>Recreo indebido: %1</b><br>Tiempo en pausa con un pomodoro en curso.").arg(fmt::hours(d.totals.paused)) + more;
    if (key == "focus") {
        const qint64 den = d.totals.study + d.totals.breaks + d.totals.undue;
        return QStringLiteral("<b>Foco: %1</b><br>De todo el tiempo de sesión, cuánto fue de %2 de verdad.")
                   .arg(den > 0 ? QStringLiteral("%1 %").arg(qRound(100.0 * d.totals.study / den)) : QStringLiteral("—"), d.studyLabel.toLower()) + more;
    }
    if (key == "sessions") return QStringLiteral("<b>Sesiones: %1</b><br>Una sesión va del primer pomodoro hasta «Me voy» o una pausa larga.").arg(d.sessions.size()) + more;
    if (key == "inactive") return QStringLiteral("<b>Horas inactivas: %1</b><br>Todo lo demás del período: dormir, comer, la vida.").arg(fmt::hours(d.totals.inactive)) + more;
    if (key == "streak") {
        const int left = qMax(0, stats::jokersToday(*m_db, d.today) - d.streak.jokersThisWeek);
        return QStringLiteral("<b>Racha: %1 %2</b>%3<br>Comodines: te %4 %5 esta semana · racha más larga: %6")
                   .arg(d.streak.days).arg(d.streak.days == 1 ? "día" : "días",
                        d.streak.since.isValid() ? QStringLiteral(" · desde el %1").arg(fmt::shortDate(d.streak.since)) : QString())
                   .arg(left == 1 ? "queda" : "quedan").arg(left).arg(d.bestStreak) + more;
    }
    if (key == "goals") return QStringLiteral("<b>Objetivos del período</b><br>Qué días se cumplió el objetivo y cuánto faltó en los que no.") + more;
    if (key == "startgoal")
        return QStringLiteral("<b>Arranque</b><br>Días en que arrancaste a la hora que te propusiste y hiciste lo mínimo (últimos 30 días).") + more;
    if (key == "mark") {
        const int lvl = ms::dayMark(d.todaySec, d.ms.markMin);
        QString s = QStringLiteral("<b>Marca de hoy: %1</b><br>Llevás %2 hoy").arg(lvl ? cap(ms::dayMarkName(lvl)) : QStringLiteral("todavía no"), fmt::hours(d.todaySec));
        if (lvl < 3) s += QStringLiteral(" · faltan %1 para %2").arg(fmt::hours(qMax<qint64>(0, qint64(d.ms.markMin[lvl]) * 60 - d.todaySec)), ms::dayMarkName(lvl + 1));
        return s + QStringLiteral("<br>Bronce %1 · plata %2 · oro %3").arg(fmt::hours(qint64(d.ms.markMin[0]) * 60), fmt::hours(qint64(d.ms.markMin[1]) * 60), fmt::hours(qint64(d.ms.markMin[2]) * 60)) + more;
    }
    if (key == "weekly") {
        const qint64 goal = qint64(d.ms.weeklyMin) * 60;
        const int left = 7 - int(fmt::mondayOf(d.today).daysTo(d.today));
        return QStringLiteral("<b>Meta semanal: %1 de %2</b><br>%3")
                   .arg(fmt::hours(d.weekSec), fmt::hours(goal),
                        d.weekSec >= goal ? QStringLiteral("¡Cumplida!")
                                          : QStringLiteral("Faltan %1 · %2 por día en los %3 que quedan")
                                                .arg(fmt::hours(goal - d.weekSec), fmt::hours((goal - d.weekSec) / qMax(1, left))).arg(left)) + more;
    }
    if (key == "records")
        return QStringLiteral("<b>Tus récords</b><br>Mejor día: %1 (%2)<br>Mejor semana: %3<br>Racha más larga: %4 días")
                   .arg(fmt::hours(d.records.bestDay), d.records.bestDayDate.isValid() ? fmt::shortDate(d.records.bestDayDate) : QStringLiteral("—"),
                        fmt::hours(d.records.bestWeek)).arg(d.bestStreak) + more;
    if (key == "level") {
        const ms::Stage st = ms::stageFor(d.allSec);
        return QStringLiteral("<b>%1</b> · %2 en total<br>%3").arg(st.label, fmt::hours(d.allSec),
                   st.toSec ? QStringLiteral("Faltan %1 para el próximo nivel").arg(fmt::hours(st.toSec - d.allSec)) : QStringLiteral("Llegaste a Maestro")) + more;
    }
    return {};
}

// Mejoras 5.0: elegir qué actividades se ven en la lista de abajo (no cambia ningún total).
void StatsWidget::pickFolders()
{
    QSet<qint64> hidden = m_canvas->data.hiddenRows;
    QMenu menu(this);
    QAction* all = menu.addAction(QStringLiteral("Mostrar todas"));
    menu.addSeparator();
    for (const auto& r : m_canvas->data.rows) {
        if (!r.id) continue;
        QAction* a = menu.addAction(QString(r.depth * 3, QChar(' ')) + r.name);
        a->setCheckable(true);
        a->setChecked(!hidden.contains(r.id));
        a->setData(qlonglong(r.id));
    }
    const QPoint at = QCursor::pos();
    QAction* picked = menu.exec(at);
    if (!picked) return;
    if (picked == all) hidden.clear();
    else if (hidden.contains(picked->data().toLongLong())) hidden.remove(picked->data().toLongLong());
    else hidden.insert(picked->data().toLongLong());
    QStringList ids;
    for (qint64 id : hidden) ids << QString::number(id);
    m_db->setSetting("stats_hidden_acts", ids.join(','));
    refresh();
    pickFolders();   // el menú se vuelve a abrir para marcar varias de una
}

QWidget* StatsWidget::canvasWidget() const { return m_canvas; }
QRectF StatsWidget::itemRect(const QString& id) const { return m_canvas->shownRect(id); }

void StatsWidget::setEditing(bool on)
{
    if (m_editing == on) return;
    m_editing = on;
    if (on && m_period == Period::Evolution) { m_period = Period::Week; m_periodGroup->button(int(m_period))->setChecked(true); m_label->setText(periodLabel()); }
    fade::cross(m_canvas, 150);
    refresh();
    emit editingChanged(on);
}

void StatsWidget::layoutAction(const QString& key)
{
    const bool simple = !m_db->setting("stats_advanced", 0).toBool();
    const QStringList f = key.split(':');   // lay:acción[:panel[:valor]] · card:hide|toggle:id
    if (f.value(0) == QLatin1String("card")) {
        QSet<QString> cs = parseCards(m_db->setting(cardsKey(simple), "").toString(), simple);
        const QString id = f.value(2);
        if (f.value(1) == QLatin1String("hide")) cs.remove(id);
        else if (cs.contains(id)) cs.remove(id); else cs.insert(id);
        QStringList l;
        for (const QString& s : allCards()) if (cs.contains(s)) l << s;
        m_db->setSetting(cardsKey(simple), l.isEmpty() ? QStringLiteral("none") : l.join(','));
        refresh();
        return;
    }
    const QString act = f.value(1);
    if (act == QLatin1String("done")) { setEditing(false); return; }
    if (act == QLatin1String("add")) {   // ＋ Tarjetas: lista con tildes
        const QSet<QString> cs = parseCards(m_db->setting(cardsKey(simple), "").toString(), simple);
        QMenu menu(this);
        for (const QString& id : allCards()) {
            QAction* a = menu.addAction(cardName(id));
            a->setCheckable(true);
            a->setChecked(cs.contains(id));
            connect(a, &QAction::triggered, this, [this, id] { layoutAction(QStringLiteral("card:toggle:") + id); });
        }
        menu.exec(QCursor::pos());
        return;
    }
    if (act == QLatin1String("reset")) {   // vuelve a los lugares y tamaños de fábrica (con el resorte)
        m_db->setSetting(gridKey(simple), "");
        m_db->setSetting(cardsKey(simple), "");
        refresh();
    }
}

// Mejoras 7.0: el formato viejo (paneles hero / chips / chart / folders con S-M-L) pasa a la grilla:
// se conserva el orden de los paneles y lo que estaba oculto.
void StatsWidget::migrateGrid()
{
    if (m_db->setting("stats_grid_migrated", 0).toBool()) return;
    static const QStringList heroIds = {"streak", "study", "records", "breaks", "undue", "paused", "inactive", "sessions"};
    static const QStringList chipIds = {"goals", "mark", "startgoal", "weekly", "focus", "level"};
    for (bool simple : {true, false}) {
        const auto old = parseLayout(m_db->setting(layoutKey(simple), m_db->setting("stats_layout", "").toString()).toString());
        QSet<QString> cs = parseCards(m_db->setting(cardsKey(simple), "").toString(), simple);
        cs.insert("chart");
        cs.insert("folders");
        QStringList order;
        for (const auto& pn : old) {
            const QStringList ids = pn.id == QLatin1String("hero") ? heroIds : pn.id == QLatin1String("chips") ? chipIds : QStringList{pn.id};
            for (const QString& id : ids) {
                order << QStringLiteral("%1").arg(id);
                if (!pn.visible) cs.remove(id);
            }
        }
        QVector<StatsData::Item> grid;
        const auto def = defaultGrid();
        for (const QString& id : order) for (const auto& d : def) if (d.id == id) grid.append(d);
        m_db->setSetting(gridKey(simple), serializeGrid(grid));
        QStringList l;
        for (const QString& id : allCards()) if (cs.contains(id)) l << id;
        m_db->setSetting(cardsKey(simple), l.isEmpty() ? QStringLiteral("none") : l.join(','));
    }
    m_db->setSetting("stats_grid_migrated", 1);
}

void StatsWidget::showDetail(const QString& key)
{
    if (key.startsWith(QLatin1String("lay:")) || key.startsWith(QLatin1String("card:"))) { layoutAction(key); return; }
    if (m_editing) return;   // editando, un click no abre detalles
    if (key == QLatin1String("folders:pick")) { pickFolders(); return; }
    const StatsData& d = m_canvas->data;
    const QDate last = qMin(d.to, d.today);
    const int days = qMax(1, int(d.from.daysTo(last)) + 1);
    const FolderTree tree(m_db->folders());
    QStringList rows;
    QString title, html, actionText;
    std::function<void()> action;

    if (key.startsWith(QLatin1String("hm:"))) {   // elegir qué colorea el mapa de calor
        m_db->setSetting("stats/heat", key.mid(3));
        refresh();
        return;
    }
    if (key.startsWith(QLatin1String("day:"))) {
        const QDate day = fmt::fromDayKey(key.mid(4));
        m_detail->showDetail(fmt::shortDate(day), dayHtml(day, false), QStringLiteral("Abrir este día"),
                             [this, day] { m_detail->closeCard(); emit dayRequested(day); });
        return;
    }

    auto intervalsDetail = [&](IntervalKind kind, const QString& what) {
        qint64 total = 0, longest = 0;
        int n = 0;
        QStringList list;
        const auto ivs = m_db->intervalsBetween(d.from, last);
        for (auto it = ivs.crbegin(); it != ivs.crend(); ++it) {
            if (it->kind != kind || it->seconds < 30) continue;
            total += it->seconds; longest = qMax<qint64>(longest, it->seconds); ++n;
            if (list.size() < 12) list << row(d.period == Period::Day ? clock(it->startedAt) : fmt::shortDate(it->day) + QStringLiteral(" ") + clock(it->startedAt), fmt::duration(it->seconds));
        }
        rows << row(QStringLiteral("Total"), fmt::hours(total)) << row(QStringLiteral("Cantidad"), QString::number(n))
             << row(QStringLiteral("Promedio"), n ? fmt::duration(total / n) : QStringLiteral("—"))
             << row(QStringLiteral("El más largo"), n ? fmt::duration(longest) : QStringLiteral("—"));
        html = table(rows);
        if (!list.isEmpty()) html += para(QStringLiteral("<br>") + dim(QStringLiteral("Los últimos %1:").arg(what))) + table(list);
    };

    if (key == "study") {
        title = QStringLiteral("%1 · %2").arg(d.studyLabel, m_label->text());
        int withStudy = 0, pomos = 0;
        qint64 best = 0; QDate bestDay;
        for (auto it = d.byDay.cbegin(); it != d.byDay.cend(); ++it) { if (it.value() > 0) ++withStudy; if (it.value() > best) { best = it.value(); bestDay = it.key(); } }
        for (const Pomodoro& p : m_db->pomodorosBetween(d.from, last)) if (!d.filter || d.filter->contains(p.folderId)) pomos += p.doneUnits();
        rows << row(QStringLiteral("Total"), fmt::hours(d.totals.study))
             << row(QStringLiteral("Pomodoros"), QString::number(pomos), pomos ? QStringLiteral("%1 cada uno, en promedio").arg(fmt::duration(d.totals.study / pomos)) : QString());
        if (d.period != Period::Day) {
            rows << row(QStringLiteral("Días"), QStringLiteral("%1 de %2").arg(withStudy).arg(days))
                 << row(QStringLiteral("Promedio por día"), fmt::hours(d.totals.study / days), withStudy ? QStringLiteral("%1 contando solo los días con pomodoros").arg(fmt::hours(d.totals.study / withStudy)) : QString())
                 << row(QStringLiteral("Mejor día"), best ? fmt::hours(best) : QStringLiteral("—"), bestDay.isValid() ? fmt::longDate(bestDay) : QString());
        }
        html = table(rows);
        QStringList acts;
        for (const auto& r : d.rows) if (r.depth == 0 && r.period > 0) acts << row(r.name.toHtmlEscaped(), fmt::hours(r.period), QStringLiteral("%1 %").arg(d.totals.study ? qRound(100.0 * r.period / d.totals.study) : 0));
        if (!acts.isEmpty()) html += para(QStringLiteral("<br>") + dim(QStringLiteral("Por actividad:"))) + table(acts);
    } else if (key == "breaks") {
        title = QStringLiteral("Recreos · %1").arg(m_label->text());
        intervalsDetail(IntervalKind::Break, QStringLiteral("recreos"));
    } else if (key == "undue") {
        title = QStringLiteral("Tiempo muerto · %1").arg(m_label->text());
        intervalsDetail(IntervalKind::Undue, QStringLiteral("ratos"));
        html = para(dim(QStringLiteral("Tiempo muerto en medio de la jornada: demoras en arrancar el recreo o el próximo pomodoro, y recreos pasados de largo. "
                                       "Solo cuenta si después volviste a arrancar ese día; «Me voy» lo corta. (Antes de la beta 19 también incluía las pausas.)"))) + html;
    } else if (key == "paused") {
        title = QStringLiteral("Recreo indebido · %1").arg(m_label->text());
        intervalsDetail(IntervalKind::Pause, QStringLiteral("pausas"));
        html = para(dim(QStringLiteral("Tiempo que pausaste un pomodoro en curso. Cuenta desde que apretás pausa hasta que seguís."))) + html;
    } else if (key == "focus") {
        title = QStringLiteral("Foco · %1").arg(m_label->text());
        const qint64 den = d.totals.study + d.totals.breaks + d.totals.undue + d.totals.paused;
        auto pct = [&](qint64 v) { return QStringLiteral("%1 %").arg(den ? qRound(100.0 * v / den) : 0); };
        rows << row(d.studyLabel, fmt::hours(d.totals.study), pct(d.totals.study))
             << row(QStringLiteral("Recreos"), fmt::hours(d.totals.breaks), pct(d.totals.breaks))
             << row(QStringLiteral("Tiempo muerto"), fmt::hours(d.totals.undue), pct(d.totals.undue))
             << row(QStringLiteral("Recreo indebido (pausas)"), fmt::hours(d.totals.paused), pct(d.totals.paused));
        html = para(dim(QStringLiteral("Foco = %1 ÷ (%1 + recreos + tiempo muerto + recreo indebido). Mide qué parte del tiempo de sesión estuviste de verdad en lo tuyo; "
                                       "el tiempo muerto entre pomodoros lo baja.").arg(d.studyLabel.toLower()))) + table(rows);
        html += para(QStringLiteral("<br>") + dim(QStringLiteral("El detalle del recreo indebido está en su tarjeta (vista avanzada) y en cada día.")));
    } else if (key == "sessions") {
        title = QStringLiteral("Sesiones · %1").arg(m_label->text());
        qint64 total = 0, longest = 0;
        for (const auto& s : d.sessions) { total += s.length(); longest = qMax(longest, s.length()); }
        const int n = int(d.sessions.size());
        rows << row(QStringLiteral("Cantidad"), QString::number(n))
             << row(QStringLiteral("Duración promedio"), n ? fmt::duration(int(total / n)) : QStringLiteral("—"))
             << row(QStringLiteral("La más larga"), n ? fmt::duration(int(longest)) : QStringLiteral("—"));
        html = para(dim(QStringLiteral("Una sesión va del primer pomodoro hasta «Me voy» o una pausa de más de %1 min (se cambia en Ajustes › General › Avanzado).")
                            .arg(m_db->setting("session_gap_min", 60).toInt()))) + table(rows);
        QStringList list;
        for (int i = int(d.sessions.size()) - 1; i >= 0 && list.size() < 12; --i) {
            const auto& s = d.sessions[i];
            list << row(fmt::shortDate(s.day), QStringLiteral("%1 – %2").arg(QDateTime::fromSecsSinceEpoch(s.start).toString("HH:mm"), QDateTime::fromSecsSinceEpoch(s.end).toString("HH:mm")),
                        QStringLiteral("%1 · %2 pomodoro%3").arg(fmt::duration(int(s.length()))).arg(s.pomos).arg(s.pomos == 1 ? "" : "s"));
        }
        if (!list.isEmpty()) html += para(QStringLiteral("<br>") + dim(QStringLiteral("Últimas:"))) + table(list);
    } else if (key == "inactive") {
        title = QStringLiteral("Horas inactivas · %1").arg(m_label->text());
        const qint64 all = d.totals.study + d.totals.breaks + d.totals.undue + d.totals.paused + d.totals.inactive;
        auto pct = [&](qint64 v) { return QStringLiteral("%1 %").arg(all ? qRound(100.0 * v / all) : 0); };
        rows << row(d.studyLabel, fmt::hours(d.totals.study), pct(d.totals.study)) << row(QStringLiteral("Recreos"), fmt::hours(d.totals.breaks), pct(d.totals.breaks))
             << row(QStringLiteral("Tiempo muerto"), fmt::hours(d.totals.undue), pct(d.totals.undue)) << row(QStringLiteral("Recreo indebido"), fmt::hours(d.totals.paused), pct(d.totals.paused)) << row(QStringLiteral("Inactivas"), fmt::hours(d.totals.inactive), pct(d.totals.inactive));
        html = para(dim(QStringLiteral("Todo lo que no es pomodoro ni recreo: dormir, comer, cursar, la vida. Así se reparte el período:"))) + table(rows);
        const routine::Explain ex = routineExplain(m_db, d.from, d.to);
        if (!ex.byCategory.isEmpty()) {
            QStringList rr;
            auto pc = [&](qint64 v) { return QStringLiteral("%1 %").arg(ex.total ? qRound(100.0 * v / ex.total) : 0); };
            for (auto it = ex.byCategory.cbegin(); it != ex.byCategory.cend(); ++it) rr << row(it.key().toHtmlEscaped(), fmt::hours(it.value()), pc(it.value()));
            rr << row(QStringLiteral("Sin motivo"), fmt::hours(ex.unexplained), pc(ex.unexplained));
            html += para(QStringLiteral("<br>") + dim(QStringLiteral("Lo inactivo, según tu rutina:"))) + table(rr);
        } else {
            html += para(QStringLiteral("<br>") + dim(QStringLiteral("Cargá tu rutina en Ajustes › Rutina (facultad, gimnasio, médico…) y acá vas a ver en qué se fue este tiempo.")));
        }
    } else if (key == "streak") {
        title = QStringLiteral("Racha: %1 %2").arg(d.streak.days).arg(d.streak.days == 1 ? "día" : "días");
        const stats::StreakRules rules = stats::StreakRules::load(*m_db);
        const int perWeek = rules.jokersOn(d.today);
        const int left = qMax(0, perWeek - d.streak.jokersThisWeek);
        rows << row(QStringLiteral("Viene desde"), d.streak.since.isValid() ? fmt::longDate(d.streak.since) : QStringLiteral("—"))
             << row(QStringLiteral("Hoy"), d.streak.todayDone ? QStringLiteral("ya sumaste") : QStringLiteral("todavía no"),
                    d.streak.todayDone ? QString() : (left > 0 ? QStringLiteral("si no hacés nada se usa un comodín") : QStringLiteral("si no hacés nada, se corta")))
             << row(QStringLiteral("Comodines"), QStringLiteral("%1 de %2 esta semana").arg(left).arg(perWeek), QStringLiteral("disponibles"))
             << row(QStringLiteral("Racha más larga"), QStringLiteral("%1 %2").arg(d.bestStreak).arg(d.bestStreak == 1 ? "día" : "días"));
        html = table(rows);
        // Las últimas 5 semanas: ● con pomodoros · ◐ comodín · ○ nada
        const QSet<QDate> studied = m_db->studiedDays();
        QString cal = QStringLiteral("<br><table cellspacing='0' cellpadding='0'><tr>");
        for (const char* h : {"L", "M", "M", "J", "V", "S", "D"}) cal += QStringLiteral("<td style='padding:2px 9px; color:%1'>%2</td>").arg(theme::textFaint.name(), QString::fromLatin1(h));
        cal += QStringLiteral("</tr>");
        const QDate first = fmt::mondayOf(d.today).addDays(-28);
        for (int w = 0; w < 5; ++w) {
            cal += QStringLiteral("<tr>");
            for (int k = 0; k < 7; ++k) {
                const QDate x = first.addDays(w * 7 + k);
                const bool inStreak = d.streak.since.isValid() && x >= d.streak.since && x <= d.today;
                QString mark = QStringLiteral("○"); QColor c = theme::textFaint;
                if (x > d.today) { mark = QStringLiteral("·"); }
                else if (studied.contains(x)) { mark = QStringLiteral("●"); c = inStreak ? theme::pomoAccent : theme::textDim; }
                else if (d.streak.jokerDays.contains(x)) { mark = QStringLiteral("◐"); c = theme::text; }
                cal += QStringLiteral("<td style='padding:2px 9px; color:%1; %3'>%2</td>").arg(c.name(), mark, x == d.today ? QStringLiteral("font-weight:700; text-decoration: underline;") : QString());
            }
            cal += QStringLiteral("</tr>");
        }
        cal += QStringLiteral("</table>");
        html += cal + para(QStringLiteral("<br>") + dim(QStringLiteral("● día con pomodoros (en rojo, los de la racha) · ◐ comodín · ○ nada.<br>"
                                                                       "Solo suman los días con algún pomodoro; los comodines la sostienen pero no agregan días. "
                                                                       "Cambiar los comodines en Ajustes vale desde ese día, no hacia atrás.")));
        if (rules.resetDay == d.today) {
            actionText = QStringLiteral("Deshacer el reinicio");
            action = [this] { stats::resetStreak(*m_db, QDate()); refresh(); showDetail(QStringLiteral("streak")); emit message(QStringLiteral("Racha recuperada")); };
        } else if (d.streak.days > 0) {
            actionText = QStringLiteral("Reiniciar racha…");
            action = [this, today = d.today, n = d.streak.days] {
                m_detail->showDetail(QStringLiteral("¿Reiniciar la racha?"),
                    para(QStringLiteral("Tu racha de <b>%1 %2</b> vuelve a cero y empieza a contar desde hoy.").arg(n).arg(n == 1 ? "día" : "días")) +
                    para(dim(QStringLiteral("Es para cuando sentís que no cumpliste. Tu racha más larga queda guardada como récord, y hoy mismo lo podés deshacer desde acá."))),
                    QStringLiteral("Sí, reiniciar"), [this, today] {
                        stats::resetStreak(*m_db, today);
                        refresh();
                        showDetail(QStringLiteral("streak"));
                        emit message(QStringLiteral("Racha reiniciada"));
                    });
            };
        }
    } else if (key == "goals") {
        title = QStringLiteral("Objetivos · %1").arg(m_label->text());
        int met = 0, n = 0;
        for (auto it = d.goals.cend(); it != d.goals.cbegin();) {
            --it;
            const bool today = it.key() == d.today;
            if (!today || it->met) { ++n; if (it->met) ++met; }
            rows << row(cap(fmt::shortDate(it.key())), QStringLiteral("%1 de %2").arg(fmt::hours(it->studied), fmt::hours(it->target)),
                        it->met ? QStringLiteral("<span style='color:%1'>cumplido</span>").arg(theme::breakAccent.name())
                                : today ? QStringLiteral("en curso · faltan %1").arg(fmt::hours(it->missing)) : QStringLiteral("faltaron %1").arg(fmt::hours(it->missing)));
        }
        html = para(QStringLiteral("<b>%1 de %2</b> días cumplidos%3").arg(met).arg(n).arg(n ? QStringLiteral(" (%1 %)").arg(qRound(100.0 * met / n)) : QString())) + table(rows);
    } else if (key == "startgoal") {
        title = QStringLiteral("Arranque · últimos 30 días");
        for (const auto& r : d.startRows) rows << row(cap(fmt::shortDate(r.first)), r.second.section(QStringLiteral(" · "), 0, 0), r.second.section(QStringLiteral(" · "), 1));
        html = para(QStringLiteral("<b>%1 de %2</b> cumplidos").arg(d.startMet).arg(d.startCounted)) + table(rows);
    } else if (key == "mark") {
        const int lvl = ms::dayMark(d.todaySec, d.ms.markMin);
        title = QStringLiteral("Marca de hoy: %1").arg(lvl ? cap(ms::dayMarkName(lvl)) : QStringLiteral("todavía no"));
        static const char* names[3] = {"Bronce", "Plata", "Oro"};
        for (int i = 0; i < 3; ++i) {
            const qint64 at = qint64(d.ms.markMin[i]) * 60;
            rows << row(QString::fromLatin1(names[i]), fmt::hours(at), d.todaySec >= at ? QStringLiteral("<span style='color:%1'>lograda</span>").arg(theme::breakAccent.name())
                                                                                        : QStringLiteral("faltan %1").arg(fmt::hours(at - d.todaySec)));
        }
        html = para(QStringLiteral("Llevás <b>%1</b> hoy.").arg(fmt::hours(d.todaySec))) + table(rows);
        // Últimos 30 días
        const auto all = stats::studyByDay(*m_db, d.today.addDays(-29), d.today, nullptr);
        int c[4] = {0, 0, 0, 0};
        for (QDate x = d.today.addDays(-29); x <= d.today; x = x.addDays(1)) ++c[ms::dayMark(all.value(x), d.ms.markMin)];
        html += para(QStringLiteral("<br>") + dim(QStringLiteral("En los últimos 30 días:"))) +
                table({row(QStringLiteral("Oro"), QString::number(c[3])), row(QStringLiteral("Plata"), QString::number(c[2])),
                       row(QStringLiteral("Bronce"), QString::number(c[1])), row(QStringLiteral("Sin marca"), QString::number(c[0]))}) +
                para(QStringLiteral("<br>") + dim(QStringLiteral("Los valores se cambian en Ajustes › Hitos.")));
    } else if (key == "weekly") {
        const qint64 goal = qint64(d.ms.weeklyMin) * 60;
        title = QStringLiteral("Meta semanal: %1 de %2").arg(fmt::hours(d.weekSec), fmt::hours(goal));
        const QDate mon = fmt::mondayOf(d.today);
        const auto all = stats::studyByDay(*m_db, QDate(2000, 1, 1), d.today, nullptr);
        static const char* dn[7] = {"Lunes", "Martes", "Miércoles", "Jueves", "Viernes", "Sábado", "Domingo"};
        for (int i = 0; i < 7; ++i) {
            const QDate x = mon.addDays(i);
            rows << row(QString::fromUtf8(dn[i]), x > d.today ? QStringLiteral("—") : fmt::hours(all.value(x)), x == d.today ? QStringLiteral("hoy") : QString());
        }
        const int left = 7 - int(mon.daysTo(d.today));
        html = para(d.weekSec >= goal ? QStringLiteral("<span style='color:%1'><b>¡Cumplida!</b></span>").arg(theme::breakAccent.name())
                                      : QStringLiteral("Faltan <b>%1</b>: son <b>%2 por día</b> en los %3 %4 que quedan (contando hoy).")
                                            .arg(fmt::hours(goal - d.weekSec), fmt::hours((goal - d.weekSec) / qMax(1, left))).arg(left).arg(left == 1 ? "día" : "días")) +
               table(rows) +
               (d.ms.dailyPomos > 0 ? para(QStringLiteral("<br>Mínimo diario: <b>%1 pomodoros</b>. Hoy llevás <b>%2</b>; esta semana lo cumpliste <b>%3 de %4</b> días.")
                                               .arg(d.ms.dailyPomos).arg(d.pomosToday).arg(d.weekMinMet).arg(d.weekMinCounted)) : QString()) +
               para(QStringLiteral("<br>") + dim(QStringLiteral("Semanas seguidas cumpliéndola: %1. Se cambia en Ajustes › Hitos.").arg(ms::weeksInARow(all, d.today, goal))));
    } else if (key == "records") {
        title = QStringLiteral("Tus récords");
        const auto all = stats::studyByDay(*m_db, QDate(2000, 1, 1), d.today, nullptr);
        const ms::Records before = ms::records(all, d.today);   // sin contar hoy: para comparar
        // Mejor mes y más pomodoros en un día
        QMap<QDate, qint64> byMonth; QMap<QDate, int> pomosByDay;
        for (auto it = all.cbegin(); it != all.cend(); ++it) byMonth[QDate(it.key().year(), it.key().month(), 1)] += it.value();
        for (const Pomodoro& p : m_db->pomodorosBetween(QDate(2000, 1, 1), d.today)) pomosByDay[p.day] += p.doneUnits();
        qint64 bestMonth = 0; QDate bestMonthDate; int mostPomos = 0; QDate mostPomosDate;
        for (auto it = byMonth.cbegin(); it != byMonth.cend(); ++it) if (it.value() > bestMonth) { bestMonth = it.value(); bestMonthDate = it.key(); }
        for (auto it = pomosByDay.cbegin(); it != pomosByDay.cend(); ++it) if (it.value() > mostPomos) { mostPomos = it.value(); mostPomosDate = it.key(); }
        const qint64 thisMonth = byMonth.value(QDate(d.today.year(), d.today.month(), 1));
        auto versus = [](qint64 now, qint64 rec, const QString& when) {
            if (rec <= 0) return QString();
            return now >= rec ? QStringLiteral("<span style='color:%1'>%2: ¡es el récord!</span>").arg(theme::breakAccent.name(), when)
                              : QStringLiteral("%1: %2 · faltan %3").arg(when, fmt::hours(now), fmt::hours(rec - now));
        };
        auto block = [&](const QString& name, const QString& value, const QString& when, const QString& vs) {
            return QStringLiteral("<tr><td style='padding:6px 16px 0 0; color:%1'>%2</td><td style='padding:6px 12px 0 0; font-size:13pt'><b>%3</b></td>"
                                  "<td style='padding:6px 0 0 0; color:%1'>%4</td></tr>"
                                  "<tr><td></td><td colspan='2' style='padding:0 0 6px 0; color:%1'>%5</td></tr>")
                .arg(theme::textDim.name(), name, value, when, vs);
        };
        QStringList b;
        b << block(QStringLiteral("Mejor día"), d.records.bestDay ? fmt::hours(d.records.bestDay) : QStringLiteral("—"),
                   d.records.bestDayDate.isValid() ? fmt::longDate(d.records.bestDayDate) : QString(), versus(d.todaySec, before.bestDay, QStringLiteral("Hoy")))
          << block(QStringLiteral("Mejor semana"), d.records.bestWeek ? fmt::hours(d.records.bestWeek) : QStringLiteral("—"),
                   d.records.bestWeekMonday.isValid() && d.weekSec < d.records.bestWeek ? fmt::weekRange(d.records.bestWeekMonday) : (d.records.bestWeek ? QStringLiteral("esta semana") : QString()),
                   versus(d.weekSec, before.bestWeek, QStringLiteral("Esta semana")))
          << block(QStringLiteral("Mejor mes"), bestMonth ? fmt::hours(bestMonth) : QStringLiteral("—"), bestMonthDate.isValid() ? fmt::monthYear(bestMonthDate) : QString(),
                   bestMonthDate == QDate(d.today.year(), d.today.month(), 1) ? QStringLiteral("es este mes") : versus(thisMonth, bestMonth, QStringLiteral("Este mes")))
          << block(QStringLiteral("Racha más larga"), QStringLiteral("%1 %2").arg(d.bestStreak).arg(d.bestStreak == 1 ? "día" : "días"), QString(),
                   d.streak.days >= d.bestStreak && d.bestStreak > 0 ? QStringLiteral("<span style='color:%1'>es la de ahora</span>").arg(theme::breakAccent.name())
                                                                     : QStringLiteral("Ahora: %1 · faltan %2").arg(d.streak.days).arg(d.bestStreak - d.streak.days))
          << block(QStringLiteral("Más pomodoros en un día"), mostPomos ? QString::number(mostPomos) : QStringLiteral("—"),
                   mostPomosDate.isValid() ? fmt::longDate(mostPomosDate) : QString(),
                   mostPomos ? QStringLiteral("Hoy: %1").arg(pomosByDay.value(d.today)) : QString());
        html = table(b);
    } else if (key == "level") {
        const ms::Stage st = ms::stageFor(d.allSec);
        title = QStringLiteral("Nivel global: %1").arg(st.label);
        static const qint64 from[8] = {0, 20, 100, 500, 1000, 2500, 5000, 10000};
        const QStringList names = ms::stageNames();
        for (int i = 0; i < names.size() && i < 8; ++i) {
            const bool here = i == st.index;
            rows << row(here ? QStringLiteral("<b style='color:%1'>▸ %2</b>").arg(theme::text.name(), names[i]) : names[i],
                        i + 1 < 8 ? QStringLiteral("%1 – %2 h").arg(from[i]).arg(from[i + 1]) : QStringLiteral("10.000 h o más"),
                        here ? QStringLiteral("estás acá") : (i < st.index ? QStringLiteral("hecho") : QString()));
        }
        html = para(QStringLiteral("Llevás <b>%1</b> en total (%2 % de las 10.000 h).%3")
                        .arg(fmt::hours(d.allSec), QString::number(d.allSec / 360000.0, 'f', 2),
                             st.toSec ? QStringLiteral(" Faltan <b>%1</b> para el próximo nivel.").arg(fmt::hours(st.toSec - d.allSec)) : QString())) +
               table(rows) + para(QStringLiteral("<br>") + dim(QStringLiteral("Cada etapa tiene subniveles I, II y III. Cada actividad tiene su propio nivel (abajo, en la lista).")));
    } else {
        return;
    }
    m_detail->showDetail(title, html, actionText, action);
}
