// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die Stabilitaetsregel fuer PureSignal — Kaltstart und Aussetzer.
//
// Anlass, 2026-08-23: der Betreiber hat auf Yuri EU2AVs "Thetis
// Extended Version" hingewiesen, die zwei Zutaten beschreibt, welche
// die Thetis-Hauptlinie nicht hat — Schutz vor IMD-Artefakten waehrend
// der Erstkalibrierung, und Beibehalten der Korrektur bei kurzen
// Datenaussetzern. Yuris Quelltext ist nicht veroeffentlicht; hier ist
// nur der Gedanke uebernommen.
//
// Der Grund, warum das eine eigene Klasse mit eigener Pruefung ist:
// PureSignal laesst sich sonst nur mit Sender und Messplatz pruefen.
// Ein Zustandsautomat aus drei Zahlen laesst sich hier pruefen — und
// ein Fehler in der Logik waere im Betrieb nur als "klingt manchmal
// komisch" aufgefallen.
//
// WAS DIESE PRUEFUNG NICHT KANN: sagen, ob die Schwellen richtig
// gewaehlt sind. Das zeigt nur der Betrieb an echter Hardware.

#include <QtTest>

#include "core/PureSignalStabilityPolicy.h"

using namespace Longpath;

class TstPureSignalStability : public QObject
{
    Q_OBJECT

private slots:
    void kaltstartHaeltDieKorrekturZurueck()
    {
        PureSignalStabilityPolicy p;
        // Erste Kalibrierung laeuft noch: nichts anwenden.
        QCOMPARE(p.decide(0, 120, 0),   PsCorrectionAction::Withhold);
        QCOMPARE(p.decide(1, 120, 100), PsCorrectionAction::Withhold);
        // Zweite fertig und Pegel in Ordnung: jetzt scharf.
        QCOMPARE(p.decide(2, 120, 200), PsCorrectionAction::Run);
        qInfo() << "Kaltstart: zurueckgehalten bis zur zweiten Kalibrierung";
    }

    void schlechterPegelHaeltNichtScharf()
    {
        // Zwei Kalibrierungen, aber der Rueckweg taugt nichts — dann
        // ist die Rechnung darauf gebaut und darf nicht gelten.
        PureSignalStabilityPolicy p;
        QCOMPARE(p.decide(2, 0, 0),    PsCorrectionAction::Withhold);
        QCOMPARE(p.decide(2, 900, 50), PsCorrectionAction::Withhold);
        QCOMPARE(p.decide(2, 120, 60), PsCorrectionAction::Run);
    }

    void einAussetzerFriertEin()
    {
        PureSignalStabilityPolicy p;
        QCOMPARE(p.decide(3, 120, 0), PsCorrectionAction::Run);

        // Rueckweg faellt aus. Nicht sofort einfrieren — kurze
        // Ausreisser sollen kein Flattern erzeugen.
        QCOMPARE(p.decide(3, 0, 10),  PsCorrectionAction::Run);
        QCOMPARE(p.decide(3, 0, 100), PsCorrectionAction::Run);
        // Nach 120 ms ist es kein Ausreisser mehr.
        QCOMPARE(p.decide(3, 0, 130), PsCorrectionAction::Hold);
        qInfo() << "Aussetzer: nach 120 ms eingefroren";
    }

    void auftauenDauertLaenger()
    {
        // Asymmetrisch mit Absicht: einfrieren schnell, auftauen
        // langsam. Sonst schaltet ein zuckender Rueckweg staendig hin
        // und her, und das ist schlimmer als beide Zustaende.
        PureSignalStabilityPolicy p;
        p.decide(3, 120, 0);
        p.decide(3, 0, 10);
        QCOMPARE(p.decide(3, 0, 200), PsCorrectionAction::Hold);

        // Daten wieder gut — aber noch nicht lange genug.
        QCOMPARE(p.decide(3, 120, 210), PsCorrectionAction::Hold);
        QCOMPARE(p.decide(3, 120, 500), PsCorrectionAction::Hold);
        // Nach 400 ms guter Daten wieder rechnen.
        QCOMPARE(p.decide(3, 120, 615), PsCorrectionAction::Run);
        qInfo() << "Auftauen: nach 400 ms guter Daten";
    }

    void einZuckenAlleinTautNichtAuf()
    {
        // Der Fall, der eine naive Fassung erwischt: waehrend des
        // Auftauens faellt der Rueckweg noch einmal aus. Dann muss die
        // Wartezeit von vorne laufen, sonst reicht ein einziges gutes
        // Paket alle 400 ms, um dauerhaft zu rechnen.
        PureSignalStabilityPolicy p;
        p.decide(3, 120, 0);
        p.decide(3, 0, 10);
        p.decide(3, 0, 200);            // eingefroren
        p.decide(3, 120, 210);          // Auftauen beginnt
        p.decide(3, 0, 300);            // Rueckschlag
        QCOMPARE(p.decide(3, 120, 400), PsCorrectionAction::Hold);
        QCOMPARE(p.decide(3, 120, 550), PsCorrectionAction::Hold);
        QCOMPARE(p.decide(3, 120, 810), PsCorrectionAction::Run);
        qInfo() << "Rueckschlag setzt die Wartezeit zurueck";
    }

    void eineNeueAussendungIstKeinKaltstart()
    {
        // Wer die Taste antippt, soll nicht jedes Mal wieder ohne
        // Korrektur senden. Eingeschwungen bleibt eingeschwungen.
        PureSignalStabilityPolicy p;
        p.decide(2, 120, 0);            // eingeschwungen
        p.onTransmitStart();
        QCOMPARE(p.decide(2, 120, 1000), PsCorrectionAction::Run);
        qInfo() << "Nach der Sendepause bleibt es scharf";
    }

    void nachDemZuruecksetzenGiltDerKaltstartWieder()
    {
        PureSignalStabilityPolicy p;
        p.decide(2, 120, 0);
        p.onCorrectionsCleared();
        QCOMPARE(p.decide(0, 120, 10), PsCorrectionAction::Withhold);
        qInfo() << "Nach dem Zuruecksetzen wieder Kaltstart";
    }

    void abgeschaltetAendertNichts()
    {
        // Ich habe keinen Sender, um die Schwellen zu belegen. Wer die
        // Regel nicht will, muss sie loswerden koennen — und dann darf
        // sie sich in KEINEM Zustand mehr einmischen.
        PureSignalStabilityPolicy p;
        p.enabled = false;
        QCOMPARE(p.decide(0, 0, 0),      PsCorrectionAction::Run);
        QCOMPARE(p.decide(0, 9999, 500), PsCorrectionAction::Run);
        qInfo() << "Abgeschaltet: immer Run";
    }
};

QTEST_APPLESS_MAIN(TstPureSignalStability)
#include "tst_puresignal_stability.moc"
