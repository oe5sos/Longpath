// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_eingangspegel.cpp  (Longpath)
// =================================================================
// „die rohdaten (natürlich mit voller bandbreite) sind bei allen
//  profilen um weiten voraus."
//
// Der Grund stand in seinen eigenen Zahlen: die Stimme kam mit -25
// bis -35 dBFS an. JEDE Schwelle dieser Kette ist in dBFS. Bei
// 15 dB zu wenig Pegel hält das Gate die Stimme für den Raum und
// macht zu, der Kompressor greift nie, der Clipper schneidet nichts
// ab — die Kette verschluckt Wörter und färbt, und das rohe Signal
// gewinnt zu Recht.
//
// Geprüft wird, dass die Verstärkung wirklich VOR allem sitzt und
// dass der gemessene Rohpegel unberührt bleibt — er ist die Zahl,
// aus der die Empfehlung gerechnet wird.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/StripChain.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

std::vector<float> ton(double amp, int n = 4800)
{
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        v[size_t(i)] = float(amp * std::sin(2.0 * M_PI * 500.0 * i / 48000.0));
    }
    return v;
}

} // namespace

class TstEingangspegel : public QObject {
    Q_OBJECT

private slots:
    // Die Verstärkung wirkt, und sie wirkt auf das, was die Stufen
    // sehen -- nicht erst irgendwo hinten.
    void theGainLiftsWhatTheStagesSee()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        c.setInputGainDb(12.0f);

        auto x = ton(0.05);              // rund -26 dBFS
        c.processMono(x.data(), int(x.size()));

        const double roh = double(c.rawInputPeakDb());
        const double nach = double(c.inputPeakDb());

        qInfo() << "roh" << roh << "dB · nach der Verstärkung" << nach << "dB";
        QVERIFY2(std::fabs(roh + 26.0) < 1.0,
                 qPrintable(QStringLiteral("Rohpegel %1 statt -26")
                                .arg(roh, 0, 'f', 1)));
        QVERIFY2(std::fabs(nach - (roh + 12.0)) < 0.3,
                 qPrintable(QStringLiteral("%1 statt %2")
                                .arg(nach, 0, 'f', 1).arg(roh + 12.0, 0, 'f', 1)));
    }

    // Ohne Verstärkung bleibt alles, wie es war -- sonst wäre die
    // neue Stufe eine stille Änderung an jedem bestehenden Profil.
    void zeroGainChangesNothing()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        c.setInputGainDb(0.0f);

        const auto src = ton(0.3);
        auto x = src;
        c.processMono(x.data(), int(x.size()));
        for (size_t i = 0; i < src.size(); ++i) { QCOMPARE(x[i], src[i]); }
        QVERIFY(std::fabs(double(c.rawInputPeakDb() - c.inputPeakDb())) < 0.01);
    }

    // Und das ist der eigentliche Befund in einer Zeile: bei zu
    // leisem Signal macht das Gate auf der Stimme zu.
    void aQuietVoiceIsEatenByTheGateUntilTheGainIsRight()
    {
        const auto src = ton(0.05);      // -26 dBFS, wie am Gerät

        const auto durch = [&](float gain) {
            StripChain c;
            c.prepare(48000.0);
            c.setEnabled(true);
            c.setInputGainDb(gain);
            c.gate().setThresholdDb(-20.0f);   // wie in einem Profil
            c.gate().setFloorDb(-30.0f);
            c.setStageEnabled(StripChain::Stage::Gate, true);
            auto x = src;
            // Mehrere Durchläufe, damit die Hüllkurve einschwingt.
            for (int k = 0; k < 20; ++k) {
                x = src;
                c.processMono(x.data(), int(x.size()));
            }
            double p = 0.0;
            for (float v : x) { p = std::max(p, std::fabs(double(v))); }
            return 20.0 * std::log10(std::max(p, 1e-9));
        };

        const double leise = durch(0.0f);
        const double laut  = durch(14.0f);
        qInfo() << "ohne Verstärkung" << leise << "dB · mit +14 dB"
                << laut << "dB";

        QVERIFY2(laut > leise + 14.0,
                 qPrintable(QStringLiteral("leise %1 dB, laut %2 dB -- das "
                                           "Gate frisst nicht, der Befund "
                                           "stimmt nicht")
                                .arg(leise, 0, 'f', 1).arg(laut, 0, 'f', 1)));
    }
};

QTEST_MAIN(TstEingangspegel)
#include "tst_eingangspegel.moc"
