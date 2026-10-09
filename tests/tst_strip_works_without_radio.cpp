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
