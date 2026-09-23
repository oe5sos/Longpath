// no-port-check: Longpath-original unit-test file.
// =================================================================
// tests/tst_p1_i2c_response_is_not_ptt.cpp  (Longpath)
// =================================================================
//
// Ein Unterrahmen mit gesetztem Bit 7 in C0 ist die ANTWORT auf eine
// I2C-Leseanfrage: die unteren sieben Bits sind die zurueckgegebene
// Adresse, nicht die Zustandsbits des Funkgeraets. Der Telemetriezweig
// von parseEp6Frame() ueberspringt solche Rahmen seit jeher — die
// Mikrofon-PTT-Auswertung las sie mit, und jede Adresse mit gesetztem
// Bit 0 wurde als gedrueckte Sendetaste gelesen.
//
// Gefunden am 2026-09-23 an der HPSDR-Werkbank, nachdem der Simulator
// I2C beantworten gelernt hat: Longpath fragt beim Verbinden die
// Version der HL2-I/O-Platine ab (Geraet 0x41, Register 0, Bus 1), die
// Antwort kommt mit C0 = 0xFD zurueck, und 60 ms spaeter stand im
// Protokoll des Simulators "PTT= 00000001". Das Geraet ging auf
// Sendung, ohne dass jemand etwas gedrueckt hat.
//
// Thetis hat das Problem nicht: dort liegt die Zustandsauswertung im
// else-Zweig der I2C-Pruefung (networkproto1.c:478-493 [@c26a8a4]).
// =================================================================

#include <QtTest/QtTest>
#include <QSignalSpy>

#include "core/P1RadioConnection.h"

using namespace Longpath;

namespace {

QByteArray makeEp6Frame(const quint8 sub0[5], const quint8 sub1[5])
{
    QByteArray pkt(1032, '\0');
    auto* p = reinterpret_cast<quint8*>(pkt.data());
    p[0] = 0xEF; p[1] = 0xFE; p[2] = 0x01; p[3] = 0x06;
    p[8] = 0x7F; p[9] = 0x7F; p[10] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[11 + i] = sub0[i]; }
    p[520] = 0x7F; p[521] = 0x7F; p[522] = 0x7F;
    for (int i = 0; i < 5; ++i) { p[523 + i] = sub1[i]; }
    return pkt;
}

} // namespace

class TestP1I2cResponseIsNotPtt : public QObject { Q_OBJECT
private slots:

    // Der Fund: die echte Antwort der I/O-Platine.
    // C0 = 0x80 | 0x7D — Antwortmarke plus zurueckgegebene Adresse 0x7D.
    // Bit 0 dieser ADRESSE ist gesetzt; eine Sendetaste ist es nicht.
    void anI2cAnswerMustNotKeyTheTransmitter()
    {
        const quint8 sub0[5] = {0xFD, 0x00, 0x00, 0x00, 0xF1};  // HW-Version 0xF1
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const QByteArray pkt = makeEp6Frame(sub0, sub1);

        P1RadioConnection conn;
        conn.init();
        QSignalSpy pttSpy(&conn, &P1RadioConnection::micPttFromRadio);
        conn.parseEp6FrameForTest(pkt);

        QCOMPARE(pttSpy.count(), 1);
        QVERIFY2(pttSpy.at(0).at(0).toBool() == false,
                 "Eine I2C-Antwort mit ungerader Adresse wurde als PTT gelesen");
    }

    // Dasselbe im zweiten Unterrahmen — beide werden geodert, also muss
    // auch der zweite die Marke beachten.
    void theSecondSubframeCountsToo()
    {
        const quint8 sub0[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        const quint8 sub1[5] = {0xFD, 0x00, 0x00, 0x00, 0xF1};
        P1RadioConnection conn;
        conn.init();
        QSignalSpy pttSpy(&conn, &P1RadioConnection::micPttFromRadio);
        conn.parseEp6FrameForTest(makeEp6Frame(sub0, sub1));
        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).toBool(), false);
    }

    // Und die Gegenprobe: eine ECHTE Sendetaste (kein Bit 7) muss
    // weiterhin durchkommen, sonst waere der Fussschalter tot.
    void arealPttStillGetsThrough()
    {
        const quint8 sub0[5] = {0x01, 0x00, 0x00, 0x00, 0x00};  // PTT gedrueckt
        const quint8 sub1[5] = {0x00, 0x00, 0x00, 0x00, 0x00};
        P1RadioConnection conn;
        conn.init();
        QSignalSpy pttSpy(&conn, &P1RadioConnection::micPttFromRadio);
        conn.parseEp6FrameForTest(makeEp6Frame(sub0, sub1));
        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).toBool(), true);
    }

    // Eine Antwort mit GERADER Adresse hat Bit 0 ohnehin nicht gesetzt —
    // sie darf ebenfalls kein PTT ergeben, und der Rahmen daneben schon.
    void anEvenAddressAnswerBesideARealPtt()
    {
        const quint8 sub0[5] = {0xFC, 0x00, 0x00, 0x00, 0xF1};  // Antwort, Adresse 0x7C
        const quint8 sub1[5] = {0x01, 0x00, 0x00, 0x00, 0x00};  // echtes PTT
        P1RadioConnection conn;
        conn.init();
        QSignalSpy pttSpy(&conn, &P1RadioConnection::micPttFromRadio);
        conn.parseEp6FrameForTest(makeEp6Frame(sub0, sub1));
        QCOMPARE(pttSpy.count(), 1);
        QCOMPARE(pttSpy.at(0).at(0).toBool(), true);
    }
};

QTEST_MAIN(TestP1I2cResponseIsNotPtt)
#include "tst_p1_i2c_response_is_not_ptt.moc"
