// =================================================================
// tests/tst_recording_scheduler.cpp  (NereusSDR)
// =================================================================
//
// Zeitgesteuerte Aufnahme: eine persistente Liste von (Startzeit,
// Dauer, Art) und ein Zeitgeber, der sie gegen die Uhr prueft.
//
// checkNow() statt echter Wartezeit — die Testuhr ist eine gewoehnliche
// QDateTime, kein Schlafen fuer echte Minuten.
//
// AppSettings ist ein Prozess-Singleton, das ueber alle Test-Binaries
// hinweg denselben Sandkasten-Pfad teilt (siehe TestSandboxInit.cpp).
// Jeder Test raeumt darum den eigenen Schluesselraum
// (Recording/Scheduled/*) zu Beginn UND am Ende auf, statt sich auf
// eine leere Ausgangslage zu verlassen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-26 — Original for NereusSDR/Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/audio/RecordingScheduler.h"

using namespace Longpath;

namespace {

void clearScheduledKeys()
{
    auto& s = AppSettings::instance();
    for (const QString& key : s.allKeys()) {
        if (key.startsWith(QStringLiteral("Recording/Scheduled/"))) {
            s.remove(key);
        }
    }
}

ScheduledRecording someEntry(const QDateTime& startAtUtc, int durationMin = 0)
{
    ScheduledRecording e;
    e.startAtUtc = startAtUtc;
    e.durationMinutes = durationMin;
    e.kind = ScheduledRecordingKind::Audio;
    e.path = QStringLiteral("/tmp/whatever.wav");
    e.enabled = true;
    return e;
}

} // namespace

class TestRecordingScheduler : public QObject
{
    Q_OBJECT

private slots:

    void init() { clearScheduledKeys(); }
    void cleanup() { clearScheduledKeys(); }

    void addAssignsAnId()
    {
        RecordingScheduler sched;
        const int id = sched.add(someEntry(QDateTime::currentDateTimeUtc()));
        QVERIFY(id > 0);
        QVERIFY(sched.entry(id).has_value());
    }

    void idsAreNeverReused()
    {
        RecordingScheduler sched;
        const int a = sched.add(someEntry(QDateTime::currentDateTimeUtc()));
        sched.remove(a);
        const int b = sched.add(someEntry(QDateTime::currentDateTimeUtc()));
        QVERIFY2(b != a, "a removed id must not be handed out again");
    }

    void removeDropsTheEntry()
    {
        RecordingScheduler sched;
        const int id = sched.add(someEntry(QDateTime::currentDateTimeUtc()));
        sched.remove(id);
        QVERIFY(!sched.entry(id).has_value());
        QCOMPARE(sched.entries().size(), size_t(0));
    }

    // Die eigentliche Persistenz: eine zweite, unabhaengige Instanz
    // muss dieselben Eintraege sehen, inklusive derselben id.
    void survivesASaveLoadRoundTrip()
    {
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);

        int id;
        {
            RecordingScheduler sched;
            id = sched.add(someEntry(start, /*durationMin=*/30));
        }

        RecordingScheduler fresh;
        fresh.loadFromSettings();

        const auto e = fresh.entry(id);
        QVERIFY(e.has_value());
        QCOMPARE(e->startAtUtc, start);
        QCOMPARE(e->durationMinutes, 30);
        QCOMPARE(e->kind, ScheduledRecordingKind::Audio);
        QCOMPARE(e->path, QStringLiteral("/tmp/whatever.wav"));
        QVERIFY(e->enabled);
    }

    void removedEntryDoesNotSurviveReload()
    {
        int id;
        {
            RecordingScheduler sched;
            id = sched.add(someEntry(QDateTime::currentDateTimeUtc()));
            sched.remove(id);
        }
        RecordingScheduler fresh;
        fresh.loadFromSettings();
        QVERIFY(!fresh.entry(id).has_value());
    }

    // ── checkNow() ────────────────────────────────────────────────────

    void firesWhenStartTimeArrives()
    {
        RecordingScheduler sched;
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);
        const int id = sched.add(someEntry(start));

        QSignalSpy dueSpy(&sched, &RecordingScheduler::recordingDue);

        sched.checkNow(start.addSecs(-1));   // one second early
        QCOMPARE(dueSpy.count(), 0);

        sched.checkNow(start);               // exactly on time
        QCOMPARE(dueSpy.count(), 1);
        QCOMPARE(dueSpy.takeFirst().at(0).toInt(), id);
    }

    void firesOnlyOnceEvenIfStillPastDueLater()
    {
        RecordingScheduler sched;
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);
        sched.add(someEntry(start));

        QSignalSpy dueSpy(&sched, &RecordingScheduler::recordingDue);
        sched.checkNow(start);
        sched.checkNow(start.addSecs(60));
        sched.checkNow(start.addSecs(120));

        QCOMPARE(dueSpy.count(), 1);
    }

    void stopsAfterTheConfiguredDuration()
    {
        RecordingScheduler sched;
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);
        const int id = sched.add(someEntry(start, /*durationMin=*/10));

        QSignalSpy stopSpy(&sched, &RecordingScheduler::recordingShouldStop);

        sched.checkNow(start);                    // starts
        QCOMPARE(stopSpy.count(), 0);
        sched.checkNow(start.addSecs(9 * 60));     // 9 min in, not yet
        QCOMPARE(stopSpy.count(), 0);
        sched.checkNow(start.addSecs(10 * 60));    // exactly at duration
        QCOMPARE(stopSpy.count(), 1);
        QCOMPARE(stopSpy.takeFirst().at(0).toInt(), id);
    }

    void zeroDurationNeverAutoStops()
    {
        RecordingScheduler sched;
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);
        sched.add(someEntry(start, /*durationMin=*/0));

        QSignalSpy stopSpy(&sched, &RecordingScheduler::recordingShouldStop);
        sched.checkNow(start);
        sched.checkNow(start.addSecs(365 * 24 * 3600));   // a year later
        QCOMPARE(stopSpy.count(), 0);
    }

    void disabledEntriesNeverFire()
    {
        RecordingScheduler sched;
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);
        const int id = sched.add(someEntry(start));
        sched.setEnabled(id, false);

        QSignalSpy dueSpy(&sched, &RecordingScheduler::recordingDue);
        sched.checkNow(start.addSecs(60));
        QCOMPARE(dueSpy.count(), 0);
    }

    // Aus-und-wieder-an muss die Aufnahme nochmal auslösen koennen —
    // sonst bleibt ein Eintrag fuer den Rest der Sitzung stumm, nur
    // weil er einmal kurz deaktiviert war.
    void reEnablingLetsItFireAgain()
    {
        RecordingScheduler sched;
        const QDateTime start =
            QDateTime(QDate(2026, 9, 1), QTime(6, 0), Qt::UTC);
        const int id = sched.add(someEntry(start));

        QSignalSpy dueSpy(&sched, &RecordingScheduler::recordingDue);
        sched.checkNow(start);
        QCOMPARE(dueSpy.count(), 1);

        sched.setEnabled(id, false);
        sched.setEnabled(id, true);
        sched.checkNow(start.addSecs(60));
        QCOMPARE(dueSpy.count(), 2);
    }
};

QTEST_MAIN(TestRecordingScheduler)
#include "tst_recording_scheduler.moc"
