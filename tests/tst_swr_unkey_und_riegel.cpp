// no-port-check: Longpath-original unit-test file. The Thetis cites below
// record which upstream lines each assertion pins; no upstream logic is
// translated in this file.
// =================================================================
// tests/tst_swr_unkey_und_riegel.cpp  (Longpath)
// =================================================================
//
// 2026-10-28. Zwei Dinge am SWR-Schutz gegen Thetis nachgezogen, und
// dieser Lauf haelt beide fest.
//
// ── Die Freigabe bei jedem Unkey ─────────────────────────────────────
//
// Thetis setzt in UIMOXChangedFalse fuenf Dinge zurueck
// (console.cs:29241-29246 [v2.10.3.15]) und ZWEI ausdruecklich nicht:
// high_swr_count und alex_swr. Wir setzten beide zurueck. Das zweite ist
// das, was man merkt: der gemessene Stehwert sprang in der Sekunde des
// Loslassens auf 1,00 -- also genau die Zahl weg, derentwegen man
// loslaesst. Dazu lief die Rechnung ueberhaupt weiter, waehrend nichts
// mehr sendete; Thetis rechnet nur unter `if (_mox)`
// (console.cs:25991 [v2.10.3.15]).
//
// ── Der Riegel nach offener Antenne ──────────────────────────────────
//
// Thetis laesst bei offener Antenne den Schluessel fallen
// (`chkMOX.Checked = false;`, console.cs:26043 [v2.10.3.15]) und warnt.
// Bei uns setzte der Controller openAntennaDetected() -- und NIEMAND las
// es. Es wurde auf ein Prozent zurueckgefahren und weitergesendet.
//
// Der Riegel selbst ist eine bewusste Abweichung von Thetis: dort darf
// PollPTT (console.cs:25470-25555 [v2.10.3.15]) im naechsten Durchgang
// gleich wieder tasten, wenn die Taste noch gedrueckt ist -- und das
// Mikrofon-PTT meldet sich auf JEDEM Statusrahmen, 50 bis 100 Mal in der
// Sekunde. Aus "Taste halten" wird dann tasten, auslösen, abtasten,
// tasten -- Dutzende Male pro Sekunde in eine offene Leitung.
// =================================================================

#include <QtTest/QtTest>
#include <QCoreApplication>
#include <QObject>
#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/HpsdrModel.h"
#include "core/MoxController.h"
#include "core/safety/SwrProtectionController.h"
#include "models/Band.h"
#include "models/RadioModel.h"
#include "core/RadioStatus.h"
#include "models/SliceModel.h"

using Longpath::AppSettings;
using Longpath::DSPMode;
using Longpath::HPSDRModel;
using Longpath::MoxController;
using Longpath::RadioModel;
using Longpath::SliceModel;
using Longpath::safety::SwrProtectionController;

namespace {

void pump()
{
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
}

// Vier Proben mit SWR 3,0 bei 100 W: genau die Trippschwelle
// (kTripDebounceCount, console.cs:26120 [v2.10.3.15]).
// rho = sqrt(25/100) = 0,5 -> swr = 1,5/0,5 = 3,0.
constexpr float kBadFwd = 100.0f;
constexpr float kBadRev = 25.0f;

} // namespace

class TstSwrUnkeyUndRiegel : public QObject
{
    Q_OBJECT

private slots:

    // ═══ §1  Der Controller ══════════════════════════════════════════

    // UIMOXChangedFalse laesst high_swr_count stehen. Unkey bei drei
    // Trippversuchen und sofort wieder hinein: die vierte Probe loest
    // aus, nicht erst die siebte.
    void unkeyBehaeltDenTrippzaehler()
    {
        SwrProtectionController c;
        c.setLimit(2.0f);

        for (int i = 0; i < 3; ++i) {
            c.ingest(kBadFwd, kBadRev, false);
        }
        QCOMPARE(c.protectFactor(), 1.0f);   // noch nicht ausgeloest

        c.onMoxOff();

        // Eine einzige weitere schlechte Probe muss reichen.
        c.ingest(kBadFwd, kBadRev, false);
        QVERIFY2(c.protectFactor() < 1.0f,
                 "Der Trippzaehler wurde beim Unkey geloescht -- Thetis "
                 "laesst ihn stehen (console.cs:29241-29246)");
        QVERIFY(c.highSwr());
    }

    // Und alex_swr ist eine ABLESUNG, kein Riegel: Thetis schreibt sie
    // nur am `end:`-Punkt einer getasteten Probe, also haelt sie durch
    // den Empfang hindurch.
    void unkeyBehaeltDenGemessenenStehwert()
    {
        SwrProtectionController c;
        c.setLimit(2.0f);
        c.ingest(kBadFwd, kBadRev, false);
        QCOMPARE(c.measuredSwr(), 3.0f);

        c.onMoxOff();

        QCOMPARE(c.measuredSwr(), 3.0f);
    }

    // Der Riegel wird beim Unkey sehr wohl gelost -- das war der Fund
    // vom 2026-08-14 und gilt unveraendert weiter.
    void unkeyLoestDenRueckfahrriegel()
    {
        SwrProtectionController c;
        c.setLimit(2.0f);
        c.setWindBackEnabled(true);
        for (int i = 0; i < 4; ++i) {
            c.ingest(kBadFwd, kBadRev, false);
        }
        QVERIFY(c.windBackLatched());
        QCOMPARE(c.protectFactor(), 0.01f);

        c.onMoxOff();

        QVERIFY(!c.windBackLatched());
        QCOMPARE(c.protectFactor(), 1.0f);
    }

    // Abgeschalteter Schutz legt keinen NEUEN Fehler an: Thetis sperrt
    // mit `swrprotection` die Pruefung, den Tripp und den Riegel
    // (console.cs:26037, 26118, 26134 [v2.10.3.15]).
    void schutzAusLegtKeinenNeuenRiegelAn()
    {
        SwrProtectionController c;
        c.setLimit(2.0f);
        c.setWindBackEnabled(true);
        c.setEnabled(false);

        for (int i = 0; i < 8; ++i) {
            c.ingest(kBadFwd, kBadRev, false);
        }

        QVERIFY(!c.windBackLatched());
        QVERIFY(!c.highSwr());
        QCOMPARE(c.protectFactor(), 1.0f);
        // Gemessen wird weiter -- nur gehandelt nicht.
        QCOMPARE(c.measuredSwr(), 3.0f);
    }

    void schutzAusMeldetKeineOffeneAntenne()
    {
        SwrProtectionController c;
        c.setEnabled(false);
        c.ingest(15.0f, 14.5f, false);
        QVERIFY(!c.openAntennaDetected());
    }

    // Der Kern der Thetis-Treue: ein EINGERASTETER Rueckfahrriegel
    // ueberlebt das Abschalten des Schutzes. Thetis wendet
    // `_wind_back_engaged` ungesperrt an (console.cs:26138-26142
    // [v2.10.3.15]) und loescht es nur in UIMOXChangedFalse.
    //
    // Vorher hob ein Haken-Klick mitten im Senden die Rueckfahrt auf und
    // ging mit voller Ansteuerung in genau das zurueck, was sie
    // ausgeloest hatte.
    void schutzAusHebtEingerastetenRiegelNichtAuf()
    {
        SwrProtectionController c;
        c.setLimit(2.0f);
        c.setWindBackEnabled(true);
        for (int i = 0; i < 4; ++i) {
            c.ingest(kBadFwd, kBadRev, false);
        }
        QVERIFY(c.windBackLatched());
        QCOMPARE(c.protectFactor(), 0.01f);

        c.setEnabled(false);
        c.ingest(kBadFwd, kBadRev, false);

        QVERIFY2(c.windBackLatched(),
                 "Schutz abschalten hat den eingerasteten Riegel gelost");
        QCOMPARE(c.protectFactor(), 0.01f);
        // HighSWR geht mit dem Haken aus, wie upstream.
        QVERIFY(!c.highSwr());

        // Erst der Unkey hebt ihn.
        c.onMoxOff();
        QCOMPARE(c.protectFactor(), 1.0f);
    }

    // ═══ §2  Der Riegel im MoxController ═════════════════════════════

    void riegelWeistDasTastenAbUndNenntDenGrund()
    {
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        QSignalSpy abgelehnt(&mox, &MoxController::moxRejected);

        mox.latchKeyBlock(RadioModel::openAntennaWarningText());
        QVERIFY(mox.keyBlockLatched());

        mox.setMox(true);
        pump();

        QVERIFY2(!mox.isMox(), "Der Riegel hat das Tasten nicht verhindert");
        QCOMPARE(abgelehnt.count(), 1);
        QCOMPARE(abgelehnt.at(0).at(0).toString(),
                 RadioModel::openAntennaWarningText());
    }

    // Die eigentliche Aufgabe: eine GEHALTENE Taste meldet sich immer
    // wieder und darf nicht durchkommen.
    void gehalteneTasteBleibtAbgewiesen()
    {
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        QSignalSpy abgelehnt(&mox, &MoxController::moxRejected);

        mox.latchKeyBlock(QStringLiteral("offene Antenne"));
        for (int i = 0; i < 25; ++i) {
            mox.onMicPttFromRadio(true);   // wie ein Statusrahmen
            QVERIFY(!mox.isMox());
        }
        pump();
        QCOMPARE(abgelehnt.count(), 25);
    }

    // Loslassen gibt frei, und der naechste Druck tastet normal.
    void loslassenGibtFreiUndDerNaechsteDruckTastet()
    {
        MoxController mox;
        mox.setTimerIntervals(0, 0, 0, 0, 0, 0);
        mox.latchKeyBlock(QStringLiteral("offene Antenne"));

        mox.setMox(false);               // die Taste kommt hoch
        QVERIFY(!mox.keyBlockLatched());

        mox.setMox(true);
        pump();
        QVERIFY2(mox.isMox(),
                 "Nach dem Loslassen muss der naechste Druck tasten");
    }

    // ═══ §3  Die Verdrahtung im RadioModel ═══════════════════════════

    // moxChanging statt hardwareFlipped: die Freigabe laeuft
    // SYNCHRON im Unkey, nicht irgendwann spaeter in der
    // Ereignisschleife.
    void freigabeLaeuftSynchronImUnkey()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(3700000.0);
        slice->setDspMode(DSPMode::USB);

        model.moxController()->setMox(true);
        pump();
        QVERIFY2(model.moxController()->isMox(), "MOX kam nicht");

        // RadioModel liest den Schalter im Konstruktor aus AppSettings,
        // und die Vorgabe dort ist "False" (RadioModel.cpp:789) -- der
        // Schutz ist also aus geliefert. Im Prueflauf ausdruecklich an.
        auto& prot = model.swrProt();
        prot.setEnabled(true);
        prot.setLimit(2.0f);
        prot.setWindBackEnabled(true);
        for (int i = 0; i < 4; ++i) {
            prot.ingest(kBadFwd, kBadRev, false);
        }
        QVERIFY(prot.windBackLatched());

        // KEIN pump() dazwischen -- das ist der Punkt.
        model.moxController()->setMox(false);

        QVERIFY2(!prot.windBackLatched(),
                 "Die Freigabe wartete auf die Ereignisschleife");
        QCOMPARE(prot.protectFactor(), 1.0f);
    }

    // Thetis' `if (_mox)`: ein Nachlauf-Rahmen nach dem Unkey darf den
    // gemessenen Stehwert nicht mehr auf 1,00 stellen.
    void derNachlaufRahmenLoeschtDieAblesungNicht()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN100D);

        auto& prot = model.swrProt();
        prot.setEnabled(true);
        prot.setLimit(2.0f);
        prot.ingest(kBadFwd, kBadRev, false);
        QCOMPARE(prot.measuredSwr(), 3.0f);

        // Ungetastet: der Rahmen traegt den letzten Koppler-Nachhall
        // plus Rauschen. Vor dem 2026-10-28 lief das durch den
        // Controller und klemmte die Ablesung auf 1,00.
        model.handlePaTelemetryNotKeyedForTest(40, 10, 0, 0, 0, 0);

        QCOMPARE(prot.measuredSwr(), 3.0f);
    }

    // Offene Antenne: abtasten, zuriegeln, warnen.
    void offeneAntenneTastetAbUndRiegeltZu()
    {
        AppSettings::instance().clear();
        RadioModel model;
        model.setHpsdrModelForTest(HPSDRModel::ANAN100D);
        model.moxController()->setTimerIntervals(0, 0, 0, 0, 0, 0);
        model.addSlice();
        SliceModel* slice = model.activeSlice();
        QVERIFY(slice != nullptr);
        slice->setFrequency(3700000.0);
        slice->setDspMode(DSPMode::USB);

        // Der Schutz ist aus geliefert (AppSettings-Vorgabe "False"),
        // und die Pruefung auf offene Antenne haengt an ihm.
        model.swrProt().setEnabled(true);

        model.moxController()->setMox(true);
        pump();
        QVERIFY2(model.moxController()->isMox(), "MOX kam nicht");

        QSignalSpy warnung(&model, &RadioModel::openAntennaWarning);

        // Die Rohwerte ruecklaufend bestimmen statt zu raten: die
        // Skalierung liegt in Tabellen je Gerät, und radioStatus sagt,
        // was eine Zahl in Watt bedeutet. So wandert der Fall mit, wenn
        // sich die Tabellen aendern.
        //
        // Schritt 1 sucht eine Vorwaertsleistung ueber 10 W bei
        // Rueckwaerts NULL -- die Differenz ist dann die ganze
        // Vorwaertsleistung, die Bedingung also sicher nicht erfuellt
        // und nichts kann dabei abtasten.
        quint16 fwdRaw = 0;
        double fwdW = 0.0;
        for (quint32 raw = 1; raw < 65535; ++raw) {
            model.handlePaTelemetryForTest(static_cast<quint16>(raw), 0,
                                           0, 0, 0, 0);
            const double f = model.radioStatus().forwardPowerWatts();
            if (f > 12.0) {
                fwdRaw = static_cast<quint16>(raw);
                fwdW = f;
                break;
            }
        }
        QVERIFY2(fwdRaw != 0, "Kein Rohwert gefunden, der ueber 12 W "
                              "vorwaerts ergibt");
        qInfo("fwdRaw %u -> %.2f W", fwdRaw, fwdW);

        // Schritt 2 zieht die Rueckwaertsleistung nach, bis weniger als
        // 1 W Differenz bleibt (console.cs:26038). Der erste Treffer
        // IST der Fall: er tastet ab.
        quint16 revRaw = 0;
        for (quint32 raw = 1; raw < 65535; ++raw) {
            model.handlePaTelemetryForTest(fwdRaw, static_cast<quint16>(raw),
                                           0, 0, 0, 0);
            const double r = model.radioStatus().reflectedPowerWatts();
            if (r > fwdW) { break; }              // ueberschossen
            if ((fwdW - r) < 1.0) {
                revRaw = static_cast<quint16>(raw);
                qInfo("revRaw %u -> %.2f W, Differenz %.2f W",
                      revRaw, r, fwdW - r);
                break;
            }
        }
        QVERIFY2(revRaw != 0,
                 "Kein Rueckwerts-Rohwert gefunden, der auf unter 1 W "
                 "Differenz kommt -- die Bedingung der offenen Antenne "
                 "(console.cs:26038)");
        pump();

        QVERIFY2(!model.moxController()->isMox(),
                 "Die offene Antenne hat den Schluessel nicht fallen "
                 "lassen -- Thetis: chkMOX.Checked = false");
        QVERIFY2(model.moxController()->keyBlockLatched(),
                 "Der Schluessel ist nicht zugeriegelt");
        QCOMPARE(warnung.count(), 1);
        QCOMPARE(warnung.at(0).at(0).toString(),
                 RadioModel::openAntennaWarningText());

        // Und eine gehaltene Taste kommt nicht durch.
        model.moxController()->onMicPttFromRadio(true);
        pump();
        QVERIFY(!model.moxController()->isMox());
    }
};

QTEST_MAIN(TstSwrUnkeyUndRiegel)
#include "tst_swr_unkey_und_riegel.moc"
