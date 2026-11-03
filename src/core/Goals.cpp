#include "core/Goals.h"
#include "core/Database.h"
#include "core/Format.h"
#include "core/FolderTree.h"

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
        out.folders = QVector<qint64>(out.blocks.size(), 0);
        out.ok = true;
        return out;
    }
    for (const Part& pt : parts(s)) {
        if (pt.count <= 0 || pt.minutes <= 0) return Parsed{};
        for (int i = 0; i < pt.count && out.blocks.size() < 60; ++i) {
            out.blocks.append(pt.minutes);
            out.folders.append(pt.folder);
        }
        out.totalMin += pt.count * pt.minutes;
    }
    out.ok = !out.blocks.isEmpty();
    return out;
}

QVector<Part> parts(const QString& spec)
{
    QVector<Part> out;
    if (spec.startsWith(QLatin1String("t:"))) return out;
    for (const QString& raw : spec.split('+', Qt::SkipEmptyParts)) {
        Part pt;
        QString part = raw.trimmed();
        const int at = part.indexOf('@');
        if (at >= 0) {
            pt.folder = part.mid(at + 1).toLongLong();
            part = part.left(at);
        }
        const QStringList nx = part.split('x');
        pt.count = nx.size() == 2 ? nx[0].toInt() : 1;
        pt.minutes = nx.size() == 2 ? nx[1].toInt() : nx[0].toInt();
        out.append(pt);
    }
    return out;
}

QString joinParts(const QVector<Part>& ps)
{
    QStringList out;
    for (const Part& pt : ps) {
        if (pt.count <= 0 || pt.minutes <= 0) continue;
        QString s = QStringLiteral("%1x%2").arg(pt.count).arg(pt.minutes);
        if (pt.folder > 0) s += QStringLiteral("@%1").arg(pt.folder);
        out << s;
    }
    return out.join('+');
}

QVector<double> fillByActivity(const Parsed& p, const QHash<qint64, qint64>& studiedByFolder, const FolderTree& tree)
{
    QVector<double> fills(p.blocks.size(), 0.0);
    QHash<qint64, qint64> left = studiedByFolder;
    auto take = [&](qint64 need, const QSet<qint64>* only) {
        qint64 got = 0;
        for (auto it = left.begin(); it != left.end() && got < need; ++it) {
            if (only && !only->contains(it.key())) continue;
            const qint64 t = qMin(it.value(), need - got);
            it.value() -= t;
            got += t;
        }
        return got;
    };
    // Primero los que piden una actividad (si no, "cualquiera" se comería su tiempo).
    for (int i = 0; i < p.blocks.size(); ++i) {
        const qint64 f = i < p.folders.size() ? p.folders[i] : 0;
        if (f <= 0) continue;
        const QSet<qint64> sub = tree.contains(f) ? tree.subtree(f) : QSet<qint64>{f};
        const qint64 need = qint64(p.blocks[i]) * 60;
        fills[i] = need > 0 ? double(take(need, &sub)) / need : 1.0;
    }
    for (int i = 0; i < p.blocks.size(); ++i) {
        const qint64 f = i < p.folders.size() ? p.folders[i] : 0;
        if (f > 0) continue;
        const qint64 need = qint64(p.blocks[i]) * 60;
        fills[i] = need > 0 ? double(take(need, nullptr)) / need : 1.0;
    }
    return fills;
}

bool allDone(const QVector<double>& fills)
{
    for (double f : fills) if (f < 0.999) return false;
    return !fills.isEmpty();
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
    static const QRegularExpression at(QStringLiteral("@\\d+"));
    s.remove(at);
    s.replace('x', QChar(0x00d7));
    s.replace('+', QStringLiteral(" + "));
    return s;
}

BlockFill fillBlocks(const QVector<int>& blocksMin, qint64 studiedSec)
{
    BlockFill f;
    qint64 left = studiedSec;
    for (int m : blocksMin) {
        const qint64 need = qint64(m) * 60;
        if (left >= need) { ++f.full; left -= need; continue; }
        f.partial = need > 0 ? double(left) / need : 0;
        break;
    }
    return f;
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
