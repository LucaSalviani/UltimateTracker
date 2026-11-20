#include "ui/StatsWidget.h"
#include "core/Kinds.h"
#include "ui/DetailCard.h"
#include "ui/EvolutionView.h"
#include "core/FolderTree.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Export.h"
#include "core/Milestones.h"
#include "core/Goals.h"
#include "core/Insights.h"
#include "ui/ActivitiesWidget.h"
#include "ui/Fade.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QButtonGroup>
#include <QComboBox>
#include <QDateTime>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QMenu>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
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
    ms::Records records;   // incluye hoy
    int bestStreak = 0;
    QHash<qint64, QColor> colors;   // M35: color de cada actividad (vacío si están apagados)
    QColor colorOf(qint64 id) const { return colors.value(id, statsInk()); }
};

class StatsCanvas : public QWidget {
public:
    explicit StatsCanvas(QWidget* parent) : QWidget(parent) { setMouseTracking(true); }
    StatsData data;
    // Mejoras 3.0: cada dato es una zona: al pasar el mouse muestra más detalle
    // y al hacer click abre su mini ventana.
    struct Zone { QRectF rect; QString key; QString tip; };
    QVector<Zone> zones;
    std::function<void(const QString&)> onZone;
    std::function<QString(const QString&)> tipFor;

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
    void mouseMoveEvent(QMouseEvent* e) override
    {
        const Zone* z = zoneAt(e->position());
        setCursor(z ? Qt::PointingHandCursor : Qt::ArrowCursor);
        const QString key = z ? z->key : QString();
        if (key != m_hoverKey) { m_hoverKey = key; update(); }
        QToolTip::hideText();   // Mejoras 4.1: el detalle sale solo con click (menos estímulo)
    }
    void leaveEvent(QEvent*) override { if (!m_hoverKey.isEmpty()) { m_hoverKey.clear(); update(); } }
    void mouseReleaseEvent(QMouseEvent* e) override
    {
        if (e->button() != Qt::LeftButton) return;
        if (const Zone* z = zoneAt(e->position()); z && onZone) onZone(z->key);
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

    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), theme::bg);
        const double L = 16, W = width() - 32;
        double y = 8;
        zones.clear();

        y = paintCards(p, L, y, W) + 10;
        y = paintStreak(p, L, y, W) + 18;
        if (hasChips()) y = paintChips(p, L, y, W) + 18;   // M44
        y = paintChart(p, L, y, W) + 22;
        paintFolders(p, L, y, W);
    }

private:
    static constexpr double kCardH = 70;
    static constexpr double kRowH = 26;
    static constexpr double kChipH = 52;

    bool goalChip() const { return data.period != StatsWidget::Period::Day && !data.goals.isEmpty(); }
    bool hasChips() const { return !buildChips().isEmpty(); }

    // M44: tarjetitas con los hitos de corto y largo plazo.
    // Mejoras 3.0: si no entran en una fila (ventana chica) bajan a dos o más, así el
    // texto se lee entero; cada una abre su detalle.
    struct Chip { QString key, label, value; double frac = -1; int dots = -1; };
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
        if (data.ms.weekly && data.ms.weeklyMin > 0) {
            const qint64 goal = qint64(data.ms.weeklyMin) * 60;
            chips.append({QStringLiteral("weekly"), QStringLiteral("Meta semanal"),
                          QStringLiteral("%1 de %2").arg(fmt::hours(data.weekSec), fmt::hours(goal)),
                          double(data.weekSec) / goal});
        }
        if (data.ms.streaks) {
            QStringList parts;
            if (data.records.bestDay > 0) parts << QStringLiteral("Día %1").arg(fmt::hours(data.records.bestDay));
            if (data.records.bestWeek > 0) parts << QStringLiteral("Semana %1").arg(fmt::hours(data.records.bestWeek));
            parts << QStringLiteral("Racha %1 %2").arg(data.bestStreak).arg(data.bestStreak == 1 ? "día" : "días");
            chips.append({QStringLiteral("records"), QStringLiteral("Tus récords"), parts.join(QStringLiteral("  ·  "))});
        }
        if (data.ms.stages) {
            const ms::Stage st = ms::stageFor(data.allSec);
            chips.append({QStringLiteral("level"), QStringLiteral("Nivel global"),
                          st.toSec ? QStringLiteral("%1 · próx. %2").arg(st.label, roundHours(st.toSec)) : st.label,
                          st.toSec ? st.progress : 1.0});
        }
        if (data.simple) {   // simple: solo objetivos y meta semanal
            QVector<Chip> keep;
            for (const Chip& ch : chips) if (ch.key == QLatin1String("goals") || ch.key == QLatin1String("weekly")) keep << ch;
            return keep;
        }
        return chips;
    }
    static constexpr double kChipGap = 10;
    static constexpr double kChipMinW = 200;
    int chipsPerRow(double W, int n) const { return qBound(1, int((W + kChipGap) / (kChipMinW + kChipGap)), qMax(1, n)); }
    double chipsHeight(double W) const
    {
        const int n = int(buildChips().size());
        if (n == 0) return 0;
        const int rows = (n + chipsPerRow(W, n) - 1) / chipsPerRow(W, n);
        return rows * kChipH + (rows - 1) * kChipGap;
    }

    double paintChips(QPainter& p, double L, double y, double W)
    {
        const QVector<Chip> chips = buildChips();
        static const QColor medal[3] = {QColor(0xc0, 0x8a, 0x5a), QColor(0xb8, 0xbc, 0xc4), QColor(0xe0, 0xb8, 0x4a)};
        const int n = int(chips.size());
        if (n == 0) return y;
        const int per = chipsPerRow(W, n);
        const int rows = (n + per - 1) / per;
        for (int i = 0; i < n; ++i) {
            const Chip& c = chips[i];
            const int row = i / per;
            const int inRow = row == rows - 1 ? n - row * per : per;   // la última fila reparte el ancho entre las que tiene
            const double cw = (W - (inRow - 1) * kChipGap) / inRow;
            const QRectF r(L + (i % per) * (cw + kChipGap), y + row * (kChipH + kChipGap), cw, kChipH);
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
            if (c.frac >= 0) {
                const QRectF track(r.left() + 12, r.bottom() - 9, r.width() - 24, 3);
                p.setPen(Qt::NoPen);
                p.setBrush(theme::surface);
                p.drawRoundedRect(track, 1.5, 1.5);
                p.setBrush(c.frac >= 1 ? theme::breakAccent : statsInk());
                p.drawRoundedRect(QRectF(track.left(), track.top(), track.width() * qMin(1.0, c.frac), 3), 1.5, 1.5);
            }
            zone(r, c.key, p);
        }
        return y + chipsHeight(W);
    }

    double chartHeight(double W) const
    {
        switch (data.period) {
        case StatsWidget::Period::Day:  return 24 + 66;
        case StatsWidget::Period::Year: return 24 + 30 + 7 * (heatCell(W) + 3) + 18 + 24;
        default:                        return 24 + 168;
        }
    }
    static double heatCell(double W) { return qBound(6.0, (W - 30) / 53.0 - 3, 14.0); }

    int contentHeight(int w) const
    {
        const double W = w - 32;
        return int(8 + kCardH + 10 + 20 + 18 + (hasChips() ? chipsHeight(W) + 18 : 0) + chartHeight(W) + 22 + 24 +
                   data.rows.size() * kRowH + 24);
    }

    void sectionTitle(QPainter& p, double x, double y, const QString& t) const
    {
        p.setFont(theme::uiFont(9, QFont::DemiBold));
        p.setPen(theme::textDim);
        p.drawText(QPointF(x, y + 13), t.toUpper());
    }

    double paintCards(QPainter& p, double L, double y, double W)
    {
        struct Card { QString label; qint64 sec; QColor accent; };
        const Card cards[4] = {
            {data.studyLabel, data.totals.study, theme::text},
            {QStringLiteral("Recreos"), data.totals.breaks, theme::text},
            {QStringLiteral("Recreos indebidos"), data.totals.undue,
             data.totals.undue > 0 ? theme::squareFinal.lighter(125) : theme::text},
            {QStringLiteral("Horas inactivas"), data.totals.inactive, theme::textDim},
        };
        const double gap = 10;
        const int n = data.simple ? 2 : 4;   // simple: estudio y recreos; avanzada: + indebidos e inactivas
        const double cw = (W - (n - 1) * gap) / n;
        for (int i = 0; i < n; ++i) {
            const QRectF r(L + i * (cw + gap), y, cw, kCardH);
            p.setPen(Qt::NoPen);
            p.setBrush(theme::surface);
            p.drawRoundedRect(r, 8, 8);
            p.setFont(theme::uiFont(8.5));
            p.setPen(theme::textDim);
            p.drawText(r.adjusted(12, 10, -8, 0), Qt::AlignLeft | Qt::AlignTop,
                       QFontMetrics(p.font()).elidedText(cards[i].label, Qt::ElideRight, int(cw - 20)));
            p.setFont(theme::uiFont(16, QFont::DemiBold));
            p.setPen(cards[i].accent);
            p.drawText(r.adjusted(12, 0, -8, -10), Qt::AlignLeft | Qt::AlignBottom, fmt::hours(cards[i].sec));
            static const char* keys[4] = {"study", "breaks", "undue", "inactive"};
            zone(r, QString::fromLatin1(keys[i]), p);
        }
        return y + kCardH;
    }

    double paintStreak(QPainter& p, double L, double y, double W)
    {
        p.setFont(theme::uiFont(9.5));
        QString s = QStringLiteral("Racha: %1 %2")
                        .arg(data.streak.days)
                        .arg(data.streak.days == 1 ? QStringLiteral("día") : QStringLiteral("días"));
        s += QStringLiteral("   ·   comodines esta semana: %1 de %2").arg(data.streak.jokersThisWeek).arg(data.jokers);
        if (data.period == StatsWidget::Period::Day && data.goals.contains(data.from)) {   // B2-3
            const auto g = data.goals.value(data.from);
            if (g.met) s += QStringLiteral("   ·   objetivo %1 cumplido").arg(fmt::hours(g.target));
            else if (data.from == data.today) s += QStringLiteral("   ·   objetivo %1: faltan %2").arg(fmt::hours(g.target), fmt::hours(g.missing));
            else s += QStringLiteral("   ·   objetivo %1: faltaron %2").arg(fmt::hours(g.target), fmt::hours(g.missing));
        }
        if (data.ms.streaks && data.bestStreak > data.streak.days)
            s += QStringLiteral("   ·   récord: %1").arg(data.bestStreak);
        if (!data.streak.todayDone) s += QStringLiteral("   ·   hoy todavía no arrancaste");
        p.setPen(theme::text);
        p.drawText(QRectF(L, y, W, 20), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(s, Qt::ElideRight, int(W)));
        zone(QRectF(L - 6, y - 2, W + 12, 24), QStringLiteral("streak"), p);
        return y + 20;
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
                  i.kind == IntervalKind::Break ? theme::timelineBreak : theme::squareFinal.darker(140));
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
        const double chartH = 128;
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
                   data.goals.isEmpty() ? QStringLiteral("- - promedio por día")
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
        if (avg > 0) {
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
        if (m == QLatin1String("undue"))    return theme::squareFinal;
        if (m == QLatin1String("inactive")) return theme::mix(theme::surface, theme::text, 0.8);
        return statsInk();
    }
    static QString heatName(const QString& m)
    {
        if (m == QLatin1String("pomos")) return QStringLiteral("Pomodoros");
        if (m == QLatin1String("goal")) return QStringLiteral("Objetivo");
        if (m == QLatin1String("undue")) return QStringLiteral("Indebido");
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
            static const char* metrics[] = {"hours", "pomos", "goal", "undue", "inactive"};
            p.setFont(theme::uiFont(8, QFont::DemiBold));
            double x = L + W;
            for (int k = 4; k >= 0; --k) {
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

    void paintFolders(QPainter& p, double L, double y, double W)
    {
        sectionTitle(p, L, y, QStringLiteral("Actividades"));
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
            p.drawText(QPointF(L, y + 14), QStringLiteral("Todavía no hay actividades: creá una desde ⋯ › Actividades…"));
            return;
        }
        for (const auto& row : data.rows) {
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

    // M39: exportar a CSV (lo que estás viendo, o todo)
    auto* exp = new QToolButton(this);
    exp->setIcon(icons::get(icons::Id::Export));
    exp->setToolTip(QStringLiteral("Exportar a CSV (Excel, Sheets)"));
    exp->setPopupMode(QToolButton::InstantPopup);
    auto* expMenu = new QMenu(exp);
    expMenu->addAction(QStringLiteral("Lo que estoy viendo (período y actividad)…"), this, [this] { exportCsv(false); });
    expMenu->addAction(QStringLiteral("Todo el historial…"), this, [this] { exportCsv(true); });
    exp->setMenu(expMenu);
    top->addSpacing(4);
    top->addWidget(exp);

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
    info->setToolTip(QStringLiteral("Cómo ver el detalle"));
    connect(info, &QToolButton::clicked, this, [this] {
        emit message(QStringLiteral("Hacé click en cualquier dato, barra o día para ver su detalle"));
    });
    top->insertWidget(top->indexOf(m_folder) + 1, m_viewBtn);
    top->addWidget(info);

    m_canvas = new StatsCanvas(this);
    m_canvas->tipFor = [this](const QString& key) { return tipFor(key); };
    m_canvas->onZone = [this](const QString& key) { showDetail(key); };
    m_detail = new DetailCard(this);
    auto* scroll = m_scroll = new QScrollArea(this);
    scroll->setWidget(m_canvas);
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
    case Period::Month: return QDate(m_anchor.year(), m_anchor.month(), 1);
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
    case Period::Month: return QDate(m_anchor.year(), m_anchor.month(), m_anchor.daysInMonth());
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
    case Period::Month: return fmt::monthYear(m_anchor);
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
    case Period::Month: m_anchor = m_anchor.addMonths(dir); break;
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
    d.simple = !m_db->setting("stats_advanced", 0).toBool();
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
    if (d.ms.dayMark || d.ms.weekly || d.ms.streaks) {
        const auto all = stats::studyByDay(*m_db, QDate(2000, 1, 1), d.today, nullptr);
        d.todaySec = all.value(d.today);
        d.weekSec = ms::weekTotal(all, d.today);
        d.records = ms::records(all, d.today.addDays(1));
        if (d.weekSec > d.records.bestWeek) d.records.bestWeek = d.weekSec;   // la semana en curso también puede ser récord
        d.bestStreak = qMax(m_db->setting("ms/best_streak", 0).toInt(), 0);
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

QString StatsWidget::dayHtml(const QDate& day, bool brief) const
{
    const StatsData& d = m_canvas->data;
    const insights::DayStat s = d.daily.value(day);
    QStringList rows;
    rows << row(d.studyLabel, fmt::duration(int(s.study)), s.pomos > 0 ? QStringLiteral("%1 pomodoro%2").arg(s.pomos).arg(s.pomos == 1 ? "" : "s") : QString());
    rows << row(QStringLiteral("Recreos"), fmt::duration(int(s.breaks)));
    rows << row(QStringLiteral("Recreo indebido"), fmt::duration(int(s.undue)));
    rows << row(QStringLiteral("Inactivo"), fmt::hours(s.inactive));
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
    if (key == "undue") return QStringLiteral("<b>Recreos indebidos: %1</b><br>Tiempo muerto en medio de la jornada: pausas, demoras en arrancar y recreos pasados de largo.").arg(fmt::hours(d.totals.undue)) + more;
    if (key == "inactive") return QStringLiteral("<b>Horas inactivas: %1</b><br>Todo lo demás del período: dormir, comer, la vida.").arg(fmt::hours(d.totals.inactive)) + more;
    if (key == "streak") {
        const int left = qMax(0, stats::jokersToday(*m_db, d.today) - d.streak.jokersThisWeek);
        return QStringLiteral("<b>Racha: %1 %2</b>%3<br>Comodines: te %4 %5 esta semana · racha más larga: %6")
                   .arg(d.streak.days).arg(d.streak.days == 1 ? "día" : "días",
                        d.streak.since.isValid() ? QStringLiteral(" · desde el %1").arg(fmt::shortDate(d.streak.since)) : QString())
                   .arg(left == 1 ? "queda" : "quedan").arg(left).arg(d.bestStreak) + more;
    }
    if (key == "goals") return QStringLiteral("<b>Objetivos del período</b><br>Qué días se cumplió el objetivo y cuánto faltó en los que no.") + more;
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

void StatsWidget::showDetail(const QString& key)
{
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
        title = QStringLiteral("Recreos indebidos · %1").arg(m_label->text());
        intervalsDetail(IntervalKind::Undue, QStringLiteral("ratos"));
        html = para(dim(QStringLiteral("Tiempo muerto en medio de la jornada: pausas, demoras en arrancar el recreo o el próximo pomodoro, y recreos pasados de largo. "
                                       "Solo cuenta si después volviste a arrancar ese día; «Me voy» lo corta."))) + html;
    } else if (key == "inactive") {
        title = QStringLiteral("Horas inactivas · %1").arg(m_label->text());
        const qint64 all = d.totals.study + d.totals.breaks + d.totals.undue + d.totals.inactive;
        auto pct = [&](qint64 v) { return QStringLiteral("%1 %").arg(all ? qRound(100.0 * v / all) : 0); };
        rows << row(d.studyLabel, fmt::hours(d.totals.study), pct(d.totals.study)) << row(QStringLiteral("Recreos"), fmt::hours(d.totals.breaks), pct(d.totals.breaks))
             << row(QStringLiteral("Recreos indebidos"), fmt::hours(d.totals.undue), pct(d.totals.undue)) << row(QStringLiteral("Inactivas"), fmt::hours(d.totals.inactive), pct(d.totals.inactive));
        html = para(dim(QStringLiteral("Todo lo que no es pomodoro ni recreo: dormir, comer, cursar, la vida. Así se reparte el período:"))) + table(rows);
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
