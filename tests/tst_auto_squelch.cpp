// =================================================================
// tests/tst_auto_squelch.cpp  (NereusSDR)
// =================================================================
//
// Die Squelch-Automatik: Schwelle = Rauschboden + Abstand.
//
// Port aus AetherSDR (SpectrumWidget.cpp:4508-4587 [@0cd4559]); der
// offene Rest von Merkmal 1 der Merkmalsliste vom 2026-08-19, nachdem
// die Linie selbst am Vortag lief.
//
// Geprueft wird der ZUSTAND und das Signal, nicht das gemalte Bild —
// dieselbe Entscheidung wie bei tst_squelch_line und tst_tune_guide.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

// no-port-check: NereusSDR-original test file.

#include <QtTest>
#include <QSignalSpy>

#include "gui/SpectrumWidget.h"

using namespace NereusSDR;

class TestAutoSquelch : public QObject
{
    Q_OBJECT

private slots:

    void offWithTenDbMarginByDefault()
    {
        SpectrumWidget w;
        QVERIFY2(!w.autoSquelchEnabled(),
                 "eine Automatik, die eine Schwelle schreibt, laeuft nicht ungefragt");
        // 10 dB wie AetherSDR m_autoSqlMarginDb{10}.
        QCOMPARE(w.autoSqlMarginDb(), 10);
    }

    // Ausgeschaltet darf kein Vorschlag herauskommen, egal welcher
    // Rauschboden hereinkommt.
    void silentWhileOff()
    {
        SpectrumWidget w;
        QSignalSpy spy(&w, &SpectrumWidget::autoSquelchThresholdSuggested);
        w.testApplyAutoSquelch(-120.0f);
        QCOMPARE(spy.count(), 0);
    }

    // Der eigentliche Rechenweg: Boden plus Abstand, in dBm. Keine
    // 0..160-Stufe wie bei AetherSDR — unser SliceModel haelt dBm, und
    // die Umrechnung waere eine Fehlerquelle ohne Nutzen.
    void thresholdIsFloorPlusMargin()
    {
        SpectrumWidget w;
        w.setAutoSquelchEnabled(true);
        w.setAutoSqlMarginDb(8);
        QSignalSpy spy(&w, &SpectrumWidget::autoSquelchThresholdSuggested);
        w.testApplyAutoSquelch(-123.0f);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), -115.0);
        QCOMPARE(w.lastAutoSqlThresholdDbm(), -115.0);
    }

    // Der Abstand ist auf 5..20 dB begrenzt, wie bei AetherSDR
    // (std::clamp(dBm, 5, 20)).
    void marginIsClamped()
    {
        SpectrumWidget w;
        w.setAutoSqlMarginDb(1);
        QCOMPARE(w.autoSqlMarginDb(), 5);
        w.setAutoSqlMarginDb(99);
        QCOMPARE(w.autoSqlMarginDb(), 20);
    }

    // Unter 1 dB Bewegung wird nicht gesendet. Ohne die Sperre laeuft bei
    // JEDEM FFT-Rahmen ein Modell-Setter samt Neuzeichnen der Linie —
    // zehn- bis dreissigmal je Sekunde.
    void tinyDriftIsNotBroadcast()
    {
        SpectrumWidget w;
        w.setAutoSquelchEnabled(true);
        QSignalSpy spy(&w, &SpectrumWidget::autoSquelchThresholdSuggested);
        w.testApplyAutoSquelch(-120.0f);
        QCOMPARE(spy.count(), 1);
        w.testApplyAutoSquelch(-120.4f);   // 0,4 dB — zu wenig
        QCOMPARE(spy.count(), 1);
        w.testApplyAutoSquelch(-118.0f);   // 2 dB — genug
        QCOMPARE(spy.count(), 2);
    }

    // Ein geaenderter Abstand muss neu senden, auch wenn der Boden steht:
    // die Schwelle hat sich bewegt, nicht der Boden.
    void changingTheMarginReEmits()
    {
        SpectrumWidget w;
        w.setAutoSquelchEnabled(true);
        w.testApplyAutoSquelch(-120.0f);
        QSignalSpy spy(&w, &SpectrumWidget::autoSquelchThresholdSuggested);
        w.setAutoSqlMarginDb(15);
        w.testApplyAutoSquelch(-120.0f);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toDouble(), -105.0);
    }

    // Die Vorbelegung von m_nfLerpAverage ist -200 dBm. Solange
    // processNoiseFloor keinen Rahmen gesehen hat, steht dort kein
    // gemessener Boden — daraus darf keine Schwelle werden.
    void theUnmeasuredFloorIsIgnored()
    {
        SpectrumWidget w;
        w.setAutoSquelchEnabled(true);
        QSignalSpy spy(&w, &SpectrumWidget::autoSquelchThresholdSuggested);
        w.testApplyAutoSquelch(-200.0f);
        QCOMPARE(spy.count(), 0);
    }

    // Beim Senden pausiert sie: das eigene Signal hebt den gemessenen
    // Boden, die Automatik zoege die Schwelle hoch, und nach dem
    // Loslassen waere alles zu.
    void transmitPauses()
    {
        SpectrumWidget w;
        w.setAutoSquelchEnabled(true);
        w.setMoxOverlay(true);
        QSignalSpy spy(&w, &SpectrumWidget::autoSquelchThresholdSuggested);
        w.testApplyAutoSquelch(-90.0f);
        QCOMPARE(spy.count(), 0);
        w.setMoxOverlay(false);
        w.testApplyAutoSquelch(-90.0f);
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestAutoSquelch)
#include "tst_auto_squelch.moc"
