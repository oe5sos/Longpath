// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Messstand. Gemessen wird WDSP selbst
// (anr.c / wcpAGC.c, Warren Pratt NR0V), nicht nachgebauter Code.
//
// "lautstaerke ist sehr leise, sobald ich auf nr1 klicke, wird es zu
// leise!" (2026-11-12, Betreiber am 40-m-Band, AF 50, Master 100).
//
// Zwei Fragen, beide gemessen statt angenommen:
//
//   1. Wie viel Pegel kostet NR1 wirklich? WDSPs anr.c ist ein LMS-Filter
//      ohne Nachverstaerkung; ein paar dB sind erwartbar, 20 waeren ein
//      Fehler. Gemessen wird der Effektivwert am Ausgang derselben Kette,
//      einmal ohne und einmal mit NR1, bei genau gleichem Eingang.
//
//   2. Was macht die gespeicherte AGC-Schwelle mit der Verstaerkung?
//      In den Einstellungen des Betreibers stehen fuer 40 m BEIDE Werte:
//        <Slice0__s__Band40m__s__AgcMaxGain>90</...>
//        <Slice0__s__Band40m__s__AgcThreshold>-20</...>
//      Die koennen nicht beide stimmen. wcpAGC.c rechnet aus der Schwelle
//      dieselbe Groesse max_gain aus, die AgcMaxGain direkt setzt --
//      welcher Wert zuletzt geschrieben wird, gewinnt.
//
// [[feedback-abwesenheit-ist-keine-aussage]]: dass im Baum keine
// Daempfung steht, heisst nicht, dass keine da ist.

#include <QtTest/QtTest>

#include "core/RxChannel.h"
#include "core/WdspEngine.h"
#include "core/WdspTypes.h"

#ifdef HAVE_WDSP
extern "C" {
#include "core/wdsp_api.h"
}
#endif

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kKanal      = 0;
constexpr int kEingang    = 1024;    // teilt WDSPs Eingangsring (2 * 4096)
constexpr int kDspGroesse = 4096;
constexpr int kRate       = 48000;

// Ein Ton bei +1000 Hz mit etwas Rauschen -- das ist, was ein Empfaenger
// im SSB-Durchlass sieht. Amplitude klein genug, dass die AGC wirklich
// verstaerken muss (sonst misst man die Begrenzung, nicht die Verstaerkung).
void fuelleBlock(std::vector<float>& i, std::vector<float>& q,
                 double& phase, double amplitude, unsigned& rngZustand)
{
    constexpr double kTonHz = 1000.0;
    const double schritt = 2.0 * M_PI * kTonHz / static_cast<double>(kRate);
    for (int n = 0; n < kEingang; ++n) {
        rngZustand = rngZustand * 1664525u + 1013904223u;
        const double rausch =
            (static_cast<double>(rngZustand >> 8) / 8388608.0 - 1.0) * amplitude * 0.05;
        i[static_cast<size_t>(n)] = static_cast<float>(amplitude * std::cos(phase) + rausch);
        q[static_cast<size_t>(n)] = static_cast<float>(amplitude * std::sin(phase) + rausch);
        phase += schritt;
        if (phase > 2.0 * M_PI) { phase -= 2.0 * M_PI; }
    }
}

} // namespace

class TestNr1PegelUndAgcSchwelle : public QObject {
    Q_OBJECT

#ifdef HAVE_WDSP
    // Faehrt `bloecke` Blocks durch die Kette und gibt den Effektivwert
    // der LETZTEN `messBloecke` zurueck -- die ersten sind Einschwingen
    // der AGC, die zaehlen nicht.
    double effektivwert(RxChannel* ch, int bloecke, int messBloecke,
                        double amplitude)
    {
        std::vector<float> inI(kEingang), inQ(kEingang);
        std::vector<float> outI(kDspGroesse * 2), outQ(kDspGroesse * 2);
        double phase = 0.0;
        unsigned rng = 12345u;
        double summe = 0.0;
        long long zahl = 0;

        for (int b = 0; b < bloecke; ++b) {
            fuelleBlock(inI, inQ, phase, amplitude, rng);
            const int heraus = kEingang;   // 48 kHz hinein wie heraus
            ch->processIq(inI.data(), inQ.data(), outI.data(), outQ.data(),
                          kEingang, heraus);
            if (b < bloecke - messBloecke) { continue; }
            for (int n = 0; n < heraus; ++n) {
                const double v = outI[static_cast<size_t>(n)];
                summe += v * v;
                ++zahl;
            }
        }
        return zahl > 0 ? std::sqrt(summe / static_cast<double>(zahl)) : 0.0;
    }
#endif

private slots:

    // ── 1. Was kostet NR1? ───────────────────────────────────────────────
    void nr1KostetWieVielPegel()
    {
#ifndef HAVE_WDSP
        QSKIP("ohne WDSP nicht messbar");
#else
        WdspEngine engine;
        engine.m_initialized = true;   // friend-Zugriff (LONGPATH_BUILD_TESTS)

        RxChannel* ch = engine.createRxChannel(kKanal, kEingang, kDspGroesse,
                                               kRate, kRate, kRate);
        QVERIFY(ch != nullptr);

        ch->setMode(DSPMode::USB);
        ch->setFilterFreqs(150.0, 3000.0);
        ch->setAgcMode(AGCMode::Med);
        ch->setAgcMaxGain(90);
        ch->setAfGain(0.5);            // wie beim Betreiber: AF-Regler auf 50
        ch->setActive(true);

        constexpr double kAmplitude = 0.001;   // rund -60 dBFS
        ch->setActiveNr(NrSlot::Off);
        const double ohne = effektivwert(ch, 120, 40, kAmplitude);

        // So steht es heute: NR1 hinter der AGC (Thetis radio.cs:1625,
        // rx_anr_position = 1). Was die AGC vorher hochgeregelt hat,
        // nimmt der LMS danach wieder weg -- niemand holt es zurueck.
        RxChannel::Nr1Tuning hinten;
        hinten.position = NrPosition::PostAgc;
        ch->setAnrTuning(hinten);
        ch->setActiveNr(NrSlot::NR1);
        const double mit = effektivwert(ch, 120, 40, kAmplitude);

        // Und derselbe Filter VOR der AGC: dann sieht die Regelung, was
        // er weggenommen hat, und regelt es wieder hoch.
        RxChannel::Nr1Tuning vorne;
        vorne.position = NrPosition::PreAgc;
        ch->setAnrTuning(vorne);
        const double mitVorne = effektivwert(ch, 120, 40, kAmplitude);

        const double verlustDb = (ohne > 0.0 && mit > 0.0)
            ? 20.0 * std::log10(mit / ohne) : -999.0;
        const double verlustVorneDb = (ohne > 0.0 && mitVorne > 0.0)
            ? 20.0 * std::log10(mitVorne / ohne) : -999.0;

        qInfo("NR1-Messung: ohne %.6f  hinter der AGC %.6f (%.1f dB)  vor der AGC %.6f (%.1f dB)",
              ohne, mit, verlustDb, mitVorne, verlustVorneDb);

        QVERIFY2(ohne > 1e-6, "ohne NR kommt schon nichts heraus");
        // Ein LMS-Entrauscher nimmt Pegel weg, das ist seine Arbeit.
        // Mehr als 12 dB waere keine Entrauschung mehr, sondern ein Leiser-
        // Regler -- dann ist dieser Fall rot und sagt, um wie viel.
        QVERIFY2(verlustDb > -14.0,
                 qPrintable(QStringLiteral("NR1 nimmt %1 dB weg").arg(verlustDb, 0, 'f', 1)));

        ch->setActive(false);
        engine.destroyRxChannel(kKanal);
#endif
    }

    // ── 2. AGC-Schwelle gegen AGC-Maximalverstaerkung ────────────────────
    //
    // Beide schreiben max_gain in wcpAGC.c. Dieser Fall misst, wie weit
    // die beiden gespeicherten Werte des Betreibers auseinanderliegen,
    // und haelt fest, welcher gewinnen MUSS.
    void schwelleUndMaximalverstaerkungSindDieselbeGroesse()
    {
#ifndef HAVE_WDSP
        QSKIP("ohne WDSP nicht messbar");
#else
        WdspEngine engine;
        engine.m_initialized = true;

        RxChannel* ch = engine.createRxChannel(kKanal, kEingang, kDspGroesse,
                                               kRate, kRate, kRate);
        QVERIFY(ch != nullptr);
        ch->setMode(DSPMode::USB);
        ch->setFilterFreqs(150.0, 3000.0);
        ch->setAgcMode(AGCMode::Med);

        // So kommen die Werte aus den Einstellungen des Betreibers.
        ch->setAgcMaxGain(90);
        const double obenNachMaxGain = ch->readBackAgcTop();
        const double passendeSchwelle = ch->readBackAgcThresh();

        // -21 statt -20: `RxChannel::setAgcThreshold` hat eine
        // Gleichheitsschranke und sein eigener Anfangswert IST -20 --
        // der gespeicherte Wert des Betreibers laeuft also heute
        // zufaellig ins Leere, statt die Verstaerkung einzureissen.
        // Genau das macht ihn so tueckisch: ein einziger Zug am
        // AGC-T-Regler und er ist wieder scharf.
        ch->setAgcThreshold(-21);
        const double obenNachSchwelle = ch->readBackAgcTop();

        qInfo("AGC: MaxGain 90 -> oben %.1f dB, dazu passende Schwelle %.1f dBm",
              obenNachMaxGain, passendeSchwelle);
        qInfo("AGC: Schwelle -21 dBm -> oben %.1f dB", obenNachSchwelle);

        QCOMPARE(qRound(obenNachMaxGain), 90);

        // Der Kern: die gespeicherte Schwelle -20 beschreibt eine voellig
        // andere Verstaerkung als die gespeicherten 90 dB. Wer sie nach
        // der Maximalverstaerkung schreibt, dreht den Empfaenger leise.
        QVERIFY2(obenNachSchwelle < obenNachMaxGain - 40.0,
                 "Schwelle -21 muesste die Verstaerkung einbrechen lassen");

        // Und darum ist die Anzeige -20 kein Messwert, sondern ein
        // stehengebliebener Vorgabewert: zu 90 dB gehoert diese Schwelle.
        QVERIFY2(passendeSchwelle < -40.0,
                 "zu 90 dB Maximalverstaerkung gehoert eine Schwelle weit unter -40 dBm");

        engine.destroyRxChannel(kKanal);
#endif
    }

    // ── 3. Die Behebung: Anzeige abgeleitet, Verstaerkung wirklich gesetzt ─
    void abgeleiteteSchwellePasstZurVerstaerkung()
    {
#ifndef HAVE_WDSP
        QSKIP("ohne WDSP nicht messbar");
#else
        WdspEngine engine;
        engine.m_initialized = true;

        RxChannel* ch = engine.createRxChannel(kKanal, kEingang, kDspGroesse,
                                               kRate, kRate, kRate);
        QVERIFY(ch != nullptr);
        ch->setMode(DSPMode::USB);
        ch->setFilterFreqs(150.0, 3000.0);
        ch->setAgcMode(AGCMode::Med);

        // Gegenprobe gegen den alten Zustand: WDSP steht nach dem Anlegen
        // auf 90 dB (WdspEngine seedet SetRXAAGCTop mit
        // kDefaultAgcMaxGainDb) -- die ANZEIGE aber auf -20 dBm, und das
        // sind laut der Messung oben zwei Zahlen, die 97 dB auseinander
        // liegen. Beides steht gleichzeitig auf dem Schirm.
        QCOMPARE(qRound(ch->readBackAgcTop()), 90);
        const int angezeigtVorher = ch->agcThreshold();
        const int wahrheitVorher = static_cast<int>(std::lround(ch->readBackAgcThresh()));
        qInfo("alter Zustand: Anzeige %d dBm, wirklich %d dBm -- %d dB Unterschied",
              angezeigtVorher, wahrheitVorher, angezeigtVorher - wahrheitVorher);
        QCOMPARE(angezeigtVorher, -20);
        QVERIFY2(angezeigtVorher - wahrheitVorher > 80,
                 "die Anzeige muesste weit neben der Wahrheit liegen");

        // Und der neue Weg: er setzt die Verstaerkung wirklich und meldet
        // die Schwelle, die dazu gehoert -- Anzeige und Gerät sagen
        // dasselbe.
        const int schwelle = ch->setzeMaxVerstaerkungUndMeldeSchwelle(90);
        qInfo("neuer Weg: 90 dB -> oben %.1f dB, Schwelle %d dBm",
              ch->readBackAgcTop(), schwelle);
        QCOMPARE(qRound(ch->readBackAgcTop()), 90);
        QCOMPARE(schwelle, ch->agcThreshold());
        QCOMPARE(schwelle, wahrheitVorher);
        QVERIFY2(schwelle < -90,
                 "zu 90 dB gehoert eine Schwelle weit unter -90 dBm");

        engine.destroyRxChannel(kKanal);
#endif
    }
};

QTEST_MAIN(TestNr1PegelUndAgcSchwelle)
#include "tst_nr1_pegel_und_agc_schwelle.moc"
