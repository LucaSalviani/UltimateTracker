// Tests del núcleo con reloj simulado: no hace falta esperar 25 minutos reales.
#include "core/Database.h"
#include "core/Goals.h"
#include "core/DayClock.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"
#include "core/Stats.h"

#include <QDateTime>
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
        s->skipBreak();                               // arranca el siguiente directo
        QCOMPARE(s->state(), SessionController::State::Running);
        advance(db, *s, 20 * 60);
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 2);
        QCOMPARE(pomos[0].linked, false);
        QCOMPARE(pomos[1].linked, true);
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
        QCOMPARE(s.days, 14);            // 12/10 → 25/10, cubierto por 2 comodines

        auto s1 = stats::streak(st, today, 1);
        QCOMPARE(s1.days, 4);            // el 23 usa el comodín, se corta el 21: 22 → 25

        st.insert(today);
        auto s2 = stats::streak(st, today, 2);
        QCOMPARE(s2.todayDone, true);
        QCOMPARE(s2.days, 15);
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
};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
