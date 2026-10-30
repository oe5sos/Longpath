// =================================================================
// tests/tst_ft8_dxpedition.cpp  (Longpath)
// =================================================================
//
// Das Nachrichtenformat 0.1 — „DXpedition Mode", also Fox/Hound.
//
// ── Warum das ueberhaupt gebaut wurde ───────────────────────────────
//
// ft8_lib laesst diesen Typ im switch von `ftx_message_decode()` in den
// default-Zweig laufen und gibt FTX_MESSAGE_RC_ERROR_TYPE zurueck:
// JEDE Nachricht einer DX-Expedition im Fox-Betrieb faellt dort still
// durch. Der Hound sieht nie, dass er aufgerufen wurde — und von
// aussen sieht das aus, als sei die Expedition nicht zu hoeren.
//
// ── Warum die Rufzeichen aus ft8_libs eigenem Packer kommen ─────────
//
// Ein reiner Rundlauf (selbst packen, selbst auspacken) waere gruen,
// auch wenn meine Bitreihenfolge nicht die von WSJT-X ist: derselbe
// Fehler in beide Richtungen hebt sich auf. Darum werden die beiden
// 28-Bit-Rufzeichenfelder hier NICHT selbst gerechnet, sondern aus
// einer Standardnachricht gezogen, die `ftx_message_encode_std`
// gepackt hat. Was dabei geprueft wird, ist genau das, was neu ist:
// wo die vier Felder in der Nutzlast liegen, und wie der Rapport
// gerechnet wird.
//
// Longpath-original.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original test file.

#include <QtTest>

extern "C" {
#include "ft8/message.h"
}

#include <cstring>

namespace {

// Die zwei 28-Bit-Felder aus einer Standardnachricht ziehen. Dort
// liegen sie als 29 Bit (28 + „portable"-Bit), und das Auspacken
// darunter ist Zeile fuer Zeile dasselbe wie in
// `ftx_message_decode_std`.
void rufzeichenfelderAus(const ftx_message_t& m,
                         quint32* n28a, quint32* n28b)
{
    quint32 n29a = (quint32(m.payload[0]) << 21);
    n29a |= (quint32(m.payload[1]) << 13);
    n29a |= (quint32(m.payload[2]) << 5);
    n29a |= (quint32(m.payload[3]) >> 3);

    quint32 n29b = (quint32(m.payload[3] & 0x07u) << 26);
    n29b |= (quint32(m.payload[4]) << 18);
    n29b |= (quint32(m.payload[5]) << 10);
    n29b |= (quint32(m.payload[6]) << 2);
    n29b |= (quint32(m.payload[7]) >> 6);

    *n28a = n29a >> 1;
    *n28b = n29b >> 1;
}

// Eine 0.1-Nutzlast bauen: 28 + 28 + 10 + 5 Bit, dann i3 = 0, n3 = 1.
ftx_message_t baue01(quint32 n28a, quint32 n28b, quint16 n10, quint8 n5)
{
    ftx_message_t m;
    ftx_message_init(&m);
    std::memset(m.payload, 0, sizeof(m.payload));

    m.payload[0] = quint8((n28a >> 20) & 0xFF);
    m.payload[1] = quint8((n28a >> 12) & 0xFF);
    m.payload[2] = quint8((n28a >> 4) & 0xFF);
    m.payload[3] = quint8(((n28a & 0x0F) << 4) | ((n28b >> 24) & 0x0F));
    m.payload[4] = quint8((n28b >> 16) & 0xFF);
    m.payload[5] = quint8((n28b >> 8) & 0xFF);
    m.payload[6] = quint8(n28b & 0xFF);
    m.payload[7] = quint8((n10 >> 2) & 0xFF);
    m.payload[8] = quint8(((n10 & 0x03) << 6) | ((n5 & 0x1F) << 1));
    // n3 = 1 steht in den Bits 71..73, i3 = 0 in 74..76.
    //
    // Beim ersten Versuch stand hier `1u << 5` — das traf Bit 74 und
    // damit das i3-Feld. Der Dekoder las dann eine Nachricht mit
    // nichtstandardmaeszigen Rufzeichen und gab „<...> 0V9K7WLGC54"
    // heraus. Die Bitlage steht in `ftx_message_get_n3`:
    //   n3 = ((payload[8] << 2) & 0x04) | ((payload[9] >> 6) & 0x03)
    // Bit 71 ist also das unterste Bit von payload[8], und 72/73 sind
    // die beiden obersten von payload[9].
    m.payload[9] = quint8(0x40);   // Bit 73 gesetzt => n3 = 1
    return m;
}

}  // namespace

class TestFt8Dxpedition : public QObject
{
    Q_OBJECT

private:
    quint32 m_k1abc{0};
    quint32 m_w9xyz{0};

private slots:
    void initTestCase()
    {
        // Die beiden Rufzeichen aus ft8_libs eigenem Packer.
        ftx_message_t std;
        ftx_message_init(&std);
        QCOMPARE(ftx_message_encode_std(&std, nullptr, "K1ABC", "W9XYZ",
                                        "RR73"),
                 FTX_MESSAGE_RC_OK);
        rufzeichenfelderAus(std, &m_k1abc, &m_w9xyz);
        QVERIFY2(m_k1abc > 2 && m_w9xyz > 2,
                 "die Rufzeichenfelder sehen nach Sonderwerten aus");
    }

    // ── Die Gegenprobe: so war es vorher ─────────────────────────────
    void vorherFielDieseNachrichtDurch()
    {
        // Genau derselbe Satz Bits, nur durch den Weg, den upstream
        // nimmt: `ftx_message_get_type` liefert den Typ richtig, und
        // GENAU DARUM stand er vorher im default-Zweig.
        const ftx_message_t m = baue01(m_k1abc, m_w9xyz, 0x155, 10);
        QCOMPARE(ftx_message_get_type(&m), FTX_MESSAGE_TYPE_DXPEDITION);
        QCOMPARE(ftx_message_get_i3(&m), quint8(0));
        QCOMPARE(ftx_message_get_n3(&m), quint8(1));
    }

    void dieBeidenRufzeichenKommenHeraus()
    {
        const ftx_message_t m = baue01(m_k1abc, m_w9xyz, 0x155, 10);
        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, nullptr, text, &off),
                 FTX_MESSAGE_RC_OK);
        const QString s = QString::fromLatin1(text);
        QVERIFY2(s.startsWith(QStringLiteral("K1ABC RR73; W9XYZ ")),
                 qPrintable(s));
    }

    void derUnbekannteFoxStehtAlsDreiPunkte()
    {
        // Ohne Hash-Tabelle kann das dritte Rufzeichen nicht aufgeloest
        // werden -- im Paket stehen nur zehn Bit. „<...>" ist dann die
        // ehrliche Antwort, kein Fehler. WSJT-X macht es genauso.
        const ftx_message_t m = baue01(m_k1abc, m_w9xyz, 0x155, 10);
        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, nullptr, text, &off),
                 FTX_MESSAGE_RC_OK);
        QVERIFY2(QString::fromLatin1(text).contains(QStringLiteral("<...>")),
                 text);
    }

    // ── Die Falle, in die man bei diesem Format tritt ────────────────
    void derRapportLaeuftInZweiDbSchritten()
    {
        // From WSJT-X packjt77.f90:313 [v2.7.0] -- `irpt = 2*n5 - 30`.
        // Wer hier einen Schritt von 1 dB annimmt, liest jeden Rapport
        // halb so gross und verschiebt ihn um 15 dB.
        struct { quint8 n5; const char* erwartet; } faelle[] = {
            {  0, "-30" },
            {  5, "-20" },
            { 10, "-10" },
            { 15, "+00" },
            { 20, "+10" },
            { 31, "+32" },
        };
        for (const auto& f : faelle) {
            const ftx_message_t m = baue01(m_k1abc, m_w9xyz, 0x155, f.n5);
            char text[64] = {};
            ftx_message_offsets_t off;
            QCOMPARE(ftx_message_decode(&m, nullptr, text, &off),
                     FTX_MESSAGE_RC_OK);
            const QString s = QString::fromLatin1(text);
            QVERIFY2(s.endsWith(QString::fromLatin1(f.erwartet)),
                     qPrintable(QStringLiteral("n5=%1 ergab \"%2\", "
                                               "erwartet war Endung %3")
                                    .arg(f.n5).arg(s)
                                    .arg(QString::fromLatin1(f.erwartet))));
        }
    }

    // Und die Gegenprobe dazu: mit einem Dezibel je Schritt kaeme bei
    // n5 = 10 die -20 heraus, nicht die -10. Steht der Faktor falsch,
    // faellt genau dieser Vergleich um.
    void mitEinemDbJeSchrittWaereEsFalsch()
    {
        const int n5 = 10;
        QCOMPARE(2 * n5 - 30, -10);
        QVERIFY2(1 * n5 - 30 != -10,
                 "wenn beide Rechnungen dasselbe ergaeben, prueft der "
                 "Fall darueber nichts");
    }

    void dieFelderLiegenNichtUebereinander()
    {
        // Zwei Nachrichten, die sich NUR im Rapport unterscheiden,
        // muessen dieselben Rufzeichen haben. Lagen die Felder falsch,
        // wuerde der Rapport in das zweite Rufzeichen bluten.
        char a[64] = {}, b[64] = {};
        ftx_message_offsets_t off;
        const ftx_message_t m1 = baue01(m_k1abc, m_w9xyz, 0x155, 3);
        const ftx_message_t m2 = baue01(m_k1abc, m_w9xyz, 0x155, 28);
        QCOMPARE(ftx_message_decode(&m1, nullptr, a, &off),
                 FTX_MESSAGE_RC_OK);
        QCOMPARE(ftx_message_decode(&m2, nullptr, b, &off),
                 FTX_MESSAGE_RC_OK);
        const QString sa = QString::fromLatin1(a);
        const QString sb = QString::fromLatin1(b);
        QCOMPARE(sa.section(QLatin1Char(' '), 0, 2),
                 sb.section(QLatin1Char(' '), 0, 2));
        QVERIFY(sa != sb);   // der Rapport unterscheidet sie sehr wohl
    }

    // ── Der EU-VHF-Contest-Austausch (2026-10-30) ───────────────────
    //
    // Betreiber: „wie weit sind wir jetzt beim anderen ft8 contest
    // programm" (MSHV). Meine erste Antwort war FALSCH -- ich hatte
    // behauptet, ft8_lib dekodiere die Contest-Formate nicht, und
    // dabei den einen uebersehen, der ihn betrifft.
    //
    // From WSJT-X lib/77bit/packjt77.f90:460-461 [v2.7.0] --
    //   else if(i3.eq.1 .or. i3.eq.2) then
    //   ! Type 1 (standard message) or Type 2 („/P" form for EU VHF
    //   ! contest)
    //
    // Typ 2 wird GENAUSO ausgepackt wie der Standardsatz; nur das
    // Portable-Bit heiszt dort „/P" statt „/R". ft8_lib bildet beide
    // auf FTX_MESSAGE_TYPE_STANDARD ab und setzt das Suffix nach i3
    // (message.c, unpack28: `if (i3 == 1) "/R" else if (i3 == 2)
    // "/P"`). Der europaeische VHF-Contest laeuft also bereits.
    //
    // Dieser Lauf nagelt das fest, damit die Aussage geprueft ist und
    // nicht behauptet.
    void derEuVhfAustauschWirdGelesen()
    {
        ftx_message_t m;
        ftx_message_init(&m);
        QCOMPARE(ftx_message_encode_std(&m, nullptr, "PA3XYZ/P",
                                        "GM4ABC/P", "JO22"),
                 FTX_MESSAGE_RC_OK);
        // i3 = 2: das ist die „/P"-Form, nicht der gewoehnliche
        // Standardsatz.
        QCOMPARE(ftx_message_get_i3(&m), quint8(2));
        QCOMPARE(ftx_message_get_type(&m), FTX_MESSAGE_TYPE_STANDARD);

        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, nullptr, text, &off),
                 FTX_MESSAGE_RC_OK);
        const QString t = QString::fromLatin1(text);
        QVERIFY2(t.contains(QStringLiteral("PA3XYZ/P")), qPrintable(t));
        QVERIFY2(t.contains(QStringLiteral("GM4ABC/P")), qPrintable(t));
        QVERIFY2(t.contains(QStringLiteral("JO22")), qPrintable(t));
    }

    // GEGENPROBE: derselbe Satz ohne „/P" ist i3 = 1 und bekommt kein
    // Suffix. Ohne diesen Lauf belegt der Fall darueber nicht, dass
    // das „/P" aus dem FORMAT kommt und nicht aus dem Text.
    void ohneSchraegstrichPIstEsDerGewoehnlicheSatz()
    {
        ftx_message_t m;
        ftx_message_init(&m);
        QCOMPARE(ftx_message_encode_std(&m, nullptr, "PA3XYZ", "GM4ABC",
                                        "JO22"),
                 FTX_MESSAGE_RC_OK);
        QCOMPARE(ftx_message_get_i3(&m), quint8(1));

        char text[64] = {};
        ftx_message_offsets_t off;
        QCOMPARE(ftx_message_decode(&m, nullptr, text, &off),
                 FTX_MESSAGE_RC_OK);
        QVERIFY2(!QString::fromLatin1(text).contains(QStringLiteral("/P")),
                 text);
    }

    // Und was WIRKLICH fehlt, damit es nicht wieder geraten wird.
    //
    // Diese drei Typen laufen in ft8_libs `switch` weiter in den
    // default-Zweig. Alle drei sind US-Conteste; fuer den
    // europaeischen VHF-Betrieb spielt keiner eine Rolle. Faellt einer
    // davon irgendwann durch, soll dieser Lauf umkippen und sagen,
    // dass die Lage sich geaendert hat.
    void dieDreiUsContestformateFehlenWeiterhin()
    {
        // ARRL Field Day (0.3/0.4), ARRL RTTY Roundup (i3=3),
        // WWROF (i3=5).
        struct { quint8 p8; quint8 p9; const char* name; } faelle[] = {
            { 0x00, 0xC0, "ARRL Field Day (0.3)" },   // n3 = 3
            { 0x00, 0x18, "ARRL RTTY Roundup" },      // i3 = 3
            { 0x00, 0x28, "WWROF" },                  // i3 = 5
        };
        for (const auto& f : faelle) {
            ftx_message_t m;
            ftx_message_init(&m);
            std::memset(m.payload, 0, sizeof(m.payload));
            m.payload[8] = f.p8;
            m.payload[9] = f.p9;
            char text[64] = {};
            ftx_message_offsets_t off;
            QVERIFY2(ftx_message_decode(&m, nullptr, text, &off)
                         == FTX_MESSAGE_RC_ERROR_TYPE,
                     f.name);
        }
    }

};

QTEST_MAIN(TestFt8Dxpedition)
#include "tst_ft8_dxpedition.moc"
