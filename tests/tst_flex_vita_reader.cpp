// =================================================================
// tests/tst_flex_vita_reader.cpp  (Longpath)
// =================================================================
//
// Der VITA-49-Leser gegen ECHTE Bytes einer FLEX-8400, aufgezeichnet am
// 2026-10-18 (172.30.30.12, SmartSDR 4.3.16.42034, API 1.4.0.0).
//
// Köpfe und Unterköpfe stehen vollständig in `flex_vita_golden.h` und
// sind gelesener Draht. Wo ein Fall einen ganzen Rahmen braucht, wird
// er aus diesen echten Köpfen und aufgefüllten Werten gebaut — das
// steht beim Fall dran.
//
// Satzbau und Messprotokoll: docs/protocols/flex-vita49-gemessen.md
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file. Die Byte-Folgen sind
// gelesener Draht, kein portierter Code.

#include <QtTest>

#include <QByteArray>

#include "core/FlexVitaReader.h"
#include "flex_vita_golden.h"

using namespace Longpath;
namespace G = LongpathTest::FlexVitaGolden;

namespace {

QByteArray roh(const char* hex)
{
    return QByteArray::fromHex(QByteArray(hex));
}

// Ein FFT-Paket aus echtem Kopf + echtem Nutzlastanfang, danach
// aufgefüllt, plus die vier Füllbytes, die die Maschine wirklich
// anhängt.
QByteArray fftPaket(const char* kopfHex, const char* ersteBinsHex,
                    int numBins, quint16 fuellwert)
{
    QByteArray p = roh(kopfHex);
    Q_ASSERT(p.size() == FlexVita::kHeaderBytes + FlexVita::kFftSubBytes);
    const QByteArray erste = roh(ersteBinsHex);
    p.append(erste);
    const int schonDa = erste.size() / 2;
    for (int i = schonDa; i < numBins; ++i) {
        p.append(static_cast<char>((fuellwert >> 8) & 0xFF));
        p.append(static_cast<char>(fuellwert & 0xFF));
    }
    p.append(QByteArray(4, '\0'));   // die echte Füllung
    return p;
}

QByteArray kachelPaket(const char* kopfHex, const char* ersteWerteHex,
                       int breite, quint16 fuellwert)
{
    QByteArray p = roh(kopfHex);
    Q_ASSERT(p.size() == FlexVita::kHeaderBytes + FlexVita::kTileSubBytes);
    const QByteArray erste = ersteWerteHex ? roh(ersteWerteHex) : QByteArray();
    p.append(erste);
    for (int i = erste.size() / 2; i < breite; ++i) {
        p.append(static_cast<char>((fuellwert >> 8) & 0xFF));
        p.append(static_cast<char>(fuellwert & 0xFF));
    }
    p.append(QByteArray(4, '\0'));
    return p;
}

}  // namespace

class TestFlexVitaReader : public QObject
{
    Q_OBJECT

private slots:
    // ─── Der Kopf ───────────────────────────────────────────────────

    void echterKopfWirdGelesen()
    {
        const FlexVita::Header h = FlexVita::parseHeader(roh(G::kFftKopfA));
        QVERIFY(h.valid);
        QCOMPARE(h.packetType, quint8(3));          // ExtDataWithStream
        QVERIFY(h.classIdPresent);
        QCOMPARE(h.counter, quint8(12));            // der Rollzähler
        QCOMPARE(h.streamId, 0x40000000u);          // = die Panadapter-Kennung
        QCOMPARE(h.infoClass, quint16(0x534C));     // „SL"
        QCOMPARE(h.packetClass,
                 static_cast<quint16>(FlexVitaClass::PanadapterFft));

        // Das T-Bit ist 0 — die vier Null-Byte am Ende sind also KEIN
        // angekündigter Anhang, sondern Füllung.
        QVERIFY(!h.trailerPresent);

        // Größe in Worten × 4 = die echte Paketgröße.
        QCOMPARE(h.sizeBytes, 1384);
    }

    void derWasserfallkopfTraegtSeineEigeneKennung()
    {
        const FlexVita::Header h = FlexVita::parseHeader(roh(G::kKachelKopf0));
        QVERIFY(h.valid);
        QCOMPARE(h.streamId, 0x42000000u);
        QCOMPARE(h.packetClass,
                 static_cast<quint16>(FlexVitaClass::WaterfallTile));
        QVERIFY(!h.trailerPresent);
    }

    void derTonkopfTraegtDieKennungAusStreamCreate()
    {
        const FlexVita::Header h = FlexVita::parseHeader(roh(G::kTonKopf));
        QVERIFY(h.valid);
        // `stream create type=remote_audio_rx` antwortete mit „4000008".
        QCOMPARE(h.streamId, 0x04000008u);
        QCOMPARE(h.packetClass,
                 static_cast<quint16>(FlexVitaClass::NarrowAudio));
        QCOMPARE(h.sizeBytes, 1052);
        QVERIFY(!h.trailerPresent);
    }

    void fremderVerkehrWirdVerworfen()
    {
        QVERIFY(!FlexVita::parseHeader(QByteArray()).valid);
        QVERIFY(!FlexVita::parseHeader(QByteArray(27, '\0')).valid);

        // Richtige Länge, falsche OUI.
        QByteArray falsch = roh(G::kFftKopfA);
        falsch[9] = char(0x99);
        QVERIFY(!FlexVita::parseHeader(falsch).valid);

        // Richtige OUI, falsche Info-Klasse.
        QByteArray falsch2 = roh(G::kFftKopfA);
        falsch2[12] = char(0x00);
        QVERIFY(!FlexVita::parseHeader(falsch2).valid);
    }

    // ─── Der FFT-Unterkopf ──────────────────────────────────────────

    void fftUnterkopfBeiderHaelften()
    {
        const FlexVita::FftSubHeader a =
            FlexVita::parseFftSubHeader(roh(G::kFftKopfA));
        QVERIFY(a.valid);
        QCOMPARE(a.startBin, quint16(0));
        QCOMPARE(a.numBins, quint16(670));
        QCOMPARE(a.binSize, quint16(2));        // uint16
        QCOMPARE(a.totalBins, quint16(1024));   // = xpixels
        QCOMPARE(a.frameIndex, 6u);

        const FlexVita::FftSubHeader b =
            FlexVita::parseFftSubHeader(roh(G::kFftKopfB));
        QVERIFY(b.valid);
        QCOMPARE(b.startBin, quint16(670));     // genau hinter A
        QCOMPARE(b.numBins, quint16(354));
        QCOMPARE(b.totalBins, quint16(1024));
        QCOMPARE(b.frameIndex, 6u);             // derselbe Rahmen

        // Die Rechnung, die im Mitschnitt 459 Mal aufging.
        QCOMPARE(int(a.numBins) + int(b.numBins), int(a.totalBins));
    }

    void unbrauchbarerUnterkopfWirdVerworfen()
    {
        QByteArray p = roh(G::kFftKopfA);
        // numBins = 0
        p[30] = 0; p[31] = 0;
        QVERIFY(!FlexVita::parseFftSubHeader(p).valid);

        // binSize = 4: dieser Leser kann nur uint16, und raten wäre
        // schlimmer als ablehnen.
        QByteArray q = roh(G::kFftKopfA);
        q[32] = 0; q[33] = 4;
        QVERIFY(!FlexVita::parseFftSubHeader(q).valid);
    }

    // ─── Der FFT-Rahmenbau ──────────────────────────────────────────

    void zweiHaelftenMachenEinenRahmen()
    {
        // Echte Köpfe, echter Nutzlastanfang, Rest aufgefüllt.
        const QByteArray a = fftPaket(G::kFftKopfA, G::kFftBinsA16, 670, 100);
        const QByteArray b = fftPaket(G::kFftKopfB, "", 354, 200);

        FlexFftAssembler asm1;
        QVERIFY(!asm1.feed(a).has_value());      // halb ist nicht fertig
        QCOMPARE(asm1.framesCompleted(), 0);

        const auto rahmen = asm1.feed(b);
        QVERIFY(rahmen.has_value());
        QCOMPARE(asm1.framesCompleted(), 1);
        QCOMPARE(asm1.framesStarted(), 1);
        QCOMPARE(asm1.framesRestarted(), 0);

        QCOMPARE(rahmen->streamId, 0x40000000u);
        QCOMPARE(rahmen->frameIndex, 6u);
        QCOMPARE(rahmen->bins.size(), 1024);

        // Die ersten acht Bins sind die ECHTEN Werte vom Draht.
        for (int i = 0; i < 8; ++i) {
            QCOMPARE(rahmen->bins.at(i), quint16(15));
        }
        // Und die zweite Hälfte sitzt an der richtigen Stelle.
        QCOMPARE(rahmen->bins.at(670), quint16(200));
        QCOMPARE(rahmen->bins.at(1023), quint16(200));
    }

    void dieVierFuellbyteWerdenNichtMitgezaehlt()
    {
        // Die Falle: wer die Anzahl aus der Datagrammlänge ableitet,
        // liest zwei Bins zu viel — beide 0, und 0 ist im Pixelcode
        // Vollausschlag. Hier trägt das Paket 4 Füllbytes; der Rahmen
        // muss trotzdem genau totalBins Werte haben, und der letzte
        // Wert darf nicht 0 sein.
        const QByteArray a = fftPaket(G::kFftKopfA, G::kFftBinsA16, 670, 100);
        QByteArray b = fftPaket(G::kFftKopfB, "", 354, 777);
        QCOMPARE(a.size(), 1384);   // genau die echte Größe
        QCOMPARE(b.size(), 752);

        FlexFftAssembler asm1;
        asm1.feed(a);
        const auto r = asm1.feed(b);
        QVERIFY(r.has_value());
        QCOMPARE(r->bins.size(), 1024);
        QCOMPARE(r->bins.at(1023), quint16(777));   // nicht 0
        QVERIFY(!r->bins.contains(quint16(0)));
    }

    void einZuKurzesDatagrammWirdGekappt()
    {
        // Der Unterkopf kündigt 670 Bins an, das Datagramm trägt 100.
        // Dann darf der fehlende Schwanz weder gelesen noch als
        // vorhanden vermerkt werden — der Rahmen bleibt unfertig.
        QByteArray kurz = roh(G::kFftKopfA);
        kurz.append(QByteArray(200, '\x11'));   // 100 Bins

        FlexFftAssembler asm1;
        QVERIFY(!asm1.feed(kurz).has_value());
        QCOMPARE(asm1.framesCompleted(), 0);

        // Der Rest desselben Rahmens kommt nicht — also bleibt es dabei.
        const QByteArray b = fftPaket(G::kFftKopfB, "", 354, 5);
        QVERIFY(!asm1.feed(b).has_value());
        QCOMPARE(asm1.framesCompleted(), 0);
    }

    void eineWiederholungMachtKeinenRahmen()
    {
        const QByteArray a = fftPaket(G::kFftKopfA, G::kFftBinsA16, 670, 100);
        FlexFftAssembler asm1;
        QVERIFY(!asm1.feed(a).has_value());
        QVERIFY(!asm1.feed(a).has_value());   // dieselben Bins nochmal
        QVERIFY(!asm1.feed(a).has_value());
        QCOMPARE(asm1.framesCompleted(), 0);
        // Paketzahlen können Vollständigkeit nicht belegen — genau
        // dafür ist die Lückenverfolgung da.
        QCOMPARE(asm1.framesStarted(), 1);
    }

    void einNeuerRahmenindexFaengtVonVorneAn()
    {
        const QByteArray a = fftPaket(G::kFftKopfA, G::kFftBinsA16, 670, 100);
        QByteArray a2 = a;
        a2[39] = char(7);   // frameIndex 6 -> 7

        FlexFftAssembler asm1;
        asm1.feed(a);
        QVERIFY(!asm1.feed(a2).has_value());
        QCOMPARE(asm1.framesStarted(), 2);
        QCOMPARE(asm1.framesRestarted(), 1);   // der erste war unfertig
    }

    void zweiStroemeStoerenSichNicht()
    {
        const QByteArray a = fftPaket(G::kFftKopfA, G::kFftBinsA16, 670, 100);
        QByteArray a_anderer = a;
        a_anderer[4] = char(0x41);   // StreamID 0x40000000 -> 0x41000000
        QByteArray b = fftPaket(G::kFftKopfB, "", 354, 200);
        QByteArray b_anderer = b;
        b_anderer[4] = char(0x41);

        FlexFftAssembler asm1;
        asm1.feed(a);
        asm1.feed(a_anderer);
        QVERIFY(asm1.feed(b_anderer).has_value());   // der andere ist fertig
        QCOMPARE(asm1.framesCompleted(), 1);
        const auto r = asm1.feed(b);
        QVERIFY(r.has_value());
        QCOMPARE(r->streamId, 0x40000000u);
        QCOMPARE(asm1.framesCompleted(), 2);
    }

    // ─── Der Kachel-Unterkopf ───────────────────────────────────────

    void kachelUnterkopfUndFestkommaFrequenzen()
    {
        const FlexVita::TileSubHeader s =
            FlexVita::parseTileSubHeader(roh(G::kKachelKopf0));
        QVERIFY(s.valid);
        QCOMPARE(s.tileWidth, quint16(658));
        QCOMPARE(s.tileHeight, quint16(1));        // eine Zeile je Kachel
        QCOMPARE(s.timecode, 4u);
        QCOMPARE(s.autoBlackLevel, 7969u);         // Schwarzpunkt, kein Pegel
        QCOMPARE(s.totalBinsInFrame, quint16(2460));
        QCOMPARE(s.firstBinIndex, quint16(0));

        // Festkomma, Hz × 2²⁰ — NICHT Hz.
        QCOMPARE(s.frameLowFreqRaw, Q_INT64_C(14542897152000));
        QCOMPARE(s.binBandwidthRaw, Q_INT64_C(196608000));
        QVERIFY(qAbs(s.lowFreqMhz - 13.869188) < 1e-6);
        QVERIFY(qAbs(s.binBandwidthMhz - 0.0001875) < 1e-9);

        // Was ein naiver Leser bekäme: 14,5 Terahertz.
        QVERIFY(double(s.frameLowFreqRaw) / 1e6 > 14e6);
    }

    void dieVierKachelnEinerZeileGehenAuf()
    {
        const char* koepfe[4] = {G::kKachelKopf0, G::kKachelKopf1,
                                 G::kKachelKopf2, G::kKachelKopf3};
        const quint16 breiten[4] = {658, 658, 658, 486};
        const quint16 ersteBins[4] = {0, 658, 1316, 1974};

        int summe = 0;
        for (int i = 0; i < 4; ++i) {
            const FlexVita::TileSubHeader s =
                FlexVita::parseTileSubHeader(roh(koepfe[i]));
            QVERIFY(s.valid);
            QCOMPARE(s.tileWidth, breiten[i]);
            QCOMPARE(s.firstBinIndex, ersteBins[i]);
            QCOMPARE(s.totalBinsInFrame, quint16(2460));
            QCOMPARE(s.timecode, 4u);     // alle dieselbe Zeile
            summe += s.tileWidth;
        }
        QCOMPARE(summe, 2460);            // 658·3 + 486
    }

    // ─── Der Kachel-Zeilenbau ───────────────────────────────────────

    void vierKachelnMachenEineZeile()
    {
        const char* koepfe[4] = {G::kKachelKopf0, G::kKachelKopf1,
                                 G::kKachelKopf2, G::kKachelKopf3};
        const int breiten[4] = {658, 658, 658, 486};

        FlexTileAssembler asm1;
        std::optional<FlexTileAssembler::Row> zeile;
        for (int i = 0; i < 4; ++i) {
            zeile = asm1.feed(kachelPaket(
                koepfe[i], i == 0 ? G::kKachelWerte0_16 : nullptr,
                breiten[i], quint16(11000 + i)));
            if (i < 3) {
                QVERIFY(!zeile.has_value());
            }
        }
        QVERIFY(zeile.has_value());
        QCOMPARE(asm1.rowsCompleted(), 1);
        QCOMPARE(asm1.rowsRestarted(), 0);

        QCOMPARE(zeile->streamId, 0x42000000u);
        QCOMPARE(zeile->timecode, 4u);
        QCOMPARE(zeile->values.size(), 2460);
        QCOMPARE(zeile->autoBlackLevel, 7969u);

        // Die ersten acht Werte sind die ECHTEN vom Draht.
        const quint16 echt[8] = {11438, 11342, 11470, 11638,
                                 11598, 11534, 11446, 11286};
        for (int i = 0; i < 8; ++i) {
            QCOMPARE(zeile->values.at(i), echt[i]);
        }
        // Und jede Kachel sitzt an ihrer Stelle.
        QCOMPARE(zeile->values.at(658), quint16(11001));
        QCOMPARE(zeile->values.at(1316), quint16(11002));
        QCOMPARE(zeile->values.at(1974), quint16(11003));

        // Oberkante = Unterkante + Binbreite × Gesamtzahl.
        QVERIFY(qAbs(zeile->lowFreqMhz - 13.869188) < 1e-6);
        QVERIFY(qAbs(zeile->highFreqMhz - (13.869188 + 0.0001875 * 2460)) < 1e-5);
    }

    void einAufgeblaehtesTotalBinsFaengtNeuAnStattUeberZuSchreiben()
    {
        // Ohne die zweite Rücksetzbedingung (totalBinsInFrame) liesse
        // ein Paket, das denselben Zeitcode mit grösserem
        // totalBinsInFrame wiederverwendet, den Puffer zu klein — und
        // der Schreiblauf ginge darüber hinaus (GHSA-7gvg-x594-pprq).
        FlexTileAssembler asm1;
        asm1.feed(kachelPaket(G::kKachelKopf0, G::kKachelWerte0_16, 658, 11000));
        QCOMPARE(asm1.rowsStarted(), 1);

        QByteArray aufgeblaeht =
            kachelPaket(G::kKachelKopf1, nullptr, 658, 12000);
        // totalBinsInFrame 2460 -> 60000, Zeitcode bleibt 4
        aufgeblaeht[28 + 32] = char(0xEA);
        aufgeblaeht[28 + 33] = char(0x60);
        QVERIFY(!asm1.feed(aufgeblaeht).has_value());
        QCOMPARE(asm1.rowsStarted(), 2);        // neuer Rahmen
        QCOMPARE(asm1.rowsRestarted(), 1);      // der alte war unfertig
        QCOMPARE(asm1.rowsCompleted(), 0);      // und nichts überschrieben
    }

    void eineKachelAusserhalbDesRahmensWirdVerworfen()
    {
        FlexTileAssembler asm1;
        QByteArray draussen =
            kachelPaket(G::kKachelKopf0, nullptr, 658, 11000);
        // firstBinIndex 0 -> 2400; 2400 + 658 > 2460
        draussen[28 + 34] = char(0x09);
        draussen[28 + 35] = char(0x60);
        QVERIFY(!asm1.feed(draussen).has_value());
        QCOMPARE(asm1.rowsCompleted(), 0);
    }

    // ─── Die Pegel ──────────────────────────────────────────────────

    void binZuDbmIstGegenZweiProbenBelegt()
    {
        // Probe aus dem Messlauf: dieselbe Station, zwei Spannen.
        // Verschiedene Rohwerte, dieselbe Leistung.
        const float a = FlexVita::binToDbm(770, -130.0, -40.0, 1000);
        const float b = FlexVita::binToDbm(594, -150.0, -50.0, 1000);
        QVERIFY2(qAbs(a - b) < 0.2f,
                 qPrintable(QStringLiteral("a=%1 b=%2").arg(a).arg(b)));
        QVERIFY(a < -108.0f && a > -111.0f);   // ~-109,4 dBm

        // Und derselbe Anteil bei anderer Höhe.
        const float c = FlexVita::binToDbm(539, -130.0, -40.0, 700);
        QVERIFY(qAbs(c - a) < 0.3f);
    }

    void dieEndenUndDieKlammerung()
    {
        // 0 = oben = max_dbm.
        QCOMPARE(FlexVita::binToDbm(0, -130.0, -40.0, 1000), -40.0f);
        // yPixels-1 = unten = min_dbm.
        QCOMPARE(FlexVita::binToDbm(999, -130.0, -40.0, 1000), -130.0f);
        // Werte AB yPixels sind am Boden abgeschnittene Messpunkte —
        // kein Hinweis auf eine höhere Skala.
        QCOMPARE(FlexVita::binToDbm(5000, -130.0, -40.0, 1000), -130.0f);
        // Unsinnige Spanne ⇒ der Boden, nicht NaN.
        QCOMPARE(FlexVita::binToDbm(100, -40.0, -40.0, 1000), -40.0f);
    }

    void kachelwerteSindEinFarbmassstabKeineDbm()
    {
        // (qint16)/128, wie die Vorlage. Die echten Werte liegen bei
        // ~11300 -> ~88, und das ist kein Pegel; die Vorlage sagt
        // dasselbe („typical noise floor ~96-106, colour-mapped").
        QCOMPARE(FlexVita::tileColourValue(11438), 11438.0f / 128.0f);
        QVERIFY(FlexVita::tileColourValue(11438) > 85.0f);
        QVERIFY(FlexVita::tileColourValue(11438) < 95.0f);
        // Vorzeichen: als int16 gelesen, nicht als uint16.
        QCOMPARE(FlexVita::tileColourValue(0xFF80), -1.0f);
    }

    // ─── Der Ton ────────────────────────────────────────────────────

    void tonIstFloat32StereoOhneUnterkopf()
    {
        // Echter Kopf + echte erste vier Werte.
        QByteArray p = roh(G::kTonKopf);
        p.append(roh(G::kTonWerte16));
        const QVector<float> s = FlexVita::narrowAudioSamples(p);
        QCOMPARE(s.size(), 4);

        // Die echten Werte vom Draht.
        QVERIFY(qAbs(s.at(0) - 0.324255f) < 1e-5f);
        QVERIFY(qAbs(s.at(1) - (-0.087125f)) < 1e-5f);
        QVERIFY(qAbs(s.at(2) - 0.240545f) < 1e-5f);
        QVERIFY(qAbs(s.at(3) - (-0.180913f)) < 1e-5f);
        // Alle endlich und in [-1,1] — so war der ganze Mitschnitt.
        for (const float v : s) {
            QVERIFY(qIsFinite(v));
            QVERIFY(v >= -1.0f && v <= 1.0f);
        }
    }

    void einVollesTonpaketSind128Rahmen()
    {
        // 1052 Byte = 28 Kopf + 1024 Nutzlast; KEINE Füllung.
        QByteArray p = roh(G::kTonKopf);
        p.append(QByteArray(1024, '\0'));
        QCOMPARE(p.size(), 1052);
        const QVector<float> s = FlexVita::narrowAudioSamples(p);
        QCOMPARE(s.size(), 256);         // 256 Werte
        QCOMPARE(s.size() / 2, 128);     // = 128 Stereorahmen, genau
    }

    void einKopfOhneNutzlastGibtNichts()
    {
        QVERIFY(FlexVita::narrowAudioSamples(roh(G::kTonKopf)).isEmpty());
        QVERIFY(FlexVita::narrowAudioSamples(QByteArray()).isEmpty());
    }

    // ─── Die Kappung bei kurzen Datagrammen ────────────────────────

    void gekappteWerteanzahl()
    {
        QCOMPARE(FlexVita::boundedPayloadValueCount(670, 2, 1340), 670);
        QCOMPARE(FlexVita::boundedPayloadValueCount(670, 2, 200), 100);
        QCOMPARE(FlexVita::boundedPayloadValueCount(670, 2, 1), 0);
        QCOMPARE(FlexVita::boundedPayloadValueCount(0, 2, 1340), 0);
        QCOMPARE(FlexVita::boundedPayloadValueCount(670, 0, 1340), 0);
        // Mehr Bytes als angekündigt: die Ankündigung gewinnt — das ist
        // die Regel, die die vier Füllbytes unschädlich macht.
        QCOMPARE(FlexVita::boundedPayloadValueCount(670, 2, 1344), 670);
    }

    // ─── Die Wache gegen den Breitenwechsel ────────────────────────
    // ACHTUNG: dieser Übergangsrahmen ist auf Martins 8400 NICHT
    // beobachtet worden (dort kam eine Breitenänderung 50 -> 1024, aber
    // kein nullgefüllter Anhang). Geprüft wird hier also mit erfundenen
    // Werten gegen die Vorlage — der einzige Teil dieser Datei, der
    // nicht an echtem Draht hängt.

    void nullgefuellterAnhangWirdErkannt()
    {
        QVector<quint16> bins(1024, 0);
        for (int i = 0; i < 512; ++i) { bins[i] = quint16(500 + i % 7); }
        QVERIFY(FlexVita::hasZeroFilledFftGrowthSuffix(bins, 512));

        // Kein Wachstum ⇒ nein.
        QVERIFY(!FlexVita::hasZeroFilledFftGrowthSuffix(bins, 1024));
        QVERIFY(!FlexVita::hasZeroFilledFftGrowthSuffix(bins, 0));

        // Zu kurzer Anhang (< 16) ⇒ nein.
        QVector<quint16> knapp(520, 0);
        for (int i = 0; i < 512; ++i) { knapp[i] = 500; }
        QVERIFY(!FlexVita::hasZeroFilledFftGrowthSuffix(knapp, 512));

        // Ein echtes übersteuertes Signal (alter Teil voller Nullen)
        // darf NICHT als Platzhalter gelten.
        QVector<quint16> uebersteuert(1024, 0);
        QVERIFY(!FlexVita::hasZeroFilledFftGrowthSuffix(uebersteuert, 512));
    }

    void dieWacheGibtNachDreiAblehnungenNach()
    {
        FlexFftGrowthGuard wache;
        QCOMPARE(wache.observe(false, 1024), FlexFftGrowthAction::Accept);

        for (int i = 0; i < 3; ++i) {
            QCOMPARE(wache.observe(true, 1024), FlexFftGrowthAction::Reject);
        }
        // Ein endlos wiederholter Platzhalter darf den Panadapter nicht
        // ewig auf der alten Breite einfrieren.
        QCOMPARE(wache.observe(true, 1024),
                 FlexFftGrowthAction::EmitWithFloorSuffix);

        // Ein gefüllter Rahmen setzt zurück.
        QCOMPARE(wache.observe(false, 1024), FlexFftGrowthAction::Accept);
        QCOMPARE(wache.consecutiveRejectedFrames(), 0);

        // Eine andere Breite fängt die Zählung neu an.
        QCOMPARE(wache.observe(true, 1024), FlexFftGrowthAction::Reject);
        QCOMPARE(wache.observe(true, 2048), FlexFftGrowthAction::Reject);
        QCOMPARE(wache.consecutiveRejectedFrames(), 1);
    }

    // ─── Die Lückenverfolgung ──────────────────────────────────────

    void lueckenverfolgung()
    {
        FlexBinCoverage c;
        c.reset(10);
        QVERIFY(!c.isComplete());
        QVERIFY(c.markRange(0, 5));
        QCOMPARE(c.uniqueValues(), 5);
        QVERIFY(!c.isComplete());

        // Dieselben Positionen nochmal: nichts Neues.
        QVERIFY(!c.markRange(0, 5));
        QCOMPARE(c.uniqueValues(), 5);

        // Überlappend: nur das Neue zählt.
        QVERIFY(c.markRange(3, 5));
        QCOMPARE(c.uniqueValues(), 8);

        QVERIFY(c.markRange(8, 2));
        QVERIFY(c.isComplete());

        // Grenzen.
        c.reset(10);
        QVERIFY(!c.markRange(-1, 2));
        QVERIFY(!c.markRange(9, 2));
        QVERIFY(!c.markRange(0, 0));
        QCOMPARE(c.uniqueValues(), 0);
    }
};

QTEST_APPLESS_MAIN(TestFlexVitaReader)
#include "tst_flex_vita_reader.moc"
