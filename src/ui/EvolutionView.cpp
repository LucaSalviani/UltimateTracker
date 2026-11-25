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
constexpr double kTilesH = 96;
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

    QVector<double> series;
    for (const auto& d : m_days) series << double(d.study);
    m_trend = insights::trend(series, 28);
    m_diary = db->dayDiaries(m_from, m_today);
    m_summary = summaryHtml();
    setMinimumHeight(contentHeight(width()));
    update();
}

// Compara las últimas 4 semanas con las 4 anteriores, métrica por métrica.
QString EvolutionView::summaryHtml() const
{
    const int n = int(m_days.size());
    const int r = qMin(28, n), pr = qMin(28, n - r);
    if (pr < 14) return QStringLiteral("Todavía no hay datos suficientes para comparar (hacen falta unas 6 semanas con la app).");
    auto avg = [&](auto get, int from, int to) { double s = 0; for (int i = from; i < to; ++i) s += get(m_days[i]); return s / qMax(1, to - from); };
    struct M { QString name; std::function<double(const insights::DayStat&)> get; bool lowerBetter; };
    const QVector<M> ms = {
        {m_studyLabel + QStringLiteral(" por día"), [](const insights::DayStat& d) { return double(d.study); }, false},
        {QStringLiteral("Recreo indebido"), [](const insights::DayStat& d) { return double(d.undue); }, true},
        {QStringLiteral("Horas inactivas"), [](const insights::DayStat& d) { return double(d.inactive); }, true},
        {QStringLiteral("Pomodoros por día"), [](const insights::DayStat& d) { return double(d.pomos); }, false},
    };
    QStringList lines;
    for (const M& m : ms) {
        const double a = avg(m.get, n - r, n), b = avg(m.get, n - r - pr, n - r);
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
    return int(8 + 40 + kTilesH + kGap + 5 * (kCardH + kGap) + 20);
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
        static const int ranges[] = {30, 90, 365, 0};
        static const char* names[] = {"30 días", "90 días", "12 meses", "Todo"};
        p.setFont(theme::uiFont(8.5, QFont::DemiBold));
        double x = L;
        for (int k = 0; k < 4; ++k) {
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

    paintTiles(p, QRectF(L, y, W, kTilesH)); y += kTilesH + kGap;

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


// Fichas: número grande + variación de los últimos 28 días contra los 28 anteriores.
void EvolutionView::paintTiles(QPainter& p, const QRectF& r)
{
    const int n = int(m_days.size());
    const int rr = qMin(28, n), pr = qMin(28, n - rr);
    struct M { QString name; std::function<double(const insights::DayStat&)> get; bool lowerBetter; bool count; };
    const QVector<M> ms = {
        {m_studyLabel + QStringLiteral(" / día"), [](const insights::DayStat& d) { return double(d.study); }, false, false},
        {QStringLiteral("Pomodoros / día"), [](const insights::DayStat& d) { return double(d.pomos); }, false, true},
        {QStringLiteral("Indebido / día"), [](const insights::DayStat& d) { return double(d.undue); }, true, false},
        {QStringLiteral("Inactivo / día"), [](const insights::DayStat& d) { return double(d.inactive); }, true, false},
    };
    auto avg = [&](const M& m, int from, int to) { double s = 0; for (int i = from; i < to; ++i) s += m.get(m_days[i]); return s / qMax(1, to - from); };
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
        const double a = rr > 0 ? avg(m, n - rr, n) : 0;
        p.setFont(theme::uiFont(17, QFont::DemiBold));
        p.setPen(theme::text);
        p.drawText(QRectF(t.left() + 12, t.top() + 28, t.width() - 24, 32), Qt::AlignLeft | Qt::AlignVCenter, m.count ? QString::number(a, 'f', 1) : fmtH(a));
        QString line = QStringLiteral("sin comparación todavía");
        QColor col = theme::textFaint;
        if (pr >= 14) {
            const double b = avg(m, n - rr - pr, n - rr);
            if (b > 0) {
                const double ch = (a - b) / b;
                if (std::abs(ch) < 0.05) { line = QStringLiteral("→ parejo"); col = theme::textDim; }
                else {
                    line = QStringLiteral("%1 %2 %").arg(ch > 0 ? QStringLiteral("↑") : QStringLiteral("↓")).arg(int(std::llround(std::abs(ch) * 100)));
                    col = ((ch > 0) != m.lowerBetter) ? theme::breakAccent : theme::squareFinal.lighter(125);
                }
                line += QStringLiteral(" vs 28 días antes");
            }
        }
        p.setFont(theme::uiFont(8.5));
        p.setPen(col);
        p.drawText(QRectF(t.left() + 12, t.top() + 64, t.width() - 24, 20), Qt::AlignLeft | Qt::AlignVCenter,
                   QFontMetrics(p.font()).elidedText(line, Qt::ElideRight, int(t.width() - 24)));
        m_zones.append({t, QStringLiteral("%1\nÚltimos %2 días: %3\nPromedio por día.").arg(m.name).arg(rr).arg(m.count ? QString::number(a, 'f', 1) : fmtH(a)), QString()});
    }
}

// ---------------------------------------------------------------------------
void EvolutionView::paintHours(QPainter& p, const QRectF& r)
{
    const QString sentence = m_trend.enough
        ? QStringLiteral("%1 Últimos 28 días: %2 por día (antes %3).").arg(m_trend.sentence().section('.', 0, 0) + QLatin1Char('.'), fmtH(m_trend.recent), fmtH(m_trend.previous))
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
        if (!z.rect.contains(e->position()) || !z.key.startsWith(QLatin1String("range:"))) continue;
        m_db->setSetting("stats/evo_range", z.key.mid(6).toInt());
        reload(m_db, m_filter);
        return;
    }
}

void EvolutionView::leaveEvent(QEvent*) { QToolTip::hideText(); }
