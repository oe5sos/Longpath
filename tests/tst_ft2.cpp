// no-port-check: Longpath-original test file. Die Bezugsdaten stammen
// aus MSHV (GPLv3, LZ2HV) und sind unten mit Fundstelle vermerkt.
// =================================================================
// tests/tst_ft2.cpp  (Longpath)
// =================================================================
//
// FT2 — die dritte Betriebsart neben FT8 und FT4.
//
// Betreiber, 2026-11-17: „FT2 - es gibt 2 open source FT2 Programme,
// bitte baue diese in longpath ein." Gemeint sind WSJT-X Improved 3.1
// (DG2YCB) und Decodium 4.0 (IU8LMC, der Erfinder der Betriebsart).
// Beide sind GPL und liegen im Quelltext vor; MSHV 2.76.7 (LZ2HV) hat
// FT2 ebenfalls und liegt in ~/Longpath/MSHV.
//
// ── Was alle drei uebereinstimmend sagen ────────────────────────────
//
// FT2 ist FT4 mit halber Symboldauer:
//
//   WSJT-X Improved  widgets/mainwindow.cpp:8189-8196  genft4_ fuer
//                    FT4 UND FT2, nur `nsps=4*288` statt `4*576`
//   Decodium 4.0     Modulator/FtxMessageEncoder.cpp   map_ft2_ft4_tones
//   MSHV 2.76.7      HvGenFt2/gen_ft2.cpp              make_c77_i4tone,
//                    Costas, rvec und Gray-Abbildung wie FT4
//
// Der Takt ist 3,75 s (`m_TRperiod=3.75` bei beiden), der Tonabstand
// 12000/288 = 41,67 Hz.
//
// ── Was dieser Lauf belegt ──────────────────────────────────────────
//
// 1. Longpath sendet DIESELBEN Toene wie MSHV — Ton fuer Ton gegen eine
//    Folge, die MSHVs eigener Sender erzeugt hat (nicht ft8_lib gegen
//    sich selbst).
// 2. Longpath LIEST, was MSHVs Sender ausgibt — mit dessen GFSK-Rechnung
//    nachgebaut und an die drei Sendezeitpunkte gelegt, die es am Band
//    gibt (0 / 150 / 300 ms).
// 3. Der eigene Rundlauf, die dB-Spalte, der Takt im Strom (Auswertung
//    VOR der Fenstergrenze), der Sendeplan, die Anrufstellen und der
//    Abstand der Plaetze.
//
// Modification history (Longpath):
//   2026-11-17 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QSignalSpy>

#include "core/DigiFrequenzen.h"
#include "core/Ft8Decoder.h"
#include "core/Ft8Stream.h"
#include "core/Ft8Tx.h"
#include "core/Ft8Uhrversatz.h"
#include "models/Band.h"

extern "C" {
#include "ft8/constants.h"
#include "ft8/encode.h"
#include "ft8/message.h"
}

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

// ── Bezugsfolgen aus MSHV ────────────────────────────────────────────
//
// Erzeugt am 2026-11-17 mit MSHV 2.76.7 `GenFt2::make_c77_i4tone`
// (HvMsPlayer/libsound/HvGenFt2/gen_ft2.cpp) und MSHVs eigenem
// LDPC-Kodierer `GenPomFt::encode174_91` (genpom.cpp), uebersetzt in
// ein Hilfsprogramm ausserhalb des Baums. Die 77 Nachrichtenbits kamen
// aus ft8_libs Packer -- verglichen wird also, was NACH dem Packen
// geschieht: XOR-Folge, CRC, LDPC, Gray-Abbildung, Synchronfolgen.
// Genau dort wuerde ein falsches FT2 entstehen.
//
// 103 Toene ohne die beiden Rampen (MSHV `nsym=103`).
struct Bezug {
    const char* text;
    const char* toene;
};
constexpr Bezug kMshvFolgen[] = {
    { "CQ OE5SOS JN78",
      "0132103311233031310102213320330311023010311033101031112002013320332310103030220321000300312310201323201" },
    { "JA1XYZ OE5SOS -11",
      "0132201111130112120102213320330311023033011003020003220130211133112310002131201121032230311013020303201" },
    { "OE5SOS JA1XYZ RR73",
      "0132223222331103210020302023302211023033013323000010210113302232302310122030132302232301330201213003201" },
};

constexpr int kFt2Symbole = 103;          // MSHV gen_ft2.cpp: nsym=103
constexpr int kRate       = Ft8Decoder::kSampleRate;
constexpr int kFensterMs  = 3750;

QVector<int> folgeAus(const char* ziffern)
{
    QVector<int> t;
    for (const char* p = ziffern; *p; ++p) { t << (*p - '0'); }
    return t;
}

// ── Ein FT2-Sender nach MSHV ─────────────────────────────────────────
//
// MSHV gen_ft2.cpp `GenFt2::genft2`, Zeilen 188-215: Frequenzpuls
// ueber drei Symbole (`gen_pulse_gfsk_(pulse, 1.5*nsps, 1.0, nsps)`,
// BT 1,0), Phasenschritt `dphi_peak*pulse*i4tone`, Laenge
// `(nsym+2)*nsps`, je eine Symbollaenge Kosinus-Rampe vorn und hinten.
// Die GFSK-Pulsformel ist die von WSJT-X (`gfsk_pulse.f90`).
//
// Bewusst NICHT ft8_libs `synth_gfsk`: die Rampe ist dort eine andere
// (ein Achtel Symbol), und der Lauf soll zeigen, dass Longpath ein
// FREMDES Signal liest.
double gfskPuls(double bt, double t)
{
    const double c = M_PI * std::sqrt(2.0 / std::log(2.0));
    return 0.5 * (std::erf(c * bt * (t + 0.5)) - std::erf(c * bt * (t - 0.5)));
}

QVector<float> mshvWelle(const QVector<int>& toene, double rate, double f0)
{
    const int nsps = int(std::lround(rate * 0.024));   // 288 @12k, 1152 @48k
    const int nsym = toene.size();
    const int nwave = (nsym + 2) * nsps;
    const double zweiPi = 2.0 * M_PI;

    std::vector<double> puls(size_t(3 * nsps));
    for (int i = 0; i < 3 * nsps; ++i) {
        puls[size_t(i)] = gfskPuls(1.0, (i - 1.5 * nsps) / double(nsps));
    }
    std::vector<double> dphi(size_t(nwave + 3 * nsps), 0.0);
    const double spitze = zweiPi / nsps;               // hmod = 1
    for (int j = 0; j < nsym; ++j) {
        for (int i = 0; i < 3 * nsps; ++i) {
            dphi[size_t(j * nsps + i)] += spitze * puls[size_t(i)] * toene[j];
        }
    }
    QVector<float> w(nwave);
    const double schritt = zweiPi * f0 / rate;
    double phi = 0.0;
    for (int k = 0; k < nwave; ++k) {
        w[k] = float(std::sin(phi));
        phi = std::fmod(phi + dphi[size_t(k)] + schritt, zweiPi);
    }
    for (int i = 0; i < nsps; ++i) {
        w[i] *= float((1.0 - std::cos(zweiPi * i / (2.0 * nsps))) / 2.0);
    }
    const int k2 = (nsym + 1) * nsps;
    for (int i = 0; i < nsps && k2 + i < nwave; ++i) {
        w[k2 + i] *= float((1.0 + std::cos(zweiPi * i / (2.0 * nsps))) / 2.0);
    }
    return w;
}

/// Ein ganzes 3,75-s-Fenster, Signal `versatzMs` nach der Grenze.
QVector<float> fensterMit(const QVector<float>& welle, double rate, int versatzMs)
{
    QVector<float> f(int(rate * kFensterMs / 1000), 0.0f);
    const int start = int(rate * versatzMs / 1000);
    for (int i = 0; i < welle.size() && start + i < f.size(); ++i) {
        f[start + i] = welle[i];
    }
    return f;
}

/// So dekodieren, wie Ft8Stream es tut: der Block beginnt
/// kFt2BlockVersatzMs nach der Grenze.
QVector<Ft8Decode> dekodiereWieDerStrom(const QVector<float>& fenster12k)
{
    const int versatz = kRate * Ft8Stream::kFt2BlockVersatzMs / 1000;
    return Ft8Decoder::decodeBlock(fenster12k.constData() + versatz,
                                   fenster12k.size() - versatz,
                                   Ft8Decoder::Protocol::Ft2);
}

QStringList wortlaute(const QVector<Ft8Decode>& funde)
{
    QStringList w;
    for (const Ft8Decode& d : funde) { w << d.text; }
    return w;
}

QVector<float> mitRauschen(const QVector<float>& rein, int nutzVon, int nutzLaenge,
                           double zielDb, uint32_t saat)
{
    // Leistung NUR ueber das Signal, nicht ueber die Stille daneben --
    // wie bei WSJT-X ist der Stoerabstand der des Signals.
    double leistung = 0.0;
    for (int i = nutzVon; i < nutzVon + nutzLaenge; ++i) {
        leistung += double(rein[i]) * double(rein[i]);
    }
    leistung /= double(nutzLaenge);
    const double imBand  = leistung / std::pow(10.0, zielDb / 10.0);
    const double jeProbe = imBand * ((kRate / 2.0) / 2500.0);
    std::mt19937 wuerfel(saat);
    std::normal_distribution<double> glocke(0.0, std::sqrt(jeProbe));
    QVector<float> aus = rein;
    for (float& v : aus) { v = float(double(v) + glocke(wuerfel)); }
    return aus;
}

}  // namespace

class TstFt2 : public QObject
{
    Q_OBJECT

private slots:
    // ── 1. Dieselben Toene wie MSHV ──────────────────────────────────
    void sendetDieselbenToeneWieMshv()
    {
        for (const Bezug& b : kMshvFolgen) {
            ftx_message_t m;
            ftx_message_init(&m);
            QCOMPARE(int(ftx_message_encode(&m, nullptr, b.text)),
                     int(FTX_MESSAGE_RC_OK));
            uint8_t eigene[FT4_NN];
            ft4_encode(m.payload, eigene);

            const QVector<int> mshv = folgeAus(b.toene);
            QCOMPARE(mshv.size(), kFt2Symbole);
            QCOMPARE(int(eigene[0]), 0);                  // Rampe vorn
            QCOMPARE(int(eigene[FT4_NN - 1]), 0);         // Rampe hinten
            int abweichend = 0;
            for (int i = 0; i < kFt2Symbole; ++i) {
                if (int(eigene[i + 1]) != mshv[i]) { ++abweichend; }
            }
            QVERIFY2(abweichend == 0,
                     qPrintable(QStringLiteral("%1: %2 von 103 Toenen anders "
                                               "als bei MSHV")
                                    .arg(QLatin1String(b.text)).arg(abweichend)));
        }
    }

    // ── 2. Was MSHV sendet, liest Longpath ───────────────────────────
    void liestMshvsSignal_data()
    {
        QTest::addColumn<int>("versatzMs");
        QTest::addColumn<int>("folge");
        // Die drei Sendezeitpunkte, die es am Band gibt:
        QTest::newRow("Decodium Linux 0 ms")      << 0   << 0;
        QTest::newRow("WSJT-X Improved 150 ms")   << 150 << 1;
        QTest::newRow("Decodium macOS 300 ms")    << 300 << 2;
        // Und mit Laufzeit im eigenen Tonweg obendrauf.
        QTest::newRow("300 ms + 100 ms Laufzeit") << 400 << 0;
    }

    void liestMshvsSignal()
    {
        QFETCH(int, versatzMs);
        QFETCH(int, folge);
        const Bezug& b = kMshvFolgen[folge];

        const QVector<float> welle = mshvWelle(folgeAus(b.toene), kRate, 1500.0);
        QCOMPARE(welle.size(), (kFt2Symbole + 2) * 288);
        const auto funde = dekodiereWieDerStrom(fensterMit(welle, kRate, versatzMs));
        qInfo().noquote() << versatzMs << "ms ->" << wortlaute(funde);
        QVERIFY2(wortlaute(funde).contains(QLatin1String(b.text)),
                 qPrintable(QStringLiteral("bei %1 ms gelesen: %2")
                                .arg(versatzMs)
                                .arg(wortlaute(funde).join(QLatin1String(" | ")))));

        const Ft8Decode& d = funde.first();
        QVERIFY2(std::abs(d.freqHz - 1500.0f) < 21.0f,      // halber Tonabstand
                 qPrintable(QStringLiteral("Tonlage %1 Hz").arg(double(d.freqHz))));
        // Der Zeitversatz zaehlt ab Blockbeginn (40 ms nach der Grenze),
        // auf ein halbes Symbol genau -- PLUS ein Symbol: ft8_lib meldet
        // den Anfang um ein Symbol spaet. Gemessen am 2026-11-17 gegen
        // MSHVs Welle bei 0/150/300/400 ms: +28, +22, +28, +24 ms.
        // Das liegt im Wasserfall, nicht an FT2: die FFT zu Block k
        // ueberspannt `nfft = 2 * block_size` Proben, die mit Block k
        // ENDEN (common/monitor.c), und gilt damit fuer seine Mitte.
        const double erwartet =
            (versatzMs - Ft8Stream::kFt2BlockVersatzMs) / 1000.0 + FT2_SYMBOL_PERIOD;
        QVERIFY2(std::abs(double(d.dtSeconds) - erwartet) <= 0.013,
                 qPrintable(QStringLiteral("dt %1 s statt %2")
                                .arg(double(d.dtSeconds)).arg(erwartet)));
    }

    // Die Grenze des Suchbandes: −10..+19 Symbole ab Blockbeginn, also
    // bis 40 + 456 = 496 ms. Bei 600 ms kann nichts mehr kommen -- das
    // haelt fest, dass die Zahlen in Ft8Stream.h stimmen und nicht nur
    // „irgendwie" funktionieren.
    void ausserhalbDesSuchbandesNichts()
    {
        const QVector<float> welle =
            mshvWelle(folgeAus(kMshvFolgen[0].toene), kRate, 1500.0);
        const auto funde = dekodiereWieDerStrom(fensterMit(welle, kRate, 600));
        QVERIFY2(funde.isEmpty(),
                 qPrintable(QStringLiteral("bei 600 ms gefunden: %1")
                                .arg(wortlaute(funde).join(QLatin1String(" | ")))));
    }

    // Und als FT4 gehoert ist dasselbe Signal nichts: der Dekoder
    // rechnet wirklich mit der FT2-Symboldauer.
    void alsFt4GehoertNichts()
    {
        const QVector<float> welle =
            mshvWelle(folgeAus(kMshvFolgen[0].toene), kRate, 1500.0);
        const QVector<float> f = fensterMit(welle, kRate, 150);
        const auto funde = Ft8Decoder::decodeBlock(f.constData(), f.size(),
                                                   Ft8Decoder::Protocol::Ft4);
        QVERIFY2(funde.isEmpty(), "ein FT2-Signal wurde als FT4 gelesen");
    }

    // ── 3. Der eigene Rundlauf ───────────────────────────────────────
    void eigenerRundlauf()
    {
        const QString text = QStringLiteral("CQ OE5SOS JN78");
        QVector<float> gemittet;
        QVERIFY(Ft8Decoder::encodeBlock(text, 1200.0f,
                                        Ft8Decoder::Protocol::Ft2, &gemittet));
        QCOMPARE(gemittet.size(), kRate * kFensterMs / 1000);   // 45000

        // So, wie Ft8Tx es hinausschickt: die Stille ueberspringen und
        // den Ton auf 150 ms legen.
        const int nutz  = int(0.5 + FT4_NN * FT2_SYMBOL_PERIOD * kRate);
        QCOMPARE(nutz, 30240);
        const int stille = (gemittet.size() - nutz) / 2;
        QCOMPARE(stille * 1000 / kRate, Ft8Tx::stilleMs(Ft8Decoder::Protocol::Ft2));
        QVector<float> nurSignal(gemittet.mid(stille, nutz));
        const auto funde = dekodiereWieDerStrom(fensterMit(nurSignal, kRate, 150));
        QVERIFY2(wortlaute(funde).contains(text),
                 qPrintable(wortlaute(funde).join(QLatin1String(" | "))));
        QVERIFY(std::abs(funde.first().freqHz - 1200.0f) < 21.0f);
    }

    // ── Die dB-Spalte ───────────────────────────────────────────────
    void stoerabstandStimmt_data()
    {
        QTest::addColumn<double>("zielDb");
        QTest::newRow("0 dB")   <<   0.0;
        QTest::newRow("-5 dB")  <<  -5.0;
        QTest::newRow("-10 dB") << -10.0;
        // Tiefer nicht: die Grenze liegt je nach Rauschwuerfel bei
        // −11 bis −12 dB (2026-11-17 mit fuenf Saaten gemessen), also
        // rund 3 dB ueber FT4 -- halbe Symboldauer, halbe Energie.
        // Decodium nennt fuer seinen eigenen Dekoder −12/−13 dB.
    }

    void stoerabstandStimmt()
    {
        QFETCH(double, zielDb);
        const QVector<float> welle =
            mshvWelle(folgeAus(kMshvFolgen[0].toene), kRate, 1500.0);
        const QVector<float> f = fensterMit(welle, kRate, 150);
        const int von = kRate * 150 / 1000;
        const QVector<float> ton = mitRauschen(f, von, welle.size(), zielDb, 2117u);
        const auto funde = dekodiereWieDerStrom(ton);
        QVERIFY2(!funde.isEmpty(),
                 qPrintable(QStringLiteral("FT2 bei %1 dB nichts").arg(zielDb)));
        qInfo().noquote() << QStringLiteral("FT2 Ziel %1 dB -> gemeldet %2 dB")
                                 .arg(zielDb, 0, 'f', 1)
                                 .arg(double(funde.first().snrDb), 0, 'f', 1);
        QVERIFY2(std::abs(double(funde.first().snrDb) - zielDb) <= 2.0,
                 qPrintable(QStringLiteral("FT2 Ziel %1 dB, gemeldet %2 dB")
                                .arg(zielDb).arg(double(funde.first().snrDb))));
    }

    // ── Der Takt im Strom: Auswertung VOR der Grenze ────────────────
    //
    // Bei FT2 bleibt nach der Grenze keine Zeit mehr zum Dekodieren:
    // die Antwort beginnt 150 ms danach. WSJT-X Improved wertet nach
    // 23*1728 Proben aus (3,312 s); Ft8Stream ebenso.
    void imStromWirdVorDerGrenzeGemeldet()
    {
        const Bezug& b = kMshvFolgen[1];
        // 48 kHz, so wie WDSP den Ton liefert -- MSHV erzeugt ihn
        // ohnehin mit 48 kHz (`nsps=4*288`).
        const QVector<float> welle = mshvWelle(folgeAus(b.toene), 48000.0, 1500.0);
        const QVector<float> mono = fensterMit(welle, 48000.0, 150);
        QVector<float> stereo;
        stereo.reserve(mono.size() * 2);
        for (float v : mono) { stereo << v << v; }

        Ft8Stream strom;
        strom.setProtocol(Ft8Decoder::Protocol::Ft2);
        QCOMPARE(strom.slotMs(), kFensterMs);
        qint64 jetzt = 999999990000LL;
        QVERIFY2(jetzt % kFensterMs == 0, "Start liegt nicht auf einer Grenze");
        const qint64 grenze = jetzt;
        strom.setClock([&jetzt]() { return jetzt; });
        QSignalSpy spion(&strom, &Ft8Stream::decoded);

        // In 20-ms-Stuecken, mit der Uhr im Gleichschritt.
        const int stueck = 960;                    // 20 ms bei 48 kHz
        qint64 gemeldetBei = -1;
        for (int pos = 0; pos + stueck <= mono.size(); pos += stueck) {
            strom.feedAudio(stereo.constData() + 2 * pos, stueck);
            jetzt = grenze + qint64(pos + stueck) * 1000 / 48000;
            if (gemeldetBei < 0 && spion.count() > 0) { gemeldetBei = jetzt - grenze; }
        }
        QCOMPARE(spion.count(), 1);
        qInfo() << "FT2 gemeldet" << gemeldetBei << "ms nach der Grenze";
        QVERIFY2(gemeldetBei > 0 && gemeldetBei < kFensterMs,
                 qPrintable(QStringLiteral("gemeldet bei %1 ms").arg(gemeldetBei)));
        QVERIFY2(gemeldetBei >= Ft8Stream::kFt2DekodierProben * 1000 / kRate,
                 "zu frueh gemeldet -- das Signal kann noch nicht durch sein");

        const auto funde = spion.at(0).at(0).value<QVector<Longpath::Ft8Decode>>();
        QVERIFY2(wortlaute(funde).contains(QLatin1String(b.text)),
                 qPrintable(wortlaute(funde).join(QLatin1String(" | "))));
        QCOMPARE(spion.at(0).at(1).toDateTime().toMSecsSinceEpoch(), grenze);

        // An der Grenze: KEINE zweite Meldung fuer dasselbe Fenster.
        jetzt = grenze + kFensterMs;
        strom.feedAudio(stereo.constData(), stueck);
        QCOMPARE(spion.count(), 1);
    }

    // ── Der Sendeplan ────────────────────────────────────────────────
    void sendeplan()
    {
        const auto p = Ft8Decoder::Protocol::Ft2;
        QCOMPARE(Ft8Tx::fensterMs(p), 3750);
        QCOMPARE(Ft8Tx::signalMs(p), 2520);               // 105 x 24 ms
        QCOMPARE(Ft8Tx::stilleMs(p), 615);                // (3750-2520)/2
        QCOMPARE(Ft8Tx::zielVersatzMs(p), qint64(150));   // WSJT-X Improved
        // FT8 und FT4 unveraendert.
        QCOMPARE(Ft8Tx::zielVersatzMs(Ft8Decoder::Protocol::Ft8), qint64(500));
        QCOMPARE(Ft8Tx::dekoderFristMs(Ft8Decoder::Protocol::Ft4), qint64(250));

        const Ft8Tx::Sendeplan s = Ft8Tx::sendeplan(
            p, Ft8Tx::dekoderFristMs(p), Ft8Tx::zielVersatzMs(p), 300);
        QCOMPARE(s.tastenNachMs, qint64(50));
        QCOMPARE(s.tonBeiMs, qint64(150));
        QCOMPARE(s.probenAbMs, qint64(615 + 50 - 150));
        // Getastet bis das Signal durch ist -- und vor dem Ende des
        // Fensters wieder auf Empfang, sonst geht der Anfang der
        // Antwort verloren.
        QVERIFY2(s.tastenNachMs + s.tastdauerMs < 3750,
                 qPrintable(QStringLiteral("getastet bis %1 ms")
                                .arg(s.tastenNachMs + s.tastdauerMs)));
        QVERIFY(s.tastenNachMs + s.tastdauerMs >= 150 + 2520);
    }

    void sechzehnFensterJeMinute()
    {
        const auto p = Ft8Decoder::Protocol::Ft2;
        const qint64 minute = 999999960000LL;              // volle Minute
        QCOMPARE(minute % 60000, qint64(0));
        for (int n = 0; n < 16; ++n) {
            const qint64 t = minute + qint64(n) * 3750 + 10;
            QCOMPARE(Ft8Tx::fensterNummer(t, p), n);
            QCOMPARE(Ft8Tx::istSendefenster(t, p, Ft8Periode::Gerade), n % 2 == 0);
        }
        // Von mitten in einem geraden Fenster bis zum naechsten geraden.
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute + 1000, p, Ft8Periode::Gerade),
                 qint64(7500 - 1000));
        QCOMPARE(Ft8Tx::bisZumNaechsten(minute + 1000, p, Ft8Periode::Ungerade),
                 qint64(3750 - 1000));
    }

    // ── Die Anrufstellen ─────────────────────────────────────────────
    // 20 m und 10 m wie Decodium; 40 m und 80 m so, wie am Band
    // tatsaechlich gesendet wird (PSK Reporter, 2026-11-17 -- siehe
    // DigiFrequenzen.cpp). Decodiums 7,062 / 3,568 waren in der Stunde
    // leer, waehrend auf 7,052 / 3,578 Europa lief.
    void anrufstellen()
    {
        using DigiFrequenzen::Digimodus;
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band20m, Digimodus::Ft2),
                 qint64(14'084'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band40m, Digimodus::Ft2),
                 qint64(7'052'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band80m, Digimodus::Ft2),
                 qint64(3'578'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band10m, Digimodus::Ft2),
                 qint64(28'184'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band6m, Digimodus::Ft2),
                 qint64(50'316'000));
        QVERIFY(DigiFrequenzen::stehtSchonDort(14'084'200, Digimodus::Ft2));
        QVERIFY(!DigiFrequenzen::stehtSchonDort(14'080'000, Digimodus::Ft2));
        QCOMPARE(QString::fromLatin1(DigiFrequenzen::name(Digimodus::Ft2)),
                 QStringLiteral("FT2"));

        // Keine FT2-Stelle darf mit 500 Hz Spielraum fuer eine FT8- oder
        // FT4-Stelle gehalten werden -- sonst stellte die Skala den
        // Dekoder auf das falsche Verfahren (Ft8Applet::folgeDerSkala).
        for (int i = 0; i < int(Band::Count); ++i) {
            const Band band = static_cast<Band>(i);
            const qint64 ft2 = DigiFrequenzen::anruffrequenz(band, Digimodus::Ft2);
            if (ft2 == 0) { continue; }
            QVERIFY2(!DigiFrequenzen::stehtSchonDort(ft2, Digimodus::Ft8),
                     qPrintable(QStringLiteral("%1 ist auch FT8").arg(ft2)));
            QVERIFY2(!DigiFrequenzen::stehtSchonDort(ft2, Digimodus::Ft4),
                     qPrintable(QStringLiteral("%1 ist auch FT4").arg(ft2)));
        }
    }

    // ── Die Plaetze liegen nicht uebereinander ───────────────────────
    void plaetzeLiegenNichtUebereinander()
    {
        const float breite = 4.0f * 12000.0f / 288.0f;     // 166,7 Hz
        const float abstand = Ft8TxSource::platzAbstandHz(Ft8Decoder::Protocol::Ft2);
        QCOMPARE(abstand, 250.0f);                          // MSHV gen_ft2.cpp:185
        QVERIFY(abstand > breite);
        const QVector<float> leiter = Ft8TxSource::tonleiter(1000.0f, 3, abstand);
        QCOMPARE(leiter.size(), 3);
        QCOMPARE(leiter[2] - leiter[1], 250.0f);
        // FT8 unveraendert.
        QCOMPARE(Ft8TxSource::platzAbstandHz(Ft8Decoder::Protocol::Ft8), 60.0f);
    }

    // ── Die Uhrwarnung ───────────────────────────────────────────────
    void uhrwarnungFuerFt2()
    {
        auto urteil = [](double dt) {
            Ft8Uhrversatz u;
            u.setzeVerfahren(Ft8Uhrversatz::Verfahren::Ft2);
            for (int p = 0; p < 4; ++p) {
                u.merke(dt);
                u.merke(dt);
                u.periodeFertig();
            }
            return u.lage();
        };
        // Decodium auf dem Mac sendet 300 ms nach der Grenze; das
        // meldet der Dekoder als +0,288 s (siehe liestMshvsSignal) --
        // und mit etwas Laufzeit im Tonweg ist es immer noch in Ordnung.
        QCOMPARE(urteil(0.288), Ft8Uhrversatz::Lage::Ok);
        QCOMPARE(urteil(0.33), Ft8Uhrversatz::Lage::Ok);
        QCOMPARE(urteil(0.38), Ft8Uhrversatz::Lage::Auffaellig);
        QCOMPARE(urteil(0.46), Ft8Uhrversatz::Lage::Schlecht);
    }
};

QTEST_MAIN(TstFt2)
#include "tst_ft2.moc"
