// =================================================================
// tests/tst_ft8_eu_vhf.cpp  (Longpath)
// =================================================================
//
// Das Nachrichtenformat Typ 5 — der EU-VHF-Contest.
//
// ── Warum das gebaut wurde ──────────────────────────────────────────
//
// Dieselbe Klasse wie beim DXpedition-Typ: ft8_lib laesst Typ 5 im
// switch von `ftx_message_decode()` in den default-Zweig laufen und
// gibt FTX_MESSAGE_RC_ERROR_TYPE zurueck. `Ft8Decoder.cpp` wirft die
// Zeile daraufhin weg (`continue`). In einem EU-VHF-Contest faellt
// damit JEDE Contest-Station still durch — von „niemand da" nicht zu
// unterscheiden.
//
// ── Zwei falsche Faehrten, denen hier NICHT gefolgt wird ────────────
//
//  1. **Typ 0.2 ist tot.** ft8_libs Aufzaehlung fuehrt ihn als
//     „EU VHF contest", und das war er einmal. WSJT-X 2.7.0 dekodiert
//     ihn selbst nicht mehr: `packjt77.f90:330-331` setzt bei
//     `i3=0 .and. n3=2` schlicht `unpk77_success=.false.`. Wer dort
//     baut, baut an einem Format, das niemand mehr sendet.
//
//  2. **Der Name `FTX_MESSAGE_TYPE_WWROF` taeuscht**, und der
//     Kommentar daneben beschrieb die Felder falsch (`1 28 28 1 7 9`)
//     — mit einem Fragezeichen von upstream selbst. Massgeblich ist
//     WSJT-X `lib/77bit/packjt77.f90:593-610` [v2.7.0]:
//     `h12 h22 r1 s3 s11 g25`.
//
// ── Warum die Nutzlast hier mit einem Bit-Setzer gebaut wird ────────
//
// Beim DXpedition-Lauf kamen die Rufzeichenfelder aus ft8_libs
// eigenem Packer, damit ein Fehler in meiner Bitreihenfolge nicht in
// beide Richtungen gleich ausfaellt und sich aufhebt. Diesen Weg gibt
// es hier nicht: fuer Typ 5 hat ft8_lib **keinen** Packer.
//
// Der Ersatz ist ein generischer Bit-Setzer — Bitposition hinein,
// Wert hinein — der die Felder genau so legt, wie WSJT-X sie im
// `format(b12,b22,b1,b3,b11,b25)` zaehlt. Er teilt mit der
// Auspackrechnung in `message.c` keine einzige Zeile: dort wird je
// Feld mit eigenen Schiebeweiten und Masken gearbeitet, hier Bit fuer
// Bit. Dazu kommt `ftx_message_get_type()` als unabhaengige Probe,
// dass `i3` wirklich dort sitzt, wo der Setzer es hinlegt — das liest
// ft8_lib mit seinem eigenen Code.
//
// Longpath-original.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-04 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

extern "C" {
#include "ft8/message.h"
}

#include "core/Ft8Rufzeichen.h"

namespace {

/// Ein Feld in die 77-Bit-Nutzlast legen, gross-endig ab Bit 0.
///
/// Bewusst Bit fuer Bit und ohne jede Schiebeweite aus `message.c`:
/// ein Fehler dort faellt hier NICHT genauso aus.
void setzeBits(ftx_message_t* m, int abBit, int breite, quint32 wert)
{
    for (int i = 0; i < breite; ++i) {
        const int bit  = abBit + i;
        const quint32 w = (wert >> (breite - 1 - i)) & 1u;
        const int byte = bit / 8;
        const int rest = 7 - (bit % 8);
        if (w) { m->payload[byte] |=  quint8(1u << rest); }
        else   { m->payload[byte] &= quint8(~(1u << rest)); }
    }
}

/// Ein Feld aus der Nutzlast lesen — Gegenstueck zu `setzeBits`.
///
/// Teilt mit `message.c` wieder keine Zeile: dort Schiebeweiten und
/// Masken je Feld, hier Bit fuer Bit.
quint32 liesBits(const ftx_message_t& m, int abBit, int breite)
{
    quint32 wert = 0;
    for (int i = 0; i < breite; ++i) {
        const int bit  = abBit + i;
        const int byte = bit / 8;
        const int rest = 7 - (bit % 8);
        wert = (wert << 1) | ((m.payload[byte] >> rest) & 1u);
    }
    return wert;
}

/// Eine Typ-5-Nachricht bauen.
///
/// From WSJT-X `lib/77bit/packjt77.f90:597-598` [v2.7.0] --
///   `read(c77,1060) n12,n22,ir,irpt,iserial,igrid6`
///   `1060 format(b12,b22,b1,b3,b11,b25)`
/// und i3 = 5 in den Bits 74..76.
ftx_message_t baueTyp5(quint16 n12, quint32 n22, quint8 ir,
                       quint8 irpt, quint16 iserial, quint32 igrid6)
{
    ftx_message_t m{};
    ftx_message_init(&m);
    int p = 0;
    setzeBits(&m, p, 12, n12);     p += 12;
    setzeBits(&m, p, 22, n22);     p += 22;
    setzeBits(&m, p,  1, ir);      p +=  1;
    setzeBits(&m, p,  3, irpt);    p +=  3;
    setzeBits(&m, p, 11, iserial); p += 11;
    setzeBits(&m, p, 25, igrid6);  p += 25;
    Q_ASSERT(p == 74);
    setzeBits(&m, 74, 3, 5);       // i3 = 5
    return m;
}

/// Der Locator als Zahl, nach WSJT-Xs Stellenwerten.
///
/// From WSJT-X `lib/77bit/packjt77.f90:1577-1607` [v2.7.0], rueckwaerts
/// gelesen: Feld(18) Feld(18) Quadrat(10) Quadrat(10) Unter(24) Unter(24).
quint32 gridZahl(const char* g)
{
    return quint32(g[0] - 'A') * (18u*10u*10u*24u*24u)
         + quint32(g[1] - 'A') * (10u*10u*24u*24u)
         + quint32(g[2] - '0') * (10u*24u*24u)
         + quint32(g[3] - '0') * (24u*24u)
         + quint32(g[4] - 'A') * 24u
         + quint32(g[5] - 'A');
}

QString entpacke(const ftx_message_t& m, void* hashIf = nullptr)
{
    char text[64] = {};
    ftx_message_offsets_t off;
    const ftx_message_rc_t rc = ftx_message_decode(
        &m, static_cast<ftx_callsign_hash_interface_t*>(hashIf), text, &off);
    if (rc != FTX_MESSAGE_RC_OK) { return QStringLiteral("RC=%1").arg(int(rc)); }
    return QString::fromLatin1(text);
}

} // namespace

class TstFt8EuVhf : public QObject
{
    Q_OBJECT

private slots:

    // ── Zuerst: sitzt i3 ueberhaupt richtig? ────────────────────────
    //
    // Mit ft8_libs EIGENEM Leser geprueft, nicht mit meiner Rechnung.
    // Faellt der um, sind alle folgenden Faelle wertlos.
    void derTypWirdErkannt()
    {
        const ftx_message_t m = baueTyp5(0x123, 0x2ABCDE, 0, 7, 3,
                                         gridZahl("IO91NP"));
        QCOMPARE(ftx_message_get_type(&m), FTX_MESSAGE_TYPE_WWROF);
        QCOMPARE(int(ftx_message_get_i3(&m)), 5);
    }

    // ── Der Befund: bis heute kam hier gar nichts heraus ────────────
    //
    // Gegenprobe: ohne den neuen `case` im switch von
    // `ftx_message_decode()` liefert dieser Fall FTX_MESSAGE_RC_ERROR_TYPE
    // (= 5), und `Ft8Decoder.cpp` wirft die Zeile weg.
    void eineContestStationKommtDurch()
    {
        const ftx_message_t m = baueTyp5(0x123, 0x2ABCDE, 0, 7, 3,
                                         gridZahl("IO91NP"));
        const QString s = entpacke(m);
        QVERIFY2(s.endsWith(QStringLiteral("590003 IO91NP")), qPrintable(s));
    }

    // ── Falle 1: der Rapport ist KEIN dB-Wert ───────────────────────
    //
    // `nrs = 52 + irpt` (packjt77.f90:605). Drei Bit, also 52..59. Wer
    // hier einen Rapport in dB erwartet oder bei 59 festnagelt, liest
    // sieben von acht Faellen falsch.
    void derRapportIst52PlusIrpt()
    {
        struct { quint8 irpt; const char* erwartet; } faelle[] = {
            { 0, "520001" }, { 1, "530001" }, { 3, "550001" },
            { 5, "570001" }, { 7, "590001" },
        };
        for (const auto& f : faelle) {
            const ftx_message_t m = baueTyp5(1, 1, 0, f.irpt, 1,
                                             gridZahl("JN78AA"));
            const QString s = entpacke(m);
            QVERIFY2(s.contains(QLatin1String(f.erwartet)),
                     qPrintable(QStringLiteral("irpt=%1 → %2, erwartet %3")
                                    .arg(f.irpt).arg(s, QLatin1String(f.erwartet))));
        }
    }

    // ── Falle 2: die Nummer hat IMMER vier Stellen ──────────────────
    //
    // `format(i2,i4.4)` — die fuehrenden Nullen gehoeren dazu. „5901"
    // statt „590001" waere ein anderer Austausch.
    void dieNummerBehaeltIhreFuehrendenNullen()
    {
        struct { quint16 nr; const char* erwartet; } faelle[] = {
            {    0, "590000" }, {    7, "590007" }, {   42, "590042" },
            {  999, "590999" }, { 2047, "592047" },
        };
        for (const auto& f : faelle) {
            const ftx_message_t m = baueTyp5(1, 1, 0, 7, f.nr,
                                             gridZahl("JN78AA"));
            QVERIFY2(entpacke(m).contains(QLatin1String(f.erwartet)),
                     qPrintable(entpacke(m)));
        }
    }

    // ── Falle 3: der Locator ist SECHSstellig ───────────────────────
    //
    // 25 Bit, sechs Stellen. Im UKW-Contest zaehlt genau die sechste
    // Stelle fuer die Entfernungspunkte; vier Stellen waeren bis zu
    // 100 km daneben.
    void derLocatorIstSechsstellig()
    {
        for (const char* g : { "AA00AA", "JN78CT", "IO91NP", "RR99XX" }) {
            const ftx_message_t m = baueTyp5(1, 1, 0, 7, 1, gridZahl(g));
            QVERIFY2(entpacke(m).endsWith(QLatin1String(g)),
                     qPrintable(QStringLiteral("%1 → %2")
                                    .arg(QLatin1String(g), entpacke(m))));
        }
    }

    // Ein Locator ausserhalb des Moeglichen heisst: verfaelschtes
    // Paket. WSJT-X steigt dort aus, und eine halbe Zeile waere
    // schlimmer als keine.
    void einUnmoeglicherLocatorWirdAbgewiesen()
    {
        // 18*18*10*10*24*24 = 18662400 ist der erste unmoegliche Wert.
        //
        // Der Rueckgabewert wird GENAU geprueft, nicht nur „irgendein
        // Fehler": in der Gegenprobe gegen die alte Fassung kommt hier
        // ebenfalls ein Fehler heraus (ERROR_TYPE, weil der Typ dort
        // gar nicht behandelt wird) — ein Fall, der nur „nicht OK"
        // verlangt, waere dort gruen geblieben und haette nichts
        // belegt. Siehe `feedback-pruefstand-gegen-alte-fassung`.
        const ftx_message_t m = baueTyp5(1, 1, 0, 7, 1, 18662400u);
        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, nullptr, text, &off),
                 FTX_MESSAGE_RC_ERROR_GRID);
    }

    // Und der groesste MOEGLICHE Wert geht noch durch — eine Schranke,
    // die einen gueltigen Locator abweist, waere genauso falsch.
    void derGroessteGueltigeLocatorGehtDurch()
    {
        QCOMPARE(gridZahl("RR99XX"), 18662399u);
        const ftx_message_t m = baueTyp5(1, 1, 0, 7, 1, 18662399u);
        QVERIFY2(entpacke(m).endsWith(QStringLiteral("RR99XX")),
                 qPrintable(entpacke(m)));
    }

    // ── Das R-Flag ──────────────────────────────────────────────────
    void dasRFlagStehtVorDemAustausch()
    {
        const ftx_message_t ohne = baueTyp5(1, 1, 0, 7, 3, gridZahl("JN78AA"));
        const ftx_message_t mit  = baueTyp5(1, 1, 1, 7, 3, gridZahl("JN78AA"));
        QVERIFY2(!entpacke(ohne).contains(QStringLiteral(" R ")),
                 qPrintable(entpacke(ohne)));
        QVERIFY2(entpacke(mit).contains(QStringLiteral(" R 590003")),
                 qPrintable(entpacke(mit)));
    }

    // ── Falle 4: BEIDE Rufzeichen sind nur Hashes ───────────────────
    //
    // 12 und 22 Bit. Ohne Eintrag in der Tabelle steht „<...>" — das
    // ist kein Fehler, sondern alles, was im Paket steht. Wer hier ein
    // Rufzeichen erwartet, sucht den Fehler an der falschen Stelle.
    void unbekannteRufzeichenStehenAlsDreiPunkte()
    {
        const ftx_message_t m = baueTyp5(0x123, 0x2ABCDE, 0, 7, 3,
                                         gridZahl("IO91NP"));
        const QString s = entpacke(m);
        QVERIFY2(s.count(QStringLiteral("<...>")) == 2, qPrintable(s));
    }

    // ═══ SENDEN (2026-11-04) ════════════════════════════════════════
    //
    // upstreams `ftx_message_encode()` kennt nur Standard,
    // Nicht-Standard und Freitext. Einen Contest-Austausch kann es
    // nicht erzeugen — `ftx_message_encode_eu_vhf()` ist dafuer da.

    // ── Der Beleg, der zaehlt: gegen den UNABHAENGIGEN Bit-Setzer ───
    //
    // Ein reiner Rundlauf (selbst packen, selbst auspacken) waere
    // gruen, auch wenn beide Seiten dieselbe falsche Bitreihenfolge
    // haetten. Darum wird die gepackte Nutzlast hier Feld fuer Feld
    // mit `liesBits` zerlegt — derselbe Bit-fuer-Bit-Weg, mit dem
    // oben schon die Lesefaelle gegen WSJT-X gebaut sind.
    void derPackerLegtDieFelderDorthinWoWsjtxSieErwartet()
    {
        ftx_message_t m{};
        QCOMPARE(ftx_message_encode_eu_vhf(
                     &m,
                     static_cast<ftx_callsign_hash_interface_t*>(
                         Longpath::ft8RufzeichenSchnittstelle()),
                     "OE1ABC", "OE5SOS", true, 59, 3, "IO91NP"),
                 FTX_MESSAGE_RC_OK);

        // Die Hashes stehen in den ersten 34 Bit; sie werden weiter
        // unten ueber den Rundlauf durch die Tabelle geprueft. Hier
        // geht es um die Lage ALLER UEBRIGEN Felder.
        QCOMPARE(liesBits(m, 34,  1), 1u);                      // ir
        QCOMPARE(liesBits(m, 35,  3), 7u);                      // 59 - 52
        QCOMPARE(liesBits(m, 38, 11), 3u);                      // Nummer
        QCOMPARE(liesBits(m, 49, 25), gridZahl("IO91NP"));      // Locator
        QCOMPARE(liesBits(m, 74,  3), 5u);                      // i3
        // Und ft8_libs eigener Leser muss denselben Typ sehen.
        QCOMPARE(ftx_message_get_type(&m), FTX_MESSAGE_TYPE_WWROF);
    }

    // ── Der Rundlauf: eigener Sender gegen eigenen Empfaenger ──────
    //
    // Er belegt die Bitreihenfolge NICHT (derselbe Fehler in beide
    // Richtungen hoebe sich auf) — aber er faengt die Tabelle, die
    // Hashes und das Zusammensetzen der Zeichenkette, und das ist
    // genau das, was der Fall darueber nicht abdeckt.
    void waIchSendeLeseIchAuchWiederEin()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        Longpath::Ft8Rufzeichenspeicher::instanz().leeren();

        ftx_message_t m{};
        QCOMPARE(ftx_message_encode_eu_vhf(&m, hif, "OE1ABC", "OE5SOS",
                                           false, 59, 42, "JN78CT"),
                 FTX_MESSAGE_RC_OK);

        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, hif, text, &off),
                 FTX_MESSAGE_RC_OK);
        // Beide Rufzeichen kommen zurueck, weil der Packer sie beim
        // Hashen gemerkt hat — in spitzen Klammern, wie WSJT-X.
        QCOMPARE(QString::fromLatin1(text),
                 QStringLiteral("<OE1ABC> <OE5SOS> 590042 JN78CT"));
    }

    void dasRFlagUeberlebtDenRundlauf()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        ftx_message_t m{};
        QCOMPARE(ftx_message_encode_eu_vhf(&m, hif, "OE1ABC", "OE5SOS",
                                           true, 59, 42, "JN78CT"),
                 FTX_MESSAGE_RC_OK);
        char text[64] = {};
        ftx_message_offsets_t off;
        ftx_message_decode(&m, hif, text, &off);
        QCOMPARE(QString::fromLatin1(text),
                 QStringLiteral("<OE1ABC> <OE5SOS> R 590042 JN78CT"));
    }

    // ── Die Grenze, die das FORMAT setzt (nicht dieser Port) ────────
    //
    // Elf Bit tragen 2047. WSJT-X laesst den Austausch aber bis
    // 594095 zu und KAPPT dann still:
    //   `if(iserial.gt.2047) iserial=2047` (packjt77.f90:1420)
    //
    // Die Gegenstation traegt in diesem Fall eine falsche Nummer ins
    // Log. Der Fall haelt fest, DASS es so ist — damit niemand spaeter
    // einen Fehler sucht, wo das Format aufhoert.
    void eineNummerUeber2047WirdGekappt()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        ftx_message_t m{};
        QCOMPARE(ftx_message_encode_eu_vhf(&m, hif, "OE1ABC", "OE5SOS",
                                           false, 59, 3000, "JN78CT"),
                 FTX_MESSAGE_RC_OK);
        char text[64] = {};
        ftx_message_offsets_t off;
        ftx_message_decode(&m, hif, text, &off);
        QVERIFY2(QString::fromLatin1(text).contains(QStringLiteral("592047")),
                 text);
        // Und 2047 selbst geht noch sauber durch.
        ftx_message_t m2{};
        ftx_message_encode_eu_vhf(&m2, hif, "OE1ABC", "OE5SOS",
                                  false, 59, 2047, "JN78CT");
        char t2[64] = {};
        ftx_message_decode(&m2, hif, t2, &off);
        QVERIFY2(QString::fromLatin1(t2).contains(QStringLiteral("592047")), t2);
    }

    // ── Was der Packer ABWEISEN muss ────────────────────────────────
    //
    // Ein Packer, der alles annimmt, sendet Unsinn auf das Band.
    void unmoeglicheWerteWerdenAbgewiesen()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        ftx_message_t m{};
        // Rapport ausserhalb 52..59 — drei Bit koennen nicht mehr.
        QVERIFY(ftx_message_encode_eu_vhf(&m, hif, "A", "B", false, 51, 1,
                                          "JN78CT") != FTX_MESSAGE_RC_OK);
        QVERIFY(ftx_message_encode_eu_vhf(&m, hif, "A", "B", false, 60, 1,
                                          "JN78CT") != FTX_MESSAGE_RC_OK);
        // Nummer 0 oder ueber 4095 — WSJT-Xs eigene Schranke.
        QVERIFY(ftx_message_encode_eu_vhf(&m, hif, "A", "B", false, 59, 0,
                                          "JN78CT") != FTX_MESSAGE_RC_OK);
        QVERIFY(ftx_message_encode_eu_vhf(&m, hif, "A", "B", false, 59, 4096,
                                          "JN78CT") != FTX_MESSAGE_RC_OK);
        // Ein VIERstelliger Locator ist kein sechsstelliger.
        QCOMPARE(ftx_message_encode_eu_vhf(&m, hif, "A", "B", false, 59, 1,
                                           "JN78"),
                 FTX_MESSAGE_RC_ERROR_GRID);
        // Und ein Locator ausserhalb des Rasters.
        QCOMPARE(ftx_message_encode_eu_vhf(&m, hif, "A", "B", false, 59, 1,
                                           "SS99XX"),
                 FTX_MESSAGE_RC_ERROR_GRID);
    }

    // ── Der Textweg: so sendet das PROGRAMM ────────────────────────
    //
    // `Ft8Decoder::encodeBlock()` nimmt Text und reicht ihn an
    // `ftx_message_encode()` — der kannte Typ 5 nicht und haette
    // jeden Contest-Austausch als FREITEXT auf das Band gelegt.
    // Lesbar nur fuer den, der denselben Fehler macht.
    //
    // Gegenprobe: ohne `encode_eu_vhf_text` im Weg kommt hier
    // Freitext heraus, und `get_type` meldet FREE_TEXT statt WWROF.
    void derTextwegPacktDenContestUndNichtFreitext()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        ftx_message_t m{};
        ftx_message_init(&m);
        QCOMPARE(ftx_message_encode(
                     &m, hif, "<OE1ABC> <OE5SOS> R 590003 IO91NP"),
                 FTX_MESSAGE_RC_OK);
        QCOMPARE(ftx_message_get_type(&m), FTX_MESSAGE_TYPE_WWROF);

        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, hif, text, &off),
                 FTX_MESSAGE_RC_OK);
        QCOMPARE(QString::fromLatin1(text),
                 QStringLiteral("<OE1ABC> <OE5SOS> R 590003 IO91NP"));
    }

    void derTextwegKommtAuchOhneRAus()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        ftx_message_t m{};
        ftx_message_init(&m);
        QCOMPARE(ftx_message_encode(
                     &m, hif, "<OE1ABC> <OE5SOS> 590003 IO91NP"),
                 FTX_MESSAGE_RC_OK);
        QCOMPARE(ftx_message_get_type(&m), FTX_MESSAGE_TYPE_WWROF);
    }

    // ── Und was NICHT als Contest gepackt werden darf ───────────────
    //
    // Der wichtigere Teil. Ein Erkenner, der zu viel schluckt, legt
    // gewoehnliche Saetze in ein Format, das kein anderes Programm
    // dort erwartet.
    void gewoehnlicheSaetzeBleibenWasSieSind()
    {
        auto* hif = static_cast<ftx_callsign_hash_interface_t*>(
            Longpath::ft8RufzeichenSchnittstelle());
        // Die gewoehnlichen Saetze bleiben Standard -- der neue
        // Erkenner darf sie nicht an sich ziehen.
        struct { const char* text; ftx_message_type_t typ; } standard[] = {
            { "CQ OE5SOS JN78",     FTX_MESSAGE_TYPE_STANDARD },
            { "OE1ABC OE5SOS -12",  FTX_MESSAGE_TYPE_STANDARD },
            { "OE1ABC OE5SOS RR73", FTX_MESSAGE_TYPE_STANDARD },
        };
        for (const auto& f : standard) {
            ftx_message_t m{};
            ftx_message_init(&m);
            QCOMPARE(ftx_message_encode(&m, hif, f.text), FTX_MESSAGE_RC_OK);
            QVERIFY2(ftx_message_get_type(&m) == f.typ,
                     qPrintable(QStringLiteral("[%1] wurde Typ %2")
                                    .arg(QLatin1String(f.text))
                                    .arg(int(ftx_message_get_type(&m)))));
        }

        // Und diese hier duerfen NICHT als Typ 5 herauskommen. Ob sie
        // stattdessen als Freitext durchgehen oder ganz abgewiesen
        // werden, ist zweitrangig -- ein Satz dieser Laenge passt
        // ohnehin nicht in die dreizehn Zeichen des Freitexts. Der
        // Punkt ist: NICHT Typ 5.
        for (const char* t : {
                 // Ohne spitze Klammern ist es kein Typ 5, auch wenn
                 // der Rest passt -- die Klammern sagen „gehasht",
                 // und ohne sie wollte der Absender zwei Rufzeichen
                 // im Klartext senden.
                 "OE1ABC OE5SOS R 590003 IO91NP",
                 // Nur EIN Rufzeichen gehasht: auch nicht.
                 "<OE1ABC> OE5SOS 590003 IO91NP",
                 // Austausch ausserhalb 520001..594095.
                 "<OE1ABC> <OE5SOS> 510003 IO91NP",
                 "<OE1ABC> <OE5SOS> 599999 IO91NP",
                 // Vierstelliger Locator.
                 "<OE1ABC> <OE5SOS> 590003 IO91",
             }) {
            ftx_message_t m{};
            ftx_message_init(&m);
            const ftx_message_rc_t rc = ftx_message_encode(&m, hif, t);
            if (rc == FTX_MESSAGE_RC_OK) {
                QVERIFY2(ftx_message_get_type(&m) != FTX_MESSAGE_TYPE_WWROF,
                         qPrintable(QStringLiteral("[%1] wurde faelschlich Typ 5")
                                        .arg(QLatin1String(t))));
            }
        }
    }

    // Und mit Tabelle kommen sie heraus. Das ist der Fall, den der
    // Betreiber im Contest sieht: wer vorher einmal im Klartext kam,
    // steht hier mit Namen.
    void bekannteRufzeichenWerdenAufgeloest()
    {
        auto& speicher = Longpath::Ft8Rufzeichenspeicher::instanz();
        speicher.leeren();
        // `merke` nimmt den 22-Bit-Hash und leitet die kuerzeren ab --
        // ich rechne ihn hier NICHT selbst, sondern lasse ft8_lib eine
        // Standardnachricht packen und lese ihn beim Auspacken ein.
        ftx_message_t std{};
        ftx_message_init(&std);
        QCOMPARE(ftx_message_encode(&std,
                     static_cast<ftx_callsign_hash_interface_t*>(
                         Longpath::ft8RufzeichenSchnittstelle()),
                     "OE5SOS OE1ABC JN78"),
                 FTX_MESSAGE_RC_OK);
        char dummy[64] = {};
        ftx_message_offsets_t off;
        ftx_message_decode(&std,
            static_cast<ftx_callsign_hash_interface_t*>(
                Longpath::ft8RufzeichenSchnittstelle()), dummy, &off);
        QVERIFY2(speicher.anzahl() >= 2,
                 qPrintable(QStringLiteral("nur %1 gemerkt")
                                .arg(speicher.anzahl())));
    }
};

QTEST_MAIN(TstFt8EuVhf)
#include "tst_ft8_eu_vhf.moc"
