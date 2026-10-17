// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigener Prueffall; die gepruefte Wortform stammt
// aus Thetis und ist in src/core/CatProtocol.{h,cpp} zitiert.
//
// Der ZZ-Satz der CAT-Protokollschicht (2026-10-13).
//
// Zeus hat in 2.0.50 angekuendigt, dass sein CAT-Server die Thetis-eigenen
// ZZ-Befehle beantwortet, und dass sich N1MM+ (Geraetetyp Flex5000 ueber
// TCP) und DXLab Commander (Geraetetyp Thetis) damit direkt anhaengen.
// Longpath sprach bis heute die 16 Kenwood-Befehle und keinen ZZ --
// `antwortAuf()` schnitt stur ZWEI Zeichen als Befehlswort ab, womit ALLE
// 239 ZZ-Befehle auf das unbekannte Praefix "ZZ" fielen und `?;` gaben.
//
// Was dieser Prueffall belegen kann und was nicht:
//
//   KANN: dass jede hier gebaute Wortform genau die Form hat, die Thetis
//         sendet und erwartet -- Feldbreiten, Vorzeichen, Bereiche,
//         Ablehnung bei fehlendem Haken, Nachlesen nach jedem Setzen.
//
//   KANN NICHT: dass N1MM+ und DXLab Commander damit zufrieden sind.
//         Beide laufen unter Windows, hier liegt keines von beiden. WELCHE
//         Befehle sie tatsaechlich senden, ist NICHT geprueft -- die
//         Auswahl folgt den Beschreibungen in Thetis' CATStructs.xml.
//         Der Beleg dafuer ist ein Lauf mit dem echten Programm.

#include <QtTest/QtTest>

#include "core/CatModeMap.h"
#include "core/CatProtocol.h"

using namespace Longpath;

class TstCatZz : public QObject {
    Q_OBJECT

private:
    CatProtocol p;

    // Mitschrieb der Haken.
    quint64 vfoA{0};
    quint64 vfoB{0};
    int     modeZZ{-1};
    bool    mox{false};
    bool    split{false};
    bool    ritAn{false};
    bool    xitAn{false};
    int     ritHz{0};
    int     xitHz{0};
    int     filterTief{0};
    int     filterHoch{0};
    int     sperre{-1};
    int     rufeSperre{0};

    /// Alle Haken anhaengen -- der Normalfall eines vollstaendig
    /// verdrahteten Geraets.
    void alleHaken()
    {
        CatProtocol::Haken h;
        h.setzeVfoA        = [this](quint64 hz) { vfoA = hz; };
        h.setzeVfoB        = [this](quint64 hz) { vfoB = hz; };
        h.setzeModeZZ      = [this](int z)      { modeZZ = z; };
        h.setzeMox         = [this](bool an)    { mox = an; };
        h.setzeSplit       = [this](bool an)    { split = an; };
        h.setzeRit         = [this](bool an)    { ritAn = an; };
        h.setzeXit         = [this](bool an)    { xitAn = an; };
        h.setzeRitHz       = [this](int hz)     { ritHz = hz; };
        h.setzeXitHz       = [this](int hz)     { xitHz = hz; };
        h.setzeFilterLow   = [this](int hz)     { filterTief = hz; };
        h.setzeFilterHigh  = [this](int hz)     { filterHoch = hz; };
        h.setzeSquelch     = [this](int w)      { sperre = w; ++rufeSperre; };
        p.setzeHaken(h);
    }

private slots:
    void initTestCase()
    {
        // PFLICHT. Ohne Testmodus liest und SCHREIBT AppSettings die
        // echte Einstellungsdatei des Betreibers (1,9 MB, sein
        // Layoutprofil). Die drei CAT-Prueffaelle vom 2026-10-11 hatten
        // die Zeile zuerst NICHT.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        p = CatProtocol();
        vfoA = vfoB = 0;
        modeZZ = -1;
        mox = split = ritAn = xitAn = false;
        ritHz = xitHz = filterTief = filterHoch = 0;
        sperre = -1;
        rufeSperre = 0;
        alleHaken();
    }

    // ── Das Befehlswort ist vier Zeichen lang ────────────────────────────

    void dasBefehlswortIstVierZeichenLang()
    {
        // DER Fund, um den es hier geht. Vor dem 2026-10-13 schnitt
        // antwortAuf() stur zwei Zeichen ab; "ZZFA;" traf damit auf das
        // Praefix "ZZ", das in keiner Tabelle steht.
        CatProtocol::Zustand z;
        z.vfoAHz = 14'074'000;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA;")),
                 QStringLiteral("ZZFA00014074000;"));

        // "ZZ;" und "ZZF;" sind keine Befehle -- zu kurz fuer ein
        // vierstelliges Wort.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZ;")),   QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZF;")),  QStringLiteral("?;"));
        // Ein unbekanntes ZZ-Wort wird abgelehnt, nicht verschwiegen.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZQQ;")), QStringLiteral("?;"));
    }

    void dieKenwoodHaelfteBleibtZweiZeichen()
    {
        // Gegenprobe zur Stufe: die vierstellige Behandlung darf die
        // zweistellige nicht anfassen.
        CatProtocol::Zustand z;
        z.vfoAHz = 14'074'000;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("FA;")),
                 QStringLiteral("FA00014074000;"));
    }

    // ── Frequenzen ───────────────────────────────────────────────────────

    void zzFaUndZzFbLesenUndSetzen()
    {
        CatProtocol::Zustand z;
        z.vfoAHz = 7'100'000;
        z.vfoBHz = 7'200'000;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA;")),
                 QStringLiteral("ZZFA00007100000;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFB;")),
                 QStringLiteral("ZZFB00007200000;"));

        // Setzen quittiert nicht -- wie bei Kenwood.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA00014225000;")), QString());
        QCOMPARE(vfoA, quint64(14'225'000));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFB00014250000;")), QString());
        QCOMPARE(vfoB, quint64(14'250'000));

        // Falsche Laenge ist kein Befehl.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA14225000;")),
                 QStringLiteral("?;"));
    }

    void ohneHakenGibtEsAuchKeineAntwort()
    {
        // Die Regel vom 2026-10-11, hier fuer den ZZ-Satz festgenagelt:
        // der Haken ist das Kennzeichen, dass eine Sache EXISTIERT, und
        // entscheidet darum auch die ABFRAGE.
        //
        // Live gesehen: Longpath antwortete auf FB; mit
        // "FB00000000000;" -- eine Frequenz fuer ein VFO B, das es nicht
        // gibt. Ein Fremdprogramm liest das und zeigt es an.
        CatProtocol::Haken leer;
        leer.setzeVfoA = [this](quint64 hz) { vfoA = hz; };
        p.setzeHaken(leer);           // nur VFO A haengt
        CatProtocol::Zustand z;
        z.vfoAHz = 7'000'000;
        z.vfoBHz = 7'200'000;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA;")),
                 QStringLiteral("ZZFA00007000000;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFB;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSP;")), QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSO;")), QStringLiteral("?;"));
    }

    void zzFtIstDieSendefrequenzUndNurLesbar()
    {
        // Namensfalle: Kenwoods FT waehlt das Sende-VFO (eine Stelle),
        // ZZFT ist die Sendefrequenz (elf Stellen).
        CatProtocol::Zustand z;
        z.vfoAHz = 14'074'000;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFT;")),
                 QStringLiteral("ZZFT00014074000;"));

        // Das SETZEN wird abgelehnt: Longpath hat keinen Split, und das
        // stillschweigend als "stimme VFO A ab" auszufuehren wuerde die
        // EMPFANGSfrequenz des Betreibers verschieben.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFT00014100000;")),
                 QStringLiteral("?;"));
        QCOMPARE(vfoA, quint64(0));
    }

    // ── Der Modus, zweistellig ───────────────────────────────────────────

    void zzMdFuehrtDenModusZweistellig()
    {
        CatProtocol::Zustand z;
        z.modeZZ = catZzZahl(DSPMode::DIGU);     // 7
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZMD;")),
                 QStringLiteral("ZZMD07;"));

        QCOMPARE(p.antwortAuf(QStringLiteral("ZZMD03;")), QString());
        QCOMPARE(modeZZ, 3);                     // CWL
        QCOMPARE(catModusAusZzZahl(modeZZ), DSPMode::CWL);

        // Bereich 0..11 -- wie Thetis' Pruefung in ZZMD.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZMD11;")), QString());
        QCOMPARE(modeZZ, 11);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZMD12;")), QStringLiteral("?;"));
        QCOMPARE(modeZZ, 11);                    // unveraendert
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZMDxx;")), QStringLiteral("?;"));
    }

    void dieZzZahlIstDerDspModeWert()
    {
        // Longpaths `enum class DSPMode` stammt aus Thetis, darum deckt
        // sich die Reihenfolge mit Mode2String. Das ist der Grund, warum
        // der ZZ-Satz hier ueberhaupt so glatt passt -- und ein Grund,
        // es festzunageln: wer DSPMode umsortiert, bricht CAT.
        QCOMPARE(catZzZahl(DSPMode::LSB),  0);
        QCOMPARE(catZzZahl(DSPMode::USB),  1);
        QCOMPARE(catZzZahl(DSPMode::DSB),  2);
        QCOMPARE(catZzZahl(DSPMode::CWL),  3);
        QCOMPARE(catZzZahl(DSPMode::CWU),  4);
        QCOMPARE(catZzZahl(DSPMode::FM),   5);
        QCOMPARE(catZzZahl(DSPMode::AM),   6);
        QCOMPARE(catZzZahl(DSPMode::DIGU), 7);
        QCOMPARE(catZzZahl(DSPMode::SPEC), 8);
        QCOMPARE(catZzZahl(DSPMode::DIGL), 9);
        QCOMPARE(catZzZahl(DSPMode::SAM),  10);
        QCOMPARE(catZzZahl(DSPMode::DRM),  11);

        // RADE kennt Thetis nicht. Gemeldet wird das SEITENBAND, nicht
        // "unbekannt": ein Logbuchprogramm traegt damit USB bzw. LSB
        // ein, was fuer den Rapport stimmt. Thetis' default gibt dort
        // "?;" -- und dieselbe Funktion fuellt das Modusfeld MITTEN im
        // 36-Zeichen-Statussatz, wo ein "?;" alles dahinter verschiebt.
        QCOMPARE(catZzZahl(DSPMode::RADE_U), 1);
        QCOMPARE(catZzZahl(DSPMode::RADE_L), 0);

        // Und der ZZ-Satz ist NICHT aus dem Kenwood-Satz ableitbar:
        // Kenwood hat fuer CW nur EINE Zahl, kennt DSB/SAM/DRM nicht.
        QCOMPARE(catKenwoodZahl(DSPMode::CWL), 7);
        QCOMPARE(catKenwoodZahl(DSPMode::CWU), 3);
        QCOMPARE(catKenwoodZahl(DSPMode::DSB), catKenwoodZahl(DSPMode::SAM));
        QVERIFY(catZzZahl(DSPMode::DSB) != catZzZahl(DSPMode::SAM));
    }

    // ── Der erweiterte Statussatz ────────────────────────────────────────

    void zzIfHatSechsunddreissigZeichenUndZweiStellenModus()
    {
        CatProtocol::Zustand z;
        z.vfoAHz = 14'225'000;
        z.stepHz = 100;
        // Die Betriebsart ist mit Bedacht EINSTELLIG gewaehlt (CWL = 3):
        // sie muss als "03" im Satz stehen. Hier stand zuerst SAM (10),
        // und die Gegenprobe hat gezeigt, dass dieser Fall damit NICHTS
        // messen konnte -- ein kaputter Formatierer, der einfach
        // QString::number() nimmt, schreibt fuer 10 ebenfalls "10", und
        // der Satz blieb 36 Zeichen lang. Genau die eine Betriebsart,
        // bei der der Fehler unsichtbar ist.
        z.modeZZ = catZzZahl(DSPMode::CWL);      // 3 -> "03"
        z.modeK  = 7;
        p.setzeZustand(z);
        const QString s = p.zzStatussatz();
        QCOMPARE(s.size(), 36);
        QCOMPARE(s.mid(0, 11), QStringLiteral("00014225000"));
        QCOMPARE(s.mid(11, 4), QStringLiteral("0010"));     // 100 Hz
        QCOMPARE(s.mid(15, 6), QStringLiteral("+00000"));
        QCOMPARE(s.mid(21, 1), QStringLiteral("0"));        // RIT aus
        QCOMPARE(s.mid(22, 1), QStringLiteral("0"));        // XIT aus
        QCOMPARE(s.mid(23, 3), QStringLiteral("000"));      // Bank (blind)
        QCOMPARE(s.mid(26, 1), QStringLiteral("0"));        // empfaengt
        QCOMPARE(s.mid(27, 2), QStringLiteral("03"));       // CWL mit Null davor
        QCOMPARE(s.mid(29, 1), QStringLiteral("0"));        // FR/FT (blind)
        QCOMPARE(s.mid(30, 1), QStringLiteral("0"));        // Scan  (blind)
        QCOMPARE(s.mid(31, 1), QStringLiteral("0"));        // Split aus
        QCOMPARE(s.mid(32, 4), QStringLiteral("0000"));     // Balance (blind)

        // Und ueber den Draht mit Praefix und Abschluss.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZIF;")),
                 QStringLiteral("ZZIF") + s + QStringLiteral(";"));

        // Dasselbe mit einer ZWEISTELLIGEN Betriebsart -- der Satz
        // bleibt 36 Zeichen, und kein Feld verschiebt sich.
        z.modeZZ = catZzZahl(DSPMode::DRM);      // 11
        p.setzeZustand(z);
        const QString z2 = p.zzStatussatz();
        QCOMPARE(z2.size(), 36);
        QCOMPARE(z2.mid(27, 2), QStringLiteral("11"));
        QCOMPARE(z2.mid(32, 4), QStringLiteral("0000"));   // Balance steht

        // Und ein Wert ausserhalb 0..11 verschiebt den Satz NICHT.
        // catZzZahl() kann das nicht liefern; die Pruefung gilt dem
        // Fall, dass jemand modeZZ von Hand setzt.
        z.modeZZ = 42;
        p.setzeZustand(z);
        const QString z3 = p.zzStatussatz();
        QCOMPARE(z3.size(), 36);
        QCOMPARE(z3.mid(27, 2), QStringLiteral("01"));
    }

    void derUnterschiedZumKenwoodSatzIstGenauEinZeichen()
    {
        // Der eigentliche Punkt der 36 gegen 35: NUR das Modusfeld ist
        // anders. Wer hier etwas glaettet, verschiebt jedes Feld hinter
        // dem Modus -- und ein Fremdprogramm liest dann die Balance als
        // Split.
        CatProtocol::Zustand z;
        z.vfoAHz = 14'225'000;
        z.stepHz = 100;
        z.modeK  = 2;                            // USB, Kenwood
        z.modeZZ = catZzZahl(DSPMode::USB);      // 1 -> "01"
        z.ritOn  = true;
        z.ritHz  = -250;
        z.mox    = true;
        p.setzeZustand(z);
        const QString k = p.statussatz();
        const QString e = p.zzStatussatz();
        QCOMPARE(k.size(), 35);
        QCOMPARE(e.size(), 36);
        // Alles VOR dem Modus deckt sich Zeichen fuer Zeichen.
        QCOMPARE(k.left(27), e.left(27));
        // Das Modusfeld: eine Stelle gegen zwei, dieselbe Betriebsart.
        QCOMPARE(k.mid(27, 1), QStringLiteral("2"));
        QCOMPARE(e.mid(27, 2), QStringLiteral("01"));
        // Alles DAHINTER deckt sich wieder.
        QCOMPARE(k.mid(28), e.mid(29));
    }

    // ── Tasten, Split ────────────────────────────────────────────────────

    void zzTxTastetUndMeldetDenEchtenSendezustand()
    {
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZTX1;")), QString());
        QVERIFY(mox);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZTX0;")), QString());
        QVERIFY(!mox);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZTX2;")), QStringLiteral("?;"));

        // Die Abfrage meldet den Sendezustand des GERAETS, nicht nur den
        // von CAT verursachten (Thetis: `CATPTT || MOX`).
        CatProtocol::Zustand z;
        z.mox = true;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZTX;")), QStringLiteral("ZZTX1;"));
    }

    void zzSpFolgtDemHaken()
    {
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSP1;")), QString());
        QVERIFY(split);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSP;")), QStringLiteral("ZZSP1;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSP2;")), QStringLiteral("?;"));
    }

    // ── RIT und XIT, jetzt GETRENNT ──────────────────────────────────────

    void zzRfUndZzXfStellenZweiVerschiedeneAblagen()
    {
        // DER Grund, warum `Zustand` seit heute zwei Ablagen fuehrt:
        // ZZXF stellt die XIT-Ablage, auch wenn RIT an ist. Mit einem
        // gemeinsamen Wert haette das eine die andere ueberschrieben.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRF+0250;")), QString());
        QCOMPARE(ritHz, 250);
        QCOMPARE(xitHz, 0);                      // unberuehrt
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXF-0100;")), QString());
        QCOMPARE(xitHz, -100);
        QCOMPARE(ritHz, 250);                    // unberuehrt

        CatProtocol::Zustand z;
        z.ritHz = 250;
        z.xitHz = -100;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRF;")), QStringLiteral("ZZRF+0250;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXF;")), QStringLiteral("ZZXF-0100;"));
    }

    void dasFuenfZeichenFeldHatImmerEinVorzeichen()
    {
        QCOMPARE(CatProtocol::vorzeichenFeld4(0),    QStringLiteral("+0000"));
        QCOMPARE(CatProtocol::vorzeichenFeld4(1),    QStringLiteral("+0001"));
        QCOMPARE(CatProtocol::vorzeichenFeld4(-1),   QStringLiteral("-0001"));
        QCOMPARE(CatProtocol::vorzeichenFeld4(9999), QStringLiteral("+9999"));

        // ABWEICHUNG VON THETIS, mit Absicht: Thetis klammert beim
        // Setzen auf +-99999 (ZZRF) bzw. +-10000 (ZZFL), kann in der
        // Antwort aber nur VIER Stellen fuehren -- aus 12000 wird dort
        // "+2000", aus 10000 "+0000". Hier wird auf das geklammert, was
        // das Feld tragen kann, damit die Antwort nie etwas anderes
        // behauptet.
        QCOMPARE(CatProtocol::vorzeichenFeld4(12000),  QStringLiteral("+9999"));
        QCOMPARE(CatProtocol::vorzeichenFeld4(-12000), QStringLiteral("-9999"));
    }

    void zzRtUndZzXsSchaltenDieAblagenEin()
    {
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRT1;")), QString());
        QVERIFY(ritAn);
        QVERIFY(!xitAn);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXS1;")), QString());
        QVERIFY(xitAn);

        CatProtocol::Zustand z;
        z.ritOn = true;
        z.xitOn = false;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRT;")), QStringLiteral("ZZRT1;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXS;")), QStringLiteral("ZZXS0;"));
    }

    void zzRuSchiebtImmerZehnHertzUndNichtDenAbstimmschritt()
    {
        // Thetis hat die modusabhaengigen Schritte (10 fuer CW, 50 fuer
        // SSB) auskommentiert stehengelassen, mit W2PAs Notiz "Changed
        // to operate in all modes". Anders als Kenwoods RU/RD, die den
        // Abstimmschritt nehmen.
        CatProtocol::Zustand z;
        z.stepHz = 1000;              // bewusst GROSS
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRU;")), QString());
        QCOMPARE(ritHz, 10);          // nicht 1000
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRU;")), QString());
        QCOMPARE(ritHz, 20);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRD;")), QString());
        QCOMPARE(ritHz, 10);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXU;")), QString());
        QCOMPARE(xitHz, 10);
        QCOMPARE(ritHz, 10);          // getrennt

        // Mit fuenf Zeichen ist es ein Setzen und geht an ZZRF.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRU+0500;")), QString());
        QCOMPARE(ritHz, 500);

        // Und ZZRU/ZZRD antworten NIE (nAns = 0).
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRU;")), QString());
    }

    void zzRuWirktAuchWennDieAblageAusIst()
    {
        // Anders als Kenwoods RU/RD, die ohne RIT/XIT ablehnen. Thetis'
        // Notiz dazu: "Want to be able to change RIT value even if it's
        // off" (W2PA).
        QVERIFY(!ritAn);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZRU;")), QString());
        QCOMPARE(ritHz, 10);
        // Gegenprobe: Kenwoods RU lehnt im selben Zustand ab.
        QCOMPARE(p.antwortAuf(QStringLiteral("RU;")), QStringLiteral("?;"));
    }

    void zzXcLoeschtNurDieXitAblage()
    {
        CatProtocol::Zustand z;
        z.ritHz = 300;
        z.xitHz = -300;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXC;")), QString());
        QCOMPARE(xitHz, 0);
        QCOMPARE(ritHz, 0);     // Haken nicht gerufen -> Mitschrieb leer
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZXC1;")), QStringLiteral("?;"));
    }

    // ── Filterkanten und Rauschsperre ────────────────────────────────────

    void zzFlUndZzFhStellenDieFilterkanten()
    {
        CatProtocol::Zustand z;
        z.filterLowHz  = 150;
        z.filterHighHz = 2850;
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFL;")), QStringLiteral("ZZFL+0150;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFH;")), QStringLiteral("ZZFH+2850;"));

        // Negativ ist richtig und kommt bei LSB vor.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFL-2850;")), QString());
        QCOMPARE(filterTief, -2850);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFH-0150;")), QString());
        QCOMPARE(filterHoch, -150);
    }

    void zzSoKenntDieStimmsperreNicht()
    {
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSO1;")), QString());
        QCOMPARE(sperre, 1);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSO0;")), QString());
        QCOMPARE(sperre, 0);
        // Thetis' 2 (Stimmsperre) ist eine gueltige Wortform und wird
        // durchgelassen -- ob sie wirkt, entscheidet die Verdrahtung
        // ueber den Nachleser.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSO2;")), QString());
        QCOMPARE(sperre, 2);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZSO3;")), QStringLiteral("?;"));
        QCOMPARE(rufeSperre, 3);
    }

    // ── Erkennung ────────────────────────────────────────────────────────

    void zzIdWirdAngenommenUndBleibtStumm()
    {
        // DXLab Commander und N1MM+ schicken das beim Verbinden. Ein
        // `?;` darauf kann ein Programm als "falsches Geraet" lesen und
        // die Verbindung gleich wieder aufgeben.
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZID;")), QString());
    }

    void zzZmUndZzZvTragenEineAntwortVeraenderlicherLaenge()
    {
        CatProtocol::Zustand z;
        z.modell  = QStringLiteral("LONGPATH");
        z.fassung = QStringLiteral("0.9.7");
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZZM;")),
                 QStringLiteral("ZZZMLONGPATH;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZZV;")),
                 QStringLiteral("ZZZV0.9.7;"));
    }

    void einSemikolonImTextWuerdeDieAntwortDurchschneiden()
    {
        // Thetis entfernt es bei ZZZV ausdruecklich
        // (`.Replace(";", "")`), bei ZZZM nicht. Hier bei beiden: ein
        // ';' mitten in der Antwort beendet fuer den Empfaenger den
        // Befehl, und der Rest wird als naechster gelesen.
        CatProtocol::Zustand z;
        z.modell  = QStringLiteral("LONG;PATH");
        z.fassung = QStringLiteral("0.9;7");
        p.setzeZustand(z);
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZZM;")),
                 QStringLiteral("ZZZMLONGPATH;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZZV;")),
                 QStringLiteral("ZZZV0.97;"));
    }

    // ── Der Nachleser ────────────────────────────────────────────────────

    void ohneWirkungGibtEsKeineQuittung()
    {
        // Die Lehre vom 2026-10-11, hier fuer den ZZ-Satz: ein blind
        // quittiertes Setzen bringt ein Contestprogramm dazu, auf der
        // alten Frequenz zu senden.
        //
        // Haken, der NICHTS tut (so wie ein Haken, dessen Scheibe weg
        // ist), plus Nachleser, der den unveraenderten Zustand liest.
        CatProtocol::Haken taub;
        taub.setzeVfoA   = [](quint64) { };
        taub.setzeModeZZ = [](int)     { };
        p.setzeHaken(taub);
        CatProtocol::Zustand fest;
        fest.vfoAHz = 7'000'000;
        fest.modeZZ = 1;
        p.setzeZustand(fest);
        p.setzeNachleser([fest]() { return fest; });

        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA00014225000;")),
                 QStringLiteral("?;"));
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZMD03;")), QStringLiteral("?;"));

        // Gegenprobe: ein Haken, der WIRKT, und ein Nachleser, der es
        // sieht -- dann quittiert es stillschweigend wie Kenwood.
        quint64 echt = 7'000'000;
        CatProtocol::Haken gut;
        gut.setzeVfoA = [&echt](quint64 hz) { echt = hz; };
        p.setzeHaken(gut);
        p.setzeNachleser([&echt]() {
            CatProtocol::Zustand z;
            z.vfoAHz = echt;
            return z;
        });
        QCOMPARE(p.antwortAuf(QStringLiteral("ZZFA00014225000;")), QString());
        QCOMPARE(echt, quint64(14'225'000));
    }
};

QTEST_MAIN(TstCatZz)
#include "tst_cat_zz.moc"
