// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_sunsdr_ein_steller.cpp  (Longpath)
// =================================================================
// Vorverstaerker und Daempfung sind an der SunSDR2 QRP EIN Steller.
//
// Beide Rahmenbauer schreiben Opcode 0x04 und legen den Stufenindex an
// dieselbe Stelle (Byte 18): 00/01/02/03 = -20/-10/0/+10 dB. Wer eines
// von beiden bewegt, verstellt das andere mit -- und die Anzeige des
// anderen stimmt danach nicht mehr.
//
// Das stand am 2026-10-11 nur im Kommentar von attenuatorFrameFor()
// ("Nutzlast = Stufenindex 00/01/02 = -20/-10/0 dB"), ohne dass
// irgendwo aufgeschrieben war, was daraus folgt. Gemessen wurde es
// dann am Geraet mit Antenne auf 14,200 MHz, am Rauschflur im I/Q:
// nach "Vorverstaerker +10 dB" (-68,0 dBFS) fiel der Pegel durch
// setAttenuator(0) auf -83,6 dBFS, also auf den Pegel von
// "Vorverstaerker 0 dB" (-81,9 dBFS).
//
// Dieser Pruefstand haelt den Befund im Code fest, ohne Geraet: wer die
// Zuordnung aendert, muss hier vorbeikommen und findet die Begruendung.
//
// Modification history (Longpath):
//   2026-10-12 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include "core/SunSdrRadioConnection.h"

using namespace Longpath;

namespace {

int opcodeVon(const QByteArray& f) { return f.size() > 2 ? int(quint8(f.at(2))) : -1; }
int stufeVon(const QByteArray& f)  { return f.size() > 18 ? int(quint8(f.at(18))) : -1; }

} // namespace

class TestSunSdrEinSteller : public QObject
{
    Q_OBJECT
private slots:
    void beideSchreibenDenselbenOpcode()
    {
        for (const int idx : {0, 1, 2, 7}) {
            const QByteArray f = SunSdrRadioConnection::preampFrameFor(idx);
            QVERIFY2(!f.isEmpty(), qPrintable(QStringLiteral("Stufe %1").arg(idx)));
            QCOMPARE(opcodeVon(f), 0x04);
        }
        for (const int dB : {0, -10, -20}) {
            const QByteArray f = SunSdrRadioConnection::attenuatorFrameFor(dB);
            QVERIFY2(!f.isEmpty(), qPrintable(QStringLiteral("%1 dB").arg(dB)));
            QCOMPARE(opcodeVon(f), 0x04);
        }
    }

    void dieStufeIstDieselbe_data()
    {
        QTest::addColumn<int>("dB");
        QTest::addColumn<int>("preampIdx");
        QTest::addColumn<int>("stufe");
        QTest::newRow("0 dB")   <<   0 << 1 << 0x02;
        QTest::newRow("-10 dB") << -10 << 2 << 0x01;
        QTest::newRow("-20 dB") << -20 << 0 << 0x00;
    }

    void dieStufeIstDieselbe()
    {
        QFETCH(int, dB);
        QFETCH(int, preampIdx);
        QFETCH(int, stufe);
        const QByteArray att = SunSdrRadioConnection::attenuatorFrameFor(dB);
        const QByteArray pre = SunSdrRadioConnection::preampFrameFor(preampIdx);
        QVERIFY(!att.isEmpty() && !pre.isEmpty());
        QCOMPARE(stufeVon(att), stufe);
        QCOMPARE(stufeVon(pre), stufe);
        // Der eigentliche Punkt: dieselbe Stufe, nicht nur dieselbe Zahl
        // an verschiedenen Stellen.
        QCOMPARE(stufeVon(att), stufeVon(pre));
    }

    void dieDaempfungHatKeineEigeneStufe()
    {
        // Jede Stufe, die die Daempfung schreiben kann, kann auch der
        // Vorverstaerker schreiben. Gaebe es eine eigene, waere die
        // Annahme "ein Steller" falsch.
        QSet<int> ausDaempfung, ausVorverstaerker;
        for (const int dB : {0, -10, -20}) {
            ausDaempfung << stufeVon(SunSdrRadioConnection::attenuatorFrameFor(dB));
        }
        for (const int idx : {0, 1, 2, 3, 7}) {
            const QByteArray f = SunSdrRadioConnection::preampFrameFor(idx);
            if (!f.isEmpty()) { ausVorverstaerker << stufeVon(f); }
        }
        QVERIFY2(ausDaempfung.subtract(ausVorverstaerker).isEmpty(),
                 "die Daempfung schreibt eine Stufe, die der "
                 "Vorverstaerker nicht kennt — dann ist es nicht mehr "
                 "derselbe Steller");
    }

    void derMitschriebNenntDasUeberschreiben()
    {
        SunSdrRadioConnection conn;
        QCOMPARE(conn.stufe04ForTest(), -1);

        // Erst +10 dB ueber den Vorverstaerker.
        QTest::ignoreMessage(QtInfoMsg,
            "SunSdr: Stufe 0x04 steht auf +10 dB (gesetzt ueber "
            "Vorverstaerker) -- Vorverstaerker und Daempfung teilen "
            "diese eine Stufe.");
        conn.merkeStufe04ForTest(SunSdrRadioConnection::preampFrameFor(7),
                                 QStringLiteral("Vorverstaerker"));
        QCOMPARE(conn.stufe04ForTest(), 0x03);

        // Dann "Daempfung 0 dB" -- und genau das stellt die +10 dB
        // still auf 0 dB zurueck. Am Geraet gemessen: -68,0 -> -83,6 dBFS.
        QTest::ignoreMessage(QtInfoMsg,
            "SunSdr: Stufe 0x04 +10 dB -> 0 dB (ueber Daempfung) -- es ist "
            "DIESELBE Stufe fuer Vorverstaerker und Daempfung, die jeweils "
            "andere Anzeige stimmt danach nicht mehr.");
        conn.merkeStufe04ForTest(SunSdrRadioConnection::attenuatorFrameFor(0),
                                 QStringLiteral("Daempfung"));
        QCOMPARE(conn.stufe04ForTest(), 0x02);

        // Dieselbe Stufe zweimal sagt nichts Neues und darf das
        // Protokoll nicht fuellen.
        conn.merkeStufe04ForTest(SunSdrRadioConnection::preampFrameFor(1),
                                 QStringLiteral("Vorverstaerker"));
        QCOMPARE(conn.stufe04ForTest(), 0x02);
    }
};

QTEST_MAIN(TestSunSdrEinSteller)
#include "tst_sunsdr_ein_steller.moc"
