// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_eq_reaches_the_monitor.cpp  (Longpath)
// =================================================================
// Eine Kurvenaenderung muss bis zum Mithoerer durchkommen.
//
// „wenn ich die EQ Kurve veraendere tut sich gar nichts." Diese
// Aussage hat drei moegliche Ursachen, und keine davon ist von aussen
// von den anderen zu unterscheiden:
//
//   1. der Entzerrer rechnet nicht,
//   2. die Aenderung erreicht den Kanalzug des Mithoerers nicht
//      (er hat einen EIGENEN, der sich die Einstellung im
//      halbsekuendigen Takt holt),
//   3. es ist nichts eingeschaltet, was man hoeren koennte.
//
// Dieser Prueftand schliesst die ersten beiden aus, indem er beide
// misst statt sie zu beteuern: einmal am Kanalzug selbst, einmal
// ueber den ganzen Weg Speichern -> zweite Kette -> Wiederherstellen.
// Bleibt Ursache 3, und die ist eine Frage an die Oberflaeche, nicht
// an den Code.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"
#include "core/strip/EqBandLayout.h"
#include "core/AppSettings.h"
#include <cmath>
#include <vector>
using namespace Longpath;

static double toneLevel(const std::vector<float>& x, double hz, int rate)
{
    const double w = 2.0 * M_PI * hz / rate;
    const double c = 2.0 * std::cos(w);
    double s1 = 0.0, s2 = 0.0;
    for (float v : x) { const double s0 = double(v) + c*s1 - s2; s2 = s1; s1 = s0; }
    return std::sqrt(std::max(0.0, s1*s1 + s2*s2 - c*s1*s2)) / x.size();
}

class TstEqReachesTheMonitor : public QObject { Q_OBJECT
private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }
    void eqActuallyDoesSomething()
    {
        StripChain c;
        c.prepare(48000.0);
        c.setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c.setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        EqBandLayout::ensureSeeded(c.eq());
        qInfo() << "activeBandCount" << c.eq().activeBandCount()
                << "eq enabled" << c.eq().isEnabled();

        // Band 5 ist der Glockenfilter bei 2 kHz.
        ClientEq::BandParams p = c.eq().band(5);
        qInfo() << "band5 vorher f" << p.freqHz << "g" << p.gainDb
                << "q" << p.q << "typ" << int(p.type) << "an" << p.enabled;
        p.freqHz = 2000.0f; p.gainDb = 12.0f; p.q = 1.0f;
        p.type = ClientEq::FilterType::Peak; p.enabled = true;
        c.eq().setBand(5, p);
        c.setStageEnabled(StripChain::Stage::Eq, true);
        qInfo() << "stage Eq an?" << c.stageEnabled(StripChain::Stage::Eq)
                << "eq.isEnabled" << c.eq().isEnabled();

        std::vector<float> x(48000);
        for (size_t i = 0; i < x.size(); ++i) {
            x[i] = float(0.05 * std::sin(2.0*M_PI*2000.0*double(i)/48000.0));
        }
        const std::vector<float> before = x;
        for (size_t at = 0; at + 64 <= x.size(); at += 64) {
            c.processMono(x.data() + at, 64);
        }
        const double gain = 20.0*std::log10(
            std::max(1e-12, toneLevel(x, 2000.0, 48000))
          / std::max(1e-12, toneLevel(before, 2000.0, 48000)));
        qInfo() << "gemessene Anhebung bei 2 kHz:" << gain << "dB";
        QVERIFY2(gain > 6.0, qPrintable(QStringLiteral("nur %1 dB").arg(gain)));

        // Und jetzt der ganze Weg, den eine Kurvenaenderung zum
        // Mithoerer nimmt: speichern, in einen ZWEITEN Kanalzug
        // wiederherstellen, und dort messen.
        StripSettings::save(c);

        StripChain m;
        m.prepare(48000.0);
        StripSettings::restore(m);
        m.setEnabled(true);
        qInfo() << "zweite Kette: Eq-Stufe an?"
                << m.stageEnabled(StripChain::Stage::Eq)
                << "Baender" << m.eq().activeBandCount();
        const ClientEq::BandParams q = m.eq().band(5);
        qInfo() << "zweite Kette band5 f" << q.freqHz << "g" << q.gainDb
                << "an" << q.enabled;

        std::vector<float> y(48000);
        for (size_t i = 0; i < y.size(); ++i) {
            y[i] = float(0.05 * std::sin(2.0*M_PI*2000.0*double(i)/48000.0));
        }
        const std::vector<float> yBefore = y;
        for (size_t at = 0; at + 64 <= y.size(); at += 64) {
            m.processMono(y.data() + at, 64);
        }
        const double g2 = 20.0*std::log10(
            std::max(1e-12, toneLevel(y, 2000.0, 48000))
          / std::max(1e-12, toneLevel(yBefore, 2000.0, 48000)));
        qInfo() << "zweite Kette, Anhebung bei 2 kHz:" << g2 << "dB";
        QVERIFY2(g2 > 6.0,
                 qPrintable(QStringLiteral("der Mithoerer bekommt die "
                                           "Kurve nicht: nur %1 dB")
                                .arg(g2)));
    }

    // Und derselbe Weg OHNE die Einstellungsdatei.
    //
    // „waere besser, dass sobald ich eine aenderung vorgenommen habe,
    // die auch sofort zu hoeren ist." Der Weg ueber die gespeicherten
    // Werte kann das nicht: gespeichert wird erst beim Loslassen der
    // Maus. copyParameters() geht direkt von Kette zu Kette, und
    // dieser Fall besteht darauf, dass dabei NICHTS in den
    // Einstellungen landet -- sonst waere es derselbe Weg mit einem
    // anderen Namen.
    void copyParametersGoesStraightAcross()
    {
        StripChain a;
        a.prepare(48000.0);
        EqBandLayout::ensureSeeded(a.eq());
        a.setStageEnabled(StripChain::Stage::Eq, true);
        ClientEq::BandParams p = a.eq().band(5);
        p.freqHz = 2000.0f; p.gainDb = 9.0f; p.q = 1.0f;
        p.type = ClientEq::FilterType::Peak; p.enabled = true;
        a.eq().setBand(5, p);

        // Eine Marke in den Einstellungen, die unangetastet bleiben
        // muss.
        AppSettings::instance().setValue(QStringLiteral("EqBand5Gain"),
                                         -99.0);

        StripChain b;
        b.prepare(48000.0);
        StripSettings::copyParameters(a, b);

        const ClientEq::BandParams q = b.eq().band(5);
        QCOMPARE(double(q.gainDb), 9.0);
        QCOMPARE(double(q.freqHz), 2000.0);
        QVERIFY(q.enabled);
        QVERIFY(b.stageEnabled(StripChain::Stage::Eq));

        QCOMPARE(AppSettings::instance()
                     .value(QStringLiteral("EqBand5Gain")).toDouble(),
                 -99.0);
    }
};
QTEST_MAIN(TstEqReachesTheMonitor)
#include "tst_eq_reaches_the_monitor.moc"
