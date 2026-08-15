// =================================================================
// tests/tst_waterfall_history_buffer.cpp  (NereusSDR)
// =================================================================
//
// Der Intensitätsspeicher hinter dem Wasserfall. Schritt 1 von
// Aufgabe #100.
//
// Zwei Eigenschaften tragen die Sache, und beide sind leicht zu
// verlieren:
//
//   • Der Speicher entsteht blockweise. Wer die App zwei Minuten laufen
//     lässt, soll nicht für zwanzig zahlen. Ein Aufruf zu viel an der
//     falschen Stelle belegt alles im Voraus, und niemand merkt es —
//     es funktioniert ja.
//
//   • configure() mit denselben Maßen tut nichts. Es wird aus dem
//     Zeichenweg gerufen; legte es jedes Mal neu an, stünde nie eine
//     zweite Zeile im Puffer, und die Historie wäre immer leer.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>

#include "gui/WaterfallHistoryBuffer.h"

using namespace NereusSDR;

namespace {

void fillRow(WaterfallHistoryBuffer& b, int row, quint8 value)
{
    quint8* p = b.writableRow(row);
    QVERIFY(p != nullptr);
    for (int x = 0; x < b.width(); ++x) { p[x] = value; }
}

} // namespace

class TestWaterfallHistoryBuffer : public QObject
{
    Q_OBJECT

private slots:
    void aFreshBufferIsNotConfigured()
    {
        WaterfallHistoryBuffer b;
        QVERIFY(!b.isConfigured());
        QCOMPARE(b.allocatedBytes(), qsizetype(0));
        QVERIFY(b.row(0) == nullptr);
        QVERIFY(b.writableRow(0) == nullptr);
    }

    void configuringSetsTheSlotsButNotTheMemory()
    {
        WaterfallHistoryBuffer b;
        b.configure(100, 1000);
        QVERIFY(b.isConfigured());
        QCOMPARE(b.width(), 100);
        QCOMPARE(b.capacityRows(), 1000);
        QCOMPARE(b.size(), QSize(100, 1000));

        // Das ist der ganze Punkt: tausend Plätze, kein Byte belegt.
        QCOMPARE(b.allocatedBytes(), qsizetype(0));
        QCOMPARE(b.allocatedChunkCount(), 0);
    }

    void memoryAppearsOneChunkAtATime()
    {
        WaterfallHistoryBuffer b;
        b.configure(100, 1000);           // 4 Blöcke: 256+256+256+232

        fillRow(b, 0, 7);
        QCOMPARE(b.allocatedChunkCount(), 1);
        QCOMPARE(b.allocatedBytes(), qsizetype(256 * 100));

        fillRow(b, 255, 8);               // derselbe Block
        QCOMPARE(b.allocatedChunkCount(), 1);

        fillRow(b, 256, 9);               // der nächste
        QCOMPARE(b.allocatedChunkCount(), 2);

        // Der letzte Block ist angebrochen: 1000 - 768 = 232 Zeilen.
        fillRow(b, 999, 10);
        QCOMPARE(b.allocatedChunkCount(), 3);
        QCOMPARE(b.allocatedBytes(),
                 qsizetype(256 * 100 + 256 * 100 + 232 * 100));
    }

    void anUntouchedRowReadsAsNothing()
    {
        WaterfallHistoryBuffer b;
        b.configure(50, 600);
        fillRow(b, 0, 3);
        // Block 0 steht, Block 1 nicht. Der nullptr ist die Auskunft
        // „hier war noch nichts" — der Aufrufer malt dann nichts,
        // statt Schwarz aus einem nie beschriebenen Block zu holen.
        QVERIFY(b.row(0) != nullptr);
        QVERIFY(b.row(300) == nullptr);
    }

    void writtenValuesComeBack()
    {
        WaterfallHistoryBuffer b;
        b.configure(4, 300);
        quint8* p = b.writableRow(5);
        p[0] = 0; p[1] = 64; p[2] = 128; p[3] = 255;

        const quint8* q = b.row(5);
        QVERIFY(q != nullptr);
        QCOMPARE(q[0], quint8(0));
        QCOMPARE(q[1], quint8(64));
        QCOMPARE(q[2], quint8(128));
        QCOMPARE(q[3], quint8(255));
    }

    void aFreshChunkStartsAtZero()
    {
        WaterfallHistoryBuffer b;
        b.configure(4, 300);
        (void)b.writableRow(0);           // belegt den Block
        const quint8* q = b.row(1);       // nie beschriebene Zeile darin
        QVERIFY(q != nullptr);
        QCOMPARE(q[0], quint8(0));
        QCOMPARE(q[3], quint8(0));
    }

    void outOfRangeRowsAreRefused()
    {
        WaterfallHistoryBuffer b;
        b.configure(10, 100);
        QVERIFY(b.writableRow(-1) == nullptr);
        QVERIFY(b.writableRow(100) == nullptr);
        QVERIFY(b.row(-1) == nullptr);
        QVERIFY(b.row(100) == nullptr);
    }

    // ── Der Aufruf aus dem Zeichenweg ────────────────────────────────
    void configuringTwiceWithTheSameSizeKeepsTheRows()
    {
        WaterfallHistoryBuffer b;
        b.configure(10, 300);
        fillRow(b, 2, 42);

        b.configure(10, 300);             // zweimal dasselbe

        const quint8* q = b.row(2);
        QVERIFY2(q != nullptr,
                 "configure() mit denselben Massen hat den Puffer "
                 "neu angelegt — die Historie waere damit immer leer");
        QCOMPARE(q[0], quint8(42));
    }

    void configuringWithNewSizeStartsOver()
    {
        WaterfallHistoryBuffer b;
        b.configure(10, 300);
        fillRow(b, 2, 42);
        b.configure(10, 600);
        QCOMPARE(b.capacityRows(), 600);
        QCOMPARE(b.allocatedBytes(), qsizetype(0));
    }

    void nonsensicalSizesReset()
    {
        WaterfallHistoryBuffer b;
        b.configure(10, 300);
        fillRow(b, 0, 1);
        b.configure(0, 300);
        QVERIFY(!b.isConfigured());
        QCOMPARE(b.allocatedBytes(), qsizetype(0));
    }

    // ── Verwerfen und Neubreite ──────────────────────────────────────
    void discardingKeepsTheSlotsAndFreesTheMemory()
    {
        WaterfallHistoryBuffer b;
        b.configure(100, 600);
        fillRow(b, 0, 5);
        QVERIFY(b.allocatedBytes() > 0);

        b.discardRows();
        QVERIFY(b.isConfigured());
        QCOMPARE(b.capacityRows(), 600);
        QCOMPARE(b.allocatedBytes(), qsizetype(0));
        QVERIFY(b.row(0) == nullptr);
    }

    void resizingWidthCarriesTheContentOver()
    {
        WaterfallHistoryBuffer b;
        b.configure(4, 300);
        quint8* p = b.writableRow(1);
        p[0] = 10; p[1] = 20; p[2] = 30; p[3] = 40;

        QVERIFY(b.resizeWidth(8));
        QCOMPARE(b.width(), 8);
        const quint8* q = b.row(1);
        QVERIFY(q != nullptr);
        // Nächster Nachbar: jeder Quellpunkt zweimal. Nicht
        // interpoliert — der Wert ist ein Index in eine Farbtabelle,
        // und der Mittelwert zweier Indizes ist nicht die mittlere Farbe.
        QCOMPARE(q[0], quint8(10));
        QCOMPARE(q[1], quint8(10));
        QCOMPARE(q[2], quint8(20));
        QCOMPARE(q[6], quint8(40));
        QCOMPARE(q[7], quint8(40));
    }

    void resizingDoesNotFillEmptyChunks()
    {
        WaterfallHistoryBuffer b;
        b.configure(4, 600);
        fillRow(b, 0, 1);                 // nur Block 0
        QCOMPARE(b.allocatedChunkCount(), 1);
        QVERIFY(b.resizeWidth(8));
        QVERIFY2(b.allocatedChunkCount() == 1,
                 "das Umrechnen hat leere Bloecke belegt — damit waere "
                 "die blockweise Belegung wieder eine vollstaendige");
    }

    void resizingToTheSameWidthIsAnEasyYes()
    {
        WaterfallHistoryBuffer b;
        b.configure(4, 300);
        fillRow(b, 0, 9);
        QVERIFY(b.resizeWidth(4));
        QCOMPARE(b.row(0)[0], quint8(9));
    }

    void resizingAnUnconfiguredBufferFails()
    {
        WaterfallHistoryBuffer b;
        QVERIFY(!b.resizeWidth(100));
        b.configure(4, 300);
        QVERIFY(!b.resizeWidth(0));
    }

    void movingTakesTheRowsAlong()
    {
        WaterfallHistoryBuffer a;
        a.configure(4, 300);
        fillRow(a, 0, 77);

        WaterfallHistoryBuffer b = std::move(a);
        QVERIFY(b.isConfigured());
        QCOMPARE(b.row(0)[0], quint8(77));
        QVERIFY(!a.isConfigured());       // NOLINT — genau das wird geprüft
    }
};

QTEST_APPLESS_MAIN(TestWaterfallHistoryBuffer)
#include "tst_waterfall_history_buffer.moc"
