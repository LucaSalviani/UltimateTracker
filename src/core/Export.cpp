#include "core/Export.h"

#include <QDateTime>
#include <QLocale>
#include <algorithm>

namespace csvexport {

QString escape(const QString& field)
{
    if (!field.contains(QLatin1Char(';')) && !field.contains(QLatin1Char('"')) &&
        !field.contains(QLatin1Char('\n')) && !field.contains(QLatin1Char('\r')))
        return field;
    QString f = field;
    f.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + f + QLatin1Char('"');
}

namespace {
struct Row {
    qint64 start = 0;
    QStringList cells;
};

QString clock(qint64 epoch) { return QDateTime::fromSecsSinceEpoch(epoch).toString(QStringLiteral("HH:mm")); }
QString minutes(qint64 sec) { return QString::number(sec / 60.0, 'f', 1).replace(QLatin1Char('.'), QLatin1Char(',')); }

QString statusName(PomoStatus s)
{
    switch (s) {
    case PomoStatus::Running: return QStringLiteral("en curso");
    case PomoStatus::Parked:  return QStringLiteral("en pausa");
    default:                  return QStringLiteral("completo");
    }
}
} // namespace

QString build(const QVector<Pomodoro>& pomos, const QVector<Interval>& intervals, const FolderTree& tree,
              const QSet<qint64>* folders, bool includeBreaks)
{
    QVector<Row> rows;
    for (const Pomodoro& p : pomos) {
        if (folders && !folders->contains(p.folderId)) continue;
        const QString path = tree.find(p.folderId) ? tree.path(p.folderId, QStringLiteral(" / ")) : QStringLiteral("Sin actividad");
        const qint64 end = qMax(p.endedAt, p.startedAt + p.studiedSec);
        rows.append({p.startedAt, {p.day.toString(Qt::ISODate), clock(p.startedAt), clock(end), QStringLiteral("pomodoro"),
                                   path, minutes(p.studiedSec), minutes(p.plannedSec), statusName(p.status), p.note}});
    }
    if (includeBreaks) {
        for (const Interval& i : intervals) {
            if (i.kind == IntervalKind::Pending) continue;   // todavía sin decidir: no es un dato
            rows.append({i.startedAt, {i.day.toString(Qt::ISODate), clock(i.startedAt), clock(i.startedAt + i.seconds),
                                       i.kind == IntervalKind::Break ? QStringLiteral("recreo") : QStringLiteral("recreo indebido"),
                                       QString(), minutes(i.seconds), QString(), QString(), QString()}});
        }
    }
    std::stable_sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.start < b.start; });

    QString out = QStringLiteral("﻿");
    out += QStringLiteral("fecha;inicio;fin;tipo;actividad;minutos;minutos planeados;estado;nota\r\n");
    for (const Row& r : rows) {
        QStringList esc;
        for (const QString& c : r.cells) esc << escape(c);
        out += esc.join(QLatin1Char(';')) + QStringLiteral("\r\n");
    }
    return out;
}

} // namespace csvexport
