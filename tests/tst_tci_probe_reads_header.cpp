// SPDX-License-Identifier: GPL-3.0-or-later
//
// Die TCI-Sonde liest den Rahmenkopf richtig.
//
// 2026-08-23. Der Betreiber hat einen SunSDR QRP; der einzige gangbare
// Weg fuehrt ueber TCI, und tools/tci_probe soll morgen beantworten,
// ob ExpertSDR3 darueber auch Spektrum und Ton herausgibt.
//
// Die Sonde kann an genau EINER Stelle still falsch liegen: beim Lesen
// des 64-Byte-Kopfes. Ein verschobener Versatz ergibt plausible
// Unsinnszahlen — "IQ-Strom, 3 Hz, 1179648 Kanaele" —, und dann
// beantworte ich dem Betreiber seine Frage falsch.
//
// Geprueft wird deshalb gegen einen Rahmen, den UNSER EIGENER Baustein
// baut. Beide Seiten stammen aus derselben Beschreibung in
// TciBinaryFrame.h; laufen sie auseinander, faellt es hier auf und
// nicht erst an seinem Geraet.
//
// Die Leseroutine ist aus tools/tci_probe.cpp uebernommen — eine
// Doppelung, aber die richtige: die Sonde ist ein eigenstaendiges
// Werkzeug ohne Bindung an LongpathObjs, und sie soll auch dann noch
// bauen, wenn hier jemand aufraeumt.

#include <QtTest>

#include "core/TciBinaryFrame.h"

using namespace Longpath;

namespace {

struct StreamHeader {
    quint32 receiver = 0, sampleRate = 0, sampleType = 0;
    quint32 length = 0, streamType = 0, channels = 0;
    bool valid = false;
};

// ZEICHENGETREU aus tools/tci_probe.cpp.
StreamHeader parseHeader(const QByteArray& frame)
{
    StreamHeader h;
    if (frame.size() < 64) { return h; }
    auto u32 = [&](int off) {
        return quint32(quint8(frame[off]))
             | (quint32(quint8(frame[off + 1])) << 8)
             | (quint32(quint8(frame[off + 2])) << 16)
             | (quint32(quint8(frame[off + 3])) << 24);
    };
    h.receiver   = u32(0);
    h.sampleRate = u32(4);
    h.sampleType = u32(8);
    h.length     = u32(20);
    h.streamType = u32(24);
    h.channels   = u32(28);
    h.valid = true;
    return h;
}

} // namespace

class TstTciProbeReadsHeader : public QObject
{
    Q_OBJECT

private slots:
    void derKopfWirdRichtigGelesen()
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

        QVERIFY2(frame.size() >= 64, "Der gebaute Rahmen ist zu kurz");

        const StreamHeader h = parseHeader(frame);
        QVERIFY(h.valid);

        qInfo() << "gelesen — Empfaenger:" << h.receiver
                << "Rate:" << h.sampleRate
                << "Typ:" << h.sampleType
                << "Strom:" << h.streamType
                << "Kanaele:" << h.channels
                << "Laenge:" << h.length;

        QCOMPARE(h.receiver,   quint32(kReceiver));
        QCOMPARE(h.sampleRate, quint32(kRate));
        QCOMPARE(h.sampleType, quint32(TciSampleType::Float32));
        QCOMPARE(h.streamType, quint32(TciStreamType::RxAudioStream));
        QCOMPARE(h.channels,   quint32(kChannels));
        QCOMPARE(h.length,     quint32(kSamples * kChannels));
    }

    void auchDerIqStrom()
    {
        // Der wichtigere Fall fuers Vorhaben: ohne IQ kein Panadapter.
        QVector<float> pcm(256 * 2, 0.0f);
        const QByteArray frame = TciBinaryFrame::buildStreamPayload(
            0, 96000, int(TciSampleType::Float32), 512,
            int(TciStreamType::IqStream), 2, pcm.constData());
        const StreamHeader h = parseHeader(frame);
        QCOMPARE(h.streamType, quint32(TciStreamType::IqStream));
        QCOMPARE(h.sampleRate, quint32(96000));
        qInfo() << "IQ-Kopf gelesen, Rate" << h.sampleRate;
    }

    void einZuKurzerRahmenWirdVerworfen()
    {
        // Gegenprobe: ein Bruchstueck darf keine Fantasiezahlen
        // liefern. Ohne diese Zeile koennte die Sonde aus Rauschen
        // einen "Strom" melden.
        QCOMPARE(parseHeader(QByteArray(32, '\0')).valid, false);
        QCOMPARE(parseHeader(QByteArray()).valid, false);
        qInfo() << "kurze Rahmen werden verworfen";
    }
};

QTEST_APPLESS_MAIN(TstTciProbeReadsHeader)
#include "tst_tci_probe_reads_header.moc"
