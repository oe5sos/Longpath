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

// Eine gestellte Uhr: der Prueflauf stellt sie vor, statt zu schlafen.
// Ohne sie laufen mehrere feedIQ() hintereinander alle in den 5-ms-
// Mindestabstand zwischen zwei Bildern, und dann zaehlen die Paare
// nicht weiter -- der Lauf wuerde etwas anderes messen als er meint.
struct Uhr
{
    qint64 ms{0};
    void weiter(qint64 schritt) { ms += schritt; }
};

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

    // ── Die verlustbehaftete Strecke kriecht nicht ──────────────────
    //
    // Von NereusSDR an unserem Port gefunden (2026-10-24): jede
    // Verwerfung laesst das naechste Bild auf ein GANZES neues Fenster
    // warten. Eine Strecke, die alle paar Fenster ein Paket verliert,
    // verwirft also dauernd und liefert fast keine Bilder mehr — bei
    // 262144 FFT und einem Verlust je 1,56 s waren es rund 4 Bilder/s.
    //
    // Darum: wer innerhalb der letzten kGapLossyLinkWindows Fenster
    // schon eine Luecke hatte, gilt als verlustbehaftet, und seine
    // naechste Luecke geht mit leichtem Schmierer durch.
    void aLossyLinkKeepsItsFrames()
    {
        FFTEngine engine(0);
        Uhr uhr;
        engine.setFrameGapClockForTest([&uhr] { return uhr.ms; });
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        // Erste Luecke auf gesunder Strecke: verwerfen, wie gehabt.
        engine.feedIQ(block(512, 0.1f));
        engine.requestWindowReset();
        engine.feedIQ(block(8, 0.1f));
        QCOMPARE(engine.windowResets(), quint64(1));
        QCOMPARE(engine.windowResetsSkipped(), quint64(0));

        // Fenster voll laufen lassen, damit ein Bild kommt — das
        // beendet das Nachfuellen, erst dann greift die Regel.
        uhr.weiter(20);
        engine.feedIQ(block(1024 + 16, 0.1f));
        QVERIFY2(frames.count() >= 1, "kein Bild nach dem Nachfuellen");

        // Zweite Luecke, nur zwei Fenster nach der ersten: die Strecke
        // ist verlustbehaftet, die Luecke geht DURCH.
        uhr.weiter(20);
        engine.feedIQ(block(1024, 0.1f));
        engine.requestWindowReset();
        engine.feedIQ(block(8, 0.1f));
        QCOMPARE(engine.windowResets(), quint64(1));
        QCOMPARE(engine.windowResetsSkipped(), quint64(1));
    }

    // ── Nach genug sauberen Fenstern verwirft sie wieder ────────────
    //
    // Die Gegenprobe zur Regel darueber: eine gesunde Strecke, die
    // gelegentlich ein Paket verliert, soll ihr Fenster weiterhin
    // wegwerfen. „Gelegentlich" heisst: weiter als
    // kGapLossyLinkWindows Fenster auseinander.
    void aHealthyLinkStillDropsTheWindow()
    {
        FFTEngine engine(0);
        Uhr uhr;
        engine.setFrameGapClockForTest([&uhr] { return uhr.ms; });
        engine.setFftSize(1024);
        engine.setOutputFps(60);

        engine.requestWindowReset();
        engine.feedIQ(block(8, 0.1f));
        QCOMPARE(engine.windowResets(), quint64(1));

        // Fuenf saubere Fenster — mehr als kGapLossyLinkWindows (4).
        // Die Uhr muss zwischen den Bildern vorgehen, sonst schluckt der
        // 5-ms-Mindestabstand sie und die Paare zaehlen nicht weiter.
        for (int i = 0; i < 5; ++i) {
            uhr.weiter(20);
            engine.feedIQ(block(1024 + 16, 0.1f));
        }

        engine.requestWindowReset();
        engine.feedIQ(block(8, 0.1f));
        QCOMPARE(engine.windowResets(), quint64(2));
        QCOMPARE(engine.windowResetsSkipped(), quint64(0));
    }

    // ── Das Bild friert nicht ein ───────────────────────────────────
    //
    // Die erste der zwei Schranken (NereusSDR, 2026-10-23): kam
    // kGapResetStallFrameIntervals Bildabstaende lang kein Bild, geht
    // die naechste Luecke durch. Ohne sie haelt eine Strecke, die
    // oefter als einmal je Fenster verliert, das Bild fuer immer auf
    // seinem letzten Stand.
    void theDisplayDoesNotFreeze()
    {
        FFTEngine engine(0);
        engine.setFftSize(1024);
        engine.setOutputFps(60);
        QSignalSpy frames(&engine, &FFTEngine::fftReadyLinear);

        // Luecke auf Luecke, jede nach einem knappen Fenster: ohne die
        // Schranke kaeme kein einziges Bild zustande.
        for (int i = 0; i < 12; ++i) {
            engine.feedIQ(block(900, 0.1f));
            engine.requestWindowReset();
        }
        engine.feedIQ(block(8, 0.1f));

        QVERIFY2(engine.windowResetsSkipped() > 0,
                 qPrintable(QStringLiteral(
                     "keine Luecke durchgelassen — verworfen %1, durchgelassen %2")
                         .arg(engine.windowResets())
                         .arg(engine.windowResetsSkipped())));
    }
};

QTEST_MAIN(TstFftWindowAfterGap)
#include "tst_fft_window_after_gap.moc"
