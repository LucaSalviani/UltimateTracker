#pragma once

#include <QDate>
#include <QString>
#include <QVector>
#include <QtGlobal>

// Estado de un pomodoro tal como se guarda en la base.
enum class PomoStatus {
    Running   = 0,  // en curso (o quedó así si la app se cerró de golpe)
    Completed = 1,  // terminado (por tiempo o a mano; studiedSec dice cuánto duró)
    Final     = 2,  // versiones viejas: se trata igual que Completed
    Parked    = 3,  // en pausa porque cambiaste de actividad: se retoma al volver a ella
};
// El rojo NO se guarda: se pinta rojo el último pomodoro de cada día (ver GridWidget).

// Tipos de intervalos que no son estudio.
enum class IntervalKind {
    Break   = 1,  // recreo dentro del tiempo pactado
    Undue   = 2,  // recreo indebido: tiempo muerto entre medio de la jornada
    Pending = 3,  // tiempo muerto todavía sin decidir: si después se vuelve a
                  // estudiar pasa a Undue; si no, era el final del día (inactivo)
};

struct Folder {
    qint64  id       = 0;
    qint64  parentId = 0;   // 0 = raíz
    QString name;
    int     sort     = 0;
    QString kind;           // Mejoras 3.0: tipo ("estudio", "trabajo"…; "" = lo hereda del padre)
};

struct Pomodoro {
    qint64     id         = 0;
    qint64     folderId   = 0;
    QDate      day;            // día en que empezó (aunque cruce la medianoche)
    qint64     startedAt  = 0; // segundos epoch
    qint64     endedAt    = 0; // segundos epoch (se actualiza mientras corre)
    int        plannedSec = 0;
    int        studiedSec = 0;
    PomoStatus status     = PomoStatus::Running;
    bool       linked     = false; // arrancó salteando el recreo: va "pegado" al anterior
    QString    note;               // "¿en qué trabajaste?" (opcional)
    // Mejoras 3.0: seguir sin recreo extiende el mismo pomodoro. Cada tramo
    // pactado, en segundos y separado por comas ("1500,1500"); vacío = uno solo.
    QString    segments;

    QVector<int> segmentList() const
    {
        QVector<int> out;
        for (const QString& s : segments.split(QLatin1Char(','), Qt::SkipEmptyParts)) out.append(s.toInt());
        return out;
    }
    // Lo que ya estaba hecho antes del tramo actual (0 si es un pomodoro común).
    int baseSec() const
    {
        const auto l = segmentList();
        int sum = 0;
        for (int i = 0; i + 1 < l.size(); ++i) sum += l[i];
        return sum;
    }
    // Tramos ya hechos: para contar pomodoros (uno que corre extendido ya trae los anteriores).
    int doneUnits() const
    {
        if (status == PomoStatus::Completed || status == PomoStatus::Final) return units();
        const int n = int(segmentList().size());
        return n > 1 ? n - 1 : 0;
    }
    // Cuántos pomodoros "vale": los tramos completos (mínimo 1 si se guardó).
    int units() const
    {
        const auto l = segmentList();
        if (l.size() <= 1) return 1;
        int n = 0, sum = 0;
        for (int s : l) { sum += s; if (studiedSec + 1 >= sum) ++n; }
        return qMax(1, n);
    }

    // Minutos que "representa" el cuadradito.
    int representedSec() const
    {
        return (status == PomoStatus::Running || status == PomoStatus::Parked) ? plannedSec : studiedSec;
    }
};

struct Interval {
    qint64       id        = 0;
    IntervalKind kind      = IntervalKind::Undue;
    QDate        day;
    qint64       startedAt = 0;
    qint64       seconds   = 0;
};
