#pragma once

#include <QDate>
#include <QString>
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
