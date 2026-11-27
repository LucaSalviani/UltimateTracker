#include "core/Report.h"

#include "core/DayClock.h"
#include "core/Export.h"
#include "core/FolderTree.h"
#include "core/Format.h"
#include "core/Insights.h"
#include "core/Notebook.h"
#include "core/Routine.h"

#include <QDir>
#include <QFile>
#include <QMap>
#include <algorithm>
#include <cmath>

namespace report {
namespace {

QString esc(const QString& s) { return s.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")); }
QString dur(double sec) { return fmt::duration(qint64(std::llround(sec))); }
QString pctText(double v) { return QStringLiteral("%1%2 %").arg(v >= 0 ? QStringLiteral("+") : QStringLiteral("−")).arg(int(std::llround(std::abs(v) * 100))); }
QString num(double v) { return QString::number(v, 'f', 1); }

const char* kMon[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
const char* kDay[] = {"lun", "mar", "mié", "jue", "vie", "sáb", "dom"};
const char* kDayFull[] = {"lunes", "martes", "miércoles", "jueves", "viernes", "sábados", "domingos"};

struct Bar { QString label, tip, pct; double v = 0; bool light = false; };

// Barras con tendencia opcional (recta de mínimos cuadrados sobre las completas).
QString svgBars(const QVector<Bar>& bars, int H = 230, bool trend = true)
{
    const int W = 900, L = 56, R = 12, T = 22, B = 26;
    if (bars.isEmpty()) return QStringLiteral("<p class='dim'>Sin datos.</p>");
    double mx = 3600;
    for (const Bar& b : bars) mx = qMax(mx, b.v);
    mx *= 1.12;
    const double pw = W - L - R, ph = H - T - B, slot = pw / bars.size(), bw = qMin(46.0, qMax(2.0, slot * 0.68));
    QString s = QStringLiteral("<svg viewBox='0 0 %1 %2' class='chart'>").arg(W).arg(H);
    for (int g = 0; g <= 4; ++g) {
        const double y = T + ph - ph * g / 4.0;
        s += QStringLiteral("<line x1='%1' x2='%2' y1='%3' y2='%3' class='grid'/><text x='%4' y='%5' class='ax' text-anchor='end'>%6</text>")
                 .arg(L).arg(W - R).arg(y, 0, 'f', 1).arg(L - 6).arg(y + 3, 0, 'f', 1).arg(dur(mx * g / 4.0));
    }
    QVector<int> full;
    for (int i = 0; i < bars.size(); ++i) {
        const Bar& b = bars[i];
        const double h = ph * b.v / mx, x = L + i * slot + (slot - bw) / 2;
        s += QStringLiteral("<rect x='%1' y='%2' width='%3' height='%4' rx='2' class='%5'><title>%6</title></rect>")
                 .arg(x, 0, 'f', 1).arg(T + ph - h, 0, 'f', 1).arg(bw, 0, 'f', 1).arg(h, 0, 'f', 1).arg(b.light ? "bar light" : "bar", esc(b.tip));
        if (!b.pct.isEmpty() && slot >= 28)
            s += QStringLiteral("<text x='%1' y='%2' class='pct %3' text-anchor='middle'>%4</text>")
                     .arg(x + bw / 2, 0, 'f', 1).arg(T + ph - h - 4, 0, 'f', 1).arg(b.pct.startsWith(QStringLiteral("+")) ? "up" : b.pct.startsWith(QStringLiteral("−")) ? "down" : "same").arg(b.pct);
        const int every = qMax(1, int(std::ceil(46.0 / slot)));
        if (i % every == 0)
            s += QStringLiteral("<text x='%1' y='%2' class='ax' text-anchor='middle'>%3</text>").arg(L + (i + 0.5) * slot, 0, 'f', 1).arg(H - 8).arg(esc(b.label));
        if (!b.light) full << i;
    }
    if (trend && full.size() >= 2) {
        const int n = int(full.size());
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (int k = 0; k < n; ++k) { sx += k; sy += bars[full[k]].v; sxx += double(k) * k; sxy += k * bars[full[k]].v; }
        const double den = n * sxx - sx * sx, sl = den != 0 ? (n * sxy - sx * sy) / den : 0, ic = (sy - sl * sx) / n;
        auto Y = [&](int k) { return T + ph - ph * qBound(0.0, (ic + sl * k) / mx, 1.0); };
        s += QStringLiteral("<line x1='%1' y1='%2' x2='%3' y2='%4' class='trend'/>")
                 .arg(L + (full.first() + 0.5) * slot, 0, 'f', 1).arg(Y(0), 0, 'f', 1).arg(L + (full.last() + 0.5) * slot, 0, 'f', 1).arg(Y(n - 1), 0, 'f', 1);
    }
    return s + QStringLiteral("</svg>");
}

// Línea de ánimo (1–5) por día.
QString svgMood(const QVector<QPair<QDate, int>>& pts)
{
    const int W = 900, H = 170, L = 30, R = 12, T = 12, B = 24;
    if (pts.size() < 2) return QStringLiteral("<p class='dim'>Todavía no hay suficientes días con ánimo anotado.</p>");
    const QDate d0 = pts.first().first, d1 = pts.last().first;
    const double span = qMax<qint64>(1, d0.daysTo(d1)), pw = W - L - R, ph = H - T - B;
    QString s = QStringLiteral("<svg viewBox='0 0 %1 %2' class='chart'>").arg(W).arg(H);
    for (int m = 1; m <= 5; ++m) {
        const double y = T + ph - ph * (m - 1) / 4.0;
        s += QStringLiteral("<line x1='%1' x2='%2' y1='%3' y2='%3' class='grid'/><text x='%4' y='%5' class='ax' text-anchor='end'>%6</text>").arg(L).arg(W - R).arg(y, 0, 'f', 1).arg(L - 6).arg(y + 3, 0, 'f', 1).arg(m);
    }
    QString path;
    for (const auto& p : pts) {
        const double x = L + pw * d0.daysTo(p.first) / span, y = T + ph - ph * (p.second - 1) / 4.0;
        path += QStringLiteral("%1%2,%3 ").arg(path.isEmpty() ? "M" : "L").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1);
        s += QStringLiteral("<circle cx='%1' cy='%2' r='2.6' class='dot'><title>%3: %4/5</title></circle>").arg(x, 0, 'f', 1).arg(y, 0, 'f', 1).arg(fmt::shortDate(p.first)).arg(p.second);
    }
    s += QStringLiteral("<path d='%1' class='line'/>").arg(path);
    s += QStringLiteral("<text x='%1' y='%2' class='ax'>%3</text><text x='%4' y='%2' class='ax' text-anchor='end'>%5</text>").arg(L).arg(H - 6).arg(fmt::shortDate(d0)).arg(W - R).arg(fmt::shortDate(d1));
    return s + QStringLiteral("</svg>");
}

// Mapa de calor: días en X, horas en Y (crecientes hacia arriba).
QString svgHeat(const insights::Rhythm& R, int startH)
{
    int k0 = 24, k1 = -1;
    for (int k = 0; k < 24; ++k) {
        qint64 v = 0;
        for (int d = 0; d < 7; ++d) v += R.sec[d][(startH + k) % 24];
        if (v > 0) { k0 = qMin(k0, k); k1 = qMax(k1, k); }
    }
    if (k1 < 0) return QStringLiteral("<p class='dim'>Todavía no hay horarios para mostrar.</p>");
    k0 = qMax(0, k0 - 1); k1 = qMin(23, k1 + 1);
    QVector<qint64> vals;
    for (int d = 0; d < 7; ++d) for (int h = 0; h < 24; ++h) if (R.sec[d][h] > 0) vals << R.sec[d][h];
    std::sort(vals.begin(), vals.end());
    const int rows = k1 - k0 + 1, W = 900, L = 50, T = 26, cwid = (W - L - 10) / 7, rh = 22, H = T + rows * rh + 28;
    QString s = QStringLiteral("<svg viewBox='0 0 %1 %2' class='chart'>").arg(W).arg(H);
    for (int d = 0; d < 7; ++d) {
        const double avg = R.dayCount[d] ? double(R.dayTotal[d]) / R.dayCount[d] : 0;
        s += QStringLiteral("<text x='%1' y='16' class='ax' text-anchor='middle'>%2/día</text><text x='%1' y='%3' class='ax b' text-anchor='middle'>%4</text>")
                 .arg(L + d * cwid + cwid / 2).arg(avg > 0 ? dur(avg) : QStringLiteral("—")).arg(H - 8).arg(kDay[d]);
        for (int k = k0; k <= k1; ++k) {
            const int h = (startH + k) % 24;
            const qint64 v = R.sec[d][h];
            double t = 0;
            if (v > 0 && vals.size() > 1) t = double(std::lower_bound(vals.begin(), vals.end(), v) - vals.begin()) / (vals.size() - 1);
            else if (v > 0) t = 1;
            s += QStringLiteral("<rect x='%1' y='%2' width='%3' height='%4' rx='3' style='%5'><title>%6 de %7 a %8 h: %9</title></rect>")
                     .arg(L + d * cwid + 1).arg(T + (k1 - k) * rh + 1).arg(cwid - 2).arg(rh - 2)
                     .arg(v > 0 ? QStringLiteral("fill:color-mix(in srgb, var(--accent) %1%, var(--surface2))").arg(int(15 + 85 * t)) : QStringLiteral("fill:var(--surface2)"))
                     .arg(kDay[d]).arg(h).arg(h + 1).arg(dur(double(v)));
        }
    }
    for (int k = k0; k <= k1; ++k)
        s += QStringLiteral("<text x='%1' y='%2' class='ax' text-anchor='end'>%3 h</text>").arg(L - 6).arg(T + (k1 - k) * rh + 15).arg((startH + k) % 24);
    return s + QStringLiteral("</svg>");
}

QString table(const QStringList& head, const QVector<QStringList>& rows, const QString& cls = QString())
{
    QString s = QStringLiteral("<table class='%1'><thead><tr>").arg(cls);
    for (const QString& h : head) s += QStringLiteral("<th>%1</th>").arg(esc(h));
    s += QStringLiteral("</tr></thead><tbody>");
    for (const QStringList& r : rows) {
        s += QStringLiteral("<tr>");
        for (const QString& c : r) s += QStringLiteral("<td>%1</td>").arg(c);   // ya escapado por quien arma la fila
        s += QStringLiteral("</tr>");
    }
    return s + QStringLiteral("</tbody></table>");
}

struct Period { QString label; QDate start, end; qint64 study = 0, breaks = 0, undue = 0; int pomos = 0, days = 0, studied = 0; bool complete = false; };

QVector<Period> buildPeriods(const QMap<QDate, insights::DayStat>& daily, const QDate& first, const QDate& today, const QString& gran)
{
    auto startOf = [&](const QDate& d) { return gran == "month" ? QDate(d.year(), d.month(), 1) : gran == "year" ? QDate(d.year(), 1, 1) : fmt::mondayOf(d); };
    auto next = [&](const QDate& s) { return gran == "month" ? s.addMonths(1) : gran == "year" ? s.addYears(1) : s.addDays(7); };
    QVector<Period> out;
    for (QDate s = startOf(first); s <= today; s = next(s)) {
        Period p; p.start = s; p.end = next(s).addDays(-1); p.complete = p.end < today;
        p.label = gran == "month" ? QStringLiteral("%1 %2").arg(QString::fromUtf8(kMon[s.month() - 1])).arg(s.year()) : gran == "year" ? QString::number(s.year()) : fmt::shortDate(s);
        for (QDate d = qMax(s, first); d <= qMin(p.end, today); d = d.addDays(1)) {
            const insights::DayStat st = daily.value(d);
            p.study += st.study; p.breaks += st.breaks; p.undue += st.undue; p.pomos += st.pomos; ++p.days;
            if (st.study > 0) ++p.studied;
        }
        out << p;
    }
    return out;
}

QVector<Bar> toBars(const QVector<Period>& ps, int maxN, const QString& unit)
{
    QVector<Bar> out;
    const int from = qMax(0, int(ps.size()) - maxN);
    for (int i = from; i < ps.size(); ++i) {
        Bar b; b.label = ps[i].label; b.v = double(ps[i].study); b.light = !ps[i].complete;
        if (ps[i].complete && i > 0 && ps[i - 1].study > 0 && ps[i - 1].complete) b.pct = pctText((double(ps[i].study) - ps[i - 1].study) / ps[i - 1].study);
        b.tip = QStringLiteral("%1 %2: %3 · %4 pomodoros%5").arg(unit, ps[i].label, dur(double(ps[i].study))).arg(ps[i].pomos).arg(ps[i].complete ? QString() : QStringLiteral(" (en curso)"));
        out << b;
    }
    return out;
}

QVector<QStringList> periodRows(const QVector<Period>& ps)
{
    QVector<QStringList> rows;
    for (int i = ps.size() - 1; i >= 0; --i) {
        const Period& p = ps[i];
        QString ch = QStringLiteral("—");
        if (i > 0 && ps[i - 1].study > 0 && p.complete && ps[i - 1].complete) ch = pctText((double(p.study) - ps[i - 1].study) / ps[i - 1].study);
        rows << QStringList{esc(p.label) + (p.complete ? QString() : QStringLiteral(" <span class='dim'>(en curso)</span>")), dur(double(p.study)), QString::number(p.pomos),
                            p.studied ? dur(double(p.study) / p.studied) : QStringLiteral("—"), QString::number(p.studied), dur(double(p.breaks)), dur(double(p.undue)), ch};
    }
    return rows;
}

QString css()
{
    return QStringLiteral(R"(
:root{--bg:#1c1c1e;--surface:#262628;--surface2:#303033;--line:#3a3a3d;--text:#ececec;--dim:#9a9a9f;--accent:#e0605a;--up:#46c9a0;--down:#ef6b64}
@media print{:root{--bg:#fff;--surface:#f4f4f5;--surface2:#e6e6e8;--line:#d4d4d8;--text:#18181b;--dim:#6b6b72}.card{break-inside:avoid}}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--text);font:14px/1.5 'Segoe UI',system-ui,sans-serif}
main{max-width:980px;margin:0 auto;padding:28px 20px 60px}h1{font-size:26px;margin:0 0 4px}h2{font-size:13px;letter-spacing:.08em;text-transform:uppercase;color:var(--dim);margin:0 0 10px}
.sub,.dim{color:var(--dim)}.card{background:var(--surface);border-radius:12px;padding:18px 20px;margin:16px 0}
.lead{font-size:16px;font-weight:600;margin:0 0 10px}
.tiles{display:grid;grid-template-columns:repeat(auto-fit,minmax(150px,1fr));gap:10px}.tile{background:var(--surface2);border-radius:10px;padding:12px 14px}.tile b{display:block;font-size:22px}.tile span{color:var(--dim);font-size:12px}
.chart{width:100%;height:auto}.grid{stroke:var(--line);stroke-width:1}.ax{fill:var(--dim);font-size:10px}.ax.b{font-weight:700;fill:var(--text);font-size:11px}
.bar{fill:var(--accent);opacity:.8}.bar.light{opacity:.35}.trend{stroke:var(--text);stroke-width:1.5;stroke-dasharray:5 4}.line{fill:none;stroke:var(--accent);stroke-width:1.6}.dot{fill:var(--accent)}
.pct{font-size:10px;font-weight:700}.pct.up{fill:var(--up)}.pct.down{fill:var(--down)}.pct.same{fill:var(--dim)}
table{width:100%;border-collapse:collapse;font-size:13px}th{text-align:left;color:var(--dim);font-weight:600;border-bottom:1px solid var(--line);padding:6px 8px}td{padding:6px 8px;border-bottom:1px solid var(--line);vertical-align:top}
tr:last-child td{border-bottom:0}.note{white-space:normal;max-width:520px}.mood{font-weight:700}
.page{border-top:1px solid var(--line);padding:12px 0}.page h3{margin:0 0 4px;font-size:15px}.page div{white-space:pre-wrap}
details summary{cursor:pointer;color:var(--dim)}
)");
}

} // namespace

QString html(const Database& db, const QDateTime& now)
{
    const QDate today = dayclock::logicalDate(now);
    const FolderTree tree(db.folders());
    const QVector<Pomodoro> pomos = db.pomodorosBetween(QDate(2000, 1, 1), today);
    QDate first = today;
    for (const Pomodoro& p : pomos) if (p.studiedSec > 0) first = qMin(first, p.day);
    const auto diaries = db.dayDiaries(QDate(2000, 1, 1), today);
    for (auto it = diaries.cbegin(); it != diaries.cend(); ++it) first = qMin(first, it.key());
    const auto dailyMap = insights::daily(db, first, today, nullptr, now);
    QMap<QDate, insights::DayStat> daily;
    for (QDate d = first; d <= today; d = d.addDays(1)) daily[d] = dailyMap.value(d);

    // ---- totales
    qint64 study = 0, breaks = 0, undue = 0, paused = 0; int nPomos = 0, studiedDays = 0, sessions = 0;
    QDate bestDay; qint64 bestDayV = 0;
    for (auto it = daily.cbegin(); it != daily.cend(); ++it) {
        study += it->study; breaks += it->breaks; undue += it->undue; paused += it->paused; nPomos += it->pomos; sessions += it->sessions;
        if (it->study > 0) ++studiedDays;
        if (it->study > bestDayV) { bestDayV = it->study; bestDay = it.key(); }
    }
    double moodSum = 0; int moodN = 0;
    for (auto it = diaries.cbegin(); it != diaries.cend(); ++it) if (it->mood >= 1 && it->mood <= 5) { moodSum += it->mood; ++moodN; }
    const auto weeks = buildPeriods(daily, first, today, QStringLiteral("week"));
    const auto months = buildPeriods(daily, first, today, QStringLiteral("month"));
    const auto years = buildPeriods(daily, first, today, QStringLiteral("year"));
    const Period* bestWeek = nullptr;
    for (const Period& w : weeks) if (!bestWeek || w.study > bestWeek->study) bestWeek = &w;

    // ---- actividades
    QHash<qint64, qint64> direct, directN;
    for (const Pomodoro& p : pomos) { direct[p.folderId] += p.studiedSec; directN[p.folderId] += 1; }
    const auto rolled = tree.rollUp(direct);

    QString h;
    h += QStringLiteral("<!doctype html><html lang='es'><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'><title>UltimateTracker · informe</title><style>%1</style></head><body><main>").arg(css());
    h += QStringLiteral("<h1>UltimateTracker · informe completo</h1><div class='sub'>Generado el %1 · %2 → %3 · todo el historial</div>")
             .arg(fmt::longDate(now.date()), first <= today ? fmt::longDate(first) : QStringLiteral("—"), fmt::longDate(today));

    // Resumen
    h += QStringLiteral("<section class='card'><h2>Resumen</h2><div class='tiles'>");
    auto tile = [&](const QString& v, const QString& l) { h += QStringLiteral("<div class='tile'><b>%1</b><span>%2</span></div>").arg(v, l); };
    tile(dur(double(study)), QStringLiteral("estudio total (%1 h)").arg(num(study / 3600.0)));
    tile(QString::number(studiedDays), QStringLiteral("días con estudio"));
    tile(QString::number(nPomos), QStringLiteral("pomodoros"));
    tile(studiedDays ? dur(double(study) / studiedDays) : QStringLiteral("—"), QStringLiteral("promedio por día con estudio"));
    tile(QString::number(sessions), QStringLiteral("sesiones"));
    tile(dur(double(breaks)), QStringLiteral("en recreos"));
    tile(dur(double(undue)), QStringLiteral("tiempo muerto"));
    tile(dur(double(paused)), QStringLiteral("recreo indebido (pausas)"));
    tile(bestDayV ? dur(double(bestDayV)) : QStringLiteral("—"), bestDayV ? QStringLiteral("mejor día (%1)").arg(esc(fmt::shortDate(bestDay))) : QStringLiteral("mejor día"));
    tile(bestWeek && bestWeek->study ? dur(double(bestWeek->study)) : QStringLiteral("—"), QStringLiteral("mejor semana"));
    tile(moodN ? QStringLiteral("%1 / 5").arg(num(moodSum / moodN)) : QStringLiteral("—"), QStringLiteral("ánimo promedio (%1 días)").arg(moodN));
    tile(QStringLiteral("%1 %").arg(num(study / 36000000.0 * 100)), QStringLiteral("camino a las 10.000 h"));
    h += QStringLiteral("</div></section>");

    // Evolución semana a semana
    auto trendSentence = [&](const QVector<Period>& ps, const QString& unit) {
        QVector<double> v;
        for (const Period& p : ps) if (p.complete) v << double(p.study);
        if (v.size() < 2) return QStringLiteral("Hacen falta al menos 2 %1 completos para ver la tendencia.").arg(unit);
        const int n = int(v.size());
        double sx = 0, sy = 0, sxx = 0, sxy = 0;
        for (int k = 0; k < n; ++k) { sx += k; sy += v[k]; sxx += double(k) * k; sxy += k * v[k]; }
        const double den = n * sxx - sx * sx, sl = den != 0 ? (n * sxy - sx * sy) / den : 0;
        const double last = v[n - 1], prev = v[n - 2];
        QString t = QStringLiteral("Último período completo: %1").arg(dur(last));
        if (prev > 0) t += QStringLiteral(" (%1 vs el anterior)").arg(pctText((last - prev) / prev));
        t += std::abs(sl) < 0.03 * (sy / n) ? QStringLiteral(". Tendencia: estable.") : QStringLiteral(". Tendencia: %1%2 por período.").arg(sl > 0 ? QStringLiteral("+") : QStringLiteral("−"), dur(std::abs(sl)));
        return t;
    };
    const QStringList pHead{"Período", "Estudio", "Pomodoros", "Promedio/día con estudio", "Días con estudio", "Recreos", "Tiempo muerto", "vs anterior"};
    h += QStringLiteral("<section class='card'><h2>Semana a semana</h2><p class='lead'>%1</p>%2<details><summary>Ver todas las semanas (%3)</summary>%4</details></section>")
             .arg(esc(trendSentence(weeks, QStringLiteral("semanas"))), svgBars(toBars(weeks, 26, QStringLiteral("Semana del"))), QString::number(weeks.size()), table(pHead, periodRows(weeks)));
    h += QStringLiteral("<section class='card'><h2>Mes a mes</h2><p class='lead'>%1</p>%2%3</section>")
             .arg(esc(trendSentence(months, QStringLiteral("meses"))), svgBars(toBars(months, 24, QStringLiteral("Mes"))), table(pHead, periodRows(months)));
    h += QStringLiteral("<section class='card'><h2>Año a año</h2>%1%2</section>").arg(svgBars(toBars(years, 10, QStringLiteral("Año"))), table(pHead, periodRows(years)));

    // Día a día (últimos 180 días)
    {
        QVector<Bar> bars;
        const QDate from = qMax(first, today.addDays(-179));
        for (QDate d = from; d <= today; d = d.addDays(1)) {
            Bar b; b.v = double(daily.value(d).study); b.label = QStringLiteral("%1/%2").arg(d.day()).arg(d.month()); b.light = d == today;
            b.tip = QStringLiteral("%1: %2 · %3 pomodoros").arg(fmt::longDate(d), dur(b.v)).arg(daily.value(d).pomos);
            bars << b;
        }
        h += QStringLiteral("<section class='card'><h2>Día a día (últimos %1 días)</h2>%2</section>").arg(bars.size()).arg(svgBars(bars, 230, true));
    }

    // Cuándo estudiás
    {
        const auto R = insights::rhythm(db, first, today, nullptr);
        QString sent;
        if (R.total >= 3600) {
            int bestD = 0; double bestA = -1;
            for (int d = 0; d < 7; ++d) { const double a = R.dayCount[d] ? double(R.dayTotal[d]) / R.dayCount[d] : 0; if (a > bestA) { bestA = a; bestD = d; } }
            int bestH = 0; qint64 bestV = -1;
            for (int k = 0; k < 24; ++k) { const int hh = (dayclock::startHour() + k) % 24; qint64 v = 0; for (int d = 0; d < 7; ++d) v += R.sec[d][hh] + R.sec[d][(hh + 1) % 24]; if (v > bestV) { bestV = v; bestH = hh; } }
            sent = QStringLiteral("Rendís más entre las %1 y las %2 h, y los %3 (%4 por día).").arg(bestH).arg((bestH + 2) % 24 == 0 ? 24 : (bestH + 2) % 24).arg(QString::fromUtf8(kDayFull[bestD]), dur(bestA));
        }
        h += QStringLiteral("<section class='card'><h2>Cuándo estudiás</h2>%1%2</section>").arg(sent.isEmpty() ? QString() : QStringLiteral("<p class='lead'>%1</p>").arg(esc(sent)), svgHeat(R, dayclock::startHour()));
    }

    // Actividades
    {
        QVector<QStringList> rows;
        QVector<qint64> ids;
        for (const Folder& f : tree.all()) ids << f.id;
        std::sort(ids.begin(), ids.end(), [&](qint64 a, qint64 b) { return rolled.value(a) > rolled.value(b); });
        const double top = ids.isEmpty() ? 1 : qMax<double>(1, rolled.value(ids.first()));
        for (qint64 id : ids) {
            if (rolled.value(id) <= 0 && direct.value(id) <= 0) continue;
            const double w = 100.0 * rolled.value(id) / top;
            rows << QStringList{esc(tree.path(id, QStringLiteral(" / "))), dur(double(rolled.value(id))), dur(double(direct.value(id))), QString::number(directN.value(id)),
                                QStringLiteral("%1 %").arg(study > 0 ? num(100.0 * rolled.value(id) / study) : QStringLiteral("0")),
                                QStringLiteral("<div style='width:160px'><div style='height:8px;border-radius:4px;background:var(--accent);opacity:.75;width:%1%'></div></div>").arg(w, 0, 'f', 1)};
        }
        const qint64 noFolder = direct.value(0);
        if (noFolder > 0) rows << QStringList{QStringLiteral("Sin actividad"), dur(double(noFolder)), dur(double(noFolder)), QString::number(directN.value(0)), QString(), QString()};
        h += QStringLiteral("<section class='card'><h2>Por actividad</h2>%1</section>")
                 .arg(rows.isEmpty() ? QStringLiteral("<p class='dim'>Sin actividades con horas.</p>") : table({"Actividad", "Total (con subactividades)", "Directo", "Pomodoros", "% del total", ""}, rows));
    }

    // Ánimo + diario
    {
        QVector<QPair<QDate, int>> pts;
        QList<QDate> keys = diaries.keys();
        std::sort(keys.begin(), keys.end());
        for (const QDate& d : keys) if (diaries[d].mood >= 1 && diaries[d].mood <= 5) pts.append({d, diaries[d].mood});
        QString moodByDay;
        {
            double sum[7] = {}; int cnt[7] = {};
            for (const auto& p : pts) { sum[p.first.dayOfWeek() - 1] += p.second; ++cnt[p.first.dayOfWeek() - 1]; }
            QStringList parts;
            for (int d = 0; d < 7; ++d) if (cnt[d]) parts << QStringLiteral("%1 %2").arg(QString::fromUtf8(kDay[d]), num(sum[d] / cnt[d]));
            if (!parts.isEmpty()) moodByDay = QStringLiteral("<p class='dim'>Ánimo promedio por día de la semana: %1.</p>").arg(parts.join(QStringLiteral(" · ")));
        }
        h += QStringLiteral("<section class='card'><h2>Cómo te fue (ánimo)</h2>%1%2</section>").arg(svgMood(pts), moodByDay);

        QVector<QStringList> rows;
        for (int i = keys.size() - 1; i >= 0; --i) {
            const QDate d = keys[i];
            const auto& dd = diaries[d];
            if (dd.note.trimmed().isEmpty() && dd.mood == 0) continue;
            const insights::DayStat st = daily.value(d);
            rows << QStringList{esc(fmt::longDate(d)), dd.mood ? QStringLiteral("<span class='mood'>%1/5</span>").arg(dd.mood) : QStringLiteral("—"),
                                st.study ? dur(double(st.study)) : QStringLiteral("—"), QStringLiteral("<div class='note'>%1</div>").arg(esc(dd.note))};
        }
        h += QStringLiteral("<section class='card'><h2>Diario (%1 días)</h2>%2</section>").arg(rows.size()).arg(rows.isEmpty() ? QStringLiteral("<p class='dim'>Todavía no hay notas ni ánimo anotados.</p>") : table({"Día", "Ánimo", "Estudio", "Nota"}, rows));
    }

    // Notas de pomodoros
    {
        QVector<Pomodoro> withNote;
        for (const Pomodoro& p : pomos) if (!p.note.trimmed().isEmpty()) withNote << p;
        std::sort(withNote.begin(), withNote.end(), [](const Pomodoro& a, const Pomodoro& b) { return a.startedAt > b.startedAt; });
        QVector<QStringList> rows;
        for (const Pomodoro& p : withNote)
            rows << QStringList{esc(fmt::shortDate(p.day)), esc(tree.find(p.folderId) ? tree.path(p.folderId, QStringLiteral(" / ")) : QStringLiteral("Sin actividad")), dur(p.studiedSec), QStringLiteral("<div class='note'>%1</div>").arg(esc(p.note))};
        h += QStringLiteral("<section class='card'><h2>Notas de pomodoros (%1)</h2>%2</section>").arg(rows.size()).arg(rows.isEmpty() ? QStringLiteral("<p class='dim'>Ningún pomodoro tiene nota.</p>") : table({"Día", "Actividad", "Duración", "Nota"}, rows));
    }

    // Libreta
    {
        const auto pages = notebook::pages(db);
        QString body;
        for (const auto& pg : pages) body += QStringLiteral("<div class='page'><h3>%1%2</h3><div>%3</div></div>").arg(pg.pinned ? QStringLiteral("📌 ") : QString(), esc(pg.title.isEmpty() ? QStringLiteral("(sin título)") : pg.title), esc(pg.body));
        h += QStringLiteral("<section class='card'><h2>Libreta (%1 páginas)</h2>%2</section>").arg(pages.size()).arg(body.isEmpty() ? QStringLiteral("<p class='dim'>La libreta está vacía.</p>") : body);
    }

    // Rutina
    {
        QVector<QStringList> rows;
        auto hm = [](int m) { return QStringLiteral("%1:%2").arg(m / 60 % 24, 2, 10, QLatin1Char('0')).arg(m % 60, 2, 10, QLatin1Char('0')); };
        for (const auto& e : routine::events(db))
            rows << QStringList{esc(e.title), esc(e.category), QStringLiteral("%1 – %2").arg(hm(e.startMin), hm(e.endMin)), e.weekdays ? esc(routine::weekdaysText(e.weekdays)) : esc(fmt::shortDate(e.day))};
        if (!rows.isEmpty()) h += QStringLiteral("<section class='card'><h2>Rutina</h2>%1</section>").arg(table({"Actividad", "Categoría", "Horario", "Cuándo"}, rows));
    }

    h += QStringLiteral("<p class='dim' style='text-align:center;margin-top:30px'>Los CSV de esta carpeta (pomodoros, días, actividades) abren directo en Excel. Para guardar este informe como PDF: imprimir → «Guardar como PDF».</p></main></body></html>");
    return h;
}

QString daysCsv(const Database& db, const QDateTime& now)
{
    const QDate today = dayclock::logicalDate(now);
    QDate first = today;
    for (const Pomodoro& p : db.pomodorosBetween(QDate(2000, 1, 1), today)) first = qMin(first, p.day);
    const auto diaries = db.dayDiaries(QDate(2000, 1, 1), today);
    for (auto it = diaries.cbegin(); it != diaries.cend(); ++it) first = qMin(first, it.key());
    const auto daily = insights::daily(db, first, today, nullptr, now);
    auto m = [](qint64 s) { return QString::number(s / 60.0, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(',')); };
    QString out = QString::fromUtf8("\xEF\xBB\xBF");
    out += QStringLiteral("fecha;día;estudio (min);pomodoros;recreos (min);tiempo muerto (min);recreo indebido (min);inactivo (min);sesiones;ánimo (1-5);nota del día\r\n");
    for (QDate d = first; d <= today; d = d.addDays(1)) {
        const insights::DayStat s = daily.value(d);
        const auto dd = diaries.value(d);
        QStringList cells{d.toString(Qt::ISODate), QString::fromUtf8(kDay[d.dayOfWeek() - 1]), m(s.study), QString::number(s.pomos), m(s.breaks), m(s.undue), m(s.paused), m(s.inactive),
                          QString::number(s.sessions), dd.mood ? QString::number(dd.mood) : QString(), dd.note};
        for (QString& c : cells) c = csvexport::escape(c);
        out += cells.join(QLatin1Char(';')) + QStringLiteral("\r\n");
    }
    return out;
}

QString activitiesCsv(const Database& db)
{
    const FolderTree tree(db.folders());
    QHash<qint64, qint64> direct, n;
    for (const Pomodoro& p : db.pomodorosBetween(QDate(2000, 1, 1), QDate(2100, 1, 1))) { direct[p.folderId] += p.studiedSec; n[p.folderId] += 1; }
    const auto rolled = tree.rollUp(direct);
    auto hrs = [](qint64 s) { return QString::number(s / 3600.0, 'f', 2).replace(QLatin1Char('.'), QLatin1Char(',')); };
    QString out = QString::fromUtf8("\xEF\xBB\xBF") + QStringLiteral("actividad;horas totales (con subactividades);horas directas;pomodoros\r\n");
    for (const Folder& f : tree.all())
        out += csvexport::escape(tree.path(f.id, QStringLiteral(" / "))) + QLatin1Char(';') + hrs(rolled.value(f.id)) + QLatin1Char(';') + hrs(direct.value(f.id)) + QLatin1Char(';') + QString::number(n.value(f.id)) + QStringLiteral("\r\n");
    return out;
}

QString writeAll(const Database& db, const QString& dir, const QDateTime& now)
{
    if (!QDir().mkpath(dir)) return QStringLiteral("No pude crear la carpeta");
    auto put = [&](const QString& name, const QString& text) {
        QFile f(QDir(dir).filePath(name));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(text.toUtf8());
        return true;
    };
    const FolderTree tree(db.folders());
    const QDate today = dayclock::logicalDate(now);
    const QString pomos = csvexport::build(db.pomodorosBetween(QDate(2000, 1, 1), today), db.intervalsBetween(QDate(2000, 1, 1), today), tree, nullptr, true);
    QString book = QString::fromUtf8("\xEF\xBB\xBF") + notebook::exportText(notebook::pages(db));
    if (!put(QStringLiteral("informe.html"), html(db, now)) || !put(QStringLiteral("pomodoros.csv"), pomos) || !put(QStringLiteral("dias.csv"), daysCsv(db, now))
        || !put(QStringLiteral("actividades.csv"), activitiesCsv(db)) || !put(QStringLiteral("libreta.txt"), book))
        return QStringLiteral("No pude escribir los archivos en esa carpeta");
    return QString();
}

} // namespace report
