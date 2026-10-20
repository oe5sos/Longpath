// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_strip_works_without_radio.cpp  (Longpath)
// =================================================================
// Eine Stimme stellt man am Schreibtisch ein, nicht auf Sendung.
//
// Der Kanalzug entstand mit der Sendepumpe und starb mit ihr. Ohne
// Funkgeraet war deshalb das ganze Kanalzugfenster tot: der
// Hauptschalter schaltete nichts, die acht Stufenseiten hatten keine
// Werte, die Uebersichtskarten zeigten acht Striche, und "Through
// your strip" im Stimmcheck lief durch gar nichts. Der Betreiber am
// 2026-10-09, mit dem Fenster offen und dem Geraet aus: „kann diese
// nicht aktivieren."
//
// Die Gegenprobe gegen die ALTE Fassung (m_stripChain erst beim
// Pumpenstart) laesst beide Faelle hier fallen -- stripChain() war
// dort null und setEnabled() hatte nichts, worauf es haette wirken
// koennen.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include <cmath>
#include <vector>

#include "core/strip/StripChain.h"
#include "core/strip/StripLiveMonitor.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstStripWorksWithoutRadio : public QObject
{
    Q_OBJECT
private slots:

    // Kein Funkgeraet, trotzdem ein Kanalzug.
    void theChainExistsBeforeAnyConnection()
    {
        RadioModel model;
        QVERIFY(!model.isConnected());
        QVERIFY2(model.stripChain() != nullptr,
                 "ohne Verbindung kein Kanalzug -- dann ist das ganze "
                 "Fenster tot");
    }

    // Und er laesst sich einschalten und einstellen.
    void everyStageCanBeSetWithTheRadioOff()
    {
        RadioModel model;
        StripChain* c = model.stripChain();
        QVERIFY(c != nullptr);

        c->setEnabled(true);
        QVERIFY(c->isEnabled());

        for (int i = 0; i < StripChain::kStageCount; ++i) {
            const auto s = static_cast<StripChain::Stage>(i);
            c->setStageEnabled(s, true);
            QVERIFY2(c->stageEnabled(s),
                     StripChain::stageName(s));
            c->setStageEnabled(s, false);
            QVERIFY(!c->stageEnabled(s));
        }
    }

    // Und er rechnet -- eingeschaltete Stufen aendern das Signal, ohne
    // dass je eine Sendepumpe lief. Das ist der Unterschied zwischen
    // "da ist ein Objekt" und "man kann damit arbeiten".
    void theChainActuallyProcessesWithTheRadioOff()
    {
        RadioModel model;
        StripChain* c = model.stripChain();
        QVERIFY(c != nullptr);
        c->prepare(48000.0);
        c->setEnabled(true);
        c->setStageEnabled(StripChain::Stage::Limiter, true);
        c->limiter().setCeilingDb(-20.0f);

        // Ein lauter Ton, deutlich ueber der Schwelle.
        std::vector<float> x(4800);
        for (size_t i = 0; i < x.size(); ++i) {
            x[i] = 0.9f * float(std::sin(2.0 * M_PI * 700.0
                                         * double(i) / 48000.0));
        }
        const std::vector<float> before = x;
        for (size_t at = 0; at + 64 <= x.size(); at += 64) {
            c->processMono(x.data() + at, 64);
        }

        double peakBefore = 0.0, peakAfter = 0.0;
        for (size_t i = 0; i < x.size(); ++i) {
            peakBefore = std::max(peakBefore, std::abs(double(before[i])));
            peakAfter  = std::max(peakAfter,  std::abs(double(x[i])));
        }
        QVERIFY2(peakAfter < peakBefore * 0.9,
                 qPrintable(QStringLiteral("Begrenzer ohne Wirkung: "
                                           "%1 -> %2")
                     .arg(peakBefore).arg(peakAfter)));
    }

    // Der Mithoerer bringt seinen eigenen Kanalzug mit und braucht
    // ebenfalls kein Geraet. Gestartet wird hier nichts -- im CI gibt
    // es weder Mikrofon noch Lautsprecher, und ein Pruefstand, der
    // Geraete verlangt, prueft die Maschine statt den Code.
    // Der Hauptschalter muss beim Mithoeren wirklich ankommen.
    //
    // Er kam lange nicht an: setStripBypassed() existierte und wurde
    // von niemandem gerufen, also lief das Mithoeren durch die ganze
    // Kette, obwohl der Schalter aus stand. Am Geraet hoerte sich das
    // an wie „komische modulation [...] wie ueberschlagen" -- ein Gate
    // auf dem eingebauten Mikrofon eines MacBook.
    void theMasterSwitchReachesTheMonitor()
    {
        StripLiveMonitor m;
        StripChain* c = m.chain();
        QVERIFY(c != nullptr);
        c->prepare(48000.0);
        c->setEnabled(true);
        for (int i = 0; i < StripChain::kStageCount; ++i) {
            c->setStageEnabled(static_cast<StripChain::Stage>(i), false);
        }
        c->setStageEnabled(StripChain::Stage::Limiter, true);
        c->limiter().setCeilingDb(-20.0f);

        const auto peakOf = [&](bool bypassed) {
            m.setStripBypassed(bypassed);
            std::vector<float> x(4800);
            for (size_t i = 0; i < x.size(); ++i) {
                x[i] = float(0.9 * std::sin(2.0 * M_PI * 700.0
                                            * double(i) / 48000.0));
            }
            m.applyStrip(x.data(), int(x.size()));
            double p = 0.0;
            for (size_t i = x.size() / 2; i < x.size(); ++i) {
                p = std::max(p, std::abs(double(x[i])));
            }
            return p;
        };

        const double raw = peakOf(true);
        const double through = peakOf(false);
        QVERIFY2(std::abs(raw - 0.9) < 0.01,
                 qPrintable(QStringLiteral("ueberbrueckt wurde doch "
                                           "gerechnet: %1").arg(raw)));
        QVERIFY2(through < raw * 0.5,
                 qPrintable(QStringLiteral("eingeschaltet aendert sich "
                                           "nichts: %1 gegen %2")
                                .arg(through).arg(raw)));
    }

    // Die eigene Kette des Mithoerers folgt dem Hauptschalter -- auch
    // in ihrem Schalterzustand, nicht nur in der Rechnung.
    //
    // Ohne Funkgeraet lesen die Kachelreihe und die Pegelbalken
    // DIESE Kette. Eine Kette, die als „eingeschaltet" dasteht,
    // waehrend sie ueberbrueckt wird, ist eine Falschaussage an
    // genau der Stelle, an der man beim Suchen hinsieht.
    void theMonitorChainShowsWhetherItIsRunning()
    {
        StripLiveMonitor m;
        QVERIFY(m.chain() != nullptr);

        m.setStripBypassed(false);
        QVERIFY2(m.chain()->isEnabled(),
                 "durchgeschaltet, aber die Kette meldet sich als aus");

        m.setStripBypassed(true);
        QVERIFY2(!m.chain()->isEnabled(),
                 "ueberbrueckt, aber die Kette meldet sich als an");
    }

    void theLiveMonitorBringsItsOwnChain()
    {
        StripLiveMonitor m;
        QVERIFY(m.chain() != nullptr);
        QVERIFY(!m.isRunning());
        // Der eigene Kanalzug ist NICHT der der Sendekette: zwei
        // Audiofaeden in denselben Filterzustaenden waeren ein
        // Datenrennen.
        RadioModel model;
        QVERIFY(m.chain() != model.stripChain());
    }
};

QTEST_MAIN(TstStripWorksWithoutRadio)
#include "tst_strip_works_without_radio.moc"
