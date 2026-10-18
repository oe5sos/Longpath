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
        // Richtiger Satzbau, aber eine Klasse, die Stufe 2 nicht baut
        // (Messwerte 0x8002 — nicht gemessen, also nicht geraten).
        QByteArray messwerte = roh(G::kFftKopfA);
        messwerte[14] = char(0x80); messwerte[15] = char(0x02);
        strom.ingestDatagramForTesting(messwerte);

        QCOMPARE(spektrum.count(), 0);
        QCOMPARE(strom.datagramsReceived(), 3);
        QCOMPARE(strom.datagramsIgnored(), 3);
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
