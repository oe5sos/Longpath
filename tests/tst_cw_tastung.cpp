// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_cw_tastung.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Die Steuerung zwischen HaliKey und Geraet
// (src/core/cw/CwTastung) gegen ein Attrappen-Tastziel: was hinausgeht,
// in welcher Reihenfolge -- und vor allem, was NICHT hinausgeht.
//
// Kein MIDI-Port und keine Tonausgabe (`setOhneGeraete`); der Sender
// wird hier nie beruehrt, das Ziel ist eine Liste von Zeilen.
//
// Modification history (Longpath):
//   2026-11-17 - Created for Longpath by Martin Fischer (OE5SOS),
//                AI-assisted via Anthropic Claude.
// =================================================================
// no-port-check: Longpath-original test.

#include <QtTest>

#include <QSignalSpy>

#include "core/AppSettings.h"
#include "core/cw/CwMithoerton.h"
#include "core/cw/CwTastung.h"

using namespace Longpath;

namespace {

// Das Geraet als Mitschrift. `erlaubt` ist die Sendefreigabe: sagt sie
// nein, bleibt MOX aus -- wie bei einer Ablehnung durch den Bandplan.
struct Attrappe : CwTastziel {
    bool bereitJa{true};
    bool erlaubt{true};
    bool mox{false};
    QStringList zeilen;

    bool bereit(QString* warum) const override
    {
        if (!bereitJa && warum != nullptr) { *warum = QStringLiteral("not in CW."); }
        return bereitJa;
    }
    bool sendet() const override { return mox; }
    bool freigabeSteht() const override { return mox; }
    void fordereSenden(bool an) override
    {
        zeilen << (an ? QStringLiteral("senden 1") : QStringLiteral("senden 0"));
        mox = an && erlaubt;
    }
    void taste(bool unten) override
    {
        zeilen << (unten ? QStringLiteral("taste 1") : QStringLiteral("taste 0"));
    }
    int zahl(const QString& zeile) const { return int(zeilen.count(zeile)); }
};

constexpr int kNachlauf = 100;   // die untere Grenze, damit der Lauf kurz bleibt

} // namespace

class TstCwTastung : public QObject {
    Q_OBJECT

    // Eine eingeschaltete Tastung ohne Geraete, an der Attrappe.
    static void richteEin(CwTastung& t, Attrappe& a, bool senderTasten,
                          CwTastung::Art art = CwTastung::Art::Handtaste)
    {
        t.setOhneGeraete(true);
        t.setZiel(&a);
        t.setArt(art);
        t.setNachlaufMs(kNachlauf);
        t.setEingeschaltet(true);
        t.setSenderTasten(senderTasten);
        a.zeilen.clear();
    }

private slots:
    void init() { AppSettings::instance().clear(); }

    // ── Ab Werk ──────────────────────────────────────────────────────
    void abWerkIstAllesAus()
    {
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        QVERIFY(!t.eingeschaltet());
        QVERIFY(!t.senderTasten());
        QCOMPARE(t.art(), CwTastung::Art::JambusB);
        QCOMPARE(t.noten().ptt, 31);
    }

    // ── Ueben: nichts geht hinaus ────────────────────────────────────
    void uebenSchicktNichtsHinaus()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, /*senderTasten=*/false);
        t.kante(true);
        t.kante(false);
        t.pttGeaendert(true);
        t.pttGeaendert(false);
        QTest::qWait(kNachlauf + 80);
        QVERIFY2(a.zeilen.isEmpty(), qPrintable(a.zeilen.join(QStringLiteral(", "))));
        QVERIFY(!t.tasteUnten());
    }

    // Ausgeschaltet erst recht nicht -- auch wenn „den Sender tasten"
    // noch von frueher steht.
    void ausgeschaltetSchicktNichtsHinaus()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, /*senderTasten=*/true);
        t.setEingeschaltet(false);
        a.zeilen.clear();
        t.kante(true);
        t.pttGeaendert(true);
        QVERIFY2(a.zeilen.isEmpty(), qPrintable(a.zeilen.join(QStringLiteral(", "))));
    }

    // ── Die Sendefreigabe sagt nein ──────────────────────────────────
    void verweigertesSendenTastetNie()
    {
        CwTastung t;
        Attrappe a;
        a.erlaubt = false;
        richteEin(t, a, true);
        for (int i = 0; i < 5; ++i) {
            t.kante(true);
            t.kante(false);
        }
        QCOMPARE(a.zahl(QStringLiteral("taste 1")), 0);
        QVERIFY(a.zahl(QStringLiteral("senden 1")) >= 1);
        QVERIFY(!t.tasteUnten());
        // Und es wird nichts „zurueckgegeben", was nie unseres war.
        QTest::qWait(kNachlauf + 80);
        QCOMPARE(a.zahl(QStringLiteral("senden 0")), 0);
    }

    // Nicht bereit (kein CW, keine Flex): nicht einmal die Anfrage.
    void nichtBereitFragtNichtEinmal()
    {
        CwTastung t;
        Attrappe a;
        a.bereitJa = false;
        richteEin(t, a, true);
        t.kante(true);
        t.kante(false);
        QVERIFY2(a.zeilen.isEmpty(), qPrintable(a.zeilen.join(QStringLiteral(", "))));
        QVERIFY(t.zustandsText().contains(QStringLiteral("not in CW.")));
    }

    // ── Tasten und Nachlauf ──────────────────────────────────────────
    void tastenDannNachlauf()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true);
        QSignalSpy spion(&t, &CwTastung::tasteGeaendert);

        t.kante(true);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("taste 1")}));
        QVERIFY(t.tasteUnten());
        t.kante(false);
        QVERIFY(!t.tasteUnten());
        // Das naechste Zeichen im Nachlauf: tasten, ohne neu zu fragen.
        t.kante(true);
        t.kante(false);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("taste 1"),
                                        QStringLiteral("taste 0"), QStringLiteral("taste 1"),
                                        QStringLiteral("taste 0")}));
        QCOMPARE(spion.count(), 4);
        // Nach dem Nachlauf geht das Senden zurueck -- einmal.
        QTRY_COMPARE_WITH_TIMEOUT(a.zahl(QStringLiteral("senden 0")), 1, 2000);
        QVERIFY(!a.mox);
        QTest::qWait(kNachlauf + 80);
        QCOMPARE(a.zahl(QStringLiteral("senden 0")), 1);
    }

    // Stand das Senden schon (MOX von Hand), wird getastet -- aber das
    // Senden gehoert nicht uns und bleibt stehen.
    void fremdesSendenBleibtStehen()
    {
        CwTastung t;
        Attrappe a;
        a.mox = true;
        richteEin(t, a, true);
        t.kante(true);
        t.kante(false);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("taste 1"), QStringLiteral("taste 0")}));
        QTest::qWait(kNachlauf + 120);
        QCOMPARE(a.zahl(QStringLiteral("senden 0")), 0);
        QVERIFY(a.mox);
    }

    // ── Das Senden faellt von aussen ─────────────────────────────────
    void sendenBeendetNimmtDieTasteHoch()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true);
        t.kante(true);
        QVERIFY(t.tasteUnten());

        a.mox = false;          // MOX aus, Stehwelle, Verbindungsende
        t.sendenBeendet();
        QVERIFY(!t.tasteUnten());
        QCOMPARE(a.zeilen.last(), QStringLiteral("taste 0"));
        QVERIFY(t.gesperrt());

        // Die Hand gibt weiter: es wird NICHT wieder angefragt.
        a.zeilen.clear();
        t.kante(false);
        t.kante(true);
        t.kante(false);
        t.kante(true);
        QVERIFY2(a.zeilen.isEmpty(), qPrintable(a.zeilen.join(QStringLiteral(", "))));

        // Erst nach dem Loslassen (ein Nachlauf lang Ruhe) geht es wieder.
        t.kante(false);
        QTRY_VERIFY_WITH_TIMEOUT(!t.gesperrt(), 2000);
        t.kante(true);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("taste 1")}));
    }

    // Ein Ende, das zu spaet gemeldet wird, waehrend laengst wieder
    // gesendet wird, nimmt nichts hoch.
    void verspaetetesEndeStoertNicht()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true);
        t.kante(true);
        t.sendenBeendet();      // a.mox steht noch
        QVERIFY(t.tasteUnten());
        QVERIFY(!t.gesperrt());
    }

    // ── Die Wache ────────────────────────────────────────────────────
    void wacheNimmtDieHaengendeTasteHoch()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true);
        t.setWacheMsFuerPruefstand(80);
        t.kante(true);
        QVERIFY(t.tasteUnten());
        QTRY_VERIFY_WITH_TIMEOUT(!t.tasteUnten(), 2000);
        QCOMPARE(a.zahl(QStringLiteral("taste 0")), 1);
        // Und das Senden geht den gewohnten Weg zurueck.
        QTRY_COMPARE_WITH_TIMEOUT(a.zahl(QStringLiteral("senden 0")), 1, 2000);
    }

    void wacheIstFuenfzehnSekunden()
    {
        QCOMPARE(CwTastung::kHoechstensUntenMs, 15000);
        QCOMPARE(CwTastung::kPttHoechstensMs, 600000);
    }

    // ── Der Fusstaster ───────────────────────────────────────────────
    // Note 31 haelt das Senden, in jeder Tastenart -- und fragt NICHT
    // nach CW (bereit() ist hier nein).
    void fusstasterHaeltDasSenden()
    {
        CwTastung t;
        Attrappe a;
        a.bereitJa = false;     // SSB an irgendeinem Geraet
        richteEin(t, a, true, CwTastung::Art::JambusB);
        t.eingang()->verarbeite({0x90, 31, 100});
        QTRY_VERIFY_WITH_TIMEOUT(t.pttGehalten(), 2000);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1")}));
        QTest::qWait(kNachlauf + 80);   // kein Nachlauf laesst ihn fallen
        QVERIFY(a.mox);
        t.eingang()->verarbeite({0x80, 31, 0});
        QTRY_VERIFY_WITH_TIMEOUT(!t.pttGehalten(), 2000);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("senden 0")}));
        QCOMPARE(a.zahl(QStringLiteral("taste 1")), 0);
    }

    // In der Art „Fusstaster" ist auch die Tastenbuchse PTT: Note 20
    // (Spitze) und Note 30 -- je nachdem, wo der Stecker sitzt.
    void fusstasterAnDerTastenbuchse_data()
    {
        QTest::addColumn<int>("note");
        QTest::newRow("Note 20, Spitze") << 20;
        QTest::newRow("Note 21, Ring") << 21;
        QTest::newRow("Note 30") << 30;
    }
    void fusstasterAnDerTastenbuchse()
    {
        QFETCH(int, note);
        CwTastung t;
        Attrappe a;
        a.bereitJa = false;
        richteEin(t, a, true, CwTastung::Art::Fusstaster);
        const auto n = static_cast<unsigned char>(note);
        t.eingang()->verarbeite({0x90, n, 100});
        QTRY_VERIFY_WITH_TIMEOUT(t.pttGehalten(), 2000);
        QVERIFY(!t.ton()->taste());     // kein Ton unter dem Fuss
        t.eingang()->verarbeite({0x90, n, 0});   // Loslassen als Anschlag 0
        QTRY_VERIFY_WITH_TIMEOUT(!t.pttGehalten(), 2000);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("senden 0")}));
    }

    void fusstasterVerweigert()
    {
        CwTastung t;
        Attrappe a;
        a.erlaubt = false;
        richteEin(t, a, true, CwTastung::Art::Fusstaster);
        t.pttGeaendert(true);
        QVERIFY(!t.pttGehalten());
        t.pttGeaendert(false);
        // Angefragt, abgelehnt -- und nichts „zurueckgegeben".
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1")}));
    }

    // Faellt das Senden von aussen, waehrend der Fuss noch unten ist,
    // holt er es nicht zurueck. Erst der naechste Tritt sendet wieder.
    void fusstasterHoltNichtZurueck()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true, CwTastung::Art::Fusstaster);
        t.pttGeaendert(true);
        QVERIFY(t.pttGehalten());
        a.mox = false;
        t.sendenBeendet();
        QVERIFY(!t.pttGehalten());
        a.zeilen.clear();
        t.pttGeaendert(false);   // der Fuss geht hoch: nichts mehr zu tun
        QVERIFY2(a.zeilen.isEmpty(), qPrintable(a.zeilen.join(QStringLiteral(", "))));
        t.pttGeaendert(true);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1")}));
    }

    void fusstasterWache()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true, CwTastung::Art::Fusstaster);
        t.setPttWacheMsFuerPruefstand(80);
        t.pttGeaendert(true);
        QVERIFY(t.pttGehalten());
        QTRY_VERIFY_WITH_TIMEOUT(!t.pttGehalten(), 2000);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("senden 0")}));
    }

    // Fuss unten, dazu die Morsetaste: getastet wird ohne neue Anfrage,
    // und das Senden faellt erst mit dem Fuss -- Taste zuerst.
    void fusstasterUndTaste()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true, CwTastung::Art::Handtaste);
        t.pttGeaendert(true);
        t.kante(true);
        t.kante(false);
        QTest::qWait(kNachlauf + 80);
        QCOMPARE(a.zahl(QStringLiteral("senden 0")), 0);
        t.kante(true);
        t.pttGeaendert(false);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("taste 1"),
                                        QStringLiteral("taste 0"), QStringLiteral("taste 1"),
                                        QStringLiteral("taste 0"), QStringLiteral("senden 0")}));
    }

    // ── Ausschalten laesst los ───────────────────────────────────────
    void ausschaltenLaesstLos()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true);
        t.kante(true);
        t.setSenderTasten(false);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("taste 1"),
                                        QStringLiteral("taste 0"), QStringLiteral("senden 0")}));
        QVERIFY(!t.tasteUnten());
    }

    void trennenLaesstLosUndVergisstDasZiel()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true);
        t.kante(true);
        t.trenneZiel();
        QCOMPARE(a.zeilen.last(), QStringLiteral("senden 0"));
        a.zeilen.clear();
        t.kante(true);
        QVERIFY(a.zeilen.isEmpty());
    }

    // ── Der Mithoerton kommt vom Faden, sofort ───────────────────────
    void mithoertonSofortUndOhneGeraet()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, /*senderTasten=*/false, CwTastung::Art::Handtaste);
        t.eingang()->verarbeite({0x90, 30, 100});
        QVERIFY(t.ton()->taste());      // ohne einen Durchlauf der Ereignisschleife
        t.eingang()->verarbeite({0x80, 30, 0});
        QVERIFY(!t.ton()->taste());
        QCoreApplication::processEvents();
        QVERIFY(a.zeilen.isEmpty());

        t.setMithoerton(false);
        t.eingang()->verarbeite({0x90, 30, 100});
        QVERIFY(!t.ton()->taste());
        t.eingang()->verarbeite({0x80, 30, 0});
    }

    // Die ganze Kette: Note -> Senke -> Schlange -> Ziel. In der Art
    // „Handtaste" ist auch Note 20 die Taste (Handbuch).
    void handtasteVonDerNoteBisZumZiel()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true, CwTastung::Art::Handtaste);
        t.eingang()->verarbeite({0x90, 20, 100});
        QTRY_VERIFY_WITH_TIMEOUT(t.tasteUnten(), 2000);
        t.eingang()->verarbeite({0x80, 20, 0});
        QTRY_VERIFY_WITH_TIMEOUT(!t.tasteUnten(), 2000);
        QCOMPARE(a.zeilen, (QStringList{QStringLiteral("senden 1"), QStringLiteral("taste 1"),
                                        QStringLiteral("taste 0")}));
    }

    // Und mit dem Geber dazwischen: ein Punkt ist eine Kante hinunter
    // und eine hinauf. Bei 5 WpM (Punkt und Pause je 240 ms) und mit dem
    // Loslassen erst NACH der ersten Kante -- ein belasteter CI-Laeufer
    // haelt einen Faden leicht 100 ms an, und bei hohem Tempo waere dann
    // schon der zweite Punkt unterwegs.
    void paddleUeberDenGeber()
    {
        CwTastung t;
        Attrappe a;
        richteEin(t, a, true, CwTastung::Art::JambusA);
        t.setWpm(5);
        t.eingang()->verarbeite({0x90, 20, 100});
        QTRY_COMPARE_WITH_TIMEOUT(a.zahl(QStringLiteral("taste 1")), 1, 5000);
        t.eingang()->verarbeite({0x80, 20, 0});
        QTRY_COMPARE_WITH_TIMEOUT(a.zahl(QStringLiteral("taste 0")), 1, 5000);
        QTest::qWait(600);   // es kommt kein zweiter Punkt nach
        QCOMPARE(a.zahl(QStringLiteral("taste 1")), 1);
        QCOMPARE(a.zahl(QStringLiteral("taste 0")), 1);
    }

    // ── Einstellungen ────────────────────────────────────────────────
    void einstellungenUeberlebenDenNeustart()
    {
        {
            CwTastung t;
            t.setOhneGeraete(true);
            t.setEingeschaltet(true);
            t.setSenderTasten(true);
            t.setArt(CwTastung::Art::Fusstaster);
            t.setWpm(28);
            t.setVertauscht(true);
            t.setNachlaufMs(450);
            t.setMithoerton(false);
            t.setTonhoeheHz(700);
            t.setTonLautstaerke(25);
            t.setPortWunsch(QStringLiteral("HaliKey MIDI"));
            MidiKeyInput::Noten n;
            n.ptt = 40;
            t.setNoten(n);
        }
        const AppSettings& s = AppSettings::instance();
        QCOMPARE(s.value(QStringLiteral("CwKey/Enabled")).toString(), QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("CwKey/KeyTransmitter")).toString(), QStringLiteral("True"));
        QCOMPARE(s.value(QStringLiteral("CwKey/Sidetone")).toString(), QStringLiteral("False"));

        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        QVERIFY(t.eingeschaltet());
        QVERIFY(t.senderTasten());
        QCOMPARE(t.art(), CwTastung::Art::Fusstaster);
        QCOMPARE(t.wpm(), 28);
        QVERIFY(t.vertauscht());
        QCOMPARE(t.nachlaufMs(), 450);
        QVERIFY(!t.mithoerton());
        QCOMPARE(t.tonhoeheHz(), 700);
        QCOMPARE(t.tonLautstaerke(), 25);
        QCOMPARE(t.portWunsch(), QStringLiteral("HaliKey MIDI"));
        QCOMPARE(t.noten().ptt, 40);
        QCOMPARE(t.noten().punkt, 20);
    }

    void unsinnInDenEinstellungen()
    {
        AppSettings& s = AppSettings::instance();
        s.setValue(QStringLiteral("CwKey/Mode"), QStringLiteral("17"));
        s.setValue(QStringLiteral("CwKey/Wpm"), QStringLiteral("900"));
        s.setValue(QStringLiteral("CwKey/HangMs"), QStringLiteral("abc"));
        s.setValue(QStringLiteral("CwKey/NotePtt"), QStringLiteral("300"));
        CwTastung t;
        t.setOhneGeraete(true);
        t.lade();
        QCOMPARE(t.art(), CwTastung::Art::Fusstaster);   // auf die Grenze geklemmt
        QCOMPARE(t.wpm(), CwTastung::kWpmMax);
        QCOMPARE(t.nachlaufMs(), 600);
        QCOMPARE(t.noten().ptt, 127);
    }
};

QTEST_GUILESS_MAIN(TstCwTastung)
#include "tst_cw_tastung.moc"
