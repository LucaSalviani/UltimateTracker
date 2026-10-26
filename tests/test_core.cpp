// Tests del núcleo con reloj simulado: no hace falta esperar 25 minutos reales.
#include "core/Database.h"
#include "core/FolderTree.h"
#include "core/SessionController.h"
#include "core/Stats.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QtTest>

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
        advance(db, *s, 2 * 60);                      // 2' de pausa → indebido
        s->resume();
        advance(db, *s, 15 * 60);                     // completa
        QCOMPARE(s->state(), SessionController::State::WorkDone);
        QCOMPARE(completed.count(), 1);

        advance(db, *s, 30);                          // tardó 30 s en elegir → indebido
        s->startBreak();
        advance(db, *s, 12 * 60);                     // recreo de 10' + 2' de alarma
        QCOMPARE(s->state(), SessionController::State::BreakOver);
        QCOMPARE(alarm.count(), 1);
        s->silenceAlarm();
        advance(db, *s, 60);                          // 1' hasta dar play → indebido
        s->start();
        advance(db, *s, 7 * 60);
        s->endSession();                              // a mitad: el último queda rojo

        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 2);
        QCOMPARE(pomos[0].status, PomoStatus::Completed);
        QCOMPARE(pomos[0].studiedSec, 25 * 60);
        QCOMPARE(pomos[1].status, PomoStatus::Final);
        QCOMPARE(pomos[1].studiedSec, 7 * 60);

        QCOMPARE(sum(db, IntervalKind::Break, d), 10 * 60);
        QCOMPARE(sum(db, IntervalKind::Undue, d), 2 * 60 + 30 + 2 * 60 + 60);
        QCOMPARE(s->sessionUndueSec(), 2 * 60 + 30 + 2 * 60 + 60);
    }

    void endSessionAfterCompletedMarksItRed()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->setWorkSec(20 * 60);
        s->start();
        advance(db, *s, 20 * 60);
        s->startBreak();
        advance(db, *s, 60);
        s->endSession();
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].status, PomoStatus::Final);
        QCOMPARE(pomos[0].studiedSec, 20 * 60);

        // Después de terminar la sesión no se cuenta nada más como indebido.
        const qint64 undueBefore = sum(db, IntervalKind::Undue, d);
        advance(db, *s, 3600);
        QCOMPARE(sum(db, IntervalKind::Undue, d), undueBefore);

        // Retomar: arranca un pomodoro nuevo.
        s->start();
        QCOMPARE(s->state(), SessionController::State::Running);
        QCOMPARE(db.pomodorosBetween(d, d).size(), 2);
    }

    void endSessionWhilePaused()
    {
        Database db;
        QVERIFY(db.open(":memory:"));
        auto* s = make(db);
        s->start();
        advance(db, *s, 300);
        s->pause();
        advance(db, *s, 120);
        s->endSession();
        const QDate d(2026, 10, 26);
        const auto pomos = db.pomodorosBetween(d, d);
        QCOMPARE(pomos.size(), 1);
        QCOMPARE(pomos[0].status, PomoStatus::Final);
        QCOMPARE(pomos[0].studiedSec, 300);
        QCOMPARE(sum(db, IntervalKind::Undue, d), 120);
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
        QCOMPARE(pomos[0].status, PomoStatus::Final);
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
};

QTEST_GUILESS_MAIN(CoreTest)
#include "test_core.moc"
