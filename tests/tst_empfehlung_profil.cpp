// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_empfehlung_profil.cpp  (Longpath)
// =================================================================
// „kannst du mal deine empfehlung bei allem eingeben?"
//
// Sie steht als Ausgangspunkt „Recommended — speech". Ein Profil,
// das eine Empfehlung sein will, muss zwei Dinge wirklich tun: die
// Stufen einschalten, die es nennt — und die ANDEREN auslassen. Ein
// Profil, das den Hall anlässt, weil niemand nachgesehen hat, ist
// schlimmer als keines.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

using namespace Longpath;

class TstEmpfehlungProfil : public QObject {
    Q_OBJECT

private slots:
    void itIsOfferedAsAStartingPoint()
    {
        bool da = false;
        for (const auto& p : StripSettings::builtInPresets()) {
            if (p.name == QStringLiteral("Recommended — speech")) { da = true; }
        }
        QVERIFY2(da, "die Empfehlung steht nicht in der Liste");
    }

    void itSwitchesOnWhatItNamesAndNothingElse()
    {
        StripChain c;
        c.prepare(48000.0);
        QVERIFY(StripSettings::applyBuiltIn(
            QStringLiteral("Recommended — speech"), c));

        // An: die sechs, die die Empfehlung begründet.
        for (const auto s : {StripChain::Stage::Denoise, StripChain::Stage::Gate,
                             StripChain::Stage::Eq,      StripChain::Stage::DeEss,
                             StripChain::Stage::Comp,    StripChain::Stage::Clip,
                             StripChain::Stage::Limiter}) {
            QVERIFY2(c.stageEnabled(s),
                     qPrintable(QStringLiteral("%1 ist aus")
                                    .arg(QString::fromLatin1(
                                        StripChain::stageName(s)))));
        }

        // Aus: die drei, die färben. Hall vor allem -- er füllt die
        // Lücken zwischen den Wörtern, und genau die braucht die
        // Gegenseite, um einen vom Rauschen zu trennen.
        for (const auto s : {StripChain::Stage::Tube, StripChain::Stage::Pudu,
                             StripChain::Stage::Reverb}) {
            QVERIFY2(!c.stageEnabled(s),
                     qPrintable(QStringLiteral("%1 ist an")
                                    .arg(QString::fromLatin1(
                                        StripChain::stageName(s)))));
        }
    }

    // Die Zahlen, auf die es ankommt -- damit ein späteres Umbauen
    // der Stufen die Empfehlung nicht stillschweigend verdreht.
    void theNumbersAreTheOnesIMeant()
    {
        StripChain c;
        c.prepare(48000.0);
        QVERIFY(StripSettings::applyBuiltIn(
            QStringLiteral("Recommended — speech"), c));

        QVERIFY(std::abs(c.nr().amount() - 0.70f) < 0.01f);
        QVERIFY(std::abs(c.comp().ratio() - 3.0f) < 0.01f);
        QVERIFY(std::abs(c.clipper().driveDb() - 3.0f) < 0.01f);
        QVERIFY2(!c.clipper().levelMatch(),
                 "der Lautstärkeabgleich ist an — damit verschenkt der "
                 "Clipper genau die Leistung, die er gewinnt");
        QVERIFY(std::abs(c.limiter().ceilingDb() + 1.0f) < 0.01f);
    }

    // Und das Ganze überlebt den Weg durch die Einstellungen: was
    // gespeichert und wieder geladen wird, ist dasselbe.
    void itSurvivesSaveAndRestore()
    {
        StripChain a;
        a.prepare(48000.0);
        QVERIFY(StripSettings::applyBuiltIn(
            QStringLiteral("Recommended — speech"), a));

        StripChain b;
        b.prepare(48000.0);
        StripSettings::copyParameters(a, b);

        for (int i = 0; i < StripChain::kStageCount; ++i) {
            const auto s = static_cast<StripChain::Stage>(i);
            QVERIFY2(a.stageEnabled(s) == b.stageEnabled(s),
                     qPrintable(QStringLiteral("%1 kam anders zurück")
                                    .arg(QString::fromLatin1(
                                        StripChain::stageName(s)))));
        }
        QVERIFY(std::abs(a.clipper().driveDb() - b.clipper().driveDb()) < 0.01f);
        QVERIFY(std::abs(a.nr().amount() - b.nr().amount()) < 0.01f);
    }
};

QTEST_MAIN(TstEmpfehlungProfil)
#include "tst_empfehlung_profil.moc"
