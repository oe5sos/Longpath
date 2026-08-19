// =================================================================
// tests/tst_signal_history_store.cpp  (NereusSDR)
// =================================================================
//
// Die Zeitverwaltung hinter dem „S-Verlauf".
//
// Port aus AetherSDR MainWindow.cpp:9568-9877 [@0cd4559].
//
// Der Grund, warum diese Klasse ueberhaupt eine eigene ist statt in
// MainWindow zu liegen: hier laesst sich die UHR EINSPEISEN. Das
// Vorbild ruft QDateTime::currentMSecsSinceEpoch() mitten in der
// Rechnung — wer das pruefen will, muss zwei Minuten warten, um zu
// sehen, ob ein Sprachsignal nach zwei Minuten als Stoerung gilt. Hier
// sind zwei Minuten eine Zahl.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>

#include "core/SignalHistoryStore.h"

using namespace NereusSDR;

namespace {

constexpr qint64 kT0 = 1'700'000'000'000LL;  // irgendein fester Zeitpunkt

DetectedVoiceSignal voice(double freqMhz, float peakDbm = -90.0f,
                          double widthHz = 2400.0)
{
    return DetectedVoiceSignal{freqMhz, peakDbm, QStringLiteral("USB"), widthHz};
}

DetectedVoiceSignal carrier(double freqMhz)
{
    return DetectedVoiceSignal{freqMhz, -80.0f, QStringLiteral("USB"), 100.0};
}

// Speist ein Signal ueber eine Spanne hinweg mit gleichmaessigen
// Treffern ein — so wie es ein laufender Panadapter taete.
void feed(SignalHistoryStore& s, const DetectedVoiceSignal& sig,
          qint64 fromMs, qint64 toMs, qint64 stepMs)
{
    for (qint64 t = fromMs; t <= toMs; t += stepMs) {
        s.ingest({sig}, t);
    }
}

} // namespace

class TestSignalHistoryStore : public QObject
{
    Q_OBJECT

private slots:

    void emptyStoreHasNothing()
    {
        SignalHistoryStore s;
        QVERIFY(s.entries().isEmpty());
        QVERIFY(s.visibleEntries().isEmpty());
    }

    // ── Zusammenfuehrung ─────────────────────────────────────────────

    void aNewSignalBecomesAnEntry()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205)}, kT0);
        QCOMPARE(s.entries().size(), 1);
        QCOMPARE(s.entries().first().firstDetectedMs, kT0);
    }

    // Innerhalb von 2 kHz ist es dieselbe Station — sonst bekaeme jede
    // Bildschwankung einen eigenen Eintrag.
    void nearbyDetectionsMergeIntoOneEntry()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205000)}, kT0);
        s.ingest({voice(14.205500)}, kT0 + 100);  // 500 Hz daneben
        QCOMPARE(s.entries().size(), 1);
        QCOMPARE(s.entries().first().hitTimestamps.size(), 2);
    }

    void distantDetectionsStaySeparate()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205)}, kT0);
        s.ingest({voice(14.210)}, kT0 + 100);  // 5 kHz daneben
        QCOMPARE(s.entries().size(), 2);
    }

    // Der lauteste Treffer bestimmt Pegel UND Frequenz: ein Signal
    // wandert im Bild, und der Gipfel ist der beste Bezugspunkt.
    void theLoudestHitSetsFrequencyAndLevel()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205000, -95.0f)}, kT0);
        s.ingest({voice(14.205400, -70.0f)}, kT0 + 100);
        QCOMPARE(s.entries().first().peakDbm, -70.0f);
        QCOMPARE(s.entries().first().freqMhz, 14.205400);
    }

    // Die groesste je gesehene Breite entscheidet ueber die Einordnung:
    // schwach wirkt ein Signal schmal, am Gipfel zeigt es seine Breite.
    void theWidestDetectionWins()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205, -90.0f, 2400.0)}, kT0);
        s.ingest({voice(14.205, -90.0f, 1000.0)}, kT0 + 100);
        QCOMPARE(s.entries().first().widthHz, 2400.0);
    }

    // ── Qualifizierung ───────────────────────────────────────────────

    // Ein einzelner Treffer ist Rauschen, keine Station. Er darf nicht
    // sichtbar werden, egal wie stark er war.
    void aSingleFlashNeverBecomesVisible()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205, -60.0f)}, kT0);
        s.rebuild(kT0 + 100, 25.0f);
        QVERIFY(s.visibleEntries().isEmpty());
    }

    // Sprache wird sichtbar nach 3 Sekunden UND mindestens 3 Treffern.
    void aSteadyVoiceSignalQualifies()
    {
        SignalHistoryStore s;
        feed(s, voice(14.205), kT0, kT0 + 3500, 200);
        s.rebuild(kT0 + 3500, 25.0f);
        QCOMPARE(s.visibleEntries().size(), 1);
        QVERIFY2(s.visibleEntries().first().confirmedVoice,
                 "eine ruhige Station gilt als bestaetigte Sprache");
        QVERIFY2(!s.visibleEntries().first().suspectQrm,
                 "Sprache ist keine Stoerung");
    }

    // Drei Sekunden allein reichen nicht — es braucht auch Treffer.
    void ageAloneIsNotEnough()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205)}, kT0);
        s.ingest({voice(14.205)}, kT0 + 3400);  // nur 2 Treffer
        s.rebuild(kT0 + 3500, 25.0f);
        QVERIFY(s.visibleEntries().isEmpty());
    }

    // Bandwechsel: bis zur Sperrfrist wird nichts neu sichtbar.
    void nothingBecomesVisibleWhileSuppressed()
    {
        SignalHistoryStore s;
        feed(s, voice(14.205), kT0, kT0 + 3500, 200);
        s.rebuild(kT0 + 3500, 25.0f, /*suppressUntilMs=*/kT0 + 10000);
        QVERIFY(s.visibleEntries().isEmpty());

        s.rebuild(kT0 + 10001, 25.0f, kT0 + 10000);
        QVERIFY2(s.visibleEntries().isEmpty(),
                 "nach der Sperre zaehlen wieder frische Treffer, nicht alte");
    }

    // ── Stoerungen ───────────────────────────────────────────────────

    // Ein Dauertraeger: schmal, ununterbrochen, ueber der Torzeit. Bei
    // 25 fps verlangt das Vorbild 70 % Belegung, also 105 Treffer in
    // 6 Sekunden.
    void aContinuousCarrierBecomesQrm()
    {
        SignalHistoryStore s;
        feed(s, carrier(14.205), kT0, kT0 + 7000, 40);  // 25 fps
        s.rebuild(kT0 + 7000, 25.0f);

        QCOMPARE(s.visibleEntries().size(), 1);
        QVERIFY2(s.visibleEntries().first().suspectQrm,
                 "ein Traeger, der 6 s ununterbrochen steht, ist eine Stoerung");
    }

    // Derselbe Traeger mit einer Luecke ist keine Stoerung: Luecken sind
    // das Merkmal von Betrieb, nicht von Stoerung.
    void aCarrierWithAGapIsNotQrm()
    {
        SignalHistoryStore s;
        feed(s, carrier(14.205), kT0, kT0 + 3000, 40);
        // 2 Sekunden Pause
        feed(s, carrier(14.205), kT0 + 5000, kT0 + 7000, 40);
        s.rebuild(kT0 + 7000, 25.0f);

        for (const auto& e : s.entries()) {
            QVERIFY2(!e.suspectQrm, "eine Luecke schliesst Stoerung aus");
        }
    }

    // Sprachbreite Signale brauchen ZWEI MINUTEN ohne Luecke, bis sie
    // als Stoerung gelten — sonst waere jedes lange QSO eine Stoerung.
    void voiceWidthNeedsTwoMinutesToCountAsQrm()
    {
        SignalHistoryStore s;
        feed(s, voice(14.205), kT0, kT0 + 30000, 40);
        s.rebuild(kT0 + 30000, 25.0f);
        QVERIFY2(!s.visibleEntries().first().suspectQrm,
                 "eine halbe Minute Sprache ist keine Stoerung");

        // Weiter bis ueber zwei Minuten, ohne Luecke.
        feed(s, voice(14.205), kT0 + 30040, kT0 + 121000, 40);
        s.rebuild(kT0 + 121000, 25.0f);
        QVERIFY2(s.visibleEntries().first().suspectQrm,
                 "zwei Minuten ununterbrochene Sprachbreite gelten als Stoerung");
    }

    // ── Vergehen ─────────────────────────────────────────────────────

    // Sichtbare Marken verschwinden nach 30 Sekunden Abwesenheit.
    void aVisibleMarkerHidesAfterThirtySeconds()
    {
        SignalHistoryStore s;
        feed(s, voice(14.205), kT0, kT0 + 3500, 200);
        s.rebuild(kT0 + 3500, 25.0f);
        QCOMPARE(s.visibleEntries().size(), 1);

        s.rebuild(kT0 + 3500 + 31000, 25.0f);
        QVERIFY2(s.visibleEntries().isEmpty(),
                 "nach 30 s ohne Sichtung ist die Marke weg");
    }

    // Der Eintrag selbst faellt erst nach der Lebensdauer weg — bis
    // dahin kann dieselbe Station zurueckkommen, ohne neu zu
    // qualifizieren.
    void theEntryItselfSurvivesUntilTheLifetime()
    {
        SignalHistoryStore s;
        s.ingest({voice(14.205)}, kT0);

        s.expire(kT0 + 59000);
        QCOMPARE(s.entries().size(), 1);

        s.expire(kT0 + 61000);
        QVERIFY(s.entries().isEmpty());
    }

    void lifetimeIsAdjustable()
    {
        SignalHistoryStore s;
        s.setLifetimeSeconds(20);
        s.ingest({voice(14.205)}, kT0);
        s.expire(kT0 + 21000);
        QVERIFY(s.entries().isEmpty());
    }

    // Die Regler des Vorbilds sind begrenzt (3..30 s, 15..300 s), und
    // die Grenzen gehoeren in die Klasse, nicht in die Bedienflaeche:
    // eine zweite Bedienflaeche wuerde sie sonst umgehen.
    void thresholdsAreClamped()
    {
        SignalHistoryStore s;
        s.setQrmGateSeconds(1);
        QCOMPARE(s.qrmGateSeconds(), 3);
        s.setQrmGateSeconds(999);
        QCOMPARE(s.qrmGateSeconds(), 30);

        s.setLifetimeSeconds(1);
        QCOMPARE(s.lifetimeSeconds(), 15);
        s.setLifetimeSeconds(9999);
        QCOMPARE(s.lifetimeSeconds(), 300);
    }

    void clearEmptiesTheStore()
    {
        SignalHistoryStore s;
        feed(s, voice(14.205), kT0, kT0 + 3500, 200);
        s.rebuild(kT0 + 3500, 25.0f);
        QVERIFY(!s.entries().isEmpty());
        s.clear();
        QVERIFY(s.entries().isEmpty());
    }

    // ── Die Bildrate zaehlt ──────────────────────────────────────────
    // Bei 10 fps sind 6 Sekunden 60 Bilder, nicht 150. Eine feste
    // Trefferzahl waere bei jeder anderen Rate falsch — deshalb rechnet
    // das Vorbild sie aus der gemessenen Rate aus (untere Schranke 30).
    void theHitRequirementFollowsTheFrameRate()
    {
        SignalHistoryStore fast;
        feed(fast, carrier(14.205), kT0, kT0 + 7000, 40);   // 25 fps
        fast.rebuild(kT0 + 7000, 25.0f);
        QVERIFY(fast.visibleEntries().first().suspectQrm);

        // Dieselbe Trefferfolge, aber die Klasse glaubt an 60 fps: dann
        // sind 175 Treffer noetig und 175 sind nicht da.
        SignalHistoryStore slow;
        feed(slow, carrier(14.205), kT0, kT0 + 7000, 40);
        slow.rebuild(kT0 + 7000, 60.0f);
        QVERIFY2(slow.visibleEntries().isEmpty(),
                 "bei hoeherer Bildrate reichen dieselben Treffer nicht");
    }
};

QTEST_MAIN(TestSignalHistoryStore)
#include "tst_signal_history_store.moc"
