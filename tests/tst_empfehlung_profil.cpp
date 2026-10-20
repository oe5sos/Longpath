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
        // 12, nicht 3. Gemessen (tst_kanalzug_messlauf): bei
        // Sprachspitzen von -12 dBFS und einer Kante bei
        // Vollaussteuerung schneidet der Clipper unter 12 dB Drive
        // nachweislich NICHTS ab -- er war nur eine Verstaerkung.
        // Der Ausgleich MUSS an sein: ohne ihn nimmt der Kompressor
        // dem Clipper 6,7 dB weg, und der Clipper ist die Stufe, die
        // den Druck macht. Gemessen: Lautheit bei gleicher Spitze
        // -15,9 gegen -13,0 LUFS, allein an diesem Haken.
        QVERIFY2(c.comp().autoMakeup(),
                 "der Makeup-Ausgleich ist aus -- dann macht mehr "
                 "Kompression die Stimme duenner");
        QVERIFY2(std::abs(c.clipper().driveDb() - 12.0f) < 0.01f,
                 qPrintable(QStringLiteral("Drive %1 statt 12 dB")
                                .arg(double(c.clipper().driveDb()))));
        // Und die Schwelle des De-Essers gilt fuer den SEITENWEG,
        // der rund 13 dB unter dem Vollband liegt. -28 hat deshalb
        // nie gegriffen; -36 ist der Anfang der Wirkung
        // (tst_bandbreiten_genau).
        QVERIFY2(std::abs(c.deEss().thresholdDb() + 36.0f) < 0.01f,
                 qPrintable(QStringLiteral("De-Esser-Schwelle %1 statt -36")
                                .arg(double(c.deEss().thresholdDb()))));
        QVERIFY2(!c.clipper().levelMatch(),
                 "der Lautstärkeabgleich ist an — damit verschenkt der "
                 "Clipper genau die Leistung, die er gewinnt");
        // -2, nicht -1: der Begrenzer laesst 0,5 dB durch und das
        // Kanalfilter des Senders hebt danach um 0,7 dB. Gemessen
        // in tst_bandbreiten_genau.
        QVERIFY2(std::abs(c.limiter().ceilingDb() + 2.0f) < 0.01f,
                 qPrintable(QStringLiteral("Decke %1 statt -2 dB")
                                .arg(double(c.limiter().ceilingDb()))));
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
