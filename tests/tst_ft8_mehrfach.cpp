// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf. Die eine MSHV-Nennung ist
// eine Herkunftsangabe im Kommentar, kein uebersetzter Quelltext.
//
// ── Mehrere QSOs gleichzeitig: MSHVs Multi Answering, Stufe 2 ──────────
//
// Stufe 1 (PR #323) legt mehrere Nachrichten in EIN Sendefenster.
// Stufe 2 ist die Buchhaltung davor: wer steht auf welchem Platz, wie
// kommt er darauf, wann geht er herunter, und wer rueckt nach.
//
// Das Modell stammt aus MSHV `src/HvTxW/hvmultianswermodw.h` [2.76.7,
// LZ2HV]: zwei Listen (`LsQueue` wartend, `LsNow` laufend) und
// `#define MAXSL 6` als Obergrenze.
//
// Der Hauptfall faehrt DREI vollstaendige QSOs nebeneinander durch --
// derselbe Gedanke wie beim RTTY-Rundlauf und beim Zweiton: ein
// Koordinator laesst sich nicht daran pruefen, dass er Zaehler
// hochzaehlt, sondern daran, dass am Ende drei fertige QSOs dastehen
// und jedes genau einmal.
//
// Gegenprobe (2026-10-24): mit nur EINEM Platz (`setPlaetze(1)`) ist
// `dreiGleichzeitigDurch` rot -- die beiden anderen warten noch, statt
// fertig zu sein. Genau das war der Stand vor Stufe 2.

#include <QtTest>

#include "core/Ft8Mehrfach.h"

using namespace Longpath;
using Z = Ft8Ablauf::Zustand;

namespace {
// QStringLiteral braucht ein echtes Literal; darum hier Funktionen
// statt constexpr-Zeiger.
QString ich()     { return QStringLiteral("OE5SOS"); }
QString locator() { return QStringLiteral("JN67VV"); }
}

class TstFt8Mehrfach : public QObject
{
    Q_OBJECT

private:
    static Ft8Mehrfach frisch(int plaetze)
    {
        Ft8Mehrfach m;
        m.setEigeneDaten(ich(), locator());
        m.setPlaetze(plaetze);
        return m;
    }

private slots:

    // ── Die Obergrenze steht in MSHV, nicht in meinem Kopf ───────────
    void dieObergrenzeIstSechs()
    {
        QCOMPARE(Ft8Mehrfach::kMaxPlaetze, 6);

        Ft8Mehrfach m = frisch(3);
        m.setPlaetze(99);
        QCOMPARE(m.plaetze(), 6);     // geklemmt, nicht abgelehnt
        m.setPlaetze(0);
        QCOMPARE(m.plaetze(), 1);
    }

    // ── Der Hauptfall ────────────────────────────────────────────────
    void dreiGleichzeitigDurch()
    {
        Ft8Mehrfach m = frisch(3);

        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("G0XYZ"),  -7));
        QVERIFY(m.nimmAnrufer(QStringLiteral("F5QRP"),  -18));
        QCOMPARE(m.aktive(), 3);
        QCOMPARE(m.wartende(), 0);

        // Erstes Fenster: drei Anrufe, auf drei Toenen mit 60 Hz
        // Abstand (MSHV gen_ft8.cpp:262).
        auto raus = m.naechsteNachrichten();
        QCOMPARE(raus.size(), 3);
        QCOMPARE(raus[0].text, QStringLiteral("DL1ABC OE5SOS JN67"));
        QCOMPARE(raus[1].text, QStringLiteral("G0XYZ OE5SOS JN67"));
        QCOMPARE(raus[2].text, QStringLiteral("F5QRP OE5SOS JN67"));
        QCOMPARE(raus[1].toneHz - raus[0].toneHz, 60.0f);
        QCOMPARE(raus[2].toneHz - raus[1].toneHz, 60.0f);

        // Alle drei antworten mit ihrem Rapport -- durcheinander, wie
        // auf dem Band. Jeder Ablauf nimmt sich das Seine.
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS G0XYZ -05"),  -7), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS F5QRP -20"), -18), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12), 1);

        raus = m.naechsteNachrichten();
        QCOMPARE(raus.size(), 3);
        QCOMPARE(raus[0].text, QStringLiteral("DL1ABC OE5SOS R-12"));

        // Alle drei schliessen ab.
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -12), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS G0XYZ RR73"),   -7), 1);
        QCOMPARE(m.verarbeite(QStringLiteral("OE5SOS F5QRP RR73"),  -18), 1);

        // Mein 73 geht hinaus, danach sind alle drei vollstaendig.
        QCOMPARE(m.naechsteNachrichten().size(), 3);
        m.fensterVerstrichen();

        const QVector<Ft8Ablauf> fertig = m.nimmFertige();
        QCOMPARE(fertig.size(), 3);
        QStringList partner;
        for (const Ft8Ablauf& a : fertig) {
            QCOMPARE(a.zustand(), Z::Fertig);
            partner << a.partner();
        }
        partner.sort();
        QCOMPARE(partner, QStringList({QStringLiteral("DL1ABC"),
                                       QStringLiteral("F5QRP"),
                                       QStringLiteral("G0XYZ")}));

        // Genau EINMAL. Beim zweiten Griff ist nichts mehr da.
        QCOMPARE(m.nimmFertige().size(), 0);
        QCOMPARE(m.aktive(), 0);
    }

    // ── Die Warteschlange ────────────────────────────────────────────
    void ueberzaehligeWartenUndRueckenNach()
    {
        Ft8Mehrfach m = frisch(2);

        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("G0XYZ"),  -7));
        QVERIFY(m.nimmAnrufer(QStringLiteral("F5QRP"), -18));   // zu viel

        QCOMPARE(m.aktive(), 2);
        QCOMPARE(m.wartende(), 1);
        QVERIFY(m.istVoll());

        // Der Wartende belegt KEINEN Ton, solange er wartet -- sonst
        // saesse er im Fenster, ohne dass jemand mit ihm spricht.
        QCOMPARE(m.naechsteNachrichten().size(), 2);

        // Einer wird fertig.
        m.verarbeite(QStringLiteral("OE5SOS DL1ABC -09"), -12);
        m.verarbeite(QStringLiteral("OE5SOS DL1ABC RR73"), -12);
        m.fensterVerstrichen();     // 73 hinaus -> fertig -> Platz frei

        QCOMPARE(m.nimmFertige().size(), 1);
        QCOMPARE(m.wartende(), 0);
        QCOMPARE(m.aktive(), 2);        // F5QRP ist nachgerueckt

        bool f5Dabei = false;
        for (const auto& b : m.belegung()) {
            if (b.partner == QStringLiteral("F5QRP")) { f5Dabei = true; }
        }
        QVERIFY2(f5Dabei, "F5QRP ist nicht nachgerueckt");
    }

    // ── Dasselbe Rufzeichen nicht zweimal ────────────────────────────
    //
    // Ohne diese Sperre antwortete man demselben im selben Fenster auf
    // zwei Toenen und zaehlte am Ende zwei QSOs.
    void dasselbeRufzeichenNurEinmal()
    {
        Ft8Mehrfach m = frisch(3);
        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(!m.nimmAnrufer(QStringLiteral("DL1ABC"), -14));
        QVERIFY(!m.nimmAnrufer(QStringLiteral("dl1abc"), -14));  // Schreibweise egal
        QCOMPARE(m.aktive(), 1);

        // Auch in der Warteschlange gilt es.
        Ft8Mehrfach eng = frisch(1);
        QVERIFY(eng.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(eng.nimmAnrufer(QStringLiteral("G0XYZ"), -7));
        QVERIFY(!eng.nimmAnrufer(QStringLiteral("G0XYZ"), -9));
        QCOMPARE(eng.wartende(), 1);
    }

    // ── Ein Platz, der aufgibt, blockiert nicht ──────────────────────
    void abgebrochenerPlatzWirdFrei()
    {
        Ft8Mehrfach m = frisch(1);
        QVERIFY(m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QVERIFY(m.nimmAnrufer(QStringLiteral("G0XYZ"), -7));
        QCOMPARE(m.wartende(), 1);

        // DL1ABC antwortet nie. Nach kMaxWiederholungen gibt der
        // Ablauf auf, und der Platz muss frei werden.
        for (int i = 0; i <= Ft8Ablauf::kMaxWiederholungen + 1; ++i) {
            m.fensterVerstrichen();
        }

        QCOMPARE(m.wartende(), 0);
        QCOMPARE(m.aktive(), 1);
        QCOMPARE(m.belegung().first().partner, QStringLiteral("G0XYZ"));
        // Ein aufgegebener Ablauf ist KEIN fertiges QSO.
        QCOMPARE(m.nimmFertige().size(), 0);
    }

    // ── Ohne eigenes Rufzeichen geht gar nichts ──────────────────────
    void ohneEigenesRufzeichenKeinPlatz()
    {
        Ft8Mehrfach m;
        m.setPlaetze(3);
        QVERIFY(!m.nimmAnrufer(QStringLiteral("DL1ABC"), -12));
        QCOMPARE(m.aktive(), 0);
    }
};

QTEST_MAIN(TstFt8Mehrfach)
#include "tst_ft8_mehrfach.moc"
