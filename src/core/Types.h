#pragma once

#include <QDate>
#include <QString>
#include <QtGlobal>

// Estado de un pomodoro tal como se guarda en la base.
enum class PomoStatus {
    Running   = 0,  // en curso (o quedó así si la app se cerró de golpe)
    Completed = 1,  // llegó al final del tiempo: se pinta negro
    Final     = 2,  // último de una sesión (terminada o abandonada): se pinta rojo
};

// Tipos de intervalos que no son estudio.
enum class IntervalKind {
    Break = 1,  // recreo dentro del tiempo pactado
    Undue = 2,  // recreo indebido: pausas y tiempo muerto con la sesión abierta
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

    // Minutos que "representa" el cuadradito.
    int representedSec() const
    {
        return status == PomoStatus::Running ? plannedSec : studiedSec;
    }
};

struct Interval {
    qint64       id        = 0;
    IntervalKind kind      = IntervalKind::Undue;
    QDate        day;
    qint64       startedAt = 0;
    qint64       seconds   = 0;
};
