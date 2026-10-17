// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_sunsdr_uebersteuerung.cpp  (Longpath)
// =================================================================
// Schlaegt die Uebersteuerungserkennung ueberhaupt an?
//
// Die QRP meldet Uebersteuerung nicht selbst (P1 und P2 haben dafuer
// ein Statusbit, die QRP nicht). Longpath liest sie deshalb seit dem
// 2026-10-03 aus dem Signal: Proben am Anschlag zaehlen, vor der
// Pegelanhebung des Profils.
//
// Belegt war davon bisher nur die eine Haelfte — dass sie NICHT falsch
// alarmiert: am echten Geraet ueber Stunden 0 Proben am Anschlag, am
// 2026-10-11 auch mit Antenne auf 20 m und 40 m (Spitze bei
// Vorverstaerker +10 dB: -53,4 dBFS, also rund 53 dB Abstand zur
// Schwelle). Die andere Haelfte — dass sie anschlaegt, wenn es denn
// klemmt — liess sich am Geraet nicht zeigen: ein Signal, das den
// Wandler an den Anschlag treibt, kann man nicht bestellen.
//
// Hier wird sie darum mit selbstgebauten Proben gefahren. Geprueft
// werden die drei Entscheidungen, die sie trifft:
//   1. knapp unter der Schwelle ist KEIN Anschlag (0,998 gegen 0,999),
//   2. eine einzelne Probe am Anschlag zaehlt, meldet aber nicht
//      (kAnschlagSchwelleAnzahl = 8 -- ein einzelner Ausreisser ist
//      kein uebersteuerter Empfaenger),
//   3. acht Proben melden, und zwar nach OBEN (adcOverflow), nicht nur
//      in den eigenen Zaehler.
//
// Modification history (Longpath):
//   2026-10-13 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include <QtTest>

#include <QSignalSpy>

#include "core/SunSdrRadioConnection.h"

using namespace Longpath;

namespace {

QVector<float> proben(int anzahl, float wert)
{
    return QVector<float>(anzahl, wert);
}

} // namespace

class TestSunSdrUebersteuerung : public QObject
{
    Q_OBJECT
private slots:
    void knappDarunterIstKeinAnschlag()
    {
        SunSdrRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::adcOverflow);
        conn.pruefeAnschlagForTest(proben(4096, 0.998f));
        conn.pruefeAnschlagForTest(proben(4096, -0.998f));
        QCOMPARE(conn.anschlagProbenForTest(), quint64(0));
        QCOMPARE(conn.anschlagMeldungenForTest(), quint64(0));
        QCOMPARE(spy.count(), 0);
    }

    void einAusreisserZaehltAberMeldetNicht()
    {
        SunSdrRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::adcOverflow);
        QVector<float> s = proben(4096, 0.1f);
        s[1234] = 1.0f;
        conn.pruefeAnschlagForTest(s);
        QCOMPARE(conn.anschlagProbenForTest(), quint64(1));
        QCOMPARE(conn.anschlagMeldungenForTest(), quint64(0));
        QCOMPARE(spy.count(), 0);
    }

    void achtProbenMeldenNachOben()
    {
        SunSdrRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::adcOverflow);
        QVector<float> s = proben(4096, 0.1f);
        for (int i = 0; i < 8; ++i) { s[100 + i] = -1.0f; }   // beide Vorzeichen zaehlen
        conn.pruefeAnschlagForTest(s);
        QCOMPARE(conn.anschlagProbenForTest(), quint64(8));
        QCOMPARE(conn.anschlagMeldungenForTest(), quint64(1));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toInt(), 0);               // der Wandler ist einer
    }

    void dieMeldungKommtNichtInJedemBlock()
    {
        // Sonst stuende bei einem dauerhaft uebersteuerten Empfaenger
        // alle 5 ms eine Warnung im Protokoll. Abstand: 500 ms.
        SunSdrRadioConnection conn;
        QSignalSpy spy(&conn, &RadioConnection::adcOverflow);
        QVector<float> s = proben(1024, 1.0f);
        for (int i = 0; i < 20; ++i) { conn.pruefeAnschlagForTest(s); }
        QCOMPARE(conn.anschlagProbenForTest(), quint64(20 * 1024));
        QCOMPARE(conn.anschlagMeldungenForTest(), quint64(1));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestSunSdrUebersteuerung)
#include "tst_sunsdr_uebersteuerung.moc"
