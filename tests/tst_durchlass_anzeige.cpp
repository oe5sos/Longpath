// no-port-check: Longpath-original test, no Thetis logic.
//
// ── Durchlass und Bandbreite am Widget (2026-11-08) ──────────────────────
//
// Betreiber an den Entwurfsblaettern: „Alle vier bauen". tst_durchlass
// prueft den Messkern ohne Fenster; dieser hier die halbe Strecke, die
// der Betreiber nimmt (memory: Pruefstaende fahren die Bedienung):
//
//   * ab Werk alle vier an, je einzeln abschaltbar und gespeichert
//   * die Klammern messen, was der Panadapter ANZEIGT
//   * ein Klick (echtes QMouseEvent) auf eine Luecke stimmt dorthin
//   * die Kante rastet beim Ziehen auf 50 Hz, mit Shift nicht
//   * die Lupe erscheint nur, wenn der Durchlass schmal ist
//   * die vier Haken stehen im Zahnrad und wirken
//
// QTest::mouseClick verpufft ohne echten Schirm -- darum sendEvent.

#include <QtTest/QtTest>
#include <QApplication>
#include <QMenu>
#include <QMouseEvent>
#include <QSignalSpy>

#include <cmath>

#include "core/AppSettings.h"
#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/spectrum/Durchlass.h"

using namespace Longpath;

namespace {

constexpr int    kBins  = 4096;
constexpr double kMitte = 14100000.0;

double lin(double dbm) { return std::pow(10.0, dbm / 10.0); }

// Rauschen bei -135 dBm (festes Muster, kein Zufall), dazu SSB-Stationen
// (300-2700 Hz ueber dem Traeger) und Traeger.
QVector<float> szene(double rate, double mitte,
                     const QVector<QPair<double, double>>& ssb,
                     const QVector<QPair<double, double>>& traeger = {})
{
    QVector<float> b(kBins);
    for (int i = 0; i < kBins; ++i) {
        b[i] = static_cast<float>(lin(-135.0 + ((i * 7) % 5) * 0.4 - 0.8));
    }
    const double hzJeBin = rate / kBins;
    auto bin = [&](double hz) {
        return static_cast<int>(std::lround((hz - mitte) / hzJeBin)) + kBins / 2;
    };
    for (const auto& st : ssb) {
        for (int k = bin(st.first + 300.0); k < bin(st.first + 2700.0); ++k) {
            if (k >= 0 && k < kBins) { b[k] += static_cast<float>(lin(st.second)); }
        }
    }
    for (const auto& t : traeger) {
        const int k = bin(t.first);
        if (k >= 0 && k < kBins) { b[k] += static_cast<float>(lin(t.second)); }
    }
    return b;
}

void einstellen(SpectrumWidget& w, double rate)
{
    w.resize(1000, 520);
    w.setSampleRate(rate);
    w.setDdcCenterFrequency(kMitte);
    w.setVfoFrequency(14111500.0);
    w.setFrequencyRange(kMitte, rate);
    w.setFilterOffset(150, 2850);
    w.setDbmRange(-150.0f, -60.0f);
}

void fuettern(SpectrumWidget& w, const QVector<float>& b, int mal = 40)
{
    for (int i = 0; i < mal; ++i) { w.updateSpectrumLinear(0, b, 1.0, 0.0); }
}

void maus(QWidget& w, QEvent::Type typ, const QPointF& pos,
          Qt::KeyboardModifiers mod = Qt::NoModifier)
{
    const Qt::MouseButtons knoepfe =
        (typ == QEvent::MouseButtonRelease) ? Qt::NoButton : Qt::LeftButton;
    QMouseEvent e(typ, pos, w.mapToGlobal(pos), Qt::LeftButton, knoepfe, mod);
    QApplication::sendEvent(&w, &e);
}

double xVon(const SpectrumWidget& w, double hz)
{
    const QRect r = w.spectrumRect();
    const double links = w.centerFrequency() - w.bandwidth() / 2.0;
    return r.left() + (hz - links) / w.bandwidth() * r.width();
}

} // namespace

class TstDurchlassAnzeige : public QObject {
    Q_OBJECT
private slots:
    void init()    { AppSettings::instance().clear(); }
    void cleanup() { AppSettings::instance().clear(); }

    void abWerkAlleVierAnUndEinzelnGespeichert()
    {
        {
            SpectrumWidget w;
            w.loadSettings();
            QVERIFY(w.passbandLabelShown());
            QVERIFY(w.signalBracketsShown());
            QVERIFY(w.gapFinderShown());
            QCOMPARE(w.passbandMagnifier(), 1);
            w.setSignalBracketsShown(false);
            w.setPassbandMagnifier(0);
            w.saveSettings();
        }
        SpectrumWidget w2;
        w2.loadSettings();
        QVERIFY(!w2.signalBracketsShown());
        QCOMPARE(w2.passbandMagnifier(), 0);
        QVERIFY(w2.passbandLabelShown());
        QVERIFY(w2.gapFinderShown());
    }

    // Klammern auf dem, was angezeigt wird: SSB 2,4 kHz breit, ~35 dB ueber
    // dem Rauschen; ein Traeger schmal.
    void dieKlammernMessenDieAnzeige()
    {
        SpectrumWidget w;
        einstellen(w, 48000.0);
        fuettern(w, szene(48000.0, kMitte, {{14110000.0, -100.0}}, {{14098500.0, -80.0}}));
        // Das Fuettern hat schon einmal ausgewertet (updateSpectrumLinear,
        // 200-ms-Takt); die Haltung -- erst beim zweiten Mal zeigen -- ist
        // in tst_durchlass geprueft. Hier zweimal, damit es sicher steht.
        w.werteDurchlassAusForTest();
        w.werteDurchlassAusForTest();
        const auto s = w.signalBracketsForTest();
        QCOMPARE(s.size(), 2);
        const double hzJePx = w.bandwidth() / (w.renderedPixels().size() - 1);
        const auto& traeger = s[0];
        const auto& ssb     = s[1];
        QVERIFY2(traeger.breiteHz < 400.0, qPrintable(QString::number(traeger.breiteHz)));
        QVERIFY2(std::abs(ssb.breiteHz - 2400.0) <= 3 * hzJePx,
                 qPrintable(QString::number(ssb.breiteHz)));
        QVERIFY2(std::abs(ssb.snrDb - 35.0f) <= 3.0f, qPrintable(QString::number(ssb.snrDb)));
    }

    // Die Auswertung laeuft im DATENWEG, ohne Pruefzugang: nur Spektren
    // einspielen, dazwischen die 200-ms-Bremse abwarten. Am 2026-11-08 lag
    // der Aufruf im GPU-Zweig -- der CPU-Bau haette Klammern, Luecken und
    // Lupe nie nachgefuehrt, und nur dieser Fall sieht das.
    void dieAuswertungLaeuftImDatenweg()
    {
        SpectrumWidget w;
        einstellen(w, 48000.0);
        const auto b = szene(48000.0, kMitte, {{14110000.0, -100.0}});
        for (int runde = 0; runde < 3; ++runde) {
            fuettern(w, b, 5);
            QTest::qWait(230);
        }
        fuettern(w, b, 5);
        QVERIFY2(!w.signalBracketsForTest().isEmpty(),
                 "ohne Pruefzugang keine Klammern -- die Auswertung haengt nicht im Datenweg");
        QVERIFY(!w.gapsForTest().isEmpty());
    }

    // Luecken und der Klick darauf -- ein echtes Mausereignis, kein Aufruf.
    void einKlickAufDieLueckeStimmtHin()
    {
        SpectrumWidget w;
        einstellen(w, 48000.0);
        fuettern(w, szene(48000.0, kMitte,
                          {{14085000.0, -100.0}, {14100000.0, -105.0}, {14112000.0, -95.0}}));
        w.werteDurchlassAusForTest();
        const auto luecken = w.gapsForTest();
        QVERIFY2(!luecken.isEmpty(), "zwischen drei Stationen muss Platz sein");
        QVERIFY(w.bestGapForTest() >= 0);

        w.paintPassbandOverlaysForTest();
        const QVector<QRectF> flaechen = w.gapClickRectsForTest();
        QVERIFY(!flaechen.isEmpty());

        QSignalSpy klick(&w, &SpectrumWidget::frequencyClicked);
        // Am LINKEN Rand des Balkens klicken: der gewoehnliche Klick stimmte
        // auf den Punkt unter der Maus, der Lueckenfinder auf die MITTE der
        // Luecke -- nur so unterscheidet der Fall die beiden.
        const QPointF ziel(flaechen.first().left() + 2.0, flaechen.first().center().y());
        maus(w, QEvent::MouseButtonPress, ziel);
        maus(w, QEvent::MouseButtonRelease, ziel);
        QCOMPARE(klick.size(), 1);

        // Die Mitte der getroffenen Luecke, nicht der Punkt unter der Maus.
        const auto& l = luecken.first();
        const double hzJePx = w.bandwidth() / (w.renderedPixels().size() - 1);
        const double erwartet = w.centerFrequency() - w.bandwidth() / 2.0
                              + 0.5 * (l.a + l.e) * hzJePx;
        QVERIFY2(std::abs(klick.first().first().toDouble() - erwartet) <= 300.0,
                 qPrintable(QStringLiteral("%1 statt %2")
                                .arg(klick.first().first().toDouble()).arg(erwartet)));
    }

    // Die obere Kante ziehen: ohne Shift auf 50 Hz gerastet, mit Shift fein.
    void dieKanteRastetBeimZiehen()
    {
        for (const bool fein : {false, true}) {
            SpectrumWidget w;
            einstellen(w, 48000.0);
            const Qt::KeyboardModifiers mod = fein ? Qt::ShiftModifier : Qt::NoModifier;
            QSignalSpy kante(&w, &SpectrumWidget::filterEdgeDragged);
            const double x = xVon(w, 14111500.0 + 2850.0);
            const double y = w.spectrumRect().top() + 0.5 * w.spectrumRect().height();
            maus(w, QEvent::MouseButtonPress, QPointF(x, y), mod);
            maus(w, QEvent::MouseMove, QPointF(x + 7.0, y), mod);
            maus(w, QEvent::MouseButtonRelease, QPointF(x + 7.0, y), mod);
            QVERIFY2(!kante.isEmpty(), "die Kante wurde nicht gegriffen");
            const int hoch = kante.last().at(1).toInt();
            const int roh = 2850 + static_cast<int>(std::round(
                7.0 * w.bandwidth() / w.spectrumRect().width()));
            if (fein) {
                QCOMPARE(hoch, roh);
            } else {
                QCOMPARE(hoch, Durchlass::raste(roh));
                QCOMPARE(hoch % Durchlass::kKantenRastHz, 0);
            }
        }
    }

    void dieLupeNurBeiSchmalemDurchlass()
    {
        SpectrumWidget w;
        einstellen(w, 48000.0);
        fuettern(w, szene(48000.0, kMitte, {{14110000.0, -100.0}}), 3);
        QVERIFY2(!w.magnifierVisibleForTest(), "bei 48 kHz ist der Durchlass breit genug");

        einstellen(w, 768000.0);
        fuettern(w, szene(768000.0, kMitte, {{14110000.0, -100.0}}), 3);
        QVERIFY2(w.magnifierVisibleForTest(), "bei 768 kHz sind 2,7 kHz wenige Punkte");
        w.paintPassbandOverlaysForTest();
        const int erwartet = static_cast<int>(2.0 * Durchlass::kLupeHalbHz / (768000.0 / kBins));
        QVERIFY2(std::abs(w.magnifierBinsForTest().size() - erwartet) <= 2,
                 qPrintable(QString::number(w.magnifierBinsForTest().size())));

        w.setPassbandMagnifier(0);
        QVERIFY(!w.magnifierVisibleForTest());
        w.setPassbandMagnifier(2);
        einstellen(w, 48000.0);
        QVERIFY2(w.magnifierVisibleForTest(), "\"immer\" heisst immer");
    }

    void dieVierHakenImZahnrad()
    {
        PanadapterApplet applet(QStringLiteral("A"));
        applet.resize(800, 400);
        SpectrumWidget* w = applet.spectrumWidget();
        QVERIFY(w);
        QMenu* m = applet.buildDisplayMenuForTesting();
        QVERIFY(m);
        QMap<QString, QAction*> haken;
        for (QAction* a : m->actions()) { haken.insert(a->text(), a); }
        for (const QString t : {QStringLiteral("Bandwidth label"),
                                QStringLiteral("Signal brackets (width, SNR)"),
                                QStringLiteral("Gap finder (CQ)"),
                                QStringLiteral("Passband magnifier")}) {
            QVERIFY2(haken.contains(t), qPrintable(t));
            QVERIFY(haken[t]->isCheckable() && haken[t]->isEnabled());
            QVERIFY2(haken[t]->isChecked(), qPrintable(t + QStringLiteral(" ab Werk an")));
        }
        haken[QStringLiteral("Bandwidth label")]->trigger();
        QVERIFY(!w->passbandLabelShown());
        haken[QStringLiteral("Signal brackets (width, SNR)")]->trigger();
        QVERIFY(!w->signalBracketsShown());
        haken[QStringLiteral("Gap finder (CQ)")]->trigger();
        QVERIFY(!w->gapFinderShown());
        haken[QStringLiteral("Passband magnifier")]->trigger();
        QCOMPARE(w->passbandMagnifier(), 0);
        delete m;
    }
};

QTEST_MAIN(TstDurchlassAnzeige)
#include "tst_durchlass_anzeige.moc"
