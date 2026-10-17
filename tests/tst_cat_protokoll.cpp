// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die gepruefte Wortform stammt
// aus Thetis und ist in src/core/CatProtocol.{h,cpp} zitiert.
//
// Die CAT-Protokollschicht, erster Gang (2026-10-11).
//
// Longpath hatte keinen CAT-Server. Es gibt rotctld fuer den Rotor, einen
// TCI-Server -- aber nichts, womit WSJT-X, fldigi, N1MM oder das
// Contestprogramm das Geraet als Kenwood ansprechen koennten. Das
// CatApplet zeigt dafuer fuenf Felder, alle mit NyiOverlay("3K").
//
// Warum das hier so kleinteilig geprueft wird: ein CAT-Server, der falsch
// antwortet, bringt ein Fremdprogramm dazu, auf der FALSCHEN FREQUENZ zu
// senden. Jede Wortform ist darum gegen Thetis belegt und hier
// festgenagelt -- nicht aus dem Kenwood-Handbuch erinnert.

#include <QtTest/QtTest>

#include "core/CatProtocol.h"

using namespace Longpath;

class TstCatProtokoll : public QObject {
    Q_OBJECT

private:
    CatProtocol p;
    // Mitschrieb dessen, was die Haken zu sehen bekamen.
    quint64 gesetztA{0};
    quint64 gesetztB{0};
    int     gesetztModeK{0};
    int     moxRufe{0};
    bool    moxZuletzt{false};
    int     splitRufe{0};
    bool    splitZuletzt{false};

    void hakenAnhaengen()
    {
        CatProtocol::Haken h;
        h.setzeVfoA   = [this](quint64 hz) { gesetztA = hz; };
        h.setzeVfoB   = [this](quint64 hz) { gesetztB = hz; };
        h.setzeModeK  = [this](int k)      { gesetztModeK = k; };
        h.setzeMox    = [this](bool on)    { ++moxRufe; moxZuletzt = on; };
        h.setzeSplit  = [this](bool on)    { ++splitRufe; splitZuletzt = on; };
        p.setzeHaken(h);
    }

private slots:
    void initTestCase()
    {
        // PFLICHT, und zwar aus zwei Gruenden.
        //
        // 1. Ohne Testmodus liest und SCHREIBT AppSettings die echte
        //    Einstellungsdatei des Betreibers (~1,9 MB, 9531 Eintraege,
        //    sein Layoutprofil, seine Container). Ein Prueffall, der
        //    dort hineinschreibt, kann einen Arbeitsplatz verstellen.
        // 2. Dieser Prueffall baut MEHRERE MainWindow hintereinander.
        //    Mit der echten Datei beendete der zweite Aufbau den
        //    Prozess mit SIGTRAP (rc=133) -- die Fallbeschreibungen
        //    waren gruen bis dahin, es gab keine Fehlermeldung, nur
        //    einen stillen Abbruch nach dem vierten Fall.
        //
        // tst_rx_equalizer.cpp hat die Zeile von Anfang an; diese drei
        // CAT-Prueffaelle nicht, und das ist am 2026-10-11 aufgefallen.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        p = CatProtocol();
        gesetztA = gesetztB = 0;
        gesetztModeK = 0;
        moxRufe = splitRufe = 0;
        moxZuletzt = splitZuletzt = false;
        hakenAnhaengen();
    }

    // ── Zerlegung ───────────────────────────────────────────────────────────

    void zuKurzWirdAbgelehnt()
    {
        // From Thetis CATParser.cs:116-123 -- kuerzer als "aa;" ist kein
        // Befehl.
        QCOMPARE(p.antwortAuf(QStringLiteral("")),    QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("I")),   QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("IF")),  QStringLiteral("?;"));  // ohne ';'
        QCOMPARE(p.antwortAuf(QStringLiteral(";;")),  QStringLiteral("?;"));
    }

    void fuehrendeAbschlusszeichenWerdenAbgeworfen()
    {
        // From Thetis CATParser.cs:417-418 -- "If the command has a leading
        // terminator(s) (like sent by WriteLog) dump it". Ohne das faellt
        // jeder WriteLog-Befehl auf den Boden.
        QCOMPARE(p.antwortAuf(QStringLiteral(";ID;")), QStringLiteral("ID900;"));
        QCOMPARE(p.antwortAuf(QStringLiteral(";;;ID;")), QStringLiteral("ID900;"));
    }

    void kleinschreibungGehtAuch()
    {
        // CheckFormat vergleicht gross (ToUpper), also muss "id;" wirken.
        QCOMPARE(p.antwortAuf(QStringLiteral("id;")), QStringLiteral("ID900;"));
    }

    void einUnbekanntesPraefixWirdAbgelehnt()
    {
        // BEWUSSTE ABWEICHUNG von Thetis: dort fallen ungebaute Befehle
        // leer durch (leere Antwort). Hier `?;`, damit ein wartendes
        // Fremdprogramm nicht bis in seinen Zeitablauf haengt. Begruendung
        // steht im Code.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZ;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("QQ;")), QStringLiteral("?;"));
    }

    // ── Frequenz ────────────────────────────────────────────────────────────

    void frequenzAbfragenGibtElfStellen()
    {
        CatProtocol::Zustand z;
        z.vfoAHz = 14225000;
        z.vfoBHz = 7100000;
        p.setzeZustand(z);
        // 11 Stellen mit fuehrenden Nullen (CATStructs.xml: FA ans=11).
        QCOMPARE(p.antwortAuf(QStringLiteral("FA;")), QStringLiteral("FA00014225000;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FB;")), QStringLiteral("FB00007100000;"));
    }

    void frequenzSetzenWirdNichtQuittiert()
    {
        // Kenwood quittiert ein Setzen nicht. From CATParser.cs:397 --
        // `else if(nAns == -1 || rtncmd == "") rtncmd = "";`
        QCOMPARE(p.antwortAuf(QStringLiteral("FA00014250000;")), QString());
        QCOMPARE(gesetztA, Q_UINT64_C(14250000));
        QCOMPARE(p.antwortAuf(QStringLiteral("FB00003573000;")), QString());
        QCOMPARE(gesetztB, Q_UINT64_C(3573000));
    }

    void einSetzenFolgtDieAbfrage()
    {
        // Die Schicht fuehrt den eigenen Zustand nach, damit ein sofortiges
        // FA; nicht den alten Wert meldet -- genau das Muster, mit dem
        // WSJT-X nach dem Abstimmen zurueckliest.
        p.antwortAuf(QStringLiteral("FA00014250000;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FA;")), QStringLiteral("FA00014250000;"));
    }

    void eineFrequenzFalscherLaengeWirdAbgelehnt()
    {
        // nSet ist 11, nicht "irgendwas". Zehn Stellen sind ein anderer
        // Befehl in einem anderen Geraet, kein wohlmeinender Tippfehler.
        QCOMPARE(p.antwortAuf(QStringLiteral("FA0014250000;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FA000014250000;")), QStringLiteral("?;"));
        QCOMPARE(gesetztA, Q_UINT64_C(0));
    }

    void eineFrequenzMitBuchstabenWirdAbgelehnt()
    {
        QCOMPARE(p.antwortAuf(QStringLiteral("FA0001425xxxx;")), QStringLiteral("?;"));
        QCOMPARE(gesetztA, Q_UINT64_C(0));
    }

    void ohneHakenWirdAuchDasABFRAGENAbgelehnt()
    {
        // Nachgetragen am 2026-10-11, nachdem der LIVE-Lauf es zeigte:
        // Longpath antwortete auf FB; mit "FB00000000000;" -- eine
        // Frequenz fuer ein VFO B, das es nicht gibt. Der Haken ist das
        // Kennzeichen dafuer, dass ein VFO existiert, und muss darum
        // auch die Abfrage entscheiden.
        //
        // Warum kein Prueffall es gesehen hat: init() haengt hier IMMER
        // beide Haken an, und der Fall darunter prueft nur das SETZEN.
        // Erst am laufenden Programm, wo setzeVfoB absichtlich fehlt,
        // kam es heraus.
        CatProtocol::Zustand z;
        z.vfoAHz = 14225000;
        z.vfoBHz = 7100000;
        p.setzeZustand(z);

        CatProtocol::Haken nurA;
        nurA.setzeVfoA = [](quint64) {};
        p.setzeHaken(nurA);

        QCOMPARE(p.antwortAuf(QStringLiteral("FA;")),
                 QStringLiteral("FA00014225000;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FB;")), QStringLiteral("?;"));
        // Auch nicht mit einem Wert im Zustand -- der zaehlt nicht, wenn
        // der VFO nicht existiert.
        QCOMPARE(p.antwortAuf(QStringLiteral("FB00007100000;")),
                 QStringLiteral("?;"));
    }

    void ohneHakenWirdAbgelehntStattStillGeschluckt()
    {
        // Der wichtigste Fall dieser Datei. Ein Setzen, das quittiert
        // wird, ohne dass etwas geschieht, laesst das Fremdprogramm auf
        // der ALTEN Frequenz senden -- und niemand sieht es.
        p.setzeHaken(CatProtocol::Haken{});
        QCOMPARE(p.antwortAuf(QStringLiteral("FA00014250000;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("MD2;")),           QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("TX;")),            QStringLiteral("?;"));
    }

    // ── Modus ───────────────────────────────────────────────────────────────

    void dieModusZahlenSindKenwoodsNichtUnsere()
    {
        // From Thetis CATCommands.cs:9996-10039 Mode2KString [v2.10.3.15]:
        //   LSB=1 USB=2 CWU=3 FM=4 AM=5 DIGL=6 CWL=7 DIGU=9
        // (DIGL/DIGU geben 1/2, wenn Thetis' DigUIsUSB gesetzt ist -- das
        //  ist eine Einstellung des SENDERS, keine der Protokollschicht.)
        // Die 8 ist in Kenwoods Satz FSK-R und bei Thetis unbesetzt.
        for (int k : {1, 2, 3, 4, 5, 6, 7, 9}) {
            CatProtocol::Zustand z;
            z.modeK = k;
            p.setzeZustand(z);
            QCOMPARE(p.antwortAuf(QStringLiteral("MD;")),
                     QStringLiteral("MD%1;").arg(k));
        }
    }

    void einModusAusserhalbEinsBisNeunWirdAbgelehnt()
    {
        // From Thetis CATCommands.cs:573-580 -- `> 0 && <= 9`, sonst Error1.
        // Die 0 ist ausdruecklich KEIN gueltiger Modus.
        QCOMPARE(p.antwortAuf(QStringLiteral("MD0;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("MDA;")), QStringLiteral("?;"));
        QCOMPARE(gesetztModeK, 0);
        QCOMPARE(p.antwortAuf(QStringLiteral("MD7;")), QString());
        QCOMPARE(gesetztModeK, 7);
    }

    // ── Statussatz ──────────────────────────────────────────────────────────

    void derStatussatzIstFuenfunddreissigZeichenLang()
    {
        // Die Laenge ist nicht Kosmetik: jedes Fremdprogramm liest feste
        // Spalten. Ein Zeichen zu viel verschiebt ALLES dahinter.
        CatProtocol::Zustand z;
        z.vfoAHz = 14225000;
        p.setzeZustand(z);
        QCOMPARE(p.statussatz().size(), 35);
        const QString a = p.antwortAuf(QStringLiteral("IF;"));
        QCOMPARE(a.size(), 2 + 35 + 1);
        QVERIFY(a.startsWith(QStringLiteral("IF")));
        QVERIFY(a.endsWith(QStringLiteral(";")));
    }

    void derStatussatzFeldweise()
    {
        // From Thetis CATCommands.cs IF() [v2.10.3.15] -- die Reihenfolge:
        //   11 Freq · 4 Schritt · 6 RIT/XIT · 1 RIT · 1 XIT · 3 Bank
        //   · 1 TX · 1 Modus · 1 FR/FT · 1 Scan · 1 Split · 4 Balance
        CatProtocol::Zustand z;
        z.vfoAHz = 14225000;
        z.stepHz = 100;
        z.itHz   = 0;
        z.modeK  = 2;
        p.setzeZustand(z);
        const QString s = p.statussatz();
        QCOMPARE(s.mid(0, 11),  QStringLiteral("00014225000"));  // Frequenz
        QCOMPARE(s.mid(11, 4),  QStringLiteral("0010"));         // 100 Hz
        QCOMPARE(s.mid(15, 6),  QStringLiteral("+00000"));       // RIT/XIT-Wert
        QCOMPARE(s.mid(21, 1),  QStringLiteral("0"));            // RIT aus
        QCOMPARE(s.mid(22, 1),  QStringLiteral("0"));            // XIT aus
        QCOMPARE(s.mid(23, 3),  QStringLiteral("000"));          // Bank (blind)
        QCOMPARE(s.mid(26, 1),  QStringLiteral("0"));            // RX
        QCOMPARE(s.mid(27, 1),  QStringLiteral("2"));            // USB
        QCOMPARE(s.mid(28, 1),  QStringLiteral("0"));            // FR/FT (blind)
        QCOMPARE(s.mid(29, 1),  QStringLiteral("0"));            // Scan (blind)
        QCOMPARE(s.mid(30, 1),  QStringLiteral("0"));            // Split aus
        QCOMPARE(s.mid(31, 4),  QStringLiteral("0000"));         // Balance (blind)
    }

    void derStatussatzZeigtSendenUndSplit()
    {
        CatProtocol::Zustand z;
        z.vfoAHz = 14225000;
        z.mox    = true;
        z.split  = true;
        z.modeK  = 1;
        p.setzeZustand(z);
        const QString s = p.statussatz();
        QCOMPARE(s.mid(26, 1), QStringLiteral("1"));   // sendet
        QCOMPARE(s.mid(27, 1), QStringLiteral("1"));   // LSB
        QCOMPARE(s.mid(30, 1), QStringLiteral("1"));   // Split
    }

    void ritUndXitSchliessenSichAus()
    {
        // From Thetis CATCommands.cs IF(): `if(RITOn) rit="1"; else if(XITOn)
        // xit="1";` -- bei BEIDEN an meldet Thetis nur RIT. Nachgebaut, nicht
        // geglaettet: ein Programm, das beide Flaggen sieht, weiss nicht,
        // welchen Wert das IT-Feld traegt.
        CatProtocol::Zustand z;
        z.ritOn = true;
        z.xitOn = true;
        z.itHz  = -250;
        p.setzeZustand(z);
        const QString s = p.statussatz();
        QCOMPARE(s.mid(15, 6), QStringLiteral("-00250"));
        QCOMPARE(s.mid(21, 1), QStringLiteral("1"));   // RIT
        QCOMPARE(s.mid(22, 1), QStringLiteral("0"));   // XIT NICHT, obwohl an
    }

    void nurXitAnZeigtXit()
    {
        CatProtocol::Zustand z;
        z.xitOn = true;
        z.itHz  = 1234;
        p.setzeZustand(z);
        const QString s = p.statussatz();
        QCOMPARE(s.mid(15, 6), QStringLiteral("+01234"));
        QCOMPARE(s.mid(21, 1), QStringLiteral("0"));
        QCOMPARE(s.mid(22, 1), QStringLiteral("1"));
    }

    void ohneRitUndXitIstDerWertNull()
    {
        // Auch wenn itHz etwas anderes sagt: ist keine der beiden Flaggen
        // gesetzt, gehoert eine 0 ins Feld, sonst meldet das Geraet eine
        // Ablage, die nicht wirkt.
        CatProtocol::Zustand z;
        z.itHz = 500;
        p.setzeZustand(z);
        QCOMPARE(p.statussatz().mid(15, 6), QStringLiteral("+00000"));
    }

    void dasItFeldHatImmerEinVorzeichen()
    {
        QCOMPARE(CatProtocol::itFeld(0),      QStringLiteral("+00000"));
        QCOMPARE(CatProtocol::itFeld(1),      QStringLiteral("+00001"));
        QCOMPARE(CatProtocol::itFeld(-1),     QStringLiteral("-00001"));
        QCOMPARE(CatProtocol::itFeld(99999),  QStringLiteral("+99999"));
        // Ueberlauf gedeckelt -- sechs Zeichen bleiben sechs Zeichen,
        // sonst verschiebt sich der ganze Satz.
        QCOMPARE(CatProtocol::itFeld(123456).size(), 6);
        QCOMPARE(CatProtocol::itFeld(123456), QStringLiteral("+99999"));
        QCOMPARE(CatProtocol::itFeld(-123456), QStringLiteral("-99999"));
    }

    void einUnmoeglicherModusWirdZuUsb()
    {
        // From Thetis CATCommands.cs IF(): `if(tempmode == "?;") rtn += "2";`
        // Mode2KString gibt den Fehler fuer SAM, DRM, SPEC -- alles, was
        // Kenwood nicht kennt. Ein leeres Feld wuerde den Satz verkuerzen.
        CatProtocol::Zustand z;
        z.modeK = 42;
        p.setzeZustand(z);
        QCOMPARE(p.statussatz().size(), 35);
        QCOMPARE(p.statussatz().mid(27, 1), QStringLiteral("2"));
        z.modeK = 0;
        p.setzeZustand(z);
        QCOMPARE(p.statussatz().mid(27, 1), QStringLiteral("2"));
    }

    // ── Schrittfeld ─────────────────────────────────────────────────────────

    void dasSchrittfeldTraegtDieSiebenSonderfaelle()
    {
        // From Thetis CATCommands.cs Step2String [v2.10.3.15], am 2.10.3.9
        // von MW0LGE angepasst. Wer das auf "Zehnerpotenz" vereinfacht,
        // bricht genau diese sieben -- darum stehen sie hier einzeln.
        QCOMPARE(CatProtocol::schrittFeld(50),      QStringLiteral("1000"));
        QCOMPARE(CatProtocol::schrittFeld(250),     QStringLiteral("1001"));
        QCOMPARE(CatProtocol::schrittFeld(500),     QStringLiteral("1010"));
        QCOMPARE(CatProtocol::schrittFeld(5000),    QStringLiteral("1011"));
        QCOMPARE(CatProtocol::schrittFeld(9000),    QStringLiteral("1100"));
        QCOMPARE(CatProtocol::schrittFeld(250000),  QStringLiteral("1101"));
        QCOMPARE(CatProtocol::schrittFeld(500000),  QStringLiteral("1110"));
    }

    void dasSchrittfeldTraegtDieGewoehnlichen()
    {
        QCOMPARE(CatProtocol::schrittFeld(1),        QStringLiteral("0000"));
        QCOMPARE(CatProtocol::schrittFeld(10),       QStringLiteral("0001"));
        QCOMPARE(CatProtocol::schrittFeld(100),      QStringLiteral("0010"));
        QCOMPARE(CatProtocol::schrittFeld(1000),     QStringLiteral("0011"));
        QCOMPARE(CatProtocol::schrittFeld(10000),    QStringLiteral("0100"));
        QCOMPARE(CatProtocol::schrittFeld(100000),   QStringLiteral("0101"));
        QCOMPARE(CatProtocol::schrittFeld(1000000),  QStringLiteral("0110"));
        QCOMPARE(CatProtocol::schrittFeld(10000000), QStringLiteral("0111"));
    }

    void einUnbekannterSchrittFaelltNachUntenNichtAufEinsHertz()
    {
        // ABWEICHUNG von Thetis, im Code begruendet: dort gibt ein Index
        // ausserhalb der Liste "0000" ("some default") -- das waere 1 Hz
        // fuer JEDEN unbekannten Schritt, auch fuer 1 MHz.
        QCOMPARE(CatProtocol::schrittFeld(3),     QStringLiteral("0000"));  // < 10
        QCOMPARE(CatProtocol::schrittFeld(75),    QStringLiteral("1000"));  // 50er
        QCOMPARE(CatProtocol::schrittFeld(3000),  QStringLiteral("0011"));  // 2,5k
        QCOMPARE(CatProtocol::schrittFeld(99999999), QStringLiteral("0111"));
        // Immer vier Zeichen, egal was hereinkommt.
        for (int hz : {0, -5, 1, 7, 333, 123456, 99999999}) {
            QCOMPARE(CatProtocol::schrittFeld(hz).size(), 4);
        }
    }

    // ── Senden ──────────────────────────────────────────────────────────────

    void txUndRxTastenUndQuittieren()
    {
        // CATStructs.xml: TX und RX haben nSet=0 (der Befehl selbst ist die
        // Tat) und nAns=1.
        QCOMPARE(p.antwortAuf(QStringLiteral("TX;")), QStringLiteral("TX0;"));
        QCOMPARE(moxRufe, 1);
        QVERIFY(moxZuletzt);
        QCOMPARE(p.antwortAuf(QStringLiteral("RX;")), QStringLiteral("RX0;"));
        QCOMPARE(moxRufe, 2);
        QVERIFY(!moxZuletzt);
        // Und der Statussatz zieht mit.
        QCOMPARE(p.antwortAuf(QStringLiteral("TX;")), QStringLiteral("TX0;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("IF;")).mid(2 + 26, 1),
                 QStringLiteral("1"));
    }

    void txMitNutzdatenWirdAbgelehnt()
    {
        // "TX1;" ist kein Kenwood-Befehl. Ihn als TX zu deuten waere
        // geraten -- und es ist der Befehl, der den Sender tastet.
        QCOMPARE(p.antwortAuf(QStringLiteral("TX1;")), QStringLiteral("?;"));
        QCOMPARE(moxRufe, 0);
    }

    // ── FR / FT, Selbstmeldung, ID ──────────────────────────────────────────

    void ftSchaltetSplitFrAberNicht()
    {
        // FT waehlt den Sende-VFO: 1 = VFO B = Split.
        QCOMPARE(p.antwortAuf(QStringLiteral("FT1;")), QString());
        QCOMPARE(splitRufe, 1);
        QVERIFY(splitZuletzt);
        QCOMPARE(p.antwortAuf(QStringLiteral("FT;")), QStringLiteral("FT1;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FT0;")), QString());
        QVERIFY(!splitZuletzt);

        // FR waehlt den Empfangs-VFO. Longpath hat dafuer keinen Begriff --
        // A IST der Empfangsslice. 0 ist erfuellbar, 1 nicht.
        QCOMPARE(p.antwortAuf(QStringLiteral("FR0;")), QString());
        QCOMPARE(p.antwortAuf(QStringLiteral("FR1;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FR;")),  QStringLiteral("FR0;"));
        // Und FR hat den Split nicht angefasst.
        QCOMPARE(splitRufe, 2);
    }

    void frUndFtAusserhalbNullBisEinsWerdenAbgelehnt()
    {
        // 2 ist bei Kenwood der Speicherkanal. Ihn als B zu deuten waere
        // geraten.
        QCOMPARE(p.antwortAuf(QStringLiteral("FT2;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("FR2;")), QStringLiteral("?;"));
        QCOMPARE(splitRufe, 0);
    }

    void dieSelbstmeldungWirdGemerktAberNichtVersprochen()
    {
        QVERIFY(!p.selbstmeldung());
        QCOMPARE(p.antwortAuf(QStringLiteral("AI1;")), QString());
        QVERIFY(p.selbstmeldung());
        QCOMPARE(p.antwortAuf(QStringLiteral("AI;")),  QStringLiteral("AI1;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("AI0;")), QString());
        QVERIFY(!p.selbstmeldung());
        QCOMPARE(p.antwortAuf(QStringLiteral("AI;")),  QStringLiteral("AI0;"));
        // 2 ist bei Kenwood eine eigene Stufe; Longpath kennt sie nicht
        // und sagt das, statt sie als 1 zu nehmen.
        QCOMPARE(p.antwortAuf(QStringLiteral("AI2;")), QStringLiteral("?;"));
    }

    void dieGeraeteKennungIstDreistellig()
    {
        // CATStructs.xml: ID ans=3. 900 = SDR-1000, wie Thetis' Vorgabe
        // (CATCommands.cs:291-...).
        QCOMPARE(p.antwortAuf(QStringLiteral("ID;")), QStringLiteral("ID900;"));
        CatProtocol::Zustand z;
        z.rigId = 13;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ID;")), QStringLiteral("ID013;"));
    }

    void einSetzenAufEinenNurLesbarenBefehlWirdAbgelehnt()
    {
        // IF und ID haben nSet=-1: es gibt sie nur als Frage.
        QCOMPARE(p.antwortAuf(QStringLiteral("IF1;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ID900;")), QStringLiteral("?;"));
    }
};

QTEST_MAIN(TstCatProtokoll)
#include "tst_cat_protokoll.moc"
