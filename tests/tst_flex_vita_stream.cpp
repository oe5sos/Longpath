// =================================================================
// tests/tst_flex_vita_stream.cpp  (Longpath)
// =================================================================
//
// Der Sockel um den VITA-49-Leser: ein UDP-Port fuer alles, Verteilung
// an der Paketklasse, und der Maszstab, der von aussen kommen MUSS —
// die Maschine meldet `y_pixels` nie zurueck (gemessen 2026-10-18).
//
// Die Datagramme sind aus echten Koepfen gebaut (tests/flex_vita_golden.h).
//
// Longpath-original. Satzbau und Messprotokoll:
// docs/protocols/flex-vita49-gemessen.md
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

#include <QSignalSpy>
#include <QUdpSocket>

#include "core/FlexVitaStream.h"
#include "flex_vita_golden.h"

using namespace Longpath;
namespace G = LongpathTest::FlexVitaGolden;

namespace {

QByteArray roh(const char* hex) { return QByteArray::fromHex(QByteArray(hex)); }

QByteArray fftPaket(const char* kopfHex, int numBins, quint16 wert)
{
    QByteArray p = roh(kopfHex);
    for (int i = 0; i < numBins; ++i) {
        p.append(static_cast<char>((wert >> 8) & 0xFF));
        p.append(static_cast<char>(wert & 0xFF));
    }
    p.append(QByteArray(4, '\0'));
    return p;
}

// Den Rahmenindex setzen. Ohne das gibt der Rahmenbauer denselben
// Rahmen zu Recht NICHT zweimal heraus — ein wiederholter Rahmen ist
// keine Nachricht. (Beim ersten Versuch dieses Pruefstands
// uebersehen; der Code war vorsichtiger als meine Erwartung.)
QByteArray mitRahmenindex(QByteArray p, quint32 index)
{
    p[36] = static_cast<char>((index >> 24) & 0xFF);
    p[37] = static_cast<char>((index >> 16) & 0xFF);
    p[38] = static_cast<char>((index >> 8) & 0xFF);
    p[39] = static_cast<char>(index & 0xFF);
    return p;
}

// Den Rollzaehler setzen: Bit 19-16 von Wort 0, also das untere
// Halbbyte von Byte 1.
QByteArray mitZaehler(QByteArray p, quint8 zaehler)
{
    p[1] = static_cast<char>((quint8(p[1]) & 0xF0) | (zaehler & 0x0F));
    return p;
}

// Die Stromkennung setzen: Wort 1, Byte 4-7, gross-endig.
QByteArray mitStrom(QByteArray p, quint32 strom)
{
    p[4] = static_cast<char>((strom >> 24) & 0xFF);
    p[5] = static_cast<char>((strom >> 16) & 0xFF);
    p[6] = static_cast<char>((strom >> 8) & 0xFF);
    p[7] = static_cast<char>(strom & 0xFF);
    return p;
}

QByteArray tonMit(quint8 zaehler, quint32 strom)
{
    QByteArray p = roh(LongpathTest::FlexVitaGolden::kTonKopf);
    p.append(QByteArray(1024, '\0'));
    return mitZaehler(mitStrom(p, strom), zaehler);
}

QByteArray kachelPaket(const char* kopfHex, int breite, quint16 wert)
{
    QByteArray p = roh(kopfHex);
    for (int i = 0; i < breite; ++i) {
        p.append(static_cast<char>((wert >> 8) & 0xFF));
        p.append(static_cast<char>(wert & 0xFF));
    }
    p.append(QByteArray(4, '\0'));
    return p;
}

}  // namespace

class TestFlexVitaStream : public QObject
{
    Q_OBJECT

private slots:
    void sockelBindetUndMeldetSichAn()
    {
        // Ein Horcher spielt die Maschine: er muss das Null-Byte sehen,
        // denn daraus lernt sie IP:Port.
        QUdpSocket maschine;
        QVERIFY(maschine.bind(QHostAddress::LocalHost, 0));

        FlexVitaStream strom;
        // Die echte Maschine hoert auf 4992; im Pruefstand kann der
        // Horcher den Port nicht waehlen, darum wird hier nur geprueft,
        // dass gebunden wird und ein Port zurueckkommt.
        const quint16 port = strom.start(QHostAddress::LocalHost);
        QVERIFY(port != 0);
        QVERIFY(strom.isRunning());
        QCOMPARE(strom.localPort(), port);

        strom.stop();
        QVERIFY(!strom.isRunning());
    }

    void ohneGeraeteadresseWirdNichtGebunden()
    {
        FlexVitaStream strom;
        QCOMPARE(strom.start(QHostAddress()), quint16(0));
        QVERIFY(!strom.isRunning());
    }

    void dieVerteilungGehtNachDerPaketklasse()
    {
        FlexVitaStream strom;
        QSignalSpy spektrum(&strom, &FlexVitaStream::spectrumReady);
        QSignalSpy zeile(&strom, &FlexVitaStream::waterfallRowReady);
        QSignalSpy ton(&strom, &FlexVitaStream::audioSamplesReady);

        // Ein voller FFT-Rahmen: zwei Haelften, echte Koepfe.
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfA, 670, 500));
        QCOMPARE(spektrum.count(), 0);
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfB, 354, 500));
        QCOMPARE(spektrum.count(), 1);
        QCOMPARE(spektrum.at(0).at(0).toUInt(), 0x40000000u);
        QCOMPARE(spektrum.at(0).at(1).value<QVector<float>>().size(), 1024);

        // Eine volle Wasserfallzeile: vier Kacheln.
        const char* koepfe[4] = {G::kKachelKopf0, G::kKachelKopf1,
                                 G::kKachelKopf2, G::kKachelKopf3};
        const int breiten[4] = {658, 658, 658, 486};
        for (int i = 0; i < 4; ++i) {
            strom.ingestDatagramForTesting(
                kachelPaket(koepfe[i], breiten[i], quint16(11000)));
        }
        QCOMPARE(zeile.count(), 1);
        QCOMPARE(zeile.at(0).at(0).toUInt(), 0x42000000u);
        QCOMPARE(zeile.at(0).at(1).value<QVector<quint16>>().size(), 2460);

        // Ton.
        QByteArray tonPaket = roh(G::kTonKopf);
        tonPaket.append(QByteArray(1024, '\0'));
        strom.ingestDatagramForTesting(tonPaket);
        QCOMPARE(ton.count(), 1);
        QCOMPARE(ton.at(0).at(0).toUInt(), 0x04000008u);
        QCOMPARE(ton.at(0).at(1).value<QVector<float>>().size(), 256);

        // 2 FFT + 4 Kacheln + 1 Ton.
        QCOMPARE(strom.datagramsReceived(), 7);
        QCOMPARE(strom.datagramsIgnored(), 0);
    }

    void fremderVerkehrWirdGezaehltUndFallengelassen()
    {
        FlexVitaStream strom;
        QSignalSpy spektrum(&strom, &FlexVitaStream::spectrumReady);

        strom.ingestDatagramForTesting(QByteArray("irgendwas"));
        strom.ingestDatagramForTesting(QByteArray(28, '\0'));
        // Richtiger Satzbau, aber eine Klasse, die es auf dieser
        // Maschine gar nicht gibt.
        //
        // Hier stand bis zum 2026-10-21 die Messwertklasse 0x8002,
        // begruendet mit „nicht gemessen, also nicht geraten". Sie ist
        // inzwischen gemessen UND gebaut (§5b des Messblatts), taugt
        // also nicht mehr als Beispiel fuer fremden Verkehr — der
        // eigene Fall darunter prueft sie jetzt.
        QByteArray fremd = roh(G::kFftKopfA);
        fremd[14] = char(0xBE); fremd[15] = char(0xEF);
        strom.ingestDatagramForTesting(fremd);

        QCOMPARE(spektrum.count(), 0);
        QCOMPARE(strom.datagramsReceived(), 3);
        QCOMPARE(strom.datagramsIgnored(), 3);
    }

    // Messwerte sind seit dem 2026-10-21 gemessen und gebaut: ein
    // Paket der Klasse 0x8002 ist kein fremder Verkehr mehr, und
    // seine Paare kommen heraus.
    void messwerteKommenHeraus()
    {
        FlexVitaStream strom;

        // Die Werte DIREKT auffangen statt ueber QSignalSpy.
        //
        // Ein Spy legt den Nutztyp als QVariant ab und braucht ihn
        // dafuer im Metasystem. Qt 6.11 (macOS) meldet ihn von selbst
        // an, die aeltere Fassung der Linux-CI nicht — der Fall war
        // dort am 2026-10-21 rot, waehrend er hier gruen blieb, und
        // ausgelesen wurde ein leerer Vorgabewert statt der Paare.
        // Der Typ ist jetzt zwar angemeldet (Q_DECLARE_METATYPE in
        // FlexVitaReader.h), aber ein Prueffall soll nicht daran
        // haengen: hier zaehlt, was aus dem Strom kommt.
        int rufe = 0;
        QVector<FlexVita::FlexMeterSample> werte;
        QObject::connect(&strom, &FlexVitaStream::meterSamplesReady,
                         [&](quint32, const QVector<FlexVita::FlexMeterSample>& w) {
            ++rufe;
            werte = w;
        });

        QByteArray p = roh(G::kFftKopfA).left(FlexVita::kHeaderBytes);
        p[14] = char(0x80); p[15] = char(0x02);
        // Index 10 (`SWR`) mit Rohwert 128 — am Geraet genau 1,000.
        p.append(char(0x00)); p.append(char(0x0A));
        p.append(char(0x00)); p.append(char(0x80));
        strom.ingestDatagramForTesting(p);

        QCOMPARE(rufe, 1);
        QCOMPARE(strom.datagramsIgnored(), 0);
        QCOMPARE(werte.size(), 1);
        QCOMPARE(werte.at(0).index, quint16(10));
        QCOMPARE(werte.at(0).raw,   qint16(128));
    }

    void derMassstabKommtVonAussen()
    {
        // Das ist der Kern: die Maschine meldet y_pixels NIE zurueck.
        // Wer den Maszstab setzt, muss ihn hier hinterlegen — sonst
        // gelten die Vorgaben, und die Pegel sind falsch.
        FlexVitaStream strom;
        QSignalSpy spektrum(&strom, &FlexVitaStream::spectrumReady);

        // Vorgabe: 700 Pixel, -130..-40.
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfA, 670, 539));
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfB, 354, 539));
        QCOMPARE(spektrum.count(), 1);
        const float mitVorgabe =
            spektrum.at(0).at(1).value<QVector<float>>().at(0);

        // Jetzt der hinterlegte Maszstab aus dem Messlauf. Mit NEUEM
        // Rahmenindex, sonst gibt der Rahmenbauer zu Recht nichts
        // heraus.
        strom.setYPixels(0x40000000u, 1000);
        strom.setDbmRange(0x40000000u, -130.0, -40.0);
        strom.ingestDatagramForTesting(
            mitRahmenindex(fftPaket(G::kFftKopfA, 670, 770), 7));
        strom.ingestDatagramForTesting(
            mitRahmenindex(fftPaket(G::kFftKopfB, 354, 770), 7));
        QCOMPARE(spektrum.count(), 2);
        const float mitMassstab =
            spektrum.at(1).at(1).value<QVector<float>>().at(0);

        // 539 bei 700 Pixeln und 770 bei 1000 sind dasselbe Signal —
        // genau die Probe aus dem Messlauf. Mit dem richtigen Maszstab
        // kommt derselbe Pegel heraus.
        QVERIFY2(qAbs(mitVorgabe - mitMassstab) < 0.3f,
                 qPrintable(QStringLiteral("Vorgabe=%1 Maszstab=%2")
                                .arg(mitVorgabe).arg(mitMassstab)));
        QVERIFY(mitMassstab < -108.0f && mitMassstab > -111.0f);

        // Und mit dem FALSCHEN Maszstab wird es sichtbar falsch: 770
        // Pixel in einer 700er Hoehe heisst abgeschnitten am Boden.
        strom.setYPixels(0x40000000u, 700);
        strom.ingestDatagramForTesting(
            mitRahmenindex(fftPaket(G::kFftKopfA, 670, 770), 8));
        strom.ingestDatagramForTesting(
            mitRahmenindex(fftPaket(G::kFftKopfB, 354, 770), 8));
        QCOMPARE(spektrum.count(), 3);
        QCOMPARE(spektrum.at(2).at(1).value<QVector<float>>().at(0), -130.0f);
    }

    void derselbeRahmenKommtNichtZweimalHeraus()
    {
        // Gelernt beim Schreiben dieses Pruefstands: ein wiederholter
        // Rahmen ist keine Nachricht. Erst ein neuer Rahmenindex gibt
        // einen neuen Rahmen.
        FlexVitaStream strom;
        QSignalSpy spektrum(&strom, &FlexVitaStream::spectrumReady);

        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfA, 670, 500));
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfB, 354, 500));
        QCOMPARE(spektrum.count(), 1);

        // Dieselben Pakete nochmal: nichts.
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfA, 670, 500));
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfB, 354, 500));
        QCOMPARE(spektrum.count(), 1);

        // Neuer Rahmenindex: wieder einer.
        strom.ingestDatagramForTesting(
            mitRahmenindex(fftPaket(G::kFftKopfA, 670, 501), 7));
        strom.ingestDatagramForTesting(
            mitRahmenindex(fftPaket(G::kFftKopfB, 354, 501), 7));
        QCOMPARE(spektrum.count(), 2);
    }

    // ── Luecken im Strom (2026-10-29) ────────────────────────
    //
    // Der Rollzaehler wurde seit dem Port gelesen und nie ausgewertet.
    // Am 2026-10-29 fiel der Empfang zweimal ganz aus, und es gab kein
    // Instrument, das „Pakete fehlen unterwegs" von „die Maschine
    // schickt nichts" getrennt haette.

    void derRollzaehlerSagtWasFehlt()
    {
        using FlexVita::fehlendeNachZaehler;
        // Der normale Schritt: nichts fehlt.
        QCOMPARE(fehlendeNachZaehler(0, 1), 0);
        QCOMPARE(fehlendeNachZaehler(14, 15), 0);
        // Ueber den Umlauf hinweg zaehlt er genauso.
        QCOMPARE(fehlendeNachZaehler(15, 0), 0);
        QCOMPARE(fehlendeNachZaehler(0, 2), 1);
        QCOMPARE(fehlendeNachZaehler(14, 1), 2);
        QCOMPARE(fehlendeNachZaehler(13, 0), 2);
        // Und die Grenze, die im Kopf der Funktion steht: ein Verlust
        // von genau 16 ist unsichtbar. Das ist keine Macke des Codes,
        // das ist die Breite des Zaehlers — wer die Zahl liest, muss
        // wissen, dass sie eine untere Schranke ist.
        QCOMPARE(fehlendeNachZaehler(7, 7), 15);
    }

    void einLueckenloserStromMeldetKeinenVerlust()
    {
        FlexVitaStream strom;
        for (int i = 0; i < 40; ++i) {
            strom.ingestDatagramForTesting(
                tonMit(quint8(i & 0x0F), 0x04000008u));
        }
        QCOMPARE(strom.verlorenePaketeGesamt(), qint64(0));
        const auto l = strom.luecken();
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.at(0).pakete, qint64(40));
        QCOMPARE(l.at(0).verloren, qint64(0));
        QCOMPARE(l.at(0).spruenge, qint64(0));
    }

    void einSprungImZaehlerWirdGezaehlt()
    {
        FlexVitaStream strom;
        const quint8 folge[] = {0, 1, 4, 5, 6, 9};   // 2 + 2 fehlen
        for (const quint8 z : folge) {
            strom.ingestDatagramForTesting(tonMit(z, 0x04000008u));
        }
        QCOMPARE(strom.verlorenePaketeGesamt(), qint64(4));
        const auto l = strom.luecken();
        QCOMPARE(l.size(), 1);
        QCOMPARE(l.at(0).verloren, qint64(4));
        QCOMPARE(l.at(0).spruenge, qint64(2));
    }

    // Die Falle, die ein Instrument zur Phantommaschine macht: der
    // Zaehler laeuft JE STROM. Mit zwei Scheiben kommen zwei
    // verschraenkte Zaehler an, und wer sie in einen Topf wirft, misst
    // bei jedem zweiten Paket einen Verlust, der nie stattfand.
    void zweiStroemeStoerenSichNicht()
    {
        FlexVitaStream strom;
        for (int i = 0; i < 16; ++i) {
            strom.ingestDatagramForTesting(
                tonMit(quint8(i & 0x0F), 0x04000008u));
            strom.ingestDatagramForTesting(
                tonMit(quint8(i & 0x0F), 0x04000009u));
        }
        QCOMPARE(strom.verlorenePaketeGesamt(), qint64(0));
        QCOMPARE(strom.luecken().size(), 2);
    }

    // GEGENPROBE zum vorigen: in EINEN Topf geworfen ergibt dieselbe
    // Folge lauter Verluste. Ohne diesen Lauf belegt die Null oben
    // nichts — sie koennte auch heissen, dass gar nicht gezaehlt wird.
    void inEinenTopfGeworfenErgaebeLauterPhantome()
    {
        quint8 letzter = 0;
        bool   gesehen = false;
        qint64 verloren = 0;
        for (int i = 0; i < 16; ++i) {
            for (int s = 0; s < 2; ++s) {
                const quint8 z = quint8(i & 0x0F);
                if (gesehen) {
                    verloren += FlexVita::fehlendeNachZaehler(letzter, z);
                }
                letzter = z;
                gesehen = true;
            }
        }
        QVERIFY2(verloren > 100,
                 "Ein gemeinsamer Zaehler muesste Phantome erzeugen; "
                 "tut er das nicht, prueft der Fall darueber nichts.");
    }

    void dasErstePaketIstKeineLuecke()
    {
        FlexVitaStream strom;
        // Zaehler 9 als Erstes — ohne Vorgaenger ist das keine Aussage.
        strom.ingestDatagramForTesting(tonMit(9, 0x04000008u));
        QCOMPARE(strom.verlorenePaketeGesamt(), qint64(0));
    }

    void unsinnigerMassstabWirdNichtUebernommen()
    {
        FlexVitaStream strom;
        strom.setYPixels(0x40000000u, 1000);
        strom.setYPixels(0x40000000u, 0);      // abgelehnt
        strom.setYPixels(0x40000000u, 1);      // abgelehnt
        strom.setDbmRange(0x40000000u, -130.0, -40.0);
        strom.setDbmRange(0x40000000u, -40.0, -130.0);  // verdreht, abgelehnt

        QSignalSpy spektrum(&strom, &FlexVitaStream::spectrumReady);
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfA, 670, 770));
        strom.ingestDatagramForTesting(fftPaket(G::kFftKopfB, 354, 770));
        QCOMPARE(spektrum.count(), 1);
        const float dbm = spektrum.at(0).at(1).value<QVector<float>>().at(0);
        // Der gute Maszstab von vorher gilt weiter.
        QVERIFY(dbm < -108.0f && dbm > -111.0f);
    }

    void jederStromHatSeinenEigenenMassstab()
    {
        FlexVitaStream strom;
        strom.setYPixels(0x40000000u, 1000);
        strom.setDbmRange(0x40000000u, -130.0, -40.0);

        QByteArray a2 = fftPaket(G::kFftKopfA, 670, 770);
        QByteArray b2 = fftPaket(G::kFftKopfB, 354, 770);
        a2[4] = char(0x41); b2[4] = char(0x41);   // anderer Strom
        strom.setYPixels(0x41000000u, 2000);
        strom.setDbmRange(0x41000000u, -140.0, -20.0);

        QSignalSpy spektrum(&strom, &FlexVitaStream::spectrumReady);
        strom.ingestDatagramForTesting(a2);
        strom.ingestDatagramForTesting(b2);
        QCOMPARE(spektrum.count(), 1);
        QCOMPARE(spektrum.at(0).at(0).toUInt(), 0x41000000u);
        // 770 von 2000 bei -140..-20 ist ein voellig anderer Pegel.
        const float dbm = spektrum.at(0).at(1).value<QVector<float>>().at(0);
        QVERIFY(dbm > -80.0f);
    }
};

QTEST_MAIN(TestFlexVitaStream)
#include "tst_flex_vita_stream.moc"
