// Tests del núcleo con reloj simulado: no hace falta esperar 25 minutos reales.
#include "core/Database.h"
#include "core/Goals.h"
#include "core/Export.h"
#include "core/Milestones.h"
#include "core/Sync.h"
#include "core/Live.h"
#include "core/Kinds.h"
#include "core/Insights.h"
#include "core/Report.h"
#include "core/Routine.h"
#include "core/Notebook.h"
#include <QSqlQuery>
#include "core/DayClock.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"
#include "core/Stats.h"

#include <QDateTime>
#include <memory>
#include <QSignalSpy>
#include <QtTest>
#include <QTemporaryDir>

class CoreTest : public QObject {
    Q_OBJECT

    qint64 m_now = 0;
    void advance(Database&, SessionController& s, qint64 seconds)
    {
        // Avanza en pasos de 1 s como haría el QTimer real.
        for (qint64 i = 0; i < seconds; ++i) {
            m_now += 1000;
            s.tick();
        }
    }
    SessionController* make(Database& db)
    {
        return new SessionController(&db, [this] { return m_now; }, /*autoTick*/ false, this);
    }
    static qint64 sum(const Database& db, IntervalKind k, const QDate& d)
    {
        qint64 out = 0;
        for (const Interval& i : db.intervalsBetween(d, d))
            if (i.kind == k) out += i.seconds;
        return out;
    }

private slots:
    void init()
    {
        dayclock::setStartHour(4);
        // Lunes 26/10/2026 10:00 hora local
        m_now = QDateTime(QDate(2026, 10, 26), QTime(10, 0)).toMSecsSinceEpoch();
    }

    void fullCycle()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        const qint64 folder = db.addFolder(0, "Facultad");
        s->setFolderId(folder);
        s->setWorkSec(25 * 60);
        s->setBreakSec(10 * 60);

        QSignalSpy completed(s, &SessionController::pomodoroCompleted);
        QSignalSpy alarm(s, &SessionController::breakFinished);

        s->start();
        QCOMPARE(s->state(), SessionController::State::Running);
        advance(db, *s, 10 * 60);
        s->pause();
        advance(db, *s, 2 * 60);                      // 2' de pausa
        s->resume();                                  // volvió → era indebido
        advance(db, *s, 15 * 60);                     // completa
        QCOMPARE(s->state(), SessionController::State::WorkDone);
        QCOMPARE(completed.count(), 1);

        advance(db, *s, 30);                          // tardó 30 s en elegir
        s->startBreak();                              // → indebido
        advance(db, *s, 12 * 60);                     // recreo de 10' + 2' de alarma
        QCOMPARE(s->state(), SessionController::State::BreakOver);
        QCOMPARE(alarm.count(), 1);
        s->silenceAlarm();
        advance(db, *s, 60);                          // 1' hasta dar play
        s->start();                                   // → los 3' de alarma+espera son indebidos
        advance(db, *s, 7 * 60);
        s->endPomodoro();                             // lo corta a mano a los 7'
        QCOMPARE(s->state(), SessionController::State::WorkDone);
        QCOMPARE(completed.count(), 1);               // cortado a mano: sin sonido

        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 2);
        QCOMPARE(pomos[0].status, PomoStatus::Completed);
        QCOMPARE(pomos[0].studiedSec, 25 * 60);
        QCOMPARE(pomos[1].status, PomoStatus::Completed);
        QCOMPARE(pomos[1].studiedSec, 7 * 60);

        QCOMPARE(sum(db, IntervalKind::Break, d), 10 * 60);
        QCOMPARE(sum(db, IntervalKind::Undue, d), 2 * 60 + 30 + 2 * 60 + 60);
    }

    void idleAtEndOfDayIsInactiveNotUndue()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->setWorkSec(20 * 60);
        s->start();
        advance(db, *s, 20 * 60);
        advance(db, *s, 3 * 3600);                    // no tomó recreo ni volvió
        const QDate d(2026, 10, 26);
        QCOMPARE(sum(db, IntervalKind::Undue, d), 0);
        QVERIFY(sum(db, IntervalKind::Pending, d) >= 3 * 3600 - 5);   // se persiste cada 5 s
        s->closeDay();
        QCOMPARE(s->state(), SessionController::State::Idle);
        QCOMPARE(sum(db, IntervalKind::Undue, d), 0);

        // Para las estadísticas, ese tiempo es inactivo.
        const auto t = stats::totals(db, d, d, nullptr, QDateTime(d, QTime(23, 59)));
        QCOMPARE(t.undue, 0);
        QCOMPARE(t.study, 20 * 60);
    }

    void gapThenStudyAgainIsUndue()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->setWorkSec(25 * 60);
        s->start();
        advance(db, *s, 25 * 60);
        advance(db, *s, 3600);                        // se colgó una hora sin recreo
        s->start();                                   // pero volvió: indebido
        const QDate d(2026, 10, 26);
        QCOMPARE(sum(db, IntervalKind::Undue, d), 3600);
        QCOMPARE(sum(db, IntervalKind::Pending, d), 0);
    }

    void endPomodoroWhilePaused()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->start();
        advance(db, *s, 300);
        s->pause();
        advance(db, *s, 120);
        s->endPomodoro();
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].studiedSec, 300);
        // Pausó y cortó: hasta que vuelva a estudiar, la pausa queda pendiente.
        QCOMPARE(sum(db, IntervalKind::Undue, d), 0);
        s->start();
        QCOMPARE(sum(db, IntervalKind::Undue, d), 120);
    }

    void endPomodoroRightAwayDiscardsIt()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->start();
        s->endPomodoro();
        const QDate d(2026, 10, 26);
        QCOMPARE(db.pomodorosBetween(d, d).size(), 0);
        QCOMPARE(s->state(), SessionController::State::Idle);
    }

    void underOneMinuteIsNotSaved()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->start();
        advance(db, *s, 45);
        s->endPomodoro();
        const QDate d(2026, 10, 26);
        QCOMPARE(db.pomodorosBetween(d, d).size(), 0);
        QCOMPARE(s->state(), SessionController::State::Idle);
        s->start();
        advance(db, *s, 61);
        s->endPomodoro();
        QCOMPARE(db.pomodorosBetween(d, d).size(), 1);
        QCOMPARE(s->state(), SessionController::State::WorkDone);
    }

    void skipBreakStartsNextLinked()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->setWorkSec(20 * 60);
        s->start();
        advance(db, *s, 20 * 60);
        advance(db, *s, 10);
        s->skipBreak();                               // Mejoras 3.0: sigue el mismo, más largo
        QCOMPARE(s->state(), SessionController::State::Running);
        advance(db, *s, 20 * 60);
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].studiedSec, 40 * 60);
        QCOMPARE(pomos[0].units(), 2);                // vale por dos
        QCOMPARE(sum(db, IntervalKind::Undue, d), 10);
    }

    void hotSwapActivityMovesCurrentPomodoro()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 so = db.addFolder(0, "SO");
        const qint64 axel = db.addFolder(0, "Clases Axel");
        auto* s = make(db);
        s->setFolderId(so);
        s->start();
        advance(db, *s, 15 * 60);
        s->moveCurrentTo(axel);                       // corrección explícita (click derecho → mover)
        advance(db, *s, 10 * 60);
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].folderId, axel);
        QCOMPARE(db.studyByFolder().value(so), 0);
        QCOMPARE(db.studyByFolder().value(axel), 25 * 60);
    }

    void switchingActivityParksAndResumes()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 so = db.addFolder(0, "SO");
        const qint64 axel = db.addFolder(0, "Clases Axel");
        auto* s = make(db);
        s->setFolderId(so);
        s->start();
        advance(db, *s, 10 * 60);
        QVERIFY(s->switchWouldPark(axel));
        s->setFolderId(axel);                         // SO queda en pausa con 10'
        QCOMPARE(s->state(), SessionController::State::Ready);
        QCOMPARE(s->parkedSeconds().value(so), 10 * 60);
        s->start();                                   // pomodoro nuevo en Axel
        advance(db, *s, 25 * 60);
        s->skipBreak();                               // (Axel otra vez)
        advance(db, *s, 5);
        s->endPomodoro();                             // 5 s: se descarta
        s->setFolderId(so);                           // vuelvo a SO: aparece pausado
        QCOMPARE(s->state(), SessionController::State::Paused);
        QCOMPARE(s->remainingSec(), 15 * 60);
        s->resume();
        advance(db, *s, 15 * 60);                     // lo termina
        const QDate d(2026, 10, 26);
        QCOMPARE(db.studyByFolder().value(so), 25 * 60);
        QCOMPARE(db.studyByFolder().value(axel), 25 * 60);
        QCOMPARE(db.pomodorosBetween(d, d).size(), 2);
        QVERIFY(s->parkedSeconds().isEmpty());
    }

    void parkedFinishesAtEndOfDayAndSurvivesRestart()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 so = db.addFolder(0, "SO");
        const qint64 axel = db.addFolder(0, "Clases Axel");
        const QDate d(2026, 10, 26);
        {
            auto* s = make(db);
            s->setFolderId(so);
            s->start();
            advance(db, *s, 12 * 60);
            s->setFolderId(axel);
            s->closeDay(false);                       // salir de la app: queda en pausa
            delete s;
        }
        auto* s2 = make(db);
        s2->recoverAfterRestart();
        QCOMPARE(s2->parkedSeconds().value(so), 12 * 60);
        s2->closeDay(true);                           // medianoche: se da por terminado
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].status, PomoStatus::Completed);
        QCOMPARE(pomos[0].studiedSec, 12 * 60);
    }

    void earlySwitchJustMovesThePomodoro()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 so = db.addFolder(0, "SO");
        const qint64 axel = db.addFolder(0, "Clases Axel");
        auto* s = make(db);
        s->setFolderId(so);
        s->start();
        advance(db, *s, 20);
        QVERIFY(!s->switchWouldPark(axel));
        s->setFolderId(axel);                         // a los 20 s: me equivoqué, se corrige
        QCOMPARE(s->state(), SessionController::State::Running);
        advance(db, *s, 25 * 60);
        QCOMPARE(db.studyByFolder().value(axel), 25 * 60);
    }

    void deleteAndMovePausedOrParked()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 so = db.addFolder(0, "SO");
        const qint64 axel = db.addFolder(0, "Axel");
        const qint64 ing = db.addFolder(0, "Inglés");
        auto* s = make(db);
        const QDate d(2026, 10, 26);
        s->setFolderId(so);
        s->start();
        advance(db, *s, 5 * 60);
        s->pause();
        s->discardCurrent();                          // borrar el pausado
        QCOMPARE(db.pomodorosBetween(d, d).size(), 0);
        QCOMPARE(s->state(), SessionController::State::Idle);

        s->start();
        advance(db, *s, 5 * 60);
        s->setFolderId(axel);                         // SO queda estacionado
        const qint64 parkedId = db.pomodorosBetween(d, d).first().id;
        QVERIFY(s->moveParked(parkedId, ing));
        QCOMPARE(s->parkedSeconds().value(ing), 5 * 60);
        QVERIFY(!s->parkedSeconds().contains(so));
        s->deleteParked(parkedId);
        QVERIFY(s->parkedSeconds().isEmpty());
        QCOMPARE(db.pomodorosBetween(d, d).size(), 0);
    }

    void nightSessionBelongsToPreviousDay()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        m_now = QDateTime(QDate(2026, 10, 27), QTime(2, 30)).toMSecsSinceEpoch();  // 02:30 del 27
        s->start();
        advance(db, *s, 25 * 60);
        const auto pomos = db.pomodorosBetween(QDate(2026, 10, 26), QDate(2026, 10, 27));
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].day, QDate(2026, 10, 26));   // antes de las 4 AM: cuenta para el 26
        QCOMPARE(dayclock::logicalDate(QDateTime(QDate(2026, 10, 27), QTime(4, 0))), QDate(2026, 10, 27));
    }

    void crashRecovery()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        {
            auto* s = make(db);
            s->start();
            advance(db, *s, 600);   // se persiste cada 5 s
            delete s;               // "se cortó la luz"
        }
        auto* s2 = make(db);
        s2->recoverAfterRestart();
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].status, PomoStatus::Completed);
        QVERIFY(pomos[0].studiedSec >= 595);
    }

    void folderRollUp()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 fac = db.addFolder(0, "Facultad");
        const qint64 so  = db.addFolder(fac, "Sistemas Operativos");
        const qint64 u3  = db.addFolder(so, "Unidad 3");
        const qint64 am  = db.addFolder(fac, "Análisis");
        FolderTree tree(db.folders());
        QCOMPARE(tree.path(u3), QString("Facultad › Sistemas Operativos › Unidad 3"));
        QHash<qint64, qint64> direct{{so, 100}, {u3, 50}, {am, 30}};
        auto total = tree.rollUp(direct);
        QCOMPARE(total[fac], 180);
        QCOMPARE(total[so], 150);
        QCOMPARE(total[u3], 50);
        QCOMPARE(tree.subtree(so).size(), 2);

        // Borrar una carpeta no borra las horas: quedan "sin carpeta".
        Pomodoro p; p.folderId = u3; p.day = QDate(2026, 10, 26); p.plannedSec = 1500;
        p.studiedSec = 1500; p.status = PomoStatus::Completed;
        db.insertPomodoro(p);
        QVERIFY(db.deleteFolder(so));
        FolderTree after(db.folders());
        QVERIFY(!after.contains(u3));
        QCOMPARE(db.studyByFolder().value(0), 1500);
    }

    // "Me voy": después del recreo te vas; al volver, ese rato no es recreo indebido.
    void endSessionNotUndue()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        std::unique_ptr<SessionController> s(make(db));
        s->setWorkSec(25 * 60);
        s->start();
        advance(db, *s, 25 * 60 + 1);
        QCOMPARE(s->state(), SessionController::State::WorkDone);
        advance(db, *s, 10 * 60);                 // 10 min sin decidir (pendiente)
        QVERIFY(s->canEndSession());
        s->endSession();
        QCOMPARE(s->state(), SessionController::State::Idle);
        advance(db, *s, 2 * 3600);                // me fui 2 horas
        s->start();                           // vuelvo y estudio
        advance(db, *s, 60);
        qint64 undue = 0;
        for (const Interval& i : db.intervalsBetween(QDate(2026, 10, 26), QDate(2026, 10, 26)))
            if (i.kind == IntervalKind::Undue) undue += i.seconds;
        QCOMPARE(undue, qint64(0));
    }

    // Mejoras 3.0: cambiar los comodines no cambia la racha hacia atrás; y se puede reiniciar.
    void streakRulesNotRetroactive()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        db.setSetting("jokers_per_week", 0);
        // lun 12/10 … dom 25/10/2026; falta el jue 15 y el mié 21
        QSet<QDate> studied;
        for (int d = 12; d <= 25; ++d) if (d != 15 && d != 21) studied.insert(QDate(2026, 10, d));
        const QDate today(2026, 10, 25);
        // Sin comodines: se cortó el 21 → 22, 23, 24, 25
        QCOMPARE(stats::streak(studied, today, stats::StreakRules::load(db)).days, 4);
        // Hoy paso a 2 comodines: lo de antes no revive
        stats::setJokers(db, 2, today);
        QCOMPARE(stats::streak(studied, today, stats::StreakRules::load(db)).days, 4);
        QCOMPARE(stats::jokersToday(db, today), 2);
        // Pero desde hoy sí cubren: falto el 26 y el 27 sigue la racha
        studied.insert(QDate(2026, 10, 27));
        const auto s = stats::streak(studied, QDate(2026, 10, 27), stats::StreakRules::load(db));
        QCOMPARE(s.days, 5);
        QCOMPARE(s.jokerDays.size(), 1);
        QCOMPARE(s.since, QDate(2026, 10, 22));
        // Y al revés: tenía 2, bajo a 0 hoy: los comodines ya usados siguen valiendo
        Database db2;
        QVERIFY(db2.open(":memory:"));
        db2.setSetting("jokers_per_week", 2);
        QCOMPARE(stats::streak(studied, today, stats::StreakRules::load(db2)).days, 12);
        stats::setJokers(db2, 0, today);
        QCOMPARE(stats::streak(studied, today, stats::StreakRules::load(db2)).days, 12);
        // Reiniciar a mano: cuenta desde ese día; deshacer la devuelve
        stats::resetStreak(db2, today);
        QCOMPARE(stats::streak(studied, today, stats::StreakRules::load(db2)).days, 1);
        stats::resetStreak(db2, QDate());
        QCOMPARE(stats::streak(studied, today, stats::StreakRules::load(db2)).days, 12);
    }

    // Mejoras 3.0: seguir sin recreo extiende el mismo pomodoro; el recreo salteado
    // se guarda y se puede tomar a mitad.
    void extendAndBreakBank()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        std::unique_ptr<SessionController> s(make(db));
        const QDate day(2026, 10, 26);
        QSignalSpy recorded(s.get(), &SessionController::studyRecorded);
        s->setWorkSec(25 * 60);
        s->setBreakSec(5 * 60);
        s->start();
        advance(db, *s, 25 * 60 + 1);
        QCOMPARE(s->state(), SessionController::State::WorkDone);
        QVERIFY(s->skipWouldExtend());
        s->skipBreak();                                   // 25 → 50, mismo cuadradito
        QCOMPARE(s->state(), SessionController::State::Running);
        QCOMPARE(db.pomodorosBetween(day, day).size(), 1);
        QCOMPARE(s->currentPomodoro()->plannedSec, 50 * 60);
        QCOMPARE(s->remainingSec(), 25 * 60);
        QCOMPARE(s->breakBankSec(), 5 * 60);
        // A los 8 minutos del segundo tramo: tomar el recreo guardado
        advance(db, *s, 8 * 60);
        QVERIFY(s->canTakeBreak());
        s->takeBreakNow();
        QCOMPARE(s->state(), SessionController::State::Break);
        advance(db, *s, 5 * 60 + 1);
        QCOMPARE(s->state(), SessionController::State::BreakOver);
        s->silenceAlarm();                                // vuelve al mismo pomodoro
        QCOMPARE(s->state(), SessionController::State::Running);
        QCOMPARE(s->breakBankSec(), 0);
        QCOMPARE(s->remainingSec(), 17 * 60);
        // Lo corto ahí: queda un cuadradito de 33m, no uno suelto de 8
        s->endPomodoro();
        auto all = db.pomodorosBetween(day, day);
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().studiedSec, 33 * 60);
        QCOMPARE(all.first().units(), 1);
        // Las horas se anotaron una sola vez: 25 + 8
        qint64 total = 0;
        for (const auto& c : recorded) total += c.at(1).toLongLong();
        QCOMPARE(total, qint64(33 * 60));
        // El recreo fue recreo, no indebido
        qint64 brk = 0;
        for (const Interval& i : db.intervalsBetween(day, day)) if (i.kind == IntervalKind::Break) brk += i.seconds;
        QCOMPARE(brk, qint64(5 * 60));
        // Otra vez: extender y completar → vale 2
        s->skipBreak();
        advance(db, *s, 25 * 60 + 1);
        all = db.pomodorosBetween(day, day);
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().studiedSec, 58 * 60);
        QCOMPARE(all.first().segments, QString("1500,480,1500"));
        // Extender y arrepentirse a los 20 s: queda como estaba
        s->skipBreak();
        advance(db, *s, 20);
        s->endPomodoro();
        all = db.pomodorosBetween(day, day);
        QCOMPARE(all.size(), 1);
        QCOMPARE(all.first().studiedSec, 58 * 60);
        QCOMPARE(all.first().status, PomoStatus::Completed);
        // Con otra actividad no se extiende: pomodoro nuevo
        const qint64 other = db.addFolder(0, "Otra");
        s->start(); advance(db, *s, 25 * 60 + 1);
        s->setFolderId(other);
        QVERIFY(!s->skipWouldExtend());
    }

    void activityKinds()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 fac = db.addFolder(0, "Facultad"), so = db.addFolder(fac, "SO"), free = db.addFolder(0, "Freelance");
        db.setFolderKind(free, "trabajo");
        FolderTree t(db.folders());
        QCOMPARE(kinds::of(t, so).done, QString("estudiado"));        // hereda (por defecto, estudio)
        QCOMPARE(kinds::of(t, free).done, QString("trabajado"));
        QCOMPARE(kinds::common(t, {so, fac}).noun, QString("estudio"));
        QCOMPARE(kinds::common(t, {so, free}).noun, QString("foco")); // mezcla → neutro
        db.setFolderKind(fac, kinds::customId("Entrenamiento", "entrenado"));
        t = FolderTree(db.folders());
        QCOMPARE(kinds::of(t, so).done, QString("entrenado"));
        QCOMPARE(kinds::of(t, so).name, QString("Entrenamiento"));
        QCOMPARE(kinds::of(t, so).doneFem, QString("entrenadas"));
    }

    void breakPauseCountsAsUndue()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        std::unique_ptr<SessionController> s(make(db));
        const QDate day(2026, 10, 26);
        s->setWorkSec(25 * 60);
        s->setBreakSec(10 * 60);
        s->start();
        advance(db, *s, 25 * 60 + 1);
        s->skipBreak();                                   // se guarda el recreo
        advance(db, *s, 25 * 60 + 1);
        QCOMPARE(s->state(), SessionController::State::WorkDone);
        QVERIFY(s->breakBankSec() > 0);
        s->startBreak();                                  // tomar el recreo borra lo guardado
        QCOMPARE(s->breakBankSec(), 0);
        advance(db, *s, 2 * 60);
        s->pauseBreak();
        QVERIFY(s->breakPaused());
        const int left = s->remainingSec();
        advance(db, *s, 3 * 60);                          // en pausa el recreo no avanza
        QCOMPARE(s->remainingSec(), left);
        s->resumeBreak();
        QVERIFY(!s->breakPaused());
        advance(db, *s, 10 * 60);
        QCOMPARE(s->state(), SessionController::State::BreakOver);
        qint64 brk = 0, undue = 0;
        for (const Interval& i : db.intervalsBetween(day, day)) {
            if (i.kind == IntervalKind::Break) brk += i.seconds;
            if (i.kind == IntervalKind::Undue) undue += i.seconds;
        }
        QVERIFY(qAbs(brk - 10 * 60) <= 2);                // lo tomado: 10 min de recreo
        QVERIFY(qAbs(undue - 3 * 60) <= 2);               // lo pausado: indebido
    }

    void sessionsCount()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 f = db.addFolder(0, "SO");
        const QDate day(2026, 11, 20);
        auto add = [&](int h, int m, int min) {
            Pomodoro p; p.folderId = f; p.day = day;
            p.startedAt = QDateTime(day, QTime(h, m)).toSecsSinceEpoch();
            p.plannedSec = p.studiedSec = min * 60; p.endedAt = p.startedAt + p.studiedSec; p.status = PomoStatus::Completed;
            db.insertPomodoro(p);
        };
        add(9, 0, 25); add(9, 35, 25);            // misma sesión (pausa de 10 min)
        add(12, 0, 25);                           // pausa larga: sesión nueva
        add(12, 30, 25); add(13, 0, 25);          // 12:30 sigue la sesión de las 12; el de las 13 la corta un «Me voy»
        db.addSessionEnd(QDateTime(day, QTime(12, 56)).toSecsSinceEpoch());   // ...salvo por un «Me voy» antes de la última
        const auto s = insights::sessions(db, day, day, nullptr);
        QCOMPARE(s.size(), 3);
        QCOMPARE(s[0].pomos, 2);
        QCOMPARE(s[1].pomos, 2);
        QCOMPARE(s[2].pomos, 1);
        const auto d = insights::daily(db, day, day, nullptr, QDateTime(day.addDays(1), QTime(12, 0)));
        QCOMPARE(d.value(day).sessions, 3);
    }

    void routineExplainsInactive()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 f = db.addFolder(0, "SO");
        const QDate day(2026, 11, 18);   // miércoles
        routine::Event fac; fac.title = "Cursada"; fac.category = "Facultad"; fac.startMin = 8 * 60; fac.endMin = 12 * 60; fac.weekdays = 1 | 2 | 4 | 8 | 16;
        routine::Event gym; gym.title = "Gym"; gym.category = "Gimnasio"; gym.startMin = 18 * 60; gym.endMin = 19 * 60; gym.weekdays = 0; gym.day = day;
        routine::save(db, fac);
        routine::save(db, gym);
        const auto evs = routine::events(db);
        QCOMPARE(evs.size(), 2);
        Pomodoro p; p.folderId = f; p.day = day;
        p.startedAt = QDateTime(day, QTime(10, 0)).toSecsSinceEpoch();   // estudia durante la cursada: no se cuenta dos veces
        p.plannedSec = p.studiedSec = 3600; p.endedAt = p.startedAt + 3600; p.status = PomoStatus::Completed;
        db.insertPomodoro(p);
        const auto ex = routine::explain(db, evs, day, QDateTime(day.addDays(1), QTime(4, 0)));
        QCOMPARE(ex.byCategory.value("Facultad"), qint64(3 * 3600));
        QCOMPARE(ex.byCategory.value("Gimnasio"), qint64(3600));
        QCOMPARE(ex.total, qint64(23 * 3600));
        QCOMPARE(ex.unexplained, qint64(19 * 3600));
        QCOMPARE(routine::weekdaysText(31), QString("Lun a Vie"));
    }

    void notebookPages()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        notebook::Page a; a.title = "Ideas"; a.body = "- [ ] leer apunte";
        notebook::Page b; b.title = "Fórmulas"; b.body = "derivadas"; b.pinned = true;
        const qint64 ia = notebook::save(db, a);
        notebook::save(db, b);
        auto all = notebook::pages(db);
        QCOMPARE(all.size(), 2);
        QVERIFY(all[0].pinned);                                   // fijadas primero
        QCOMPARE(notebook::pages(db, "apunte").size(), 1);        // busca en el texto
        QCOMPARE(notebook::pages(db, "Fórm").size(), 1);          // y en el título
        a = notebook::get(db, ia);
        a.body = "- [x] leer apunte";
        notebook::save(db, a);
        QCOMPARE(notebook::get(db, ia).body, QString("- [x] leer apunte"));
        QVERIFY(notebook::exportText(notebook::pages(db)).contains("Ideas"));
        notebook::remove(db, ia);
        QCOMPARE(notebook::pages(db).size(), 1);
    }

    void insightsAndDiary()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 f = db.addFolder(0, "SO");
        const QDate today(2026, 11, 20);
        for (int i = 0; i < 40; ++i) {   // 40 días, cada vez un poco más
            Pomodoro p;
            p.folderId = f; p.day = today.addDays(-i);
            p.startedAt = QDateTime(p.day, QTime(10, 0)).toSecsSinceEpoch();
            p.plannedSec = p.studiedSec = (3600 + (40 - i) * 60);
            p.endedAt = p.startedAt + p.studiedSec; p.status = PomoStatus::Completed;
            db.insertPomodoro(p);
        }
        const auto days = insights::daily(db, today.addDays(-39), today, nullptr, QDateTime(today, QTime(12, 0)));
        QCOMPARE(days.size(), 40);
        QCOMPARE(days.value(today).pomos, 1);
        QVERIFY(days.value(today.addDays(-5)).inactive > 0);
        QVector<double> v;
        for (const auto& d : days) v << d.study;
        const auto t = insights::trend(v, 14);
        QVERIFY(t.enough);
        QVERIFY(t.slope > 0 && t.pct > 0);   // va al alza
        QVERIFY(t.sentence().contains("alza"));
        const auto rh = insights::rhythm(db, today.addDays(-39), today, nullptr);
        QVERIFY(rh.total > 0);
        qint64 cells = 0;
        for (int d = 0; d < 7; ++d) for (int h = 0; h < 24; ++h) cells += rh.sec[d][h];
        QCOMPARE(cells, rh.total);
        QVERIFY(rh.dayCount[0] + rh.dayCount[1] + rh.dayCount[2] + rh.dayCount[3] + rh.dayCount[4] + rh.dayCount[5] + rh.dayCount[6] > 0);
        db.setDayNote(today, "nota <b>de prueba</b>"); db.setDayMood(today, 4);
        const QString html = report::html(db, QDateTime(today, QTime(12, 0)));
        QVERIFY(html.contains("Semana a semana") && html.contains("Cuándo estudiás"));
        QVERIFY(html.contains("nota &lt;b&gt;de prueba&lt;/b&gt;"));   // el diario va escapado
        QVERIFY(report::daysCsv(db, QDateTime(today, QTime(12, 0))).contains("nota <b>de prueba</b>"));
        const auto avg = insights::movingAverage({1, 2, 3, 4}, 2);
        QCOMPARE(avg.value(3), 3.5);
        // Historial de meta semanal: el pasado no se reescribe
        const QDate mon(2026, 11, 16);
        insights::recordWeekGoal(db, mon.addDays(-7), 600);
        insights::recordWeekGoal(db, mon, 900);
        QCOMPARE(insights::weekGoals(db).value(mon.addDays(-7)), 600);
        QCOMPARE(insights::weekGoals(db).value(mon), 900);
        // Diario
        db.setDayNote(today, "  repasé SO  ");
        db.setDayMood(today, 4);
        const auto dd = db.dayDiary(today);
        QCOMPARE(dd.note, QString("repasé SO"));
        QCOMPARE(dd.mood, 4);
        QCOMPARE(db.dayDiaries(today.addDays(-3), today).size(), 1);
        db.setDayTitle(today, "Objetivo");   // el título no pisa la nota
        QCOMPARE(db.dayDiary(today).note, QString("repasé SO"));
    }

    void manualPomodoro()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 f = db.addFolder(0, "SO");
        Pomodoro p;
        p.folderId = f; p.day = QDate(2026, 11, 18); p.startedAt = 1000; p.endedAt = 2500;
        p.plannedSec = p.studiedSec = 1500; p.status = PomoStatus::Completed; p.manual = true;
        const qint64 id = db.insertPomodoro(p);
        QVERIFY(id > 0);
        const auto back = db.pomodoro(id);
        QVERIFY(back && back->manual);
        QCOMPARE(back->studiedSec, 1500);
        QVERIFY(db.studiedDays().contains(QDate(2026, 11, 18)));   // cuenta para la racha
    }

    void goalDurations()
    {
        QCOMPARE(goals::parseDuration("3h30"), 210);
        QCOMPARE(goals::parseDuration("3:30"), 210);
        QCOMPARE(goals::parseDuration(" 3 h 30 "), 210);
        QCOMPARE(goals::parseDuration("4"), 240);
        QCOMPARE(goals::parseDuration("210"), 210);
        QCOMPARE(goals::parseDuration("45m"), 45);
        QCOMPARE(goals::parseDuration("2.5h"), 150);
        QCOMPARE(goals::parseDuration("hola"), -1);
        QCOMPARE(goals::formatDuration(210), QString("3h30"));
        QCOMPARE(goals::formatDuration(240), QString("4h"));
        QCOMPARE(goals::formatDuration(45), QString("45 min"));
        const auto p = goals::parse("4x25", 25);
        QCOMPARE(goals::withBreaksSec(p, 5 * 60), qint64(100 * 60 + 15 * 60));   // 3 recreos entre 4 bloques
    }

    void streakWithJokers()
    {
        // Semana del lun 19/10 al dom 25/10, hoy lun 26/10.
        QSet<QDate> st;
        const QDate today(2026, 10, 26);
        for (QDate d(2026, 10, 12); d <= QDate(2026, 10, 25); d = d.addDays(1))
            st.insert(d);
        st.remove(QDate(2026, 10, 21));  // un día sin estudiar en la semana pasada
        st.remove(QDate(2026, 10, 23));  // otro

        auto s = stats::streak(st, today, 2);
        QCOMPARE(s.todayDone, false);
        QCOMPARE(s.days, 12);            // 12/10 → 25/10 con 2 comodines: 12 días estudiados

        auto s1 = stats::streak(st, today, 1);
        QCOMPARE(s1.days, 3);            // el 23 usa el comodín, se corta el 21: 22, 24 y 25

        st.insert(today);
        auto s2 = stats::streak(st, today, 2);
        QCOMPARE(s2.todayDone, true);
        QCOMPARE(s2.days, 13);
    }

    void milestones()
    {
        QCOMPARE(stats::milestonesCrossed(0, 3599), QVector<int>{});
        QCOMPARE(stats::milestonesCrossed(3500, 3700), QVector<int>{1});
        QCOMPARE(stats::milestonesCrossed(9 * 3600, 26 * 3600), (QVector<int>{10, 25}));
        QCOMPARE(stats::milestonesCrossed(49 * 3600, 51 * 3600), QVector<int>{50});
    }

    void inactiveHours()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const QDate d(2026, 10, 25);
        Pomodoro p; p.day = d; p.plannedSec = 3600; p.studiedSec = 3600; p.status = PomoStatus::Completed;
        db.insertPomodoro(p);
        db.insertInterval(IntervalKind::Break, d, 0, 600);
        db.insertInterval(IntervalKind::Undue, d, 0, 300);
        const auto t = stats::totals(db, d, d, nullptr, QDateTime(QDate(2026, 10, 26), QTime(10, 0)));
        QCOMPARE(t.study, 3600);
        QCOMPARE(t.inactive, 86400 - 3600 - 600 - 300);
    }

    // M6: nota, corregir duración y deshacer un borrado (mismo id)
    void noteDurationAndUndo()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        Pomodoro p; p.day = QDate(2026, 10, 26); p.startedAt = 1000; p.endedAt = 2500;
        p.plannedSec = 1500; p.studiedSec = 1500; p.status = PomoStatus::Completed;
        const qint64 id = db.insertPomodoro(p);
        db.setPomodoroNote(id, "  ejercicios de paginación ");
        QCOMPARE(db.pomodoro(id)->note, QString("ejercicios de paginación"));

        db.setPomodoroDuration(id, 2400);   // en realidad fueron 40 min
        auto q = db.pomodoro(id);
        QCOMPARE(q->studiedSec, 2400);
        QCOMPARE(q->plannedSec, 2400);
        QCOMPARE(q->endedAt, qint64(3400));
        db.setPomodoroDuration(id, 600);    // y si fueron 10: lo pactado no se achica
        QCOMPARE(db.pomodoro(id)->plannedSec, 2400);

        const Pomodoro copy = *db.pomodoro(id);
        db.deletePomodoro(id);
        QVERIFY(!db.pomodoro(id));
        QCOMPARE(db.insertPomodoro(copy), id);
        QCOMPARE(db.pomodoro(id)->note, copy.note);
    }

    // M7: metas diarias
    void goalParsing()
    {
        QCOMPARE(goals::normalizeCombo("3x30 + 2×25 + 2*60"), QString("3x30+2x25+2x60"));
        QCOMPARE(goals::normalizeCombo("25, 25, 50"), QString("1x25+1x25+1x50"));
        QCOMPARE(goals::normalizeCombo("2x1h"), QString("2x60"));
        QCOMPARE(goals::normalizeCombo("1h30"), QString("1x90"));
        QVERIFY(goals::normalizeCombo("hola").isEmpty());
        const auto c = goals::parse("3x30+2x25", 25);
        QVERIFY(c.ok);
        QCOMPARE(c.blocks, (QVector<int>{30, 30, 30, 25, 25}));
        QCOMPARE(c.totalMin, 140);
        const auto t = goals::parse("t:240", 25);   // 4 h con pomodoros de 25 → 10
        QVERIFY(t.timeMode);
        QCOMPARE(t.blocks.size(), 10);
        QCOMPARE(goals::parse("t:240", 50).blocks.size(), 5);
        QVERIFY(!goals::parse("", 25).ok);
    }

    void goalRulesLikeACalendar()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const QDate mon(2026, 10, 26), tue = mon.addDays(1), sat = mon.addDays(5), nextMon = mon.addDays(7);
        auto specOn = [&](const QDate& d) { auto r = goals::resolve(db.goals(), d); return r ? r->spec : QString("-"); };

        goals::save(db, mon, "", "t:240", goals::kWeekdays);    // 4 h de lunes a viernes
        QCOMPARE(specOn(mon), QString("t:240"));
        QCOMPARE(specOn(tue), QString("t:240"));
        QCOMPARE(specOn(sat), QString("-"));
        QCOMPARE(specOn(nextMon), QString("t:240"));

        goals::save(db, tue, "parcial", "3x50", 0);              // el martes, algo puntual
        QCOMPARE(specOn(tue), QString("3x50"));
        QCOMPARE(specOn(mon.addDays(2)), QString("t:240"));

        goals::removeDay(db, mon.addDays(3));                    // el jueves libre
        QCOMPARE(specOn(mon.addDays(3)), QString(""));
        QVERIFY(goals::resolve(db.goals(), mon.addDays(3))->spec.isEmpty());

        goals::save(db, nextMon, "", "t:180", goals::kEveryDay); // desde el lunes que viene cambia
        QCOMPARE(specOn(mon), QString("t:240"));                 // el pasado no cambia
        QCOMPARE(specOn(nextMon.addDays(5)), QString("t:180"));

        goals::removeFromDay(db, nextMon.addDays(2));            // y desde el miércoles, nada
        QCOMPARE(specOn(nextMon.addDays(1)), QString("t:180"));
        QCOMPARE(specOn(nextMon.addDays(2)), QString("-"));
        QCOMPARE(specOn(mon.addDays(4)), QString("t:240"));
    }

    // M9: copia de seguridad consistente
    void backupCopiesData()
    {
        QTemporaryDir tmp;
        Database db;
        QVERIFY(db.open(tmp.filePath("a.db")));
        Pomodoro p; p.day = QDate(2026, 10, 26); p.plannedSec = 1500; p.studiedSec = 1500; p.status = PomoStatus::Completed;
        db.insertPomodoro(p);
        QVERIFY(db.backupTo(tmp.filePath("copy.db")));
        Database copy;
        QVERIFY(copy.open(tmp.filePath("copy.db")));
        QCOMPARE(copy.pomodorosBetween(p.day, p.day).size(), 1);
    }

    // M11: horas previas
    void priorHoursCountInHistoryOnly()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 f = db.addFolder(0, "Piano");
        Pomodoro p; p.folderId = f; p.day = QDate(2026, 10, 26); p.plannedSec = 1500; p.studiedSec = 1500;
        p.status = PomoStatus::Completed;
        db.insertPomodoro(p);
        db.setFolderPriorSeconds(f, 120 * 3600);
        QCOMPARE(db.studyByFolder().value(f), qint64(120 * 3600 + 1500));
        QCOMPARE(db.studyByFolder(p.day, p.day).value(f), qint64(1500));   // un período: solo lo registrado
    }

    // M21: el objetivo se llena por tiempo, no por cantidad de pomodoros
    void goalFillsByTime()
    {
        const QVector<int> b{40, 40, 40, 40};
        auto f = goals::fillBlocks(b, 6 * 60);           // dos de 3 minutos
        QCOMPARE(f.full, 0);
        QVERIFY(qAbs(f.partial - 0.15) < 1e-9);
        f = goals::fillBlocks(b, 100 * 60);
        QCOMPARE(f.full, 2);
        QVERIFY(qAbs(f.partial - 0.5) < 1e-9);
        f = goals::fillBlocks(b, 200 * 60);
        QCOMPARE(f.full, 4);
        QCOMPARE(f.partial, 0.0);
    }

    // B2: objetivo con actividades
    void goalByActivity()
    {
        QVector<Folder> fs{{1, 0, "Facultad", 0}, {2, 1, "AM2", 0}, {3, 1, "SO", 0}, {4, 0, "Inglés", 0}};
        const FolderTree tree(fs);
        QCOMPARE(goals::joinParts(goals::parts("1x25@2+3x30@3+2x25")), QString("1x25@2+3x30@3+2x25"));
        QCOMPARE(goals::describe("1x25@2+3x30"), QString("1×25 + 3×30"));
        const auto p = goals::parse("1x25@2+2x30@3+1x25", 25);
        QCOMPARE(p.blocks, (QVector<int>{25, 30, 30, 25}));
        QCOMPARE(p.folders, (QVector<qint64>{2, 3, 3, 0}));
        // 40' de SO y 25' de Inglés: SO llena un bloque y un tercio del otro; Inglés llena "cualquiera"; AM2 nada.
        QHash<qint64, qint64> st{{3, 40 * 60}, {4, 25 * 60}};
        auto f = goals::fillByActivity(p, st, tree);
        QCOMPARE(f[0], 0.0);
        QCOMPARE(f[1], 1.0);
        QVERIFY(qAbs(f[2] - 1.0 / 3) < 1e-9);
        QCOMPARE(f[3], 1.0);
        QVERIFY(!goals::allDone(f));
        // Un bloque de "Facultad" acepta lo estudiado en sus materias.
        const auto q = goals::parse("2x25@1", 25);
        f = goals::fillByActivity(q, {{2, 25 * 60}, {3, 25 * 60}}, tree);
        QVERIFY(goals::allDone(f));
        // Sin actividades = como antes (por tiempo, en orden).
        const auto r = goals::parse("2x25", 25);
        f = goals::fillByActivity(r, {{4, 30 * 60}}, tree);
        QCOMPARE(f[0], 1.0);
        QVERIFY(qAbs(f[1] - 0.2) < 1e-9);
    }

    // M25: color elegido por actividad
    void folderColorPersists()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const qint64 f = db.addFolder(0, "Piano");
        QVERIFY(db.folderColors().isEmpty());
        db.setFolderColor(f, "#ff8a00");
        QCOMPARE(db.folderColors().value(f), QString("#ff8a00"));
        db.setFolderColor(f, QString());
        QVERIFY(!db.folderColors().contains(f));
    }

    // M40: deshacer el borrado de una actividad (con subactividades y pomodoros)
    void folderDeleteUndo()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        const int base = db.folders().size();   // la base nueva ya trae "General"
        const qint64 a = db.addFolder(0, "Facultad");
        const qint64 b = db.addFolder(a, "Física");
        const qint64 c = db.addFolder(b, "Unidad 1");
        db.setFolderColor(b, "#123456");
        Pomodoro p; p.folderId = c; p.day = QDate(2026, 10, 1); p.startedAt = 1000; p.endedAt = 2500;
        p.plannedSec = 1500; p.studiedSec = 1500; p.status = PomoStatus::Completed;
        const qint64 pid = db.insertPomodoro(p);
        const auto backup = db.backupFolder(a);
        QCOMPARE(backup.rows.size(), 3);
        QVERIFY2(db.deleteFolder(a), qPrintable(db.lastError()));
        QCOMPARE(db.folders().size(), base);
        QCOMPARE(db.pomodoro(pid)->folderId, qint64(0));
        QVERIFY(db.restoreFolder(backup));
        QCOMPARE(db.folders().size(), base + 3);
        QCOMPARE(db.pomodoro(pid)->folderId, c);
        QCOMPARE(db.folderColors().value(b), QString("#123456"));
        const FolderTree tree(db.folders());
        QCOMPARE(tree.path(c), QString("Facultad › Física › Unidad 1"));
    }

    void goalsRestore()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        GoalRule r; r.title = "x"; r.spec = "t:120"; r.start = QDate(2026, 1, 1); r.weekdays = goals::kEveryDay;
        r.id = db.insertGoal(r);
        const auto before = db.goals();
        db.deleteGoal(r.id);
        QVERIFY(db.goals().isEmpty());
        db.restoreGoals(before);
        QCOMPARE(db.goals().size(), 1);
        QCOMPARE(db.goals().first().id, r.id);
        QCOMPARE(db.goals().first().spec, QString("t:120"));
    }

    // M44: hitos
    void stages()
    {
        QCOMPARE(ms::stageFor(0).label, QString("Novato"));
        QCOMPARE(ms::stageFor(19 * 3600).label, QString("Novato"));
        QCOMPARE(ms::stageFor(20 * 3600).label, QString("Aprendiz I"));
        QCOMPARE(ms::stageFor(60 * 3600).label, QString("Aprendiz II"));
        QCOMPARE(ms::stageFor(99 * 3600).label, QString("Aprendiz III"));
        QCOMPARE(ms::stageFor(440 * 3600).label, QString("Practicante III"));
        QCOMPARE(ms::stageFor(10000LL * 3600).label, QString("Maestro"));
        const auto st = ms::stageFor(150 * 3600);   // Practicante I: 100 → 233,3
        QCOMPARE(st.sub, 1);
        QVERIFY(st.progress > 0.37 && st.progress < 0.38);
    }

    void hourCurve()
    {
        QCOMPARE(ms::hoursCrossed(0, 3600), QVector<int>{1});
        QCOMPARE(ms::hoursCrossed(4 * 3600, 11 * 3600), (QVector<int>{5, 10}));
        QCOMPARE(ms::hoursCrossed(30 * 3600, 49 * 3600), QVector<int>{});
        QCOMPARE(ms::hoursCrossed(990 * 3600, 1600 * 3600), (QVector<int>{1000, 1500}));
    }

    void shortTerm()
    {
        const int marks[3] = {120, 240, 360};
        QCOMPARE(ms::dayMark(119 * 60, marks), 0);
        QCOMPARE(ms::dayMark(4 * 3600, marks), 2);
        QCOMPARE(ms::dayMark(9 * 3600, marks), 3);
        QMap<QDate, qint64> d;
        const QDate mon(2026, 10, 19);   // lunes
        d[mon.addDays(-7)] = 5 * 3600;   d[mon.addDays(-6)] = 6 * 3600;   // semana anterior: 11 h
        d[mon] = 3 * 3600;               d[mon.addDays(1)] = 7 * 3600;    // esta semana: 10 h
        const auto r = ms::records(d, mon.addDays(1));   // antes del martes
        QCOMPARE(r.bestDay, qint64(6 * 3600));
        QCOMPARE(r.bestWeek, qint64(11 * 3600));
        QCOMPARE(ms::weekTotal(d, mon.addDays(3)), qint64(10 * 3600));
        QCOMPARE(ms::weeksInARow(d, mon.addDays(2), 10 * 3600), 2);
        QCOMPARE(ms::weeksInARow(d, mon.addDays(2), 12 * 3600), 0);
    }

    // Celu-3: dos bases que se sincronizan a través de un "servidor" en memoria.
    // En vivo: el mismo reloj en la PC y el celu (como Spotify).
    void liveTwoDevices()
    {
        Database pc, cel;
        QVERIFY(pc.open(":memory:"));
        QVERIFY(cel.open(":memory:"));
        pc.setSetting("sync/device", "pc");
        cel.setSetting("sync/device", "cel");
        SyncStore spc(pc), scel(cel);
        QMap<QString, SyncRecord> rows;
        auto syncRows = [&] {
            for (SyncStore* s : {&spc, &scel}) {
                const auto out = s->pending(1000);
                for (const auto& r : out) {
                    const QString k = r.kind + "/" + r.uid;
                    if (!rows.contains(k) || rows[k].updatedAt < r.updatedAt) rows[k] = r;
                }
                s->acknowledge(out);
            }
            spc.apply(rows.values().toVector());
            scel.apply(rows.values().toVector());
        };
        const qint64 so = pc.addFolder(0, "SO");
        syncRows();
        const qint64 soCel = cel.idForUid("folders", pc.uidFor("folders", so));
        QVERIFY(soCel > 0);

        std::unique_ptr<SessionController> a(make(pc)), b(make(cel));
        a->setFolderId(so);
        b->setFolderId(soCel);
        auto clock = [this] { return m_now; };
        LiveLink la(&pc, a.get(), nullptr, "la PC", clock), lb(&cel, b.get(), nullptr, "el celu", clock);
        // El servidor: guarda la foto solo si la versión es más nueva.
        qint64 sv = 0;
        QJsonObject sdata;
        auto serve = [&](const QJsonObject& d, qint64 v) { if (v > sv) { sv = v; sdata = d; sdata.insert("v", double(v)); } };
        connect(&la, &LiveLink::published, this, serve);
        connect(&lb, &LiveLink::published, this, serve);
        auto both = [&](qint64 sec) { for (qint64 i = 0; i < sec; ++i) { m_now += 1000; a->tick(); b->tick(); } };

        // 1) Play en la PC: el celu lo ve corriendo, con el mismo tiempo.
        a->setWorkSec(25 * 60);
        a->start();
        la.flush();
        syncRows();
        lb.receive(sdata);
        QVERIFY(b->passive());
        QCOMPARE(b->state(), SessionController::State::Running);
        both(60);
        QCOMPARE(b->remainingSec(), a->remainingSec());
        QCOMPARE(lb.remoteLabel(), QString("la PC"));

        // 2) Pausa en el celu: lo toma el celu; la PC queda espejo en pausa.
        b->pause();
        QVERIFY(!b->passive());
        lb.flush();
        la.receive(sdata);
        QVERIFY(a->passive());
        QCOMPARE(a->state(), SessionController::State::Paused);
        QCOMPARE(a->remainingSec(), b->remainingSec());
        both(30);
        QCOMPARE(a->remainingSec(), b->remainingSec());   // en pausa no corre en ninguno

        // 3) Seguir en la PC y dejar que termine: lo guarda la PC; el celu suena pero no guarda.
        a->resume();
        la.flush();
        lb.receive(sdata);
        QVERIFY(b->passive());
        QSignalSpy doneCel(b.get(), &SessionController::pomodoroCompleted);
        QSignalSpy savedCel(b.get(), &SessionController::studyRecorded);
        QSignalSpy savedPc(a.get(), &SessionController::studyRecorded);
        both(24 * 60 + 5);
        QCOMPARE(a->state(), SessionController::State::WorkDone);
        QCOMPARE(b->state(), SessionController::State::WorkDone);
        QCOMPARE(doneCel.count(), 1);
        QCOMPARE(savedCel.count(), 0);
        QCOMPARE(savedPc.count(), 1);
        la.flush();
        lb.receive(sdata);
        syncRows();
        QCOMPARE(pc.pomodorosBetween(QDate(2026, 10, 26), QDate(2026, 10, 26)).size(), 1);
        QCOMPARE(cel.pomodorosBetween(QDate(2026, 10, 26), QDate(2026, 10, 26)).size(), 1);
        QCOMPARE(cel.pomodorosBetween(QDate(2026, 10, 26), QDate(2026, 10, 26)).first().studiedSec, 25 * 60);

        // 4) Recreo desde el celu: no se vuelve a guardar el pomodoro; la PC suena al terminar.
        b->setBreakSec(5 * 60);
        b->startBreak();
        QCOMPARE(savedCel.count(), 0);
        lb.flush();
        la.receive(sdata);
        QCOMPARE(a->state(), SessionController::State::Break);
        QSignalSpy alarmPc(a.get(), &SessionController::breakFinished);
        both(5 * 60 + 2);
        QCOMPARE(alarmPc.count(), 1);
        QCOMPARE(b->state(), SessionController::State::BreakOver);
        b->silenceAlarm();
        lb.flush();
        la.receive(sdata);
        QCOMPARE(a->state(), SessionController::State::Idle);
        QVERIFY(!a->passive());

        // 5) La PC arranca otro y se "duerme" (sin internet ni ticks). El celu lo ve
        //    terminar y arranca el recreo: el pomodoro se guarda en el celu.
        a->start();
        la.flush();
        syncRows();
        lb.receive(sdata);
        QVERIFY(b->passive());
        for (int i = 0; i < 26 * 60; ++i) { m_now += 1000; b->tick(); }
        QCOMPARE(b->state(), SessionController::State::WorkDone);
        b->startBreak();
        QCOMPARE(savedCel.count(), 1);   // lo guardó el celu
        lb.flush();
        // La PC se despierta: también lo da por terminado, pero su foto es vieja y pierde.
        a->tick();
        la.flush();
        la.receive(sdata);
        QVERIFY(a->passive());
        QCOMPARE(a->state(), SessionController::State::Break);
        syncRows();
        QCOMPARE(pc.pomodorosBetween(QDate(2026, 10, 26), QDate(2026, 10, 26)).size(), 2);
        QCOMPARE(cel.pomodorosBetween(QDate(2026, 10, 26), QDate(2026, 10, 26)).size(), 2);

        // 6) Los dos tocan a la vez: gana el primero que llega; el otro se pone al día.
        b->endBreakEarly();   // celu → listo
        lb.flush();
        la.receive(sdata);
        a->start();           // la PC arranca…
        b->start();           // …y el celu también, sin enterarse
        la.flush();
        lb.flush();           // misma versión: el servidor se queda con la de la PC
        lb.receive(sdata);
        la.receive(sdata);
        QVERIFY(b->passive());
        QVERIFY(!a->passive());
        QCOMPARE(b->state(), SessionController::State::Running);

        // 7) Una foto de ayer no se copia.
        QJsonObject old = sdata;
        old.insert("at", double(m_now - 24 * 3600 * 1000));
        old.insert("v", double(sv + 10));
        old.insert("device", "otro");
        lb.receive(old);
        QCOMPARE(b->state(), SessionController::State::Idle);
    }

    void syncTwoDevices()
    {
        Database pc, cel;
        QVERIFY(pc.open(":memory:"));
        QVERIFY(cel.open(":memory:"));
        SyncStore spc(pc), scel(cel);
        QMap<QString, SyncRecord> server;   // kind/uid → último
        auto push = [&](SyncStore& s) {
            const auto out = s.pending(1000);
            for (const auto& r : out) {
                const QString k = r.kind + "/" + r.uid;
                if (!server.contains(k) || server[k].updatedAt < r.updatedAt) server[k] = r;
            }
            s.acknowledge(out);
        };
        auto pull = [&](SyncStore& s) { return s.apply(server.values().toVector()); };

        // En la PC: actividades, un pomodoro, un objetivo con actividad y un título de día.
        const qint64 fac = pc.addFolder(0, "Facultad");
        const qint64 so = pc.addFolder(fac, "SO");
        Pomodoro p; p.folderId = so; p.day = QDate(2026, 11, 2); p.startedAt = 1000; p.endedAt = 2500;
        p.plannedSec = 1500; p.studiedSec = 1500; p.status = PomoStatus::Completed; p.note = "memoria";
        pc.insertPomodoro(p);
        GoalRule g; g.spec = QString("2x25@%1").arg(so); g.start = QDate(2026, 11, 2); g.weekdays = 0;
        pc.insertGoal(g);
        pc.setDayTitle(QDate(2026, 11, 2), "Parcial");
        QVERIFY(spc.pendingCount() > 0);
        push(spc);
        QCOMPARE(spc.pendingCount(), 0);

        // El celu baja todo. Su "General" se une con la de la PC (no queda duplicada).
        QVERIFY(pull(scel) > 0);
        const FolderTree t(cel.folders());
        QCOMPARE(t.all().size(), 3);
        qint64 soCel = 0;
        for (const Folder& f : t.all()) if (f.name == "SO") soCel = f.id;
        QVERIFY(soCel > 0);
        QCOMPARE(t.path(soCel), QString("Facultad › SO"));
        const auto pomos = cel.pomodorosBetween(QDate(2026, 11, 2), QDate(2026, 11, 2));
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].folderId, soCel);
        QCOMPARE(pomos[0].note, QString("memoria"));
        QCOMPARE(cel.goals().first().spec, QString("2x25@%1").arg(soCel));
        QCOMPARE(cel.dayTitle(QDate(2026, 11, 2)), QString("Parcial"));
        QCOMPARE(scel.pendingCount(), 0);   // aplicar lo que llega no se vuelve a mandar
        // Un pomodoro de otro aparato no es "nuestro" para cerrar al abrir la app.
        Pomodoro run; run.folderId = so; run.day = QDate(2026, 11, 3); run.startedAt = 5000; run.endedAt = 5100;
        run.plannedSec = 1500; run.status = PomoStatus::Running;
        pc.insertPomodoro(run);
        push(spc);
        pull(scel);
        QCOMPARE(cel.pomodorosWithStatus(PomoStatus::Running).size(), 0);
        QCOMPARE(pc.pomodorosWithStatus(PomoStatus::Running).size(), 1);

        // En el celu: renombrar y una nota nueva; vuelve a la PC.
        QTest::qWait(5);
        cel.renameFolder(soCel, "Sistemas Operativos");
        push(scel);
        pull(spc);
        QCOMPARE(FolderTree(pc.folders()).find(so)->name, QString("Sistemas Operativos"));

        // Borrar en la PC llega como borrado al celu.
        pc.deletePomodoro(pc.pomodorosBetween(QDate(2026, 11, 2), QDate(2026, 11, 2))[0].id);
        push(spc);
        pull(scel);
        QCOMPARE(cel.pomodorosBetween(QDate(2026, 11, 2), QDate(2026, 11, 2)).size(), 0);

        // Gana el cambio más nuevo: si el celu cambió algo después, lo viejo de la PC no lo pisa.
        QTest::qWait(5);
        pc.renameFolder(so, "SO viejo");
        QTest::qWait(1100);
        cel.renameFolder(soCel, "SO nuevo");
        push(scel);
        push(spc);   // llega después pero es más viejo: el servidor se queda con el del celu
        pull(spc);
        pull(scel);
        QCOMPARE(FolderTree(pc.folders()).find(so)->name, QString("SO nuevo"));
        QCOMPARE(FolderTree(cel.folders()).find(soCel)->name, QString("SO nuevo"));
    }

    // M39: CSV
    void csvExport()
    {
        QVector<Folder> fs{{1, 0, "Facultad", 0}, {2, 1, "Física; II", 0}};
        const FolderTree tree(fs);
        Pomodoro p;
        p.id = 1; p.folderId = 2; p.day = QDate(2026, 10, 27);
        p.startedAt = QDateTime(p.day, QTime(10, 0)).toSecsSinceEpoch();
        p.studiedSec = 25 * 60; p.plannedSec = 25 * 60; p.endedAt = p.startedAt + 1500;
        p.status = PomoStatus::Completed; p.note = "dijo \"hola\"";
        Interval b; b.kind = IntervalKind::Break; b.day = p.day; b.startedAt = p.endedAt; b.seconds = 300;
        Interval pend; pend.kind = IntervalKind::Pending; pend.day = p.day; pend.startedAt = p.endedAt + 300; pend.seconds = 60;
        const QString csv = csvexport::build({p}, {pend, b}, tree);
        const QStringList lines = csv.split("\r\n", Qt::SkipEmptyParts);
        QCOMPARE(lines.size(), 3);   // encabezado + pomodoro + recreo (el pendiente no)
        QVERIFY(lines[0].startsWith(QChar(0xFEFF)));
        QCOMPARE(lines[1], QString("2026-10-27;10:00;10:25;pomodoro;\"Facultad / Física; II\";25,0;25,0;completo;\"dijo \"\"hola\"\"\""));
        QVERIFY(lines[2].startsWith("2026-10-27;10:25;10:30;recreo;;5,0"));
        const QSet<qint64> other{1};
        QCOMPARE(csvexport::build({p}, {}, tree, &other, false).split("\r\n", Qt::SkipEmptyParts).size(), 1);
    }
};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
