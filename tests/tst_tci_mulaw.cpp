// no-port-check: Longpath-eigen. mu-law ist kein Thetis-Format — TCI kennt
// nur Int16/Int24/Int32/Float32. Der Typ 101 ist eine Longpath-Erweiterung
// wie UInt8Dbm = 100 beim Spektrumstrom.
//
// ── Wozu ────────────────────────────────────────────────────────────────────
//
// Der Tonstrom ist mit Abstand der teuerste Teil der Handfunke: bei 12 kHz
// mono Int16 sind das 24,9 von 29,5 kB/s, ueber Mobilfunk 106 MB je Stunde.
// mu-law halbiert das auf 13,3 kB/s.
//
// Das ist nur dann ein guter Tausch, wenn man es NICHT hoert. Genau das misst
// dieser Pruefstand — nicht "der Code laeuft durch", sondern:
//
//   1. Der Stoerabstand an einem Signal, das wie Funkempfang aussieht.
//   2. Dass der Fehler dem PEGEL FOLGT (das ist der ganze Sinn des
//      Kompandierens) — bei 8 Bit linear bleibt er als fester Teppich liegen
//      und ist in CW-Pausen zu hoeren.
//   3. Dass beide Seiten dieselbe Kennlinie benutzen. Die Dekodierung steht
//      zweimal: hier in C++ und in handfunke/tci.js. Laufen sie
//      auseinander, klingt der Ton verzerrt statt falsch — und das faellt
//      niemandem auf.

#include <QtTest>
#include <cmath>
#include <vector>

#include "core/TciBinaryFrame.h"

using namespace Longpath;

namespace {

// Stoerabstand in dB zwischen Original und dekodierter Fassung.
double snrDb(const std::vector<float>& orig, const std::vector<float>& zurueck)
{
    double signal = 0.0, fehler = 0.0;
    for (size_t i = 0; i < orig.size(); ++i) {
        signal += double(orig[i]) * orig[i];
        const double d = double(orig[i]) - zurueck[i];
        fehler += d * d;
    }
    if (fehler <= 0.0) { return 999.0; }
    return 10.0 * std::log10(signal / fehler);
}

// Einmal durch Kodierung und Dekodierung.
std::vector<float> rundlauf(const std::vector<float>& in, TciSampleType typ)
{
    const QByteArray roh = TciBinaryFrame::encodeSamples(
        in.data(), static_cast<int>(in.size()), static_cast<int>(typ));
    return TciBinaryFrame::decodeSamples(roh, 0, static_cast<int>(in.size()),
                                         static_cast<int>(typ));
}

// Ein Signal, das aussieht wie Funkempfang: Sprachband-Ton auf Rauschen.
std::vector<float> funkton(int n, double amplitude)
{
    std::vector<float> v(static_cast<std::size_t>(n), 0.0f);
    // Fester Pseudozufall — Math.random/Date sind in Pruefstaenden unerwuenscht
    // und ein reproduzierbarer Lauf ist ohnehin besser.
    quint32 z = 22031969u;
    for (int i = 0; i < n; ++i) {
        z = z * 1664525u + 1013904223u;
        const double rausch = (double(z >> 8) / double(0x00FFFFFF) - 0.5) * 0.04;
        const double ton = 0.7 * std::sin(2.0 * M_PI * 800.0 * i / 12000.0)
                         + 0.3 * std::sin(2.0 * M_PI * 1900.0 * i / 12000.0);
        v[size_t(i)] = float(amplitude * (ton + rausch));
    }
    return v;
}

}  // namespace

class TestTciMuLaw : public QObject {
    Q_OBJECT

private slots:
    // ── Ein Byte je Abtastung, das ist der ganze Zweck ──────────────────────
    void ein_byte_je_abtastung()
    {
        QCOMPARE(TciBinaryFrame::bytesPerSample(
                     static_cast<int>(TciSampleType::MuLaw8)), 1);

        const std::vector<float> in = funkton(1200, 0.5);
        const QByteArray roh = TciBinaryFrame::encodeSamples(
            in.data(), int(in.size()), int(TciSampleType::MuLaw8));
        QCOMPARE(roh.size(), 1200);

        // Zum Vergleich: dasselbe in Int16.
        const QByteArray i16 = TciBinaryFrame::encodeSamples(
            in.data(), int(in.size()), int(TciSampleType::Int16));
        QCOMPARE(i16.size(), 2400);
    }

    // ── Der Stoerabstand bei normalem Pegel ─────────────────────────────────
    void stoerabstand_traegt_sprache()
    {
        const std::vector<float> in = funkton(12000, 0.5);
        const double snr = snrDb(in, rundlauf(in, TciSampleType::MuLaw8));
        qInfo() << "mu-law bei halbem Pegel:" << snr << "dB";
        QVERIFY2(snr > 30.0,
                 qPrintable(QStringLiteral("Nur %1 dB — das waere hoerbar")
                                .arg(snr, 0, 'f', 1)));
    }

    // ── Der eigentliche Punkt: der Fehler folgt dem Pegel ───────────────────
    //
    // Ein leises Signal muss denselben Stoerabstand bekommen wie ein lautes.
    // Genau das kann 8 Bit linear nicht, und genau deshalb ist mu-law die
    // richtige Wahl und nicht einfach "die Haelfte der Bits".
    void fehler_folgt_dem_pegel()
    {
        const std::vector<float> laut  = funkton(12000, 0.8);
        const std::vector<float> leise = funkton(12000, 0.02);   // 32 dB leiser

        const double snrLaut  = snrDb(laut,  rundlauf(laut,  TciSampleType::MuLaw8));
        const double snrLeise = snrDb(leise, rundlauf(leise, TciSampleType::MuLaw8));
        qInfo() << "mu-law laut:" << snrLaut << "dB, leise:" << snrLeise << "dB";

        QVERIFY2(snrLeise > 25.0,
                 qPrintable(QStringLiteral(
                     "Leise nur %1 dB — dann rauscht es in den CW-Pausen")
                     .arg(snrLeise, 0, 'f', 1)));
        // Der Abstand zwischen laut und leise darf klein bleiben; bei einem
        // linearen Format waere er ungefaehr so gross wie der Pegelunterschied.
        QVERIFY2(std::fabs(snrLaut - snrLeise) < 12.0,
                 qPrintable(QStringLiteral(
                     "Laut %1 dB gegen leise %2 dB — der Fehler folgt dem "
                     "Pegel nicht, dann kompandiert die Kennlinie nicht")
                     .arg(snrLaut, 0, 'f', 1).arg(snrLeise, 0, 'f', 1)));
    }

    // ── Randwerte kippen nicht ──────────────────────────────────────────────
    void randwerte_bleiben_heil()
    {
        const std::vector<float> rand = {
            0.0f, 1.0f, -1.0f, 0.999999f, -0.999999f,
            1.5f, -1.5f,                 // ueber Vollaussteuerung: klemmen
            1.0f / 32768.0f, -1.0f / 32768.0f,
        };
        const std::vector<float> zurueck = rundlauf(rand, TciSampleType::MuLaw8);
        QCOMPARE(zurueck.size(), rand.size());
        for (size_t i = 0; i < rand.size(); ++i) {
            QVERIFY2(zurueck[i] >= -1.05f && zurueck[i] <= 1.05f,
                     qPrintable(QStringLiteral("Wert %1 kam als %2 zurueck")
                                    .arg(double(rand[i])).arg(double(zurueck[i]))));
            // Das Vorzeichen muss stimmen — ein Kipper waere als Knacken
            // hoerbar.
            if (std::fabs(rand[i]) > 0.01f) {
                QVERIFY2((rand[i] < 0) == (zurueck[i] < 0),
                         qPrintable(QStringLiteral(
                             "Vorzeichen gekippt: %1 wurde %2")
                             .arg(double(rand[i])).arg(double(zurueck[i]))));
            }
        }
    }

    // ── Gegen die NORM, nicht gegen uns selbst ──────────────────────────────
    //
    // Der Pruefpunkt darunter vergleicht C++ mit einer hier nachgebauten
    // Fassung der JavaScript-Rechnung. Das faengt Drift zwischen den beiden
    // Umsetzungen — aber nicht den Fall, dass BEIDE denselben Fehler haben.
    // Genau der ist am 2026-09-30 eingetreten: der Kodierer lieferte -2,5 dB
    // Stoerabstand, und dieser Vergleich blieb gruen, weil er die Dekodierung
    // prueft und beide Dekodierer richtig waren.
    //
    // Also eine dritte, unabhaengige Referenz: feste Wertepaare aus ITU-T
    // G.711 beziehungsweise der Sun-Referenz g711.c. Die stehen hier als
    // Zahlen und nicht als Formel — eine Formel koennte denselben Denkfehler
    // tragen wie der Code.
    void normwerte_stimmen()
    {
        struct Paar { quint8 byte; int pcm; const char* was; };
        // ulaw2linear() der Sun-Referenz, nachgeschlagen, nicht gerechnet:
        static const Paar kDekodiert[] = {
            { 0xFF,      0, "Null, positives Vorzeichen" },
            { 0x7F,      0, "Null, negatives Vorzeichen" },
            { 0x00, -32124, "Vollaussteuerung negativ" },
            { 0x80,  32124, "Vollaussteuerung positiv" },
            // Segment 0, Mantisse 15. Nachgerechnet an der Sun-Formel,
            // Schritt fuer Schritt — der erste Entwurf dieser Tabelle hatte
            // hier 264 stehen, aus dem Kopf geschrieben statt gerechnet:
            //   u_val = ~0xF0 = 0x0F
            //   t = ((0x0F & 0xF) << 3) + 0x84 = 120 + 132 = 252
            //   t <<= (0x0F & 0x70) >> 4 = 0            -> 252
            //   (0x0F & 0x80) == 0, also t - 0x84       -> 120
            { 0xF0,    120, "Segment 0, Mantisse 15" },
            { 0x70,   -120, "dasselbe negativ" },
        };
        QByteArray eins(1, '\0');
        for (const auto& p : kDekodiert) {
            eins[0] = char(p.byte);
            const auto v = TciBinaryFrame::decodeSamples(
                eins, 0, 1, int(TciSampleType::MuLaw8));
            QCOMPARE(v.size(), size_t(1));
            const int gerechnet = int(std::lround(double(v[0]) * 32768.0));
            QVERIFY2(std::abs(gerechnet - p.pcm) <= 1,
                     qPrintable(QStringLiteral(
                         "Byte 0x%1 (%2): G.711 sagt %3, wir rechnen %4")
                         .arg(p.byte, 2, 16, QLatin1Char('0'))
                         .arg(QLatin1String(p.was)).arg(p.pcm).arg(gerechnet)));
        }

        // Und die Gegenrichtung: linear2ulaw der Referenz.
        struct Paar2 { float wert; quint8 byte; };
        static const Paar2 kKodiert[] = {
            { 0.0f,      0xFF },   // Null -> 0xFF
            { 1.0f,      0x80 },   // Vollaussteuerung positiv
            { -1.0f,     0x00 },   // Vollaussteuerung negativ
        };
        for (const auto& p : kKodiert) {
            const QByteArray roh = TciBinaryFrame::encodeSamples(
                &p.wert, 1, int(TciSampleType::MuLaw8));
            QCOMPARE(roh.size(), 1);
            const quint8 gerechnet = quint8(roh.at(0));
            QVERIFY2(gerechnet == p.byte,
                     qPrintable(QStringLiteral(
                         "Wert %1: G.711 sagt 0x%2, wir kodieren 0x%3")
                         .arg(double(p.wert))
                         .arg(p.byte, 2, 16, QLatin1Char('0'))
                         .arg(gerechnet, 2, 16, QLatin1Char('0'))));
        }
    }

    // ── Die Klemme haelt ────────────────────────────────────────────────────
    //
    // kClip = 32635 ist die einzige Wand gegen Segment 8, das es nicht gibt:
    // ohne sie liefe die Segmentsuche bei sehr grossen Werten ueber die
    // Tabelle hinaus. Bisher hat kein Pruefpunkt sie beruehrt.
    void klemme_haelt_gegen_segment_acht()
    {
        // Weit ueber Vollaussteuerung, in beide Richtungen.
        const std::vector<float> wild = { 1.5f, -1.5f, 10.0f, -10.0f,
                                          1.0f, -1.0f, 0.9999f };
        const QByteArray roh = TciBinaryFrame::encodeSamples(
            wild.data(), int(wild.size()), int(TciSampleType::MuLaw8));
        QCOMPARE(roh.size(), int(wild.size()));

        // Jedes Byte muss ein gueltiges mu-law-Byte sein, und das Segment
        // (die drei Bits 4..6 des INVERTIERTEN Bytes) darf nie 8 ergeben —
        // mehr als 7 passt gar nicht hinein, ein Ueberlauf zeigte sich also
        // als Vorzeichenkipper oder als falsches Segment.
        for (int i = 0; i < roh.size(); ++i) {
            const int u = (~int(quint8(roh.at(i)))) & 0xFF;
            const int seg = (u >> 4) & 0x07;
            QVERIFY2(seg <= 7, "Segment ausserhalb der Tabelle");
            // Das Vorzeichen muss das des Eingangs sein.
            const bool negativ = (u & 0x80) != 0;
            QVERIFY2(negativ == (wild[size_t(i)] < 0.0f),
                     qPrintable(QStringLiteral("Vorzeichen gekippt bei %1")
                                    .arg(double(wild[size_t(i)]))));
        }

        // Und zurueck: nichts darf ueber Vollaussteuerung herauskommen.
        const auto zurueck = TciBinaryFrame::decodeSamples(
            roh, 0, int(wild.size()), int(TciSampleType::MuLaw8));
        for (float v : zurueck) {
            QVERIFY2(v >= -1.0f && v <= 1.0f,
                     qPrintable(QStringLiteral("Wert %1 ausserhalb [-1,1]")
                                    .arg(double(v))));
        }
    }

    // ── Beide Seiten rechnen gleich ─────────────────────────────────────────
    //
    // Bildet die Dekodiertabelle aus handfunke/tci.js (case 101) Zeile fuer
    // Zeile nach und vergleicht sie mit der C++-Dekodierung. Laufen die
    // beiden auseinander, hoert Martin einen verzerrten Ton und niemand
    // weiss, warum.
    void browser_und_server_rechnen_gleich()
    {
        // Genau die Rechnung aus tci.js:
        //   const inv = ~u & 0xFF;
        //   const vorzeichen = (inv & 0x80) ? -1 : 1;
        //   const segment = (inv >> 4) & 0x07;
        //   const mantisse = inv & 0x0F;
        //   let betrag = ((mantisse << 3) + 0x84) << segment;
        //   betrag -= 0x84;
        //   tabelle[u] = vorzeichen * betrag / 32768;
        std::vector<float> ausJs(256);
        for (int u = 0; u < 256; ++u) {
            const int inv = (~u) & 0xFF;
            const int vorzeichen = (inv & 0x80) ? -1 : 1;
            const int segment  = (inv >> 4) & 0x07;
            const int mantisse = inv & 0x0F;
            int betrag = ((mantisse << 3) + 0x84) << segment;
            betrag -= 0x84;
            ausJs[size_t(u)] = float(vorzeichen * betrag) / 32768.0f;
        }

        // Dieselben 256 Bytes durch die C++-Dekodierung.
        QByteArray alle(256, '\0');
        for (int u = 0; u < 256; ++u) { alle[u] = char(u); }
        const std::vector<float> ausCpp = TciBinaryFrame::decodeSamples(
            alle, 0, 256, int(TciSampleType::MuLaw8));

        QCOMPARE(ausCpp.size(), size_t(256));
        for (int u = 0; u < 256; ++u) {
            QVERIFY2(std::fabs(ausCpp[size_t(u)] - ausJs[size_t(u)]) < 1e-6f,
                     qPrintable(QStringLiteral(
                         "Byte %1: C++ gibt %2, der Browser %3")
                         .arg(u).arg(double(ausCpp[size_t(u)]))
                         .arg(double(ausJs[size_t(u)]))));
        }
    }
};

QTEST_GUILESS_MAIN(TestTciMuLaw)
#include "tst_tci_mulaw.moc"
