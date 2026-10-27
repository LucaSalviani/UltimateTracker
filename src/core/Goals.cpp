#include "core/Goals.h"
#include "core/Database.h"
#include "core/Format.h"

#include <QRegularExpression>
#include <QStringList>

namespace goals {

namespace {
// "30", "1h", "1h30", "90m" → minutos (0 si no se entiende)
int tokenMinutes(QString t)
{
    t = t.trimmed().toLower();
    static const QRegularExpression re(QStringLiteral("^(?:(\\d+)\\s*h)?\\s*(?:(\\d+)\\s*(?:m|min)?)?$"));
    const auto m = re.match(t);
    if (!m.hasMatch() || t.isEmpty()) return 0;
    return m.captured(1).toInt() * 60 + m.captured(2).toInt();
}

std::optional<GoalRule> recurringOn(const QVector<GoalRule>& rules, const QDate& d)
{
    std::optional<GoalRule> best;
    for (const GoalRule& r : rules)
        if (r.weekdays != 0 && appliesOn(r, d) && (!best || r.id > best->id)) best = r;
    return best;
}

std::optional<GoalRule> oneOffOn(const QVector<GoalRule>& rules, const QDate& d)
{
    std::optional<GoalRule> best;
    for (const GoalRule& r : rules)
        if (r.weekdays == 0 && r.start == d && (!best || r.id > best->id)) best = r;
    return best;
}
} // namespace

Parsed parse(const QString& spec, int pomodoroMin)
{
    Parsed out;
    const QString s = spec.trimmed();
    if (s.isEmpty()) return out;
    if (s.startsWith(QLatin1String("t:"))) {
        out.timeMode = true;
        out.totalMin = s.mid(2).toInt();
        if (out.totalMin <= 0) return out;
        const int len = qMax(1, pomodoroMin);
        const int n = (out.totalMin + len - 1) / len;
        out.blocks = QVector<int>(qMin(n, 60), len);
        out.ok = true;
        return out;
    }
    for (const QString& part : s.split('+', Qt::SkipEmptyParts)) {
        const QStringList nx = part.split('x');
        const int count = nx.size() == 2 ? nx[0].toInt() : 1;
        const int min = nx.size() == 2 ? nx[1].toInt() : nx[0].toInt();
        if (count <= 0 || min <= 0) return Parsed{};
        for (int i = 0; i < count && out.blocks.size() < 60; ++i) out.blocks.append(min);
        out.totalMin += count * min;
    }
    out.ok = !out.blocks.isEmpty();
    return out;
}

QString normalizeCombo(const QString& text)
{
    QString t = text.toLower();
    t.replace(QChar(0x00d7), 'x');   // ×
    t.replace('*', 'x');
    t.replace(',', '+');
    t.replace(';', '+');
    QStringList parts;
    for (const QString& raw : t.split('+', Qt::SkipEmptyParts)) {
        const QString part = raw.trimmed();
        if (part.isEmpty()) continue;
        const QStringList nx = part.split('x');
        int count = 1, min = 0;
        if (nx.size() == 2) {
            count = nx[0].trimmed().toInt();
            min = tokenMinutes(nx[1]);
        } else if (nx.size() == 1) {
            min = tokenMinutes(nx[0]);
        }
        if (count <= 0 || count > 40 || min <= 0 || min > 600) return {};
        parts << QStringLiteral("%1x%2").arg(count).arg(min);
    }
    return parts.join('+');
}

QString describe(const QString& spec)
{
    if (spec.startsWith(QLatin1String("t:"))) return fmt::duration(spec.mid(2).toInt() * 60);
    QString s = spec;
    s.replace('x', QChar(0x00d7));
    s.replace('+', QStringLiteral(" + "));
    return s;
}

bool appliesOn(const GoalRule& r, const QDate& d)
{
    if (!d.isValid() || !r.start.isValid()) return false;
    if (r.weekdays == 0) return r.start == d;
    if (d < r.start) return false;
    if (r.end.isValid() && d > r.end) return false;
    return (r.weekdays >> (d.dayOfWeek() - 1)) & 1;
}

std::optional<GoalRule> resolve(const QVector<GoalRule>& rules, const QDate& d)
{
    if (auto one = oneOffOn(rules, d)) return one;
    return recurringOn(rules, d);
}

void save(Database& db, const QDate& d, const QString& title, const QString& spec, int weekdays)
{
    const QVector<GoalRule> rules = db.goals();
    const auto one = oneOffOn(rules, d);
    if (weekdays == 0) {
        GoalRule r = one.value_or(GoalRule{});
        r.title = title.trimmed();
        r.spec = spec;
        r.start = d;
        r.end = {};
        r.weekdays = 0;
        if (r.id) db.updateGoal(r); else db.insertGoal(r);
        return;
    }
    // Se repite: la puntual de ese día ya no hace falta.
    if (one) db.deleteGoal(one->id);
    if (auto rec = recurringOn(rules, d)) {
        if (rec->start == d) {
            rec->title = title.trimmed();
            rec->spec = spec;
            rec->weekdays = weekdays;
            db.updateGoal(*rec);
            return;
        }
        rec->end = d.addDays(-1);   // el pasado queda como estaba
        db.updateGoal(*rec);
    }
    GoalRule r;
    r.title = title.trimmed();
    r.spec = spec;
    r.start = d;
    r.weekdays = weekdays;
    db.insertGoal(r);
}

void removeDay(Database& db, const QDate& d)
{
    const QVector<GoalRule> rules = db.goals();
    const auto one = oneOffOn(rules, d);
    if (recurringOn(rules, d)) {
        // Hay una que se repite: se pisa ese día con "sin meta".
        GoalRule r = one.value_or(GoalRule{});
        r.title.clear();
        r.spec.clear();
        r.start = d;
        r.weekdays = 0;
        if (r.id) db.updateGoal(r); else db.insertGoal(r);
    } else if (one) {
        db.deleteGoal(one->id);
    }
}

void removeFromDay(Database& db, const QDate& d)
{
    const QVector<GoalRule> rules = db.goals();
    if (const auto one = oneOffOn(rules, d)) db.deleteGoal(one->id);
    if (auto rec = recurringOn(rules, d)) {
        if (rec->start >= d) {
            db.deleteGoal(rec->id);
        } else {
            rec->end = d.addDays(-1);
            db.updateGoal(*rec);
        }
    }
}

} // namespace goals
