// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_fft_window_after_gap.cpp  (Longpath)
// =================================================================
//
// Der Panadapter schiebt seine Abtastwerte in ein gleitendes Fenster und
// rechnet erst, wenn es voll ist. Fehlen mittendrin Pakete — WLAN,
// ausgelastete Gegenstelle, ein HL2 an einer langsamen Leitung —, steht
// im Fenster ein Sprung. Ein Sprung ist breitbandig: er malt einen
// Schmierer ueber die ganze Bildbreite, der aussieht wie ein Signal.
//
// Longpath zaehlte solche Loecher bisher nur (iqPacketLoss, Anzeige in
// der Titelleiste). Seit dem 2026-09-22 melden beide Protokolle sie
// ausserdem als iqSequenceGap, und jeder Panadapter wirft daraufhin sein
// angefangenes Fenster weg.
//
// Hier steht, was das Fenster selbst tut: die Fahne wird auf dem Faden
// abgeholt, dem das Fenster gehoert, der Zaehler laeuft mit, und ein
// halbvolles Fenster liefert nach dem Verwerfen kein Bild mehr, bis
// wieder genug FRISCHE Werte da sind.
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/FFTEngine.h"

using namespace Longpath;

namespace {

// Ein Block Rauschen, verschachtelt I/Q, wie ihn die Verbindung liefert.
QVector<float> block(int pairs, float value)
{
    QVector<float> v;
    v.reserve(pairs * 2);
    for (int i = 0; i < pairs; ++i) { v << value << value; }
    return v;
}

} // namespace

class TstFftWindowAfterGap : public QObject { Q_OBJECT
private slots:

    void aFullWindowGivesAFrame()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);
        // Gerechnet wird, wenn nach dem vollen Fenster der naechste Wert
        // ankommt — darum ein paar Werte mehr als die Fenstergroesse.
        engine.feedIQ(block(1024 + 16, 0.1f));
        QVERIFY2(frames.count() >= 1, "Ein volles Fenster hat kein Bild ergeben");
    }

    void theDiscardedWindowDoesNotProduceAFrame()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        // Halbes Fenster — noch kein Bild.
        engine.feedIQ(block(512, 0.1f));
        QCOMPARE(frames.count(), 0);
        QCOMPARE(engine.windowResets(), quint64(0));

        // Loch im Strom.
        engine.requestWindowReset();

        // Die zweite Haelfte allein darf jetzt KEIN Bild mehr ergeben:
        // das angefangene Fenster ist verworfen, es fehlen wieder 512.
        engine.feedIQ(block(512, 0.1f));
        QCOMPARE(frames.count(), 0);
        QCOMPARE(engine.windowResets(), quint64(1));

        // Erst mit dem Rest frischer Werte kommt wieder ein Bild.
        engine.feedIQ(block(512 + 16, 0.1f));
        QVERIFY2(frames.count() >= 1,
                 "Nach dem Verwerfen kam kein Bild mehr, obwohl genug frische Werte da waren");
    }

    void theFlagIsPickedUpOnTheFeedingThreadOnly()
    {
        // requestWindowReset() darf von jedem Faden kommen; wirksam wird
        // es erst im naechsten feedIQ(). Der Zaehler bleibt so lange 0.
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.requestWindowReset();
        QCOMPARE(engine.windowResets(), quint64(0));
        engine.feedIQ(block(8, 0.0f));
        QCOMPARE(engine.windowResets(), quint64(1));
        // Zweimal hintereinander gesetzt zaehlt einmal — die Fahne ist
        // ein Zustand, kein Ereigniszaehler.
        engine.requestWindowReset();
        engine.requestWindowReset();
        engine.feedIQ(block(8, 0.0f));
        QCOMPARE(engine.windowResets(), quint64(2));
    }
};

QTEST_MAIN(TstFftWindowAfterGap)
#include "tst_fft_window_after_gap.moc"
