// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_cw_tastweg_freigabe.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Die Stelle, an der die Morsetaste auf die
// Sendefreigabe trifft:
//
//   1. BandPlanGuard::checkMoxAllowed laesst CW durch die Betriebsarten-
//      Pruefung, wenn -- und nur wenn -- der Aufrufer sagt, dass der
//      Traeger ausserhalb unserer Sendekette getastet wird. Die
//      Frequenzpruefung laeuft dabei unveraendert: geoeffnet wird eine
//      Betriebsart, nie eine Bandkante.
//   2. FlexCwTastziel schickt ohne Flex nichts und nennt den Grund.
//   3. RadioModel besitzt die Tastung, ab Werk aus, und oeffnet den
//      CW-Weg nicht, solange keine Flex haengt.
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test.

#include <QtTest>

#include "core/AppSettings.h"
#include "core/WdspTypes.h"
#include "core/cw/CwTastung.h"
#include "core/safety/BandPlanGuard.h"
#include "models/Band.h"
#include "models/FlexCwTastziel.h"
#include "models/RadioModel.h"

using namespace Longpath;
using namespace Longpath::safety;

namespace {

constexpr Region kRegion = Region::UnitedStates;
constexpr std::int64_t kImBand = 14'050'000;      // 20 m, CW-Teil
constexpr std::int64_t kDaneben = 13'900'000;     // unter der Bandkante

BandPlanGuard::MoxCheckResult pruefe(std::int64_t hz, DSPMode art, bool vonAussen)
{
    BandPlanGuard guard;
    return guard.checkMoxAllowed(kRegion, hz, art, Band::Band20m, Band::Band20m,
                                 /*preventDifferentBand=*/false, /*extended=*/false,
                                 vonAussen);
}

} // namespace

class TstCwTastwegFreigabe : public QObject {
    Q_OBJECT

private slots:
    void init() { AppSettings::instance().clear(); }

    // Ohne die Ansage bleibt alles, wie es war.
    void ohneAnsageBleibtCwGesperrt()
    {
        BandPlanGuard guard;
        const auto alt = guard.checkMoxAllowed(kRegion, kImBand, DSPMode::CWU, Band::Band20m,
                                               Band::Band20m, false, false);
        QVERIFY(!alt.ok);
        QCOMPARE(alt.reason, QStringLiteral("CW TX coming in Phase 3M-2"));
        QVERIFY(!pruefe(kImBand, DSPMode::CWL, false).ok);
    }

    void mitAnsageGehtCwImBand()
    {
        QVERIFY(pruefe(kImBand, DSPMode::CWU, true).ok);
        QVERIFY(pruefe(kImBand, DSPMode::CWL, true).ok);
    }

    // Die Bandkante gilt weiter -- das ist die Zeile, auf die es ankommt.
    void mitAnsageBleibtDieBandkante()
    {
        const auto r = pruefe(kDaneben, DSPMode::CWU, true);
        QVERIFY(!r.ok);
        QCOMPARE(r.reason, QStringLiteral("Frequency outside TX-allowed range"));
    }

    // Geoeffnet wird CW, sonst nichts.
    void mitAnsageBleibenDieAnderenArten()
    {
        QVERIFY(!pruefe(kImBand, DSPMode::AM, true).ok);
        QVERIFY(!pruefe(kImBand, DSPMode::FM, true).ok);
        QVERIFY(pruefe(14'200'000, DSPMode::USB, true).ok);
        QVERIFY(pruefe(14'200'000, DSPMode::USB, false).ok);
    }

    void wortlautAufDerLeitung()
    {
        QCOMPARE(FlexCwTastziel::tastBefehl(true), QStringLiteral("cw key immediate 1"));
        QCOMPARE(FlexCwTastziel::tastBefehl(false), QStringLiteral("cw key immediate 0"));
    }

    // MOX allein reicht nicht: `flexTasten` kann nach der Freigabe noch
    // nein sagen (Stehwelle, Transverter, Verriegelung), und ein `cw key`
    // ohne `xmit` liesse eine Flex mit Break-in von selbst senden.
    void moxAlleinReichtNicht()
    {
        QVERIFY(FlexCwTastziel::darfTasten(true, true, true));
        QVERIFY(!FlexCwTastziel::darfTasten(true, true, false));
        QVERIFY(!FlexCwTastziel::darfTasten(true, false, true));
        QVERIFY(!FlexCwTastziel::darfTasten(false, true, true));
    }

    void zielOhneGeraet()
    {
        FlexCwTastziel ziel(nullptr);
        QString warum;
        QVERIFY(!ziel.bereit(&warum));
        QVERIFY(!warum.isEmpty());
        QVERIFY(!ziel.sendet());
        QVERIFY(!ziel.freigabeSteht());
        ziel.fordereSenden(true);   // darf nicht stuerzen
        ziel.taste(true);
        ziel.taste(false);
    }

    void radioModelBesitztDieTastungAbWerkAus()
    {
        RadioModel radio;
        CwTastung* t = radio.cwTastung();
        QVERIFY(t != nullptr);
        QVERIFY(!t->eingeschaltet());
        QVERIFY(!t->senderTasten());
        QVERIFY(!radio.cwTastwegOffen());
        QVERIFY(!radio.flexGetastet());
        // Ohne Verbindung ist auch ein „Tasten" keines.
        QVERIFY(!radio.flexTasten(true));
        QVERIFY(!radio.flexGetastet());

        FlexCwTastziel ziel(&radio);
        QString warum;
        QVERIFY(!ziel.bereit(&warum));
        QCOMPARE(warum, QStringLiteral("no FlexRadio is connected."));
        QVERIFY(!ziel.sendet());

        // Eingeschaltet und „Sender tasten" -- aber keine Flex: der CW-Weg
        // durch die Betriebsarten-Pruefung bleibt zu.
        t->setOhneGeraete(true);
        t->setEingeschaltet(true);
        t->setSenderTasten(true);
        QVERIFY(!radio.cwTastwegOffen());
        // Und eine Kante geht nirgendwohin: nicht bereit, MOX bleibt aus.
        t->kante(true);
        t->kante(false);
        QVERIFY(!radio.mox());
        QVERIFY(!t->tasteUnten());
    }
};

QTEST_GUILESS_MAIN(TstCwTastwegFreigabe)
#include "tst_cw_tastweg_freigabe.moc"
