#include "ui/EvolutionView.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Milestones.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextDocument>
#include <QToolTip>
#include <cmath>
#include <functional>

namespace {
constexpr double kPad = 16;
constexpr double kCardH = 168;
constexpr double kTilesH = 112;
constexpr double kGranH = 36;   // fila de pastillas Semanas | Meses | Años
constexpr double kProgH = 200;
constexpr double kRhythmH = 250;
constexpr double kGap = 14;
QColor ink() { return theme::pomoAccent; }

QString fmtH(double sec)   // "1h32" o "45m"
{
    return fmt::duration(qint64(std::llround(sec)));
}
QString pct(double v) { return QStringLiteral("%1%2 %").arg(v >= 0 ? QStringLiteral("+") : QStringLiteral("−")).arg(int(std::llround(std::abs(v) * 100))); }
} // namespace

EvolutionView::EvolutionView(QWidget* parent) : QWidget(parent)
{
    setMouseTracking(true);
}

void EvolutionView::setRange(int days)
{
    m_range = days;
}

void EvolutionView::reload(Database* db, const QSet<qint64>* filter)
{
    m_db = db;
    m_filterCopy = filter ? *filter : QSet<qint64>{};
    m_filter = filter ? &m_filterCopy : nullptr;
    m_range = db->setting("stats/evo_range", 90).toInt();
    const QDateTime now = QDateTime::currentDateTime();
    m_today = dayclock::logicalDate(now);
    // Desde el primer día con datos (o desde hace `m_range` días).
    QDate first = m_today;
    for (const QDate& d : db->studiedDays()) first = qMin(first, d);
    m_from = m_range > 0 ? m_today.addDays(-(m_range - 1)) : qMin(first, m_today.addDays(-27));
    if (m_range == 0 && m_from.daysTo(m_today) > 1500) m_from = m_today.addDays(-1500);

    const auto daily = insights::daily(*db, m_from, m_today, m_filter, now);
    m_dates.clear(); m_days.clear(); m_weeks.clear();
    for (QDate d = m_from; d <= m_today; d = d.addDays(1)) { m_dates << d; m_days << daily.value(d); }

    // Semanas (lunes a domingo) que tocan el rango.
    const auto goals = insights::weekGoals(*db);
    QMap<QDate, Week> weeks;
    for (int i = 0; i < m_dates.size(); ++i) {
        const QDate mon = fmt::mondayOf(m_dates[i]);
        Week& w = weeks[mon];
        w.monday = mon;
        ++w.days;
        w.study += m_days[i].study; w.undue += m_days[i].undue; w.inactive += m_days[i].inactive; w.breaks += m_days[i].breaks;
        const int lv = db->setting("ms/day/" + fmt::dayKey(m_dates[i]), 0).toInt();
        if (lv >= 1 && lv <= 3) ++w.marks[lv - 1];
    }
    for (auto it = weeks.begin(); it != weeks.end(); ++it) it->goalMin = goals.value(it.key(), -1);
    m_weeks = weeks.values().toVector();

    // Ventana de comparación: el período elegido (7, 15, 30…); en "Todo", hasta 30 días.
    m_first = first;
    m_cmpLen = m_range > 0 ? m_range : qBound(7, int(m_from.daysTo(m_today) + 1) / 2, 30);
    const auto cmp = insights::daily(*db, m_today.addDays(-2 * m_cmpLen + 1), m_today, m_filter, now);
    m_cmp.clear();
    for (QDate d = m_today.addDays(-2 * m_cmpLen + 1); d <= m_today; d = d.addDays(1)) m_cmp << cmp.value(d);

    QVector<double> series, cmpSeries;
    for (const auto& d : m_days) series << double(d.study);
    // Sólo cuentan los días desde el primero con datos (antes de usar la app no es "0 horas").
    for (int i = 0; i < m_cmp.size(); ++i)
        if (m_today.addDays(-(m_cmp.size() - 1 - i)) >= m_first) cmpSeries << double(m_cmp[i].study);
    m_trend = insights::trend(series, m_cmpLen);
    const auto ct = insights::trend(cmpSeries, m_cmpLen);
    m_trend.recent = ct.recent; m_trend.previous = ct.previous; m_trend.pct = ct.pct; m_trend.enough = ct.enough;
    // Buckets por período (siempre desde el primer día con datos, no depende del rango elegido).
    m_gran = db->setting("stats/evo_gran", "week").toString();
    {
        auto startOf = [&](const QDate& d) {
            if (m_gran == "month") return QDate(d.year(), d.month(), 1);
            if (m_gran == "year") return QDate(d.year(), 1, 1);
            return fmt::mondayOf(d);
        };
        auto next = [&](const QDate& s) {
            if (m_gran == "month") return s.addMonths(1);
            if (m_gran == "year") return s.addYears(1);
            return s.addDays(7);
        };
        const int maxN = m_gran == "year" ? 10 : 12;
        QDate st = startOf(m_today);
        for (int i = 1; i < maxN && startOf(m_first) < st; ++i) {
            const QDate prev = m_gran == "month" ? st.addMonths(-1) : m_gran == "year" ? st.addYears(-1) : st.addDays(-7);
            st = prev;
        }
        const auto bd = insights::daily(*db, st, m_today, m_filter, now);
        m_buckets.clear();
        for (QDate s0 = st; s0 <= m_today; s0 = next(s0)) {
            Bucket b; b.start = s0; b.end = next(s0).addDays(-1);
            b.complete = b.end < m_today;
            const QDate from = qMax(s0, m_first), to = qMin(b.end, m_today);
            b.days = from <= to ? int(from.daysTo(to)) + 1 : 0;
            for (QDate d = s0; d <= to; d = d.addDays(1)) { const auto it = bd.value(d); b.study += it.study; b.pomos += it.pomos; }
            m_buckets << b;
        }
    }
    // Ritmo semanal/horario: al menos los últimos 90 días, para que haya de qué hablar.
    {
        const QDate rf = qMin(m_from, m_today.addDays(-89));
        m_rhythmDays = int(rf.daysTo(m_today)) + 1;
        m_rhythm = insights::rhythm(*db, rf, m_today, m_filter);
    }
    m_diary = db->dayDiaries(m_from, m_today);
    m_summary = summaryHtml();
    setMinimumHeight(contentHeight(width()));
    update();
}

QString EvolutionView::cmpLabel() const
{
    if (m_cmpLen == 7) return QStringLiteral("semana anterior");
    if (m_cmpLen == 365) return QStringLiteral("12 meses antes");
    return QStringLiteral("%1 días antes").arg(m_cmpLen);
}

// Compara el período elegido con el anterior de igual largo, métrica por métrica.
QString EvolutionView::summaryHtml() const
{
    const int L = m_cmpLen;
    const int curDays = int(qMax<qint64>(0, m_first.daysTo(m_today) + 1));
    const int cn = qMin(L, curDays), pn = qMin(L, qMax(0, curDays - L));
    if (pn < 3) return QStringLiteral("Todavía no hay datos suficientes para comparar (hacen falta unos días antes del período).");
    auto avg = [&](auto get, int from, int to) { double s = 0; for (int i = from; i < to; ++i) s += get(m_cmp[i]); return s / qMax(1, to - from); };
    struct M { QString name; std::function<double(const insights::DayStat&)> get; bool lowerBetter; };
    const QVector<M> ms = {
        {m_studyLabel + QStringLiteral(" por día"), [](const insights::DayStat& d) { return double(d.study); }, false},
        {QStringLiteral("Recreo indebido"), [](const insights::DayStat& d) { return double(d.undue); }, true},
        {QStringLiteral("Horas inactivas"), [](const insights::DayStat& d) { return double(d.inactive); }, true},
        {QStringLiteral("Pomodoros por día"), [](const insights::DayStat& d) { return double(d.pomos); }, false},
    };
    const int N = int(m_cmp.size());
    QStringList lines;
    for (const M& m : ms) {
        const double a = avg(m.get, N - cn, N), b = avg(m.get, N - L - pn, N - L);
        if (b <= 0 && a <= 0) continue;
        const double ch = b > 0 ? (a - b) / b : 1.0;
        QString verdict, color;
        if (std::abs(ch) < 0.05) { verdict = QStringLiteral("parejo"); color = theme::textDim.name(); }
        else if ((ch > 0) != m.lowerBetter) { verdict = QStringLiteral("mejoró"); color = theme::breakAccent.name(); }
        else { verdict = QStringLiteral("empeoró"); color = theme::squareFinal.lighter(125).name(); }
        const bool isCount = m.name.startsWith(QLatin1String("Pomodoros"));
        const QString now = isCount ? QString::number(a, 'f', 1) : fmtH(a), was = isCount ? QString::number(b, 'f', 1) : fmtH(b);
        lines << QStringLiteral("<tr><td style='padding:2px 16px 2px 0'>%1</td><td style='padding:2px 12px 2px 0;color:%2'><b>%3</b></td>"
                                "<td style='padding:2px 0;color:%4'>%5 (antes %6, %7)</td></tr>")
                     .arg(m.name, color, verdict, theme::textDim.name(), now, was, b > 0 ? pct(ch) : QStringLiteral("nuevo"));
    }
    return QStringLiteral("<table cellspacing='0'>%1</table>").arg(lines.join(QString()));
}

int EvolutionView::contentHeight(int) const
{
    // pastillas + fichas + 5 tarjetas
    return int(8 + 40 + kGranH + kProgH + kGap + kTilesH + kGap + kRhythmH + kGap + 5 * (kCardH + kGap) + 20);
}

void EvolutionView::card(QPainter& p, const QRectF& r, const QString& title, const QString& sentence) const
{
    p.setPen(Qt::NoPen);
    p.setBrush(theme::surface);
    p.drawRoundedRect(r, 10, 10);
    p.setFont(theme::uiFont(8, QFont::DemiBold));
    p.setPen(theme::textDim);
    p.drawText(QPointF(r.left() + 14, r.top() + 20), title.toUpper());
    p.setFont(theme::uiFont(10.5, QFont::DemiBold));
    p.setPen(theme::text);
    p.drawText(QRectF(r.left() + 14, r.top() + 26, r.width() - 28, 22), Qt::AlignLeft | Qt::AlignVCenter,
               QFontMetrics(p.font()).elidedText(sentence, Qt::ElideRight, int(r.width() - 28)));
}

QRectF EvolutionView::plotArea(const QRectF& c) const
{
    return QRectF(c.left() + 44, c.top() + 58, c.width() - 44 - 16, c.height() - 58 - 26);
}

void EvolutionView::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), theme::bg);
    m_zones.clear();
    if (!m_db) return;
    const double L = kPad, W = width() - 2 * kPad;
    double y = 8;

    // Rango
    {
        static const int ranges[] = {7, 15, 30, 90, 365, 0};
        static const char* names[] = {"7 días", "15 días", "30 días", "90 días", "12 meses", "Todo"};
        p.setFont(theme::uiFont(8.5, QFont::DemiBold));
        double x = L;
        for (int k = 0; k < 6; ++k) {
            const double w = QFontMetricsF(p.font()).horizontalAdvance(QString::fromUtf8(names[k])) + 22;
            const QRectF pill(x, y, w, 24);
            const bool on = m_range == ranges[k];
            p.setPen(on ? Qt::NoPen : QPen(theme::line, 1));
            if (on) p.setBrush(theme::mix(theme::surface, ink(), 0.45)); else p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(pill, 12, 12);
            p.setPen(on ? theme::text : theme::textDim);
            p.drawText(pill, Qt::AlignCenter, QString::fromUtf8(names[k]));
            m_zones.append({pill, QString(), QStringLiteral("range:%1").arg(ranges[k])});
            x += w + 6;
        }
        p.setFont(theme::uiFont(8));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(x + 8, y, W - x, 24), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("%1 → %2").arg(fmt::shortDate(m_from), fmt::shortDate(m_today)));
        y += 40;
    }

    // Progreso por período
    {
        static const char* keys[] = {"week", "month", "year"};
        static const char* names[] = {"Semana a semana", "Mes a mes", "Año a año"};
        p.setFont(theme::uiFont(8.5, QFont::DemiBold));
        double x = L;
        for (int k = 0; k < 3; ++k) {
            const double w = QFontMetricsF(p.font()).horizontalAdvance(QString::fromUtf8(names[k])) + 22;
            const QRectF pill(x, y, w, 24);
            const bool on = m_gran == keys[k];
            p.setPen(on ? Qt::NoPen : QPen(theme::line, 1));
            if (on) p.setBrush(theme::mix(theme::surface, ink(), 0.45)); else p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(pill, 12, 12);
            p.setPen(on ? theme::text : theme::textDim);
            p.drawText(pill, Qt::AlignCenter, QString::fromUtf8(names[k]));
            m_zones.append({pill, QString(), QStringLiteral("gran:%1").arg(keys[k])});
            x += w + 6;
        }
        y += kGranH;
        paintProgress(p, QRectF(L, y, W, kProgH)); y += kProgH + kGap;
    }

    paintTiles(p, QRectF(L, y, W, kTilesH)); y += kTilesH + kGap;

    paintRhythm(p, QRectF(L, y, W, kRhythmH)); y += kRhythmH + kGap;
    paintHours(p, QRectF(L, y, W, kCardH)); y += kCardH + kGap;
    paintWeeklyBars(p, QRectF(L, y, W, kCardH), QStringLiteral("Recreo indebido por semana"), QString(),
                    [](const Week& w) { return double(w.undue); }, [](double v) { return fmtH(v); },
                    theme::squareFinal, QStringLiteral("indebido")); y += kCardH + kGap;
    paintWeeklyBars(p, QRectF(L, y, W, kCardH), QStringLiteral("Horas inactivas por día (promedio de cada semana)"), QString(),
                    [](const Week& w) { return w.days ? double(w.inactive) / w.days : 0.0; }, [](double v) { return QStringLiteral("%1 h").arg(v / 3600.0, 0, 'f', 1); },
                    theme::mix(theme::surface, theme::text, 0.7), QStringLiteral("inactivo")); y += kCardH + kGap;
    paintGoals(p, QRectF(L, y, W, kCardH)); y += kCardH + kGap;
    paintMood(p, QRectF(L, y, W, kCardH));
}


// Semana a semana (o mes a mes, año a año): total de cada período, % contra el anterior y tendencia.
void EvolutionView::paintProgress(QPainter& p, const QRectF& r)
{
    const QString unit = m_gran == "month" ? QStringLiteral("mes") : m_gran == "year" ? QStringLiteral("año") : QStringLiteral("semana");
    const QString units = m_gran == "month" ? QStringLiteral("meses") : m_gran == "year" ? QStringLiteral("años") : QStringLiteral("semanas");
    const QString title = m_gran == "month" ? QStringLiteral("Mes a mes") : m_gran == "year" ? QStringLiteral("Año a año") : QStringLiteral("Semana a semana");
    static const char* mon[] = {"ene","feb","mar","abr","may","jun","jul","ago","sep","oct","nov","dic"};
    auto labelOf = [&](const Bucket& b) {
        if (m_gran == "month") return QString::fromUtf8(mon[b.start.month() - 1]);
        if (m_gran == "year") return QString::number(b.start.year());
        return fmt::shortDate(b.start).section(' ', -2);
    };
    auto nameOf = [&](const Bucket& b) {
        if (m_gran == "month") return QStringLiteral("%1 %2").arg(QString::fromUtf8(mon[b.start.month() - 1])).arg(b.start.year());
        if (m_gran == "year") return QString::number(b.start.year());
        return QStringLiteral("Semana del %1").arg(fmt::shortDate(b.start));
    };
    // Períodos completos con datos: base de la tendencia.
    QVector<int> idx;
    for (int i = 0; i < m_buckets.size(); ++i) if (m_buckets[i].complete && m_buckets[i].days > 0) idx << i;
    QString sentence;
    double slope = 0, icpt = 0;
    const int n = int(idx.size());
    if (n >= 2) {
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (int k = 0; k < n; ++k) { const double v = double(m_buckets[idx[k]].study); sx += k; sy += v; sxx += double(k) * k; sxy += k * v; }
        const double den = n * sxx - sx * sx;
        slope = den != 0 ? (n * sxy - sx * sy) / den : 0;
        icpt = (sy - slope * sx) / n;
        const double last = double(m_buckets[idx[n - 1]].study), prev = double(m_buckets[idx[n - 2]].study);
        const QString lastTxt = QStringLiteral("%1 %2 completa: %3").arg(unit == "semana" || unit == "mes" ? QStringLiteral("Última") : QStringLiteral("Último"), unit, fmtH(last));
        const QString chg = prev > 0 ? QStringLiteral(" (%1 vs la anterior)").arg(pct((last - prev) / prev)) : QString();
        const double rel = sy > 0 ? slope / (sy / n) : 0;
        const QString tr = std::abs(rel) < 0.03 ? QStringLiteral("Tendencia: estable.")
            : QStringLiteral("Tendencia: %1%2 por %3.").arg(slope > 0 ? QStringLiteral("+") : QStringLiteral("−"), fmtH(std::abs(slope)), unit);
        sentence = lastTxt + chg + QStringLiteral(". ") + tr;
    } else {
        sentence = QStringLiteral("Hace falta al menos 2 %1 completos para ver la tendencia.").arg(units);
    }
    card(p, r, title, sentence);
    const QRectF a = plotArea(r);
    const int m = int(m_buckets.size());
    if (m == 0) return;
    double maxV = 3600;
    for (const Bucket& b : m_buckets) maxV = qMax(maxV, double(b.study));
    maxV *= 1.18;   // lugar para el % arriba de la barra
    p.setFont(theme::uiFont(7.5));
    for (int g = 0; g <= 3; ++g) {
        const double gy = a.bottom() - a.height() * g / 3.0;
        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(a.left(), gy), QPointF(a.right(), gy));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(r.left() + 4, gy - 7, 36, 14), Qt::AlignRight | Qt::AlignVCenter, fmtH(maxV * g / 3.0));
    }
    const double slot = a.width() / m;
    const double bw = qMin(60.0, qMax(4.0, slot * 0.62));
    for (int i = 0; i < m; ++i) {
        const Bucket& b = m_buckets[i];
        const double h = a.height() * double(b.study) / maxV;
        const double cx = a.left() + (i + 0.5) * slot;
        const QRectF bar(cx - bw / 2, a.bottom() - h, bw, h);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::mix(theme::surface, ink(), b.complete ? 0.62 : 0.3));
        if (b.study > 0) p.drawRoundedRect(bar, 3, 3);
        QString tip = QStringLiteral("%1%2\n%3 de estudio · %4 pomodoro(s)").arg(nameOf(b), b.complete ? QString() : QStringLiteral(" (en curso)"), fmtH(double(b.study))).arg(b.pomos);
        if (b.days > 0) tip += QStringLiteral("\nPromedio: %1 por día").arg(fmtH(double(b.study) / b.days));
        // % contra el anterior (solo entre períodos completos)
        if (b.complete && i > 0 && m_buckets[i - 1].complete && m_buckets[i - 1].study > 0 && b.days > 0) {
            const double ch = (double(b.study) - m_buckets[i - 1].study) / m_buckets[i - 1].study;
            p.setFont(theme::uiFont(7.5, QFont::DemiBold));
            p.setPen(std::abs(ch) < 0.05 ? theme::textDim : ch > 0 ? theme::breakAccent : theme::squareFinal.lighter(125));
            p.drawText(QRectF(cx - slot / 2, bar.top() - 15, slot, 13), Qt::AlignCenter, pct(ch));
            tip += QStringLiteral("\n%1 vs el anterior").arg(pct(ch));
        }
        m_zones.append({QRectF(a.left() + i * slot, a.top(), slot, a.height()), tip, QString()});
        p.setFont(theme::uiFont(7));
        p.setPen(theme::textFaint);
        if (m <= 14 || i % 2 == 0) p.drawText(QRectF(cx - slot / 2 - 10, a.bottom() + 3, slot + 20, 12), Qt::AlignCenter, labelOf(b));
    }
    if (n >= 2) {   // recta de tendencia sobre los períodos completos
        const auto pt = [&](int k) { return QPointF(a.left() + (idx[k] + 0.5) * slot, a.bottom() - a.height() * qBound(0.0, (icpt + slope * k) / maxV, 1.0)); };
        p.setPen(QPen(theme::text, 1.3, Qt::DashLine));
        p.drawLine(pt(0), pt(n - 1));
    }
    p.setFont(theme::uiFont(7.5));
    p.setPen(theme::textDim);
    p.drawText(QRectF(r.left() + 14, r.top() + 8, r.width() - 28, 14), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("horas de cada %1 (la clara sigue en curso)   - -  tendencia").arg(unit));
}

// Mapa de calor: día de la semana × hora del reloj.
void EvolutionView::paintRhythm(QPainter& p, const QRectF& r)
{
    static const char* dn[] = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
    static const char* dnFull[] = {"lunes", "martes", "miércoles", "jueves", "viernes", "sábados", "domingos"};
    const insights::Rhythm& R = m_rhythm;
    const int startH = dayclock::startHour();
    QString sentence;
    if (R.total < 3600) {
        sentence = QStringLiteral("Hace falta algo más de estudio registrado para ver tus horarios.");
    } else {
        // mejor franja de 2 horas seguidas y mejor día (promedio por día de ese tipo)
        int bestH = 0; qint64 bestV = -1;
        for (int k = 0; k < 24; ++k) {
            const int h = (startH + k) % 24, h2 = (h + 1) % 24;
            qint64 v = 0;
            for (int d = 0; d < 7; ++d) v += R.sec[d][h] + R.sec[d][h2];
            if (v > bestV) { bestV = v; bestH = h; }
        }
        int bestD = 0; double bestA = -1;
        for (int d = 0; d < 7; ++d) {
            const double a = R.dayCount[d] ? double(R.dayTotal[d]) / R.dayCount[d] : 0;
            if (a > bestA) { bestA = a; bestD = d; }
        }
        sentence = QStringLiteral("Rendís más entre las %1 y las %2 h, y los %3 (%4 por día).")
                       .arg(bestH).arg((bestH + 2) % 24 == 0 ? 24 : (bestH + 2) % 24).arg(QString::fromUtf8(dnFull[bestD]), fmtH(bestA));
    }
    card(p, r, QStringLiteral("Cuándo estudiás (últimos %1 días)").arg(m_rhythmDays), sentence);
    const double left = r.left() + 50, right = r.right() - 70, top = r.top() + 58, bottom = r.bottom() - 28;
    const double cw = (right - left) / 24.0, ch = (bottom - top) / 7.0;
    qint64 mx = 1;
    for (int d = 0; d < 7; ++d) for (int h = 0; h < 24; ++h) mx = qMax(mx, R.sec[d][h]);
    double mxAvg = 1;
    for (int d = 0; d < 7; ++d) if (R.dayCount[d]) mxAvg = qMax(mxAvg, double(R.dayTotal[d]) / R.dayCount[d]);
    for (int d = 0; d < 7; ++d) {
        p.setFont(theme::uiFont(8));
        p.setPen(theme::textDim);
        p.drawText(QRectF(r.left() + 10, top + d * ch, 36, ch), Qt::AlignRight | Qt::AlignVCenter, QString::fromUtf8(dn[d]));
        for (int k = 0; k < 24; ++k) {
            const int h = (startH + k) % 24;
            const QRectF c(left + k * cw + 1, top + d * ch + 1, cw - 2, ch - 2);
            const double t = double(R.sec[d][h]) / mx;
            p.setPen(Qt::NoPen);
            p.setBrush(R.sec[d][h] > 0 ? theme::mix(theme::surface, ink(), 0.18 + 0.82 * std::sqrt(t)) : theme::mix(theme::surface, theme::bg, 0.5));
            p.drawRoundedRect(c, 3, 3);
            m_zones.append({c, QStringLiteral("%1 de %2 a %3 h\n%4 en total (últimos %5 días)").arg(QString::fromUtf8(dn[d])).arg(h).arg(h + 1).arg(fmtH(double(R.sec[d][h]))).arg(m_rhythmDays), QString()});
        }
        // promedio por día de ese tipo, a la derecha
        const double avg = R.dayCount[d] ? double(R.dayTotal[d]) / R.dayCount[d] : 0;
        p.setPen(theme::textDim);
        p.setFont(theme::uiFont(8));
        p.drawText(QRectF(right + 8, top + d * ch, 60, ch), Qt::AlignLeft | Qt::AlignVCenter, avg > 0 ? fmtH(avg) : QStringLiteral("—"));
        m_zones.append({QRectF(right + 8, top + d * ch, 60, ch), QStringLiteral("Promedio de los %1: %2 por día").arg(QString::fromUtf8(dnFull[d]), fmtH(avg)), QString()});
    }
    p.setFont(theme::uiFont(7));
    p.setPen(theme::textFaint);
    for (int k = 0; k < 24; k += 2)
        p.drawText(QRectF(left + k * cw - 6, bottom + 4, cw + 12, 12), Qt::AlignCenter, QString::number((startH + k) % 24));
    p.drawText(QRectF(right + 8, r.top() + 42, 62, 14), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("por día"));
}

// Fichas: promedio diario del período elegido y variación contra el período anterior de igual largo.
void EvolutionView::paintTiles(QPainter& p, const QRectF& r)
{
    const int L = m_cmpLen, N = int(m_cmp.size());
    // Sólo cuentan los días desde el primero con datos.
    const int sinceFirst = int(qMax<qint64>(0, m_first.daysTo(m_today) + 1));
    const int cn = qMin(L, sinceFirst), pn = qMin(L, qMax(0, sinceFirst - L));
    struct M { QString name; std::function<double(const insights::DayStat&)> get; bool lowerBetter; bool count; };
    const QVector<M> ms = {
        {m_studyLabel + QStringLiteral(" / día"), [](const insights::DayStat& d) { return double(d.study); }, false, false},
        {QStringLiteral("Pomodoros / día"), [](const insights::DayStat& d) { return double(d.pomos); }, false, true},
        {QStringLiteral("Indebido / día"), [](const insights::DayStat& d) { return double(d.undue); }, true, false},
        {QStringLiteral("Inactivo / día"), [](const insights::DayStat& d) { return double(d.inactive); }, true, false},
    };
    auto sum = [&](const M& m, int from, int to) { double s = 0; for (int i = from; i < to; ++i) s += m.get(m_cmp[i]); return s; };
    auto fmtV = [&](const M& m, double v) { return m.count ? QString::number(v, 'f', 1) : fmtH(v); };
    const double gap = 8, w = (r.width() - 3 * gap) / 4;
    for (int k = 0; k < ms.size(); ++k) {
        const M& m = ms[k];
        const QRectF t(r.left() + k * (w + gap), r.top(), w, r.height());
        p.setPen(Qt::NoPen);
        p.setBrush(theme::surface);
        p.drawRoundedRect(t, 10, 10);
        p.setFont(theme::uiFont(8, QFont::DemiBold));
        p.setPen(theme::textDim);
        p.drawText(QRectF(t.left() + 12, t.top() + 8, t.width() - 24, 16), Qt::AlignLeft | Qt::AlignVCenter, m.name.toUpper());
        const double totA = cn > 0 ? sum(m, N - cn, N) : 0;
        const double a = cn > 0 ? totA / cn : 0;
        p.setFont(theme::uiFont(17, QFont::DemiBold));
        p.setPen(theme::text);
        p.drawText(QRectF(t.left() + 12, t.top() + 28, t.width() - 24, 32), Qt::AlignLeft | Qt::AlignVCenter, fmtV(m, a));
        QString line = QStringLiteral("sin comparación todavía"), line2;
        QColor col = theme::textFaint;
        double b = 0;
        if (pn >= 3) {
            const double totB = sum(m, N - L - pn, N - L);
            b = totB / pn;
            if (b > 0) {
                const double ch = (a - b) / b;
                if (std::abs(ch) < 0.05) { line = QStringLiteral("→ parejo"); col = theme::textDim; }
                else {
                    line = QStringLiteral("%1 %2 %").arg(ch > 0 ? QStringLiteral("↑") : QStringLiteral("↓")).arg(int(std::llround(std::abs(ch) * 100)));
                    col = ((ch > 0) != m.lowerBetter) ? theme::breakAccent : theme::squareFinal.lighter(125);
                }
                line += QStringLiteral(" vs %1").arg(cmpLabel());
                const double d = a - b;
                const QString ds = m.count ? QString::number(std::abs(d), 'f', 1) : fmtH(std::abs(d));
                line2 = QStringLiteral("%1%2 por día · total %3 (antes %4)").arg(d >= 0 ? QStringLiteral("+") : QStringLiteral("−"), ds, fmtV(m, totA).replace(QLatin1Char('.'), QLatin1Char('.')), fmtV(m, totB));
                if (m.count) line2 = QStringLiteral("%1%2 por día · total %3 (antes %4)").arg(d >= 0 ? QStringLiteral("+") : QStringLiteral("−"), ds).arg(int(std::llround(totA))).arg(int(std::llround(totB)));
            }
        }
        p.setFont(theme::uiFont(8.5));
        p.setPen(col);
        p.drawText(QRectF(t.left() + 12, t.top() + 62, t.width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(line, Qt::ElideRight, int(t.width() - 24)));
        if (!line2.isEmpty()) {
            p.setFont(theme::uiFont(8));
            p.setPen(theme::textDim);
            p.drawText(QRectF(t.left() + 12, t.top() + 80, t.width() - 24, 18), Qt::AlignLeft | Qt::AlignVCenter,
                       QFontMetrics(p.font()).elidedText(line2, Qt::ElideRight, int(t.width() - 24)));
        }
        m_zones.append({t, QStringLiteral("%1\nPeríodo: últimos %2 días (promedio por día: %3).\n%4").arg(m.name).arg(cn).arg(fmtV(m, a), line2.isEmpty() ? QStringLiteral("Sin período anterior con datos para comparar.") : line + QStringLiteral(" · ") + line2), QString()});
    }
}

// ---------------------------------------------------------------------------
void EvolutionView::paintHours(QPainter& p, const QRectF& r)
{
    const QString sentence = m_trend.enough
        ? QStringLiteral("%1 %2: %3 por día (antes %4).").arg(m_trend.sentence().section('.', 0, 0) + QLatin1Char('.'), m_cmpLen == 7 ? QStringLiteral("Última semana") : QStringLiteral("Últimos %1 días").arg(m_cmpLen), fmtH(m_trend.recent), fmtH(m_trend.previous))
        : m_trend.sentence();
    card(p, r, QStringLiteral("Horas con tendencia"), sentence);
    const QRectF a = plotArea(r);
    const int n = int(m_days.size());
    if (n == 0) return;
    const bool weekly = n > 100;   // muchos días: barras semanales
    QVector<double> v;
    QVector<QString> tips;
    if (weekly) {
        for (const Week& w : m_weeks) {
            v << (w.days ? double(w.study) / w.days : 0);
            tips << QStringLiteral("Semana del %1\n%2 por día (%3 en total)").arg(fmt::shortDate(w.monday), fmtH(v.last()), fmtH(double(w.study)));
        }
    } else {
        for (int i = 0; i < n; ++i) {
            v << double(m_days[i].study);
            tips << QStringLiteral("%1\n%2 · %3 pomodoro(s)").arg(fmt::longDate(m_dates[i]), fmtH(v.last())).arg(m_days[i].pomos);
        }
    }
    const int m = int(v.size());
    double maxV = 3600;
    for (double x : v) maxV = qMax(maxV, x);
    const auto ma = insights::movingAverage(v, weekly ? 4 : 7);
    for (double x : ma) maxV = qMax(maxV, x);
    // ejes
    p.setFont(theme::uiFont(7.5));
    for (int g = 0; g <= 3; ++g) {
        const double gy = a.bottom() - a.height() * g / 3.0;
        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(a.left(), gy), QPointF(a.right(), gy));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(r.left() + 4, gy - 7, 36, 14), Qt::AlignRight | Qt::AlignVCenter, fmtH(maxV * g / 3.0));
    }
    const double slot = a.width() / m;
    const double bw = qMax(1.5, slot * 0.7);
    for (int i = 0; i < m; ++i) {
        const double h = a.height() * v[i] / maxV;
        const QRectF bar(a.left() + i * slot + (slot - bw) / 2, a.bottom() - h, bw, h);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::mix(theme::surface, ink(), 0.34));
        if (v[i] > 0) p.drawRoundedRect(bar, qMin(2.0, bw / 2), qMin(2.0, bw / 2));
        m_zones.append({QRectF(a.left() + i * slot, a.top(), slot, a.height()), tips[i], QString()});
    }
    auto pt = [&](int i, double val) { return QPointF(a.left() + (i + 0.5) * slot, a.bottom() - a.height() * val / maxV); };
    // promedio móvil
    QPainterPath line;
    for (int i = 0; i < m; ++i) i == 0 ? line.moveTo(pt(i, ma[i])) : line.lineTo(pt(i, ma[i]));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(ink().lighter(125), 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(line);
    // recta de tendencia (sobre la misma serie que se ve)
    QVector<double> series = v;
    const insights::Trend t = insights::trend(series, weekly ? 4 : 28);
    if (m >= 8) {
        p.setPen(QPen(theme::text, 1.4, Qt::DashLine));
        p.drawLine(pt(0, qMax(0.0, t.intercept)), pt(m - 1, qMax(0.0, t.intercept + t.slope * (m - 1))));
    }
    // leyenda
    p.setFont(theme::uiFont(7.5));
    p.setPen(theme::textDim);
    p.drawText(QRectF(r.left() + 14, r.bottom() - 20, r.width() - 28, 16), Qt::AlignRight | Qt::AlignVCenter,
               weekly ? QStringLiteral("barras: promedio diario de cada semana   —  promedio de 4 semanas   - -  tendencia")
                      : QStringLiteral("barras: horas de cada día   —  promedio de 7 días   - -  tendencia"));
    if (m_trend.enough == false && qFuzzyIsNull(maxV - 3600) && m_trend.recent <= 0)
        p.drawText(a, Qt::AlignCenter, QStringLiteral("Sin horas en este período"));
}

void EvolutionView::paintWeeklyBars(QPainter& p, const QRectF& r, const QString& title, const QString&,
                                    const std::function<double(const Week&)>& value, const std::function<QString(double)>& label,
                                    const QColor& color, const QString&)
{
    // frase: últimas 4 semanas vs 4 anteriores
    QVector<double> v;
    for (const Week& w : m_weeks) v << value(w);
    QString sentence;
    if (v.size() >= 8) {
        double a = 0, b = 0;
        for (int i = 0; i < 4; ++i) { a += v[v.size() - 1 - i]; b += v[v.size() - 5 - i]; }
        a /= 4; b /= 4;
        const double ch = b > 0 ? (a - b) / b : 0;
        sentence = QStringLiteral("Últimas 4 semanas: %1 por semana (antes %2%3).").arg(label(a), label(b), b > 0 ? QStringLiteral(", ") + pct(ch) : QString());
    } else if (!v.isEmpty()) {
        sentence = QStringLiteral("Esta semana: %1.").arg(label(v.last()));
    } else {
        sentence = QStringLiteral("Sin datos todavía.");
    }
    // Para "inactivas" el valor ya es por día: la frase no dice "por semana".
    if (title.startsWith(QLatin1String("Horas inactivas"))) sentence.replace(QStringLiteral("por semana"), QStringLiteral("por día"));
    card(p, r, title, sentence);
    const QRectF a = plotArea(r);
    const int m = int(v.size());
    if (m == 0) return;
    double maxV = 1;
    for (double x : v) maxV = qMax(maxV, x);
    p.setFont(theme::uiFont(7.5));
    for (int g = 0; g <= 3; ++g) {
        const double gy = a.bottom() - a.height() * g / 3.0;
        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(a.left(), gy), QPointF(a.right(), gy));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(r.left() + 4, gy - 7, 36, 14), Qt::AlignRight | Qt::AlignVCenter, label(maxV * g / 3.0));
    }
    const double slot = a.width() / m;
    const double bw = qMax(3.0, slot * 0.66);
    const auto ma = insights::movingAverage(v, 4);
    QPainterPath line;
    for (int i = 0; i < m; ++i) {
        const double h = a.height() * v[i] / maxV;
        const QRectF bar(a.left() + i * slot + (slot - bw) / 2, a.bottom() - h, bw, h);
        p.setPen(Qt::NoPen);
        p.setBrush(theme::mix(theme::surface, color, 0.62));
        if (v[i] > 0) p.drawRoundedRect(bar, 2, 2);
        const QPointF q(a.left() + (i + 0.5) * slot, a.bottom() - a.height() * ma[i] / maxV);
        i == 0 ? line.moveTo(q) : line.lineTo(q);
        const Week& w = m_weeks[i];
        m_zones.append({QRectF(a.left() + i * slot, a.top(), slot, a.height()),
                        QStringLiteral("Semana del %1\n%2").arg(fmt::shortDate(w.monday), label(v[i])), QString()});
        if (m <= 30 || i % qMax(1, m / 12) == 0) {
            p.setFont(theme::uiFont(7));
            p.setPen(theme::textFaint);
            p.drawText(QRectF(a.left() + i * slot - 14, a.bottom() + 3, slot + 28, 12), Qt::AlignCenter, fmt::shortDate(w.monday).section(' ', -2));
        }
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme::text, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(line);
    p.setFont(theme::uiFont(7.5));
    p.setPen(theme::textDim);
    p.drawText(QRectF(r.left() + 14, r.bottom() - 18, r.width() - 28, 14), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("barras: cada semana   —  promedio móvil de 4 semanas"));
}

void EvolutionView::paintGoals(QPainter& p, const QRectF& r)
{
    // solo semanas completas (la actual se marca aparte)
    QVector<Week> ws;
    for (const Week& w : m_weeks) ws << w;
    int met = 0, withGoal = 0;
    for (const Week& w : ws) if (w.goalMin > 0 && w.monday < fmt::mondayOf(m_today)) { ++withGoal; if (w.study >= qint64(w.goalMin) * 60) ++met; }
    const QString sentence = withGoal > 0 ? QStringLiteral("Cumpliste la meta en %1 de %2 semanas con meta anotada.").arg(met).arg(withGoal)
                                          : QStringLiteral("El historial de metas empieza a anotarse desde que actualizaste a esta versión.");
    card(p, r, QStringLiteral("Metas semanales y marcas"), sentence);
    const QRectF a = plotArea(r);
    const int m = int(ws.size());
    if (m == 0) return;
    double maxV = 3600;
    for (const Week& w : ws) { maxV = qMax(maxV, double(w.study)); if (w.goalMin > 0) maxV = qMax(maxV, w.goalMin * 60.0); }
    p.setFont(theme::uiFont(7.5));
    for (int g = 0; g <= 3; ++g) {
        const double gy = a.bottom() - a.height() * g / 3.0;
        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(a.left(), gy), QPointF(a.right(), gy));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(r.left() + 4, gy - 7, 36, 14), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("%1 h").arg(maxV * g / 3.0 / 3600.0, 0, 'f', 0));
    }
    const double slot = a.width() / m;
    const double bw = qMax(4.0, slot * 0.62);
    static const QColor medal[3] = {QColor(0xc0, 0x7a, 0x45), QColor(0xc8, 0xc8, 0xc8), QColor(0xe2, 0xb0, 0x4a)};
    for (int i = 0; i < m; ++i) {
        const Week& w = ws[i];
        const bool current = w.monday == fmt::mondayOf(m_today);
        const bool ok = w.goalMin > 0 && w.study >= qint64(w.goalMin) * 60;
        const double h = a.height() * double(w.study) / maxV;
        const QRectF bar(a.left() + i * slot + (slot - bw) / 2, a.bottom() - h, bw, h);
        p.setPen(Qt::NoPen);
        p.setBrush(ok ? theme::mix(theme::surface, theme::breakAccent, 0.8) : theme::mix(theme::surface, ink(), current ? 0.5 : 0.34));
        if (w.study > 0) p.drawRoundedRect(bar, 2, 2);
        if (w.goalMin > 0) {   // la meta de esa semana: una rayita
            const double gy = a.bottom() - a.height() * (w.goalMin * 60.0) / maxV;
            p.setPen(QPen(theme::text, 1.6, Qt::SolidLine, Qt::FlatCap));
            p.drawLine(QPointF(a.left() + i * slot + 1, gy), QPointF(a.left() + (i + 1) * slot - 1, gy));
        }
        // marcas de la semana: puntitos abajo
        double dy = a.bottom() + 5;
        for (int k = 2; k >= 0; --k)
            for (int c = 0; c < w.marks[k]; ++c) {
                p.setPen(Qt::NoPen);
                p.setBrush(medal[k]);
                p.drawEllipse(QPointF(a.left() + (i + 0.5) * slot, dy + 3), 2.4, 2.4);
                dy += 6.5;
            }
        QString tip = QStringLiteral("Semana del %1\n%2").arg(fmt::shortDate(w.monday), fmtH(double(w.study)));
        if (w.goalMin > 0) tip += QStringLiteral(" de %1 · %2").arg(fmtH(w.goalMin * 60.0), ok ? QStringLiteral("cumplida") : (current ? QStringLiteral("en curso") : QStringLiteral("no cumplida")));
        else tip += QStringLiteral(" · sin meta anotada");
        const int total = w.marks[0] + w.marks[1] + w.marks[2];
        if (total) tip += QStringLiteral("\nMarcas: %1 bronce · %2 plata · %3 oro").arg(w.marks[0]).arg(w.marks[1]).arg(w.marks[2]);
        m_zones.append({QRectF(a.left() + i * slot, a.top(), slot, a.height() + 40), tip, QString()});
    }
    p.setFont(theme::uiFont(7.5));
    p.setPen(theme::textDim);
    p.drawText(QRectF(r.left() + 14, r.bottom() - 18, r.width() - 28, 14), Qt::AlignRight | Qt::AlignVCenter,
               QStringLiteral("verde = meta cumplida   —  meta de la semana   ●  marcas del día (bronce, plata, oro)"));
}

void EvolutionView::paintMood(QPainter& p, const QRectF& r)
{
    double sum = 0; int cnt = 0;
    for (auto it = m_diary.cbegin(); it != m_diary.cend(); ++it) if (it->mood > 0) { sum += it->mood; ++cnt; }
    const QString sentence = cnt > 0 ? QStringLiteral("Promedio de ánimo: %1 de 5 (%2 días anotados).").arg(sum / cnt, 0, 'f', 1).arg(cnt)
                                     : QStringLiteral("Todavía no anotaste cómo te fue. Se pregunta al cerrar el día, en la nota del objetivo.");
    card(p, r, QStringLiteral("Cómo te fue (diario)"), sentence);
    const QRectF a = plotArea(r);
    const int n = int(m_dates.size());
    p.setFont(theme::uiFont(7.5));
    for (int g = 1; g <= 5; ++g) {
        const double gy = a.bottom() - a.height() * (g - 1) / 4.0;
        p.setPen(QPen(theme::line, 1));
        p.drawLine(QPointF(a.left(), gy), QPointF(a.right(), gy));
        p.setPen(theme::textFaint);
        p.drawText(QRectF(r.left() + 4, gy - 7, 36, 14), Qt::AlignRight | Qt::AlignVCenter, QString::number(g));
    }
    if (cnt == 0 || n == 0) return;
    const double slot = a.width() / n;
    auto pt = [&](int i, double mood) { return QPointF(a.left() + (i + 0.5) * slot, a.bottom() - a.height() * (mood - 1) / 4.0); };
    QPainterPath line;
    bool started = false;
    for (int i = 0; i < n; ++i) {
        const auto d = m_diary.value(m_dates[i]);
        if (d.mood <= 0) continue;
        // promedio de la última semana (solo días anotados)
        double s = 0; int c = 0;
        for (int j = qMax(0, i - 6); j <= i; ++j) { const int mv = m_diary.value(m_dates[j]).mood; if (mv > 0) { s += mv; ++c; } }
        const QPointF q = pt(i, s / c);
        started ? line.lineTo(q) : line.moveTo(q);
        started = true;
        p.setPen(Qt::NoPen);
        p.setBrush(theme::mix(theme::surface, ink(), 0.55));
        p.drawEllipse(pt(i, d.mood), 3.2, 3.2);
        QString tip = QStringLiteral("%1\nÁnimo: %2 de 5").arg(fmt::longDate(m_dates[i])).arg(d.mood);
        if (!d.note.isEmpty()) tip += QStringLiteral("\n") + (d.note.size() > 120 ? d.note.left(120) + QStringLiteral("…") : d.note);
        m_zones.append({QRectF(pt(i, d.mood).x() - qMax(4.0, slot / 2), a.top(), qMax(8.0, slot), a.height()), tip, QString()});
    }
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(theme::text, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.drawPath(line);
    p.setFont(theme::uiFont(7.5));
    p.setPen(theme::textDim);
    p.drawText(QRectF(r.left() + 14, r.bottom() - 18, r.width() - 28, 14), Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("puntos: cada día   —  promedio de 7 días"));
}

// ---------------------------------------------------------------------------
void EvolutionView::mouseMoveEvent(QMouseEvent* e)
{
    for (const Zone& z : m_zones) {
        if (!z.rect.contains(e->position())) continue;
        setCursor(z.key.isEmpty() && z.tip.isEmpty() ? Qt::ArrowCursor : Qt::PointingHandCursor);   // Mejoras 4.1: el detalle sale con click
        if (m_hoverTips && !z.tip.isEmpty()) QToolTip::showText(e->globalPosition().toPoint(), z.tip, this, z.rect.toRect());
        return;
    }
    setCursor(Qt::ArrowCursor);
    QToolTip::hideText();
}

void EvolutionView::mouseReleaseEvent(QMouseEvent* e)
{
    if (e->button() != Qt::LeftButton) return;
    for (const Zone& z : m_zones)
        if (z.rect.contains(e->position()) && !z.tip.isEmpty()) {
            QToolTip::showText(e->globalPosition().toPoint(), z.tip, this, z.rect.toRect(), 8000);
            return;
        }
    for (const Zone& z : m_zones) {
        if (z.rect.contains(e->position()) && z.key.startsWith(QLatin1String("gran:"))) {
            m_db->setSetting("stats/evo_gran", z.key.mid(5));
            reload(m_db, m_filter);
            return;
        }
        if (!z.rect.contains(e->position()) || !z.key.startsWith(QLatin1String("range:"))) continue;
        m_db->setSetting("stats/evo_range", z.key.mid(6).toInt());
        reload(m_db, m_filter);
        return;
    }
}

void EvolutionView::leaveEvent(QEvent*) { QToolTip::hideText(); }
