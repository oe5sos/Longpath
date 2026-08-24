// SPDX-License-Identifier: GPL-3.0-or-later
//
// TciBinaryFrame::parseStreamHeader / headerMatchesPayload — die
// Bausteine, auf denen TciClient::handleBinaryMessage steht.
//
// Anlass, 2026-08-24: tools/tci_probe.cpp hatte lange eine EIGENE,
// verdoppelte Leseroutine (siehe tst_tci_probe_reads_header.cpp). Die
// Sonde ist bewusst eigenstaendig geblieben — sie muss auch bauen,
// wenn hier jemand aufraeumt. Aber TciClient.cpp, das jetzt am
// laufenden Betrieb haengt, ruft die ECHTEN Bausteine in
// TciBinaryFrame.h/.cpp auf. Diese Pruefung deckt genau die.
//
// Zwei Dinge werden hier bewusst hart geprueft, weil TciClient sich
// bei genau diesen zweien gegen den Kopf entscheidet und stattdessen
// dem Textkanal bzw. dem Stromtyp vertraut (docs/TCI-SunSDR-gemessen.md):
//   - headerMatchesPayload muss einen abgeschnittenen Rahmen ablehnen,
//     nicht Fantasiewerte daraus entpacken.
//   - der Kopf-Leser liefert die Rohwerte unveraendert — die Deutung
//     (welcher Rate, welcher Kanalzahl man traut) liegt bewusst NICHT
//     hier, sondern in TciClient.

#include <QtTest>

#include "core/TciBinaryFrame.h"

using namespace Longpath;

class TstTciBinaryFrameRoundtrip : public QObject
{
    Q_OBJECT

private slots:
    void kopfWirdRichtigGelesen()
    {
        const int kReceiver = 1;
        const int kRate     = 48000;
        const int kChannels = 2;
        const int kSamples  = 512;   // je Kanal

        QVector<float> pcm(kSamples * kChannels, 0.25f);
        const QByteArray frame = TciBinaryFrame::buildStreamPayload(
            kReceiver, kRate, int(TciSampleType::Float32),
            kSamples * kChannels, int(TciStreamType::RxAudioStream),
            kChannels, pcm.constData());

        const TciStreamHeader h = TciBinaryFrame::parseStreamHeader(frame);
        QVERIFY(h.valid);
        QCOMPARE(h.receiver,   kReceiver);
        QCOMPARE(h.sampleRate, kRate);
        QCOMPARE(h.sampleType, int(TciSampleType::Float32));
        QCOMPARE(h.streamType, int(TciStreamType::RxAudioStream));
        QCOMPARE(h.channels,   kChannels);
        QCOMPARE(h.length,     kSamples * kChannels);
        QCOMPARE(h.frameBytes, frame.size());
        QVERIFY(TciBinaryFrame::headerMatchesPayload(h));
    }

    void abgeschnittenerRahmenWirdAbgelehnt()
    {
        // Genau der Fall, den ein SunSDR-Netzweg mit Paketverlust
        // erzeugen wuerde: der Kopf ist da und plausibel, die Nutzlast
        // fehlt teilweise. Wer hier trotzdem entpackt, liest ueber das
        // Ende des Puffers hinaus.
        QVector<float> pcm(512 * 2, 0.0f);
        QByteArray frame = TciBinaryFrame::buildStreamPayload(
            0, 48000, int(TciSampleType::Float32), 512 * 2,
            int(TciStreamType::IqStream), 2, pcm.constData());
        QVERIFY(frame.size() > 64);
        frame.chop(frame.size() - 64 - 100);   // Kopf + 100 von den erwarteten 4096 Byte

        const TciStreamHeader h = TciBinaryFrame::parseStreamHeader(frame);
        QVERIFY2(h.valid, "der Kopf allein ist noch lesbar");
        QVERIFY2(!TciBinaryFrame::headerMatchesPayload(h),
                 "ein abgeschnittener Rahmen darf nicht als passend gelten");
    }

    void zuKurzerRahmenIstUngueltig()
    {
        // Gegenprobe: aus Rauschen darf kein "Strom" entstehen.
        QCOMPARE(TciBinaryFrame::parseStreamHeader(QByteArray(32, '\0')).valid, false);
        QCOMPARE(TciBinaryFrame::parseStreamHeader(QByteArray()).valid, false);
    }

    void iqStromRoundtrip()
    {
        // Der fuers Vorhaben wichtigere Fall: ohne IQ kein Panadapter.
        // Encoden, entpacken, Werte muessen exakt zurueckkommen --
        // TciClient::handleBinaryMessage vertraut decodeSamples blind,
        // sobald headerMatchesPayload zugestimmt hat.
        QVector<float> pcm(256 * 2);
        for (int i = 0; i < pcm.size(); ++i) {
            pcm[i] = (i % 2 == 0) ? (0.001f * i) : (-0.001f * i);
        }
        const QByteArray frame = TciBinaryFrame::buildStreamPayload(
            0, 96000, int(TciSampleType::Float32), pcm.size(),
            int(TciStreamType::IqStream), 2, pcm.constData());

        const TciStreamHeader h = TciBinaryFrame::parseStreamHeader(frame);
        QVERIFY(TciBinaryFrame::headerMatchesPayload(h));

        const std::vector<float> decoded =
            TciBinaryFrame::decodeSamples(frame, 64, h.length, h.sampleType);
        QCOMPARE(int(decoded.size()), pcm.size());
        for (int i = 0; i < pcm.size(); ++i) {
            QCOMPARE(decoded[static_cast<size_t>(i)], pcm[i]);
        }
    }
};

QTEST_APPLESS_MAIN(TstTciBinaryFrameRoundtrip)
#include "tst_tci_binary_frame_roundtrip.moc"
