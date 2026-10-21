// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_eigene_tonquelle_ersetzt_das_mikrofon.cpp  (Longpath)
// =================================================================
//
// Der Sender war getastet, und es ging nichts hinaus.
//
// Gefunden am 2026-10-21 am laufenden Geraet: Betreiber drueckte CQ
// auf FT8, im Protokoll stand `mox=true` ueber volle 15 Sekunden --
// und in jeder Zeile derselben 15 Sekunden `maxTX=0`. Traeger
// getastet, null Modulation.
//
// Die Ursache stand seit dem 2026-04-28 im Kopf von TxChannel:
//
//     „driven and no longer pulls from m_micRouter"
//     „the PC-mic path no longer pulls from it"
//
// Die Sendepumpe wurde damals von ZIEHEN auf SCHIEBEN umgestellt.
// FT8 (2026-10-15) und RTTY (2026-10-16) haengten ihren Ton aber
// genau in diesen Router -- ein Ende, aus dem niemand mehr holt.
//
// Warum `tst_ft8_senden` trotzdem gruen war: er faehrt eine
// geschlossene Schleife und holt die Proben DIREKT an der Quelle ab.
// Er belegt den Erzeuger, nicht die Verdrahtung. Dieser Lauf hier
// prueft die Verdrahtung -- also das, was gefehlt hat.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/TxMicRouter.h"
#include "core/audio/CompositeTxMicRouter.h"

using namespace Longpath;

namespace {

/// Eine Quelle, die einen erkennbaren Ton liefert -- hier schlicht
/// lauter Einsen, damit „kam etwas an" nicht von Rundungen abhaengt.
class LauteQuelle : public TxMicRouter {
public:
    int pullSamples(float* dst, int n) override
    {
        for (int i = 0; i < n; ++i) { dst[i] = 1.0f; }
        ++rufe;
        return n;
    }
    int rufe{0};
};

} // namespace

class TstEigeneTonquelle : public QObject
{
    Q_OBJECT

private slots:

    void ohneEigeneQuelleErsetztNichtsDasMikrofon()
    {
        CompositeTxMicRouter r(nullptr, nullptr, false);
        QVERIFY2(!r.eigeneTonquelleAktiv(),
                 "ein frischer Router behauptet eine eigene Tonquelle");

        // Auch mit gewaehlter Betriebsart, aber OHNE angemeldete
        // Quelle: dann faellt der Router auf das Mikrofon zurueck
        // (siehe pullSamples), und der Sendefaden darf es NICHT
        // abklemmen.
        r.setActiveSource(MicSource::Ft8);
        QVERIFY2(!r.eigeneTonquelleAktiv(),
                 "FT8 gewaehlt, aber keine Quelle angemeldet -- das "
                 "Mikrofon muesste weiter gelten");
    }

    void ft8UndRttyErsetzenDasMikrofon()
    {
        LauteQuelle ft8;
        LauteQuelle rtty;
        CompositeTxMicRouter r(nullptr, nullptr, false);
        r.setFt8Source(&ft8);
        r.setRttySource(&rtty);

        // Mikrofon gewaehlt: keine Uebersteuerung.
        r.setActiveSource(MicSource::Pc);
        QVERIFY(!r.eigeneTonquelleAktiv());

        r.setActiveSource(MicSource::Ft8);
        QVERIFY2(r.eigeneTonquelleAktiv(),
                 "FT8 laeuft, meldet sich aber nicht als eigene Quelle "
                 "-- genau so ging am 2026-10-21 nichts hinaus");

        r.setActiveSource(MicSource::Rtty);
        QVERIFY2(r.eigeneTonquelleAktiv(), "RTTY ebenso");
    }

    void undDerTonKommtAuchWirklichHeraus()
    {
        // Die Meldung allein genuegt nicht: der Sendefaden holt danach
        // ueber DIESELBE Schnittstelle, also muss sie liefern.
        LauteQuelle ft8;
        CompositeTxMicRouter r(nullptr, nullptr, false);
        r.setFt8Source(&ft8);
        r.setActiveSource(MicSource::Ft8);

        std::vector<float> block(480, 0.0f);
        const int got = r.pullSamples(block.data(), int(block.size()));
        QCOMPARE(got, int(block.size()));
        QCOMPARE(ft8.rufe, 1);

        // „maxTX" des Prueflaufs: war irgendetwas ungleich null dabei?
        float spitze = 0.0f;
        for (float v : block) { spitze = std::max(spitze, std::abs(v)); }
        QVERIFY2(spitze > 0.5f,
                 "der Block ist still -- genau das meldete das Geraet "
                 "als maxTX=0");
    }
};

QTEST_APPLESS_MAIN(TstEigeneTonquelle)
#include "tst_eigene_tonquelle_ersetzt_das_mikrofon.moc"
