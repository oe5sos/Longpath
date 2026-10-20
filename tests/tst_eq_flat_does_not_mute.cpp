// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_eq_flat_does_not_mute.cpp  (Longpath)
// =================================================================
// „Flach" heisst durchsichtig, nicht stumm.
//
// Der Knopf „Flat" setzte den Ausgangspegel des Entzerrers auf 0 --
// und das ist ein LINEARER Faktor, kein Dezibelwert. Null bedeutet
// dort nicht „unveraendert", sondern „mal null". Die Kette war
// danach vollstaendig stumm, und zwar dauerhaft: der Wert wird
// gespeichert.
//
// Der Betreiber meldete es als „wenn ich channel strip aktiviere
// hoere ich gar nichts", und das Protokoll bestaetigte es auf die
// Nachkommastelle: in -33 dBFS, out -120 dBFS.
//
// Es ist die naheliegendste Verwechslung in diesem ganzen Fenster:
// jeder andere Pegel darin ist in Dezibel, wo null „unveraendert"
// heisst. Genau bei dem einen, der es nicht ist, heisst null „aus".
// Deshalb steht hier ein Prueffall und nicht nur ein Kommentar.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/ClientEq.h"
#include "core/strip/EqBandLayout.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <cmath>
#include <vector>

using namespace Longpath;

class TstEqFlatDoesNotMute : public QObject
{
    Q_OBJECT
private slots:

    // Der Vorgabewert ist EINS. Wer ihn auf null setzt, schaltet ab.
    void masterGainIsLinearAndOneIsUnity()
    {
        ClientEq eq;
        QCOMPARE(double(eq.masterGain()), 1.0);

        eq.setMasterGain(0.0f);
        QCOMPARE(double(eq.masterGain()), 0.0);
        eq.setMasterGain(1.0f);
        QCOMPARE(double(eq.masterGain()), 1.0);
    }

    // Eine gespeicherte Null wird beim Wiederherstellen repariert.
    //
    // Sonst ueberlebt sie jeden Profilwechsel und jeden Neustart --
    // „die profile kann ich auch nicht mehr aktivieren" war genau
    // das. Erreichbar ist eine echte Null ueber die Oberflaeche
    // nicht: der Ausgangssteller reicht bis -60 dB. Sie kann nur aus
    // einem Fehler stammen.
    void aStoredZeroIsRepairedOnRestore()
    {
        QStandardPaths::setTestModeEnabled(true);

        StripChain a;
        a.prepare(48000.0);
        EqBandLayout::ensureSeeded(a.eq());
        a.eq().setMasterGain(0.0f);
        StripSettings::save(a);

        StripChain b;
        b.prepare(48000.0);
        StripSettings::restore(b);
        QCOMPARE(double(b.eq().masterGain()), 1.0);

        // Ein gueltiger Wert bleibt unangetastet.
        a.eq().setMasterGain(0.5f);
        StripSettings::save(a);
        StripChain c;
        c.prepare(48000.0);
        StripSettings::restore(c);
        QCOMPARE(double(c.eq().masterGain()), 0.5);
    }

    // Und das Ganze am Ton: eine flache Kette laesst durch.
    void aFlattenedEqPassesTheSignal()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        EqBandLayout::ensureSeeded(c.eq());
        c.setStageEnabled(StripChain::Stage::Eq, true);

        // Das, was der Knopf „Flat" tut.
        for (int i = 0; i < EqBandLayout::kBandCount; ++i) {
            ClientEq::BandParams p = c.eq().band(i);
            p.gainDb = 0.0f;
            p.enabled = false;
            c.eq().setBand(i, p);
        }
        c.eq().setMasterGain(1.0f);

        std::vector<float> x(4800);
        for (size_t i = 0; i < x.size(); ++i) {
            x[i] = float(0.3 * std::sin(2.0 * M_PI * 700.0
                                        * double(i) / 48000.0));
        }
        for (size_t at = 0; at + 64 <= x.size(); at += 64) {
            c.processMono(x.data() + at, 64);
        }
        double peak = 0.0;
        for (size_t i = x.size() / 2; i < x.size(); ++i) {
            peak = std::max(peak, std::abs(double(x[i])));
        }
        QVERIFY2(peak > 0.25,
                 qPrintable(QStringLiteral("eine flache Kette ist stumm: "
                                           "Spitze %1").arg(peak)));
    }
};

QTEST_MAIN(TstEqFlatDoesNotMute)
#include "tst_eq_flat_does_not_mute.moc"
