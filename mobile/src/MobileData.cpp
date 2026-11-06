// Lo que la app del celu muestra además del reloj: objetivo del día, cuadraditos
// (nota, actividad, borrar), estadísticas, hitos, ajustes y actividades.
// Usa el mismo motor que la PC (core/), así los números dan igual en los dos.
#include "MobileApp.h"
#include "core/DayClock.h"
#include "core/Format.h"
#include "core/Goals.h"
#include "core/Milestones.h"
#include "core/Stats.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QLocale>
#include <QTimer>
#include <cmath>
#include <functional>

namespace {
const char* kMonths[] = {"ene", "feb", "mar", "abr", "may", "jun", "jul", "ago", "sep", "oct", "nov", "dic"};
const char* kDays[] = {"L", "M", "M", "J", "V", "S", "D"};

QString cap(const QString& s) { return s.isEmpty() ? s : s.left(1).toUpper() + s.mid(1); }
} // namespace

// ---------------------------------------------------------------------------
// Objetivo del día

QHash<qint64, qint64> MobileApp::studiedByFolder(const QVector<Pomodoro>& dayPomos, bool reserve) const
{
    QHash<qint64, qint64> by;
    const auto cur = m_session.currentPomodoro();
    for (const Pomodoro& p : dayPomos) {
        qint64 sec = p.studiedSec;
        if (cur && p.id == cur->id) {
            sec = cur->studiedSec;
            if (reserve) sec = qMax<qint64>(sec, cur->plannedSec);
        }
        by[p.folderId] += sec;
    }
    return by;
}

void MobileApp::reloadGoal()
{
    const QDate today = dayclock::today();
    m_goal.clear();
    m_goal["has"] = false;
    const auto r = goals::resolve(m_db->goals(), today);
    if (!r || r->spec.isEmpty()) return;
    const auto parsed = goals::parse(r->spec, workMin());
    if (!parsed.ok) return;
    const auto pomos = m_db->pomodorosBetween(today, today);
    // B2-1: por defecto el pomodoro que corre ocupa su bloque (como en la PC).
    const bool merge = m_db->setting("goal_running", 0).toInt() == 1;
    const auto fills = goals::fillByActivity(parsed, studiedByFolder(pomos, !merge), m_tree);
    const auto doneFills = goals::fillByActivity(parsed, studiedByFolder(pomos, false), m_tree);
    QVariantList blocks;
    int full = 0;
    for (int i = 0; i < parsed.blocks.size(); ++i) {
        QVariantMap b;
        const qint64 f = parsed.folders.value(i);
        b["minutes"] = parsed.blocks[i];
        b["folder"] = f;
        b["color"] = f > 0 ? m_colors.value(f, QStringLiteral("#a8a8a8")) : QString();
        b["fill"] = fills.value(i);
        b["done"] = doneFills.value(i);
        if (doneFills.value(i) >= 0.999) ++full;
        blocks.append(b);
    }
    qint64 studied = 0;
    for (const auto& v : studiedByFolder(pomos, false)) studied += v;
    m_goal["has"] = true;
    m_goal["timeMode"] = parsed.timeMode;
    m_goal["describe"] = goals::describe(r->spec);
    m_goal["blocks"] = blocks;
    m_goal["full"] = full;
    m_goal["count"] = int(parsed.blocks.size());
    m_goal["targetMin"] = parsed.totalMin;
    m_goal["studied"] = fmt::duration(studied);
    m_goal["target"] = fmt::duration(qint64(parsed.totalMin) * 60);
    // Mejoras 2.0: con los recreos en el medio
    m_goal["withBreaks"] = parsed.blocks.size() > 1
        ? fmt::duration(goals::withBreaksSec(parsed, goals::breakSecFor(*m_db, breakMin() * 60))) : QString();
    m_goal["progress"] = parsed.totalMin > 0 ? qMin(1.0, double(studied) / (parsed.totalMin * 60.0)) : 0.0;
    m_goal["complete"] = goals::allDone(doneFills);
    m_goal["repeats"] = r->weekdays != 0;
    QStringList acts;
    for (qint64 f : parsed.folders)
        if (f > 0 && m_tree.find(f) && !acts.contains(m_tree.find(f)->name)) acts << m_tree.find(f)->name;
    m_goal["activities"] = acts.join(QStringLiteral(", "));
}

// M17: el próximo pomodoro viene con la duración (y la actividad) del objetivo.
void MobileApp::preloadFromGoal()
{
    using S = SessionController::State;
    const S s = m_session.state();
    if (s != S::Idle && s != S::Ready) return;
    const QDate today = dayclock::today();
    const auto r = goals::resolve(m_db->goals(), today);
    if (!r) return;
    const auto parsed = goals::parse(r->spec, m_db->setting("work_min", 25).toInt());
    if (!parsed.ok || parsed.timeMode) return;
    const auto fills = goals::fillByActivity(parsed, studiedByFolder(m_db->pomodorosBetween(today, today), false), m_tree);
    int next = -1;
    for (int i = 0; i < fills.size(); ++i)
        if (fills[i] < 0.999 && parsed.folders.value(i) > 0 && m_tree.subtree(parsed.folders[i]).contains(m_session.folderId())) { next = i; break; }
    if (next < 0)
        for (int i = 0; i < fills.size(); ++i) if (fills[i] < 0.999) { next = i; break; }
    if (next < 0) return;
    m_session.setWorkSec(parsed.blocks[next] * 60);
    const qint64 f = parsed.folders.value(next);
    if (f > 0 && m_tree.contains(f) && !m_tree.subtree(f).contains(m_session.folderId()) && !m_session.switchWouldPark(f)) {
        m_session.setFolderId(f);
        m_db->setSetting("last_folder", f);
    }
    emit changed();
    emit ticked();
}

QString MobileApp::dayTitle() const { return m_db->dayTitle(dayclock::today()); }

void MobileApp::setDayTitle(const QString& text)
{
    m_db->setDayTitle(dayclock::today(), text.trimmed());
    reloadToday();
    bump();
}

QVariantMap MobileApp::goalDraft() const
{
    QVariantMap d;
    const auto r = goals::resolve(m_db->goals(), dayclock::today());
    const bool has = r && !r->spec.isEmpty();
    d["has"] = has;
    d["weekdays"] = has ? r->weekdays : 0;
    d["timeMode"] = has && r->spec.startsWith(QLatin1String("t:"));
    d["totalMin"] = has && r->spec.startsWith(QLatin1String("t:")) ? r->spec.mid(2).toInt() : 240;
    QVariantList parts;
    if (has && !r->spec.startsWith(QLatin1String("t:")))
        for (const goals::Part& p : goals::parts(r->spec))
            parts.append(QVariantMap{{"count", p.count}, {"minutes", p.minutes}, {"folder", p.folder}});
    if (parts.isEmpty()) parts.append(QVariantMap{{"count", 4}, {"minutes", workMin()}, {"folder", 0}});
    d["parts"] = parts;
    return d;
}

static QVector<goals::Part> toParts(const QVariantList& list)
{
    QVector<goals::Part> out;
    for (const QVariant& v : list) {
        const QVariantMap m = v.toMap();
        goals::Part p;
        p.count = qBound(0, m.value("count").toInt(), 50);
        p.minutes = qBound(1, m.value("minutes").toInt(), 240);
        p.folder = m.value("folder").toLongLong();
        if (p.count > 0) out.append(p);
    }
    return out;
}

QString MobileApp::describeParts(bool timeMode, int totalMin, const QVariantList& parts) const
{
    if (timeMode) {
        const auto p = goals::parse(QStringLiteral("t:%1").arg(totalMin), workMin());
        QString out = fmt::duration(qint64(totalMin) * 60) + QStringLiteral(" de estudio");
        if (p.ok && p.blocks.size() > 1)
            out += QStringLiteral(" · ≈ %1 con recreos").arg(fmt::duration(goals::withBreaksSec(p, goals::breakSecFor(*m_db, breakMin() * 60))));
        return out;
    }
    int total = 0, n = 0;
    for (const goals::Part& p : toParts(parts)) { total += p.count * p.minutes; n += p.count; }
    if (n == 0) return QStringLiteral("Sin bloques");
    const int brk = goals::breakSecFor(*m_db, breakMin() * 60);
    QString out = QStringLiteral("%1 %2 · %3").arg(n).arg(n == 1 ? "cuadradito" : "cuadraditos", fmt::duration(qint64(total) * 60));
    if (n > 1) out += QStringLiteral(" · ≈ %1 con recreos").arg(fmt::duration(qint64(total) * 60 + qint64(n - 1) * brk));
    return out;
}

void MobileApp::saveGoal(bool timeMode, int totalMin, const QVariantList& parts, int weekdays)
{
    QString spec;
    if (timeMode) spec = QStringLiteral("t:%1").arg(qBound(5, totalMin, 24 * 60));
    else spec = goals::joinParts(toParts(parts));
    if (spec.isEmpty()) return;
    goals::save(*m_db, dayclock::today(), QString(), spec, weekdays);
    reloadToday();
    preloadFromGoal();
    bump();
    emit toast(QStringLiteral("Objetivo guardado"));
}

void MobileApp::removeGoal(bool fromTodayOn)
{
    if (fromTodayOn) goals::removeFromDay(*m_db, dayclock::today());
    else goals::removeDay(*m_db, dayclock::today());
    reloadToday();
    bump();
    emit toast(QStringLiteral("Objetivo quitado"));
}

// ---------------------------------------------------------------------------
// Cuadraditos

QVariantMap MobileApp::pomodoroInfo(qint64 id) const
{
    QVariantMap m;
    const auto p = m_db->pomodoro(id);
    if (!p) return m;
    const auto cur = m_session.currentPomodoro();
    const bool running = cur && cur->id == id;
    const Pomodoro& q = running ? *cur : *p;
    m["id"] = id;
    m["running"] = running;
    m["parked"] = q.status == PomoStatus::Parked;
    m["note"] = q.note;
    m["folder"] = q.folderId;
    m["folderName"] = m_tree.find(q.folderId) ? m_tree.path(q.folderId) : QStringLiteral("Sin actividad");
    m["color"] = m_colors.value(q.folderId, QStringLiteral("#a8a8a8"));
    m["studied"] = fmt::duration(q.studiedSec);
    m["planned"] = fmt::duration(q.plannedSec);
    const QDateTime a = QDateTime::fromSecsSinceEpoch(q.startedAt);
    const QDateTime b = QDateTime::fromSecsSinceEpoch(qMax(q.endedAt, q.startedAt));
    m["when"] = a.toString(QStringLiteral("HH:mm")) + QStringLiteral(" – ") + (running ? QStringLiteral("ahora") : b.toString(QStringLiteral("HH:mm")));
    m["day"] = fmt::longDate(q.day);
    return m;
}

void MobileApp::setNote(qint64 id, const QString& note)
{
    m_db->setPomodoroNote(id, note.trimmed());
    reloadToday();
    bump();
}

void MobileApp::movePomodoro(qint64 id, qint64 folderId)
{
    const auto cur = m_session.currentPomodoro();
    if (cur && cur->id == id) {
        m_session.moveCurrentTo(folderId);
        m_session.setFolderId(folderId);
        m_db->setSetting("last_folder", folderId);
        emit changed();
    } else {
        m_db->setPomodoroFolder(id, folderId);
    }
    reloadToday();
    bump();
}

void MobileApp::deletePomodoro(qint64 id)
{
    const auto cur = m_session.currentPomodoro();
    if (cur && cur->id == id) {   // el que corre: se descarta (no hay deshacer)
        m_session.discardCurrent();
        reloadToday();
        bump();
        emit toast(QStringLiteral("Pomodoro descartado"));
        return;
    }
    m_deleted = m_db->pomodoro(id);
    m_db->deletePomodoro(id);
    reloadToday();
    bump();
    emit toast(QStringLiteral("Cuadradito borrado"));
}

void MobileApp::undoDelete()
{
    if (!m_deleted) return;
    Pomodoro p = *m_deleted;
    m_deleted.reset();
    p.id = 0;
    m_db->insertPomodoro(p);
    reloadToday();
    bump();
}

// ---------------------------------------------------------------------------
// Estadísticas

QVariantMap MobileApp::stats(const QString& period, int offset, qint64 folder) const
{
    const QDate today = dayclock::today();
    QDate from, to;
    QString label;
    if (period == QLatin1String("day")) {
        from = to = today.addDays(offset);
        label = offset == 0 ? QStringLiteral("Hoy") : offset == -1 ? QStringLiteral("Ayer") : cap(fmt::longDate(from));
    } else if (period == QLatin1String("week")) {
        from = fmt::mondayOf(today).addDays(7 * offset);
        to = from.addDays(6);
        label = offset == 0 ? QStringLiteral("Esta semana") : offset == -1 ? QStringLiteral("La semana pasada") : fmt::weekRange(from);
    } else if (period == QLatin1String("month")) {
        from = QDate(today.year(), today.month(), 1).addMonths(offset);
        to = from.addMonths(1).addDays(-1);
        label = cap(fmt::monthYear(from));
    } else {
        from = QDate(today.year() + offset, 1, 1);
        to = QDate(today.year() + offset, 12, 31);
        label = QString::number(from.year());
    }

    QSet<qint64> sub;
    const bool filtered = folder > 0 && m_tree.contains(folder);
    if (filtered) sub = m_tree.subtree(folder);
    const auto t = stats::totals(*m_db, from, to, filtered ? &sub : nullptr, QDateTime::currentDateTime());
    const auto pomos = m_db->pomodorosBetween(from, to);
    const auto cur = m_session.currentPomodoro();
    int count = 0;
    qint64 study = 0;
    QMap<QDate, qint64> byDay;
    QHash<qint64, qint64> direct;
    for (const Pomodoro& p : pomos) {
        if (filtered && !sub.contains(p.folderId)) continue;
        const qint64 sec = (cur && cur->id == p.id) ? cur->studiedSec : p.studiedSec;
        if (p.status != PomoStatus::Running && p.status != PomoStatus::Parked) ++count;
        study += sec;
        byDay[p.day] += sec;
        direct[p.folderId] += sec;
    }
    const int days = qMax<qint64>(1, from.daysTo(qMin(to, today)) + 1);

    // Objetivos de cada día (cumplido / no) para marcar las barras.
    const auto rules = m_db->goals();
    const int pomoMin = m_db->setting("work_min", 25).toInt();
    QHash<QDate, QVector<Pomodoro>> pomosByDay;
    for (const Pomodoro& p : pomos) pomosByDay[p.day].append(p);
    int goalDays = 0, goalMet = 0;
    auto goalState = [&](const QDate& d) -> int {   // 0 sin objetivo, 1 cumplido, 2 no cumplido, 3 en curso
        const auto r = goals::resolve(rules, d);
        if (!r || r->spec.isEmpty() || d > today) return 0;
        const auto parsed = goals::parse(r->spec, pomoMin);
        if (!parsed.ok) return 0;
        const bool ok = goals::allDone(goals::fillByActivity(parsed, studiedByFolder(pomosByDay.value(d), false), m_tree));
        return ok ? 1 : (d == today ? 3 : 2);
    };

    QVariantList bars;
    qint64 maxSec = 0;
    auto addBar = [&](const QString& lbl, qint64 sec, int goal, bool current, bool future) {
        bars.append(QVariantMap{{"label", lbl}, {"sec", sec}, {"text", sec > 0 ? fmt::duration(sec) : QString()},
                                {"goal", goal}, {"current", current}, {"future", future}});
        maxSec = qMax(maxSec, sec);
    };
    if (period == QLatin1String("week") || period == QLatin1String("month")) {
        for (QDate d = from; d <= to; d = d.addDays(1)) {
            const int g = goalState(d);
            if (g == 1 || g == 2) { ++goalDays; if (g == 1) ++goalMet; }
            const QString lbl = period == QLatin1String("week") ? QString::fromLatin1(kDays[d.dayOfWeek() - 1]) : QString::number(d.day());
            addBar(lbl, byDay.value(d), g, d == today, d > today);
        }
    } else if (period == QLatin1String("year")) {
        for (int m = 1; m <= 12; ++m) {
            const QDate a(from.year(), m, 1), b = a.addMonths(1).addDays(-1);
            qint64 s = 0;
            for (auto it = byDay.lowerBound(a); it != byDay.cend() && it.key() <= b; ++it) s += it.value();
            addBar(QString::fromLatin1(kMonths[m - 1]), s, 0, today >= a && today <= b, a > today);
        }
        for (QDate d = from; d <= qMin(to, today); d = d.addDays(1)) {
            const int g = goalState(d);
            if (g == 1 || g == 2) { ++goalDays; if (g == 1) ++goalMet; }
        }
    }

    // Por actividad (se suma hacia arriba; se muestran las de primer nivel o las hijas del filtro).
    const auto rolled = m_tree.rollUp(direct);
    QVariantList acts;
    const QVector<qint64> ids = filtered ? m_tree.children(folder) : m_tree.children(0);
    struct A { qint64 id; qint64 sec; };
    QVector<A> list;
    for (qint64 id : ids) if (rolled.value(id) > 0) list.append({id, rolled.value(id)});
    if (filtered && direct.value(folder) > 0 && !list.isEmpty()) list.append({folder, direct.value(folder)});
    if (!filtered && direct.value(0) > 0) list.append({0, direct.value(0)});
    std::sort(list.begin(), list.end(), [](const A& a, const A& b) { return a.sec > b.sec; });
    for (const A& a : list) {
        QString name = a.id == 0 ? QStringLiteral("Sin actividad")
                     : (a.id == folder ? m_tree.find(a.id)->name + QStringLiteral(" (directo)") : m_tree.find(a.id)->name);
        acts.append(QVariantMap{{"id", a.id}, {"name", name}, {"color", m_colors.value(a.id, QStringLiteral("#a8a8a8"))},
                                {"text", fmt::duration(a.sec)}, {"share", study > 0 ? double(a.sec) / study : 0.0},
                                {"hasChildren", a.id > 0 && a.id != folder && !m_tree.children(a.id).isEmpty()}});
    }

    // Día: los cuadraditos de ese día, con su nota.
    QVariantList tiles;
    if (period == QLatin1String("day")) {
        int n = 0;
        for (const Pomodoro& p : pomos) {
            if (p.status == PomoStatus::Parked) continue;
            if (filtered && !sub.contains(p.folderId)) continue;
            const bool running = cur && cur->id == p.id;
            tiles.append(QVariantMap{{"id", p.id}, {"number", ++n}, {"running", running},
                                     {"minutes", qMax(1, (running ? cur->plannedSec : p.representedSec()) / 60)},
                                     {"color", m_colors.value(p.folderId, QStringLiteral("#a8a8a8"))},
                                     {"name", m_tree.find(p.folderId) ? m_tree.find(p.folderId)->name : QStringLiteral("Sin actividad")},
                                     {"note", p.note},
                                     {"when", QDateTime::fromSecsSinceEpoch(p.startedAt).toString(QStringLiteral("HH:mm"))},
                                     {"studied", fmt::duration(running ? cur->studiedSec : p.studiedSec)}});
        }
    }

    QVariantMap out;
    out["label"] = label;
    out["from"] = fmt::dayKey(from);
    out["isCurrent"] = offset == 0;
    out["total"] = study > 0 ? fmt::hours(study) : QStringLiteral("0m");
    out["totalSec"] = study;
    out["pomodoros"] = count;
    out["breaks"] = fmt::duration(t.breaks);
    out["undue"] = fmt::duration(t.undue);
    out["inactive"] = fmt::duration(t.inactive);
    out["average"] = fmt::duration(study / days);
    out["days"] = days;
    int studiedDays = 0;
    for (qint64 v : byDay) if (v > 0) ++studiedDays;
    out["studiedDays"] = studiedDays;
    out["bars"] = bars;
    out["maxSec"] = maxSec;
    out["acts"] = acts;
    out["tiles"] = tiles;
    out["goalDays"] = goalDays;
    out["goalMet"] = goalMet;
    const auto r = goals::resolve(rules, from);
    out["dayGoal"] = period == QLatin1String("day") && r && !r->spec.isEmpty() ? goals::describe(r->spec) : QString();
    out["dayGoalState"] = period == QLatin1String("day") ? goalState(from) : 0;
    out["dayTitle"] = period == QLatin1String("day") ? m_db->dayTitle(from) : QString();
    out["folderName"] = filtered ? m_tree.find(folder)->name : QString();
    return out;
}

// ---------------------------------------------------------------------------
// Hitos

QVariantMap MobileApp::milestones() const
{
    const ms::Config cfg = ms::Config::load(*m_db);
    const QDate today = dayclock::today();
    QVariantMap out;
    out["cfg"] = QVariantMap{{"day", cfg.dayMark}, {"streaks", cfg.streaks}, {"weekly", cfg.weekly},
                             {"hours", cfg.hours}, {"stages", cfg.stages}, {"month", cfg.month}};

    // Largo plazo: nivel global y por actividad.
    auto direct = m_db->studyByFolder();
    const auto cur = m_session.currentPomodoro();
    if (cur) direct[cur->folderId] += cur->studiedSec - qMin<qint64>(cur->studiedSec, m_db->pomodoro(cur->id) ? m_db->pomodoro(cur->id)->studiedSec : 0);
    qint64 all = 0;
    for (qint64 v : direct) all += v;
    auto stageMap = [](qint64 sec) {
        const ms::Stage s = ms::stageFor(sec);
        QVariantMap m{{"label", s.label}, {"name", s.name}, {"index", s.index}, {"progress", s.progress},
                      {"hours", fmt::hours(sec)}, {"next", s.toSec > 0 ? fmt::hours(s.toSec) : QString()},
                      {"left", s.toSec > 0 ? fmt::hours(qMax<qint64>(0, s.toSec - sec)) : QString()}};
        return m;
    };
    QVariantMap global = stageMap(all);
    global["to10k"] = qMin(1.0, all / (10000.0 * 3600));
    global["pct10k"] = QString::number(all / (10000.0 * 3600) * 100, 'f', all < 360000 ? 2 : 1);
    out["global"] = global;
    const auto rolled = m_tree.rollUp(direct);
    QVariantList acts;
    std::function<void(qint64, int)> add = [&](qint64 parent, int depth) {
        for (qint64 id : m_tree.children(parent)) {
            QVariantMap m = stageMap(rolled.value(id));
            m["id"] = id;
            m["name"] = m_tree.find(id)->name;
            m["color"] = m_colors.value(id);
            m["depth"] = depth;
            m["sec"] = rolled.value(id);
            // próxima marca de horas (curva que se estira)
            int nextMark = 0;
            for (int h : ms::hourMarks()) if (qint64(h) * 3600 > rolled.value(id)) { nextMark = h; break; }
            m["nextMark"] = nextMark;
            m["markProgress"] = nextMark > 0 ? double(rolled.value(id)) / (nextMark * 3600.0) : 1.0;
            acts.append(m);
            add(id, depth + 1);
        }
    };
    add(0, 0);
    out["acts"] = acts;
    QVariantList names;
    for (const QString& n : ms::stageNames()) names.append(n);
    out["stageNames"] = names;

    // Corto plazo
    auto byDay = stats::studyByDay(*m_db, QDate(2000, 1, 1), today, nullptr);
    if (cur) byDay[today] += cur->studiedSec - (m_db->pomodoro(cur->id) ? m_db->pomodoro(cur->id)->studiedSec : 0);
    const qint64 todaySec = byDay.value(today);
    const int level = ms::dayMark(todaySec, cfg.markMin);
    QVariantMap mark{{"level", level}, {"name", level ? cap(ms::dayMarkName(level)) : QString()}, {"today", fmt::hours(todaySec)}};
    if (level < 3) {
        const qint64 next = qint64(cfg.markMin[level]) * 60;
        const qint64 prev = level > 0 ? qint64(cfg.markMin[level - 1]) * 60 : 0;
        mark["nextName"] = cap(ms::dayMarkName(level + 1));
        mark["nextAt"] = fmt::hours(next);
        mark["left"] = fmt::duration(qMax<qint64>(0, next - todaySec));
        mark["progress"] = double(todaySec - prev) / qMax<qint64>(1, next - prev);
    } else {
        mark["progress"] = 1.0;
    }
    QVariantList marks;
    for (int i = 0; i < 3; ++i) marks.append(fmt::hours(qint64(cfg.markMin[i]) * 60));
    mark["marks"] = marks;
    out["mark"] = mark;

    const int jokers = m_db->setting("jokers_per_week", 2).toInt();
    const auto streak = stats::streak(m_db->studiedDays(), today, jokers);
    const int best = qMax(streak.days, m_db->setting("ms/best_streak", 0).toInt());
    int nextStreak = 0;
    for (int d : ms::streakMarks()) if (d > streak.days) { nextStreak = d; break; }
    out["streak"] = QVariantMap{{"days", streak.days}, {"best", best}, {"todayDone", streak.todayDone},
                                {"jokersLeft", qMax(0, jokers - streak.jokersThisWeek)}, {"jokers", jokers}, {"next", nextStreak}};
    const ms::Records rec = ms::records(byDay, today.addDays(1));
    out["records"] = QVariantMap{{"bestDay", rec.bestDay > 0 ? fmt::hours(rec.bestDay) : QString()},
                                 {"bestDayDate", rec.bestDayDate.isValid() ? fmt::shortDate(rec.bestDayDate) : QString()},
                                 {"bestWeek", rec.bestWeek > 0 ? fmt::hours(rec.bestWeek) : QString()},
                                 {"bestWeekDate", rec.bestWeekMonday.isValid() ? fmt::weekRange(rec.bestWeekMonday) : QString()}};
    const qint64 week = ms::weekTotal(byDay, today);
    const qint64 weekGoal = qint64(cfg.weeklyMin) * 60;
    out["week"] = QVariantMap{{"done", fmt::hours(week)}, {"goal", fmt::hours(weekGoal)},
                              {"progress", weekGoal > 0 ? qMin(1.0, double(week) / weekGoal) : 0.0},
                              {"left", fmt::hours(qMax<qint64>(0, weekGoal - week))},
                              {"inARow", ms::weeksInARow(byDay, today, weekGoal)},
                              {"daysLeft", 7 - fmt::mondayOf(today).daysTo(today)}};

    // Mediano plazo: este mes contra el anterior.
    const QDate mStart(today.year(), today.month(), 1);
    qint64 month = 0, prevMonth = 0, prevSamePoint = 0;
    int monthDays = 0;
    for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) {
        if (it.key() >= mStart) { month += it.value(); if (it.value() > 0) ++monthDays; }
        else if (it.key() >= mStart.addMonths(-1)) {
            prevMonth += it.value();
            if (it.key().day() <= today.day()) prevSamePoint += it.value();
        }
    }
    out["month"] = QVariantMap{{"name", cap(fmt::monthYear(today))}, {"total", fmt::hours(month)}, {"days", monthDays},
                               {"prev", fmt::hours(prevMonth)}, {"prevSamePoint", fmt::hours(prevSamePoint)},
                               {"delta", prevSamePoint > 0 ? int(std::lround(100.0 * (month - prevSamePoint) / prevSamePoint)) : 0},
                               {"hasPrev", prevSamePoint > 0}};
    return out;
}

// ---------------------------------------------------------------------------
// Avisos al guardar un pomodoro (lo mismo que festeja la PC)

bool MobileApp::once(const QString& key)
{
    if (!m_db->setting(key).toString().isEmpty()) return false;
    m_db->setSetting(key, 1);
    return true;
}

void MobileApp::afterSave(qint64 id)
{
    if (id <= 0) return;
    const QDate today = dayclock::today();
    // Objetivo del día
    const QString key = "goal_done/" + fmt::dayKey(today);
    if (m_db->setting(key).toString().isEmpty()) {
        const auto r = goals::resolve(m_db->goals(), today);
        if (r && !r->spec.isEmpty()) {
            const auto parsed = goals::parse(r->spec, m_db->setting("work_min", 25).toInt());
            QVector<Pomodoro> done;
            qint64 sec = 0;
            for (const Pomodoro& p : m_db->pomodorosBetween(today, today))
                if (p.status != PomoStatus::Running && p.status != PomoStatus::Parked) { done.append(p); sec += p.studiedSec; }
            if (parsed.ok && goals::allDone(goals::fillByActivity(parsed, studiedByFolder(done, false), m_tree))) {
                m_db->setSetting(key, QDateTime::currentDateTime().toString(Qt::ISODate));
                emit celebrate(QStringLiteral("Objetivo del día"), QStringLiteral("¡Cumplido!"),
                               QStringLiteral("%1 estudiadas hoy").arg(fmt::duration(sec)));
            }
        }
    }

    const ms::Config cfg = ms::Config::load(*m_db);
    const auto byDay = stats::studyByDay(*m_db, QDate(2000, 1, 1), today, nullptr);
    const qint64 todaySec = byDay.value(today);
    const QString dk = fmt::dayKey(today);
    const QString wk = fmt::dayKey(fmt::mondayOf(today));
    if (cfg.weekly) {
        const qint64 week = ms::weekTotal(byDay, today);
        const qint64 goal = qint64(cfg.weeklyMin) * 60;
        if (goal > 0 && week >= goal && once("ms/week/" + wk)) {
            const int row = ms::weeksInARow(byDay, today, goal);
            if (ms::weekStreakMarks().contains(row))
                emit celebrate(QStringLiteral("Constancia"), QStringLiteral("%1 semanas seguidas").arg(row), QStringLiteral("cumpliendo la meta semanal"));
            else
                emit celebrate(QStringLiteral("Meta semanal"), QStringLiteral("¡Cumplida!"), QStringLiteral("%1 esta semana").arg(fmt::hours(week)));
        }
    }
    if (cfg.streaks) {
        const ms::Records rec = ms::records(byDay, today);
        int priorDays = 0;
        for (auto it = byDay.cbegin(); it != byDay.cend(); ++it) if (it.key() < today && it.value() > 0) ++priorDays;
        if (priorDays >= 3 && rec.bestDay >= 3600 && todaySec > rec.bestDay && once("ms/rec_day/" + dk))
            emit celebrate(QStringLiteral("Récord personal"), QStringLiteral("Tu mejor día"),
                           QStringLiteral("%1 (antes %2)").arg(fmt::hours(todaySec), fmt::hours(rec.bestDay)));
        const qint64 week = ms::weekTotal(byDay, today);
        if (rec.bestWeek >= 2 * 3600 && week > rec.bestWeek && once("ms/rec_week/" + wk))
            emit celebrate(QStringLiteral("Récord personal"), QStringLiteral("Tu mejor semana"),
                           QStringLiteral("%1 (antes %2)").arg(fmt::hours(week), fmt::hours(rec.bestWeek)));
        const auto streak = stats::streak(m_db->studiedDays(), today, m_db->setting("jokers_per_week", 2).toInt());
        const int best = m_db->setting("ms/best_streak", 0).toInt();
        if (streak.days > best) m_db->setSetting("ms/best_streak", streak.days);
        if (ms::streakMarks().contains(streak.days) && once("ms/streak/" + dk))
            emit celebrate(QStringLiteral("Racha"), QStringLiteral("%1 días").arg(streak.days), QStringLiteral("seguidos estudiando"));
    }
    if (cfg.dayMark) {
        const int level = ms::dayMark(todaySec, cfg.markMin);
        if (level > m_db->setting("ms/day/" + dk, 0).toInt()) {
            m_db->setSetting("ms/day/" + dk, level);
            emit celebrate(QStringLiteral("Marca del día"), cap(ms::dayMarkName(level)), QStringLiteral("%1 hoy").arg(fmt::hours(todaySec)));
        }
    }
}

void MobileApp::onStudyRecorded(qint64 folderId, qint64 seconds)
{
    if (seconds <= 0) return;
    const ms::Config cfg = ms::Config::load(*m_db);
    if (!cfg.hours && !cfg.stages) return;
    const auto direct = m_db->studyByFolder();
    const auto totals = m_tree.rollUp(direct);
    if (cfg.stages) {
        for (qint64 id : m_tree.ancestorsAndSelf(folderId)) {
            const qint64 after = totals.value(id);
            const ms::Stage a = ms::stageFor(after - seconds), b = ms::stageFor(after);
            if (a.label == b.label || !m_db->markMilestone(id, -(b.index * 10 + b.sub))) continue;
            emit celebrate(a.index != b.index ? QStringLiteral("Nueva etapa") : QStringLiteral("Subiste de nivel"), b.label, m_tree.find(id)->name);
        }
        qint64 all = 0;
        for (qint64 v : direct) all += v;
        const ms::Stage a = ms::stageFor(all - seconds), b = ms::stageFor(all);
        if (a.label != b.label && m_db->markMilestone(-1, -(b.index * 10 + b.sub)))
            emit celebrate(QStringLiteral("Nivel global"), b.label, QStringLiteral("%1 en total").arg(fmt::hours(all)));
    }
    if (cfg.hours) {
        for (qint64 id : m_tree.ancestorsAndSelf(folderId)) {
            const qint64 after = totals.value(id);
            for (int h : ms::hoursCrossed(after - seconds, after))
                if (m_db->markMilestone(id, h))
                    emit celebrate(QStringLiteral("Horas"), QStringLiteral("%1 h").arg(h), m_tree.find(id)->name);
        }
    }
}

// ---------------------------------------------------------------------------
// Ajustes y actividades

QVariant MobileApp::setting(const QString& key, const QVariant& def) const { return m_db->setting(key, def); }

void MobileApp::setSetting(const QString& key, const QVariant& value)
{
    m_db->setSetting(key, value);
    if (key == QLatin1String("day_start_hour")) dayclock::setStartHour(value.toInt());
    if (key == QLatin1String("break_min")) setBreakMin(value.toInt());
    reloadToday();
    bump();
}

QVariantMap MobileApp::folderInfo(qint64 id) const
{
    QVariantMap m;
    const Folder* f = m_tree.find(id);
    if (!f) return m;
    m["id"] = id;
    m["name"] = f->name;
    m["path"] = m_tree.path(id);
    m["color"] = m_colors.value(id);
    m["prior"] = m_db->priorByFolder().value(id) / 3600.0;
    m["total"] = fmt::hours(m_tree.rollUp(m_db->studyByFolder()).value(id));
    m["children"] = int(m_tree.children(id).size());
    return m;
}

void MobileApp::renameFolder(qint64 id, const QString& name)
{
    if (name.trimmed().isEmpty()) return;
    m_db->renameFolder(id, name.trimmed());
    reloadFolders();
    emit changed();
    reloadToday();
    bump();
}

void MobileApp::setFolderColor(qint64 id, const QString& color)
{
    m_db->setFolderColor(id, color);
    reloadFolders();
    emit changed();
    reloadToday();
    bump();
}

void MobileApp::setFolderPriorHours(qint64 id, double hours)
{
    m_db->setFolderPriorSeconds(id, qMax<qint64>(0, qint64(std::lround(hours * 3600))));
    bump();
}

bool MobileApp::deleteFolder(qint64 id)
{
    if (m_tree.subtree(id).contains(m_session.folderId()) && m_session.inProgress()) {
        emit toast(QStringLiteral("Primero terminá el pomodoro de esa actividad"));
        return false;
    }
    const bool ok = m_db->deleteFolder(id);
    reloadFolders();
    if (!m_tree.contains(m_session.folderId())) {
        const qint64 first = m_tree.all().isEmpty() ? 0 : m_tree.all().first().id;
        m_session.setFolderId(first);
        m_db->setSetting("last_folder", first);
    }
    emit changed();
    reloadToday();
    bump();
    return ok;
}

QStringList MobileApp::palette() const
{
    return {"#6f9bd1", "#d18f6f", "#8fc28a", "#c79bd6", "#d6c06f", "#6fc2bd", "#d16f8f", "#a8a8a8",
            "#e0483f", "#5a7bd8", "#4fae6b", "#b07ad8"};
}

QString MobileApp::version() const { return QCoreApplication::applicationVersion(); }

void MobileApp::bump()
{
    ++m_revision;
    emit dataChanged();
}

void MobileApp::endSession()
{
    if (!m_session.canEndSession()) return;
    m_session.endSession();
    reloadToday();
    bump();
    emit toast(QStringLiteral("Sesión terminada. Hasta la vuelta"));
}

void MobileApp::movePomodoros(const QVariantList& ids, qint64 folderId)
{
    int n = 0;
    for (const QVariant& v : ids) {
        const auto p = m_db->pomodoro(v.toLongLong());
        if (!p || p->status == PomoStatus::Running || p->status == PomoStatus::Parked || p->folderId == folderId) continue;
        m_db->setPomodoroFolder(p->id, folderId);
        ++n;
    }
    reloadToday();
    bump();
    if (n > 0) emit toast(QStringLiteral("%1 %2 a %3").arg(n).arg(n == 1 ? "movido" : "movidos", m_tree.find(folderId) ? m_tree.find(folderId)->name : QStringLiteral("sin actividad")));
}

QVariantList MobileApp::sameActivityThatDay(qint64 id) const
{
    QVariantList out;
    const auto p = m_db->pomodoro(id);
    if (!p) return out;
    for (const Pomodoro& q : m_db->pomodorosBetween(p->day, p->day))
        if (q.folderId == p->folderId && q.status != PomoStatus::Running && q.status != PomoStatus::Parked) out.append(q.id);
    return out;
}
