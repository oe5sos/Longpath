// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf, kein Port.
//
// ── Die Anzeigeregler wirkten nur auf Pan 0 ─────────────────────────────
//
// Gefunden am 2026-10-22 als Fortsetzung von Befund 4 aus NereusSDRs
// PureSignal-Diagnose. Dort hing das Zweiton-Tor einmalig an
// activeSpectrumWidget(); beim Nachsehen, was sonst noch so gebunden
// ist, fiel die Knopfleiste des Panadapters auf.
//
// Jeder Pan bekommt seine eigene SpectrumOverlayPanel
// (MainWindow.cpp: `new SpectrumOverlayPanel(sw)` je Pan). Verdrahtet
// war aber nur EINE davon -- m_overlayPanel, also die von Pan 0, und
// zwar gegen activeSpectrumWidget(), also ebenfalls Pan 0. Jedes dieser
// sieben Signale kam im ganzen Programm genau einmal vor:
//
//   wfColorGainChanged        Wasserfall-Farbverstaerkung
//   wfBlackLevelChanged       Wasserfall-Schwarzpunkt
//   colorSchemeChanged        Farbtafel
//   spectrumRenderModeChanged 2D/3D
//   cursorFreqVisibleChanged  Frequenz am Zeiger
//   fillColorChanged          Fuellfarbe
//   fillAlphaChanged          Deckkraft
//
// Folge am Geraet: wer im Zweipan-Bild den Regler im FLYOUT des zweiten
// Pans zieht, bewegt nichts. Nicht den zweiten Pan, und auch nicht den
// ersten -- das Signal lief ins Leere, weil die Leiste des zweiten Pans
// an keinem Empfaenger hing.
//
// Gegenprobe (2026-10-22, gegen 332256009): jeder der sieben Faelle
// faellt mit dem unveraenderten Vorgabewert um -- der Regler bewegte
// nichts.

#include <QtTest>

#include "gui/MainWindow.h"
#include "gui/PanadapterStack.h"
#include "gui/SpectrumOverlayPanel.h"
#include "gui/SpectrumWidget.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstAnzeigereglerAufJedemPan : public QObject
{
    Q_OBJECT

private:
    MainWindow*          m_mw{nullptr};
    SpectrumWidget*      m_pan1{nullptr};
    SpectrumOverlayPanel* m_leiste1{nullptr};

private slots:

    void initTestCase()
    {
        m_mw = new MainWindow();
        // Echte Breite: bei 800x800 schneidet der Schirm Spalten ab und
        // die Knopfleiste bekommt moeglicherweise keinen Platz.
        m_mw->resize(1700, 1000);
        m_mw->show();
        QVERIFY(QTest::qWaitForWindowExposed(m_mw, kFensterFrist));

        auto* stapel = m_mw->findChild<PanadapterStack*>();
        QVERIFY2(stapel != nullptr, "kein Panadapter-Stapel am Fenster");
        stapel->applyLayout(QStringLiteral("2h"),
                            {QStringLiteral("pan-0"), QStringLiteral("pan-1")});
        QTest::qWait(50);

        m_pan1 = stapel->spectrum(QStringLiteral("pan-1"));
        QVERIFY2(m_pan1 != nullptr, "pan-1 hat kein Spektrum");

        // Die Leiste haengt als Kind am Spektrum des eigenen Pans.
        m_leiste1 = m_pan1->findChild<SpectrumOverlayPanel*>();
        QVERIFY2(m_leiste1 != nullptr, "pan-1 hat keine Knopfleiste");
    }

    void cleanupTestCase()
    {
        delete m_mw;
        m_mw = nullptr;
    }

    // ── Wasserfall ──────────────────────────────────────────────────────

    void farbverstaerkungWirktAufPan1()
    {
        const int vorher = m_pan1->wfColorGain();
        const int anders = (vorher >= 50) ? 20 : 80;
        emit m_leiste1->wfColorGainChanged(anders);
        QTest::qWait(10);
        QCOMPARE(m_pan1->wfColorGain(), anders);
    }

    void schwarzpunktWirktAufPan1()
    {
        const int vorher = m_pan1->wfBlackLevel();
        const int anders = (vorher >= 60) ? 15 : 110;
        emit m_leiste1->wfBlackLevelChanged(anders);
        QTest::qWait(10);
        QCOMPARE(m_pan1->wfBlackLevel(), anders);
    }

    void farbtafelWirktAufPan1()
    {
        const WfColorScheme vorher = m_pan1->wfColorScheme();
        // Irgendeine ANDERE Tafel aus der Liste.
        const int anzahl = static_cast<int>(WfColorScheme::Count);
        QVERIFY2(anzahl >= 2, "nur eine Farbtafel: nichts zu pruefen");
        const int andersIdx =
            (static_cast<int>(vorher) == 0) ? 1 : 0;
        emit m_leiste1->colorSchemeChanged(andersIdx);
        QTest::qWait(10);
        QCOMPARE(static_cast<int>(m_pan1->wfColorScheme()), andersIdx);
    }

    // Die Begrenzung muss auch auf dem zweiten Pan greifen -- der
    // Lambda-Umweg mit qBound war bisher nur fuer Pan 0 gebaut.
    void farbtafelAusserhalbDerListeWirdBegrenzt()
    {
        const int anzahl = static_cast<int>(WfColorScheme::Count);
        emit m_leiste1->colorSchemeChanged(999);
        QTest::qWait(10);
        QCOMPARE(static_cast<int>(m_pan1->wfColorScheme()), anzahl - 1);
        emit m_leiste1->colorSchemeChanged(-7);
        QTest::qWait(10);
        QCOMPARE(static_cast<int>(m_pan1->wfColorScheme()), 0);
    }

    // ── Spektrum ────────────────────────────────────────────────────────

    void zeichenmodusWirktAufPan1()
    {
        emit m_leiste1->spectrumRenderModeChanged(1);
        QTest::qWait(10);
        QCOMPARE(m_pan1->spectrumRenderMode(), SpectrumRenderMode::Mode3D);
        emit m_leiste1->spectrumRenderModeChanged(0);
        QTest::qWait(10);
        QCOMPARE(m_pan1->spectrumRenderMode(), SpectrumRenderMode::Mode2D);
    }

    void frequenzAmZeigerWirktAufPan1()
    {
        const bool vorher = m_pan1->cursorFreqVisible();
        emit m_leiste1->cursorFreqVisibleChanged(!vorher);
        QTest::qWait(10);
        QCOMPARE(m_pan1->cursorFreqVisible(), !vorher);
    }

    void fuellfarbeWirktAufPan1()
    {
        const QColor probe(17, 99, 173);
        QVERIFY2(m_pan1->fillColor() != probe,
                 "Probefarbe gleicht der Vorgabe: Fall waere blind");
        emit m_leiste1->fillColorChanged(probe);
        QTest::qWait(10);
        QCOMPARE(m_pan1->fillColor(), probe);
    }

    void deckkraftWirktAufPan1()
    {
        const float vorher = m_pan1->fillAlpha();
        const float anders = (vorher > 0.5f) ? 0.2f : 0.8f;
        emit m_leiste1->fillAlphaChanged(anders);
        QTest::qWait(10);
        QVERIFY2(qFuzzyCompare(m_pan1->fillAlpha(), anders),
                 qPrintable(QStringLiteral("Deckkraft %1 statt %2")
                                .arg(m_pan1->fillAlpha()).arg(anders)));
    }

    // ── Pan 0 muss es weiter koennen ────────────────────────────────────
    //
    // Der alte Einmal-Block wurde entfernt, damit Pan 0 nicht doppelt
    // haengt. Also hier nachgehalten, dass er ueber den neuen Weg noch
    // bedient wird -- sonst waere die Behebung ein Tausch von „Pan 1 tot"
    // gegen „Pan 0 tot".
    void pan0WirdWeiterBedient()
    {
        auto* stapel = m_mw->findChild<PanadapterStack*>();
        QVERIFY(stapel != nullptr);
        SpectrumWidget* pan0 = stapel->spectrum(QStringLiteral("pan-0"));
        QVERIFY(pan0 != nullptr);
        auto* leiste0 = pan0->findChild<SpectrumOverlayPanel*>();
        QVERIFY2(leiste0 != nullptr, "pan-0 hat keine Knopfleiste");

        const int vorher = pan0->wfColorGain();
        const int anders = (vorher >= 50) ? 23 : 77;
        emit leiste0->wfColorGainChanged(anders);
        QTest::qWait(10);
        QCOMPARE(pan0->wfColorGain(), anders);

        const bool sichtbar = pan0->cursorFreqVisible();
        emit leiste0->cursorFreqVisibleChanged(!sichtbar);
        QTest::qWait(10);
        QCOMPARE(pan0->cursorFreqVisible(), !sichtbar);
    }

    // ── Und Pan 0 darf dabei nicht mitwandern ───────────────────────────
    //
    // Der naheliegende Fehler bei der Behebung waere, alle Pans an
    // dieselbe Leiste zu haengen. Dann zieht ein Regler beide Bilder.
    void pan0BleibtVonPan1Unberuehrt()
    {
        auto* stapel = m_mw->findChild<PanadapterStack*>();
        QVERIFY(stapel != nullptr);
        SpectrumWidget* pan0 = stapel->spectrum(QStringLiteral("pan-0"));
        QVERIFY(pan0 != nullptr);

        const int vorher = pan0->wfColorGain();
        const int anders = (vorher >= 50) ? 11 : 91;
        emit m_leiste1->wfColorGainChanged(anders);
        QTest::qWait(10);
        QCOMPARE(m_pan1->wfColorGain(), anders);
        QCOMPARE(pan0->wfColorGain(), vorher);
    }
};

QTEST_MAIN(TstAnzeigereglerAufJedemPan)
#include "tst_anzeigeregler_auf_jedem_pan.moc"
