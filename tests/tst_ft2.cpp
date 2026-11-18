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
#include "core/AppSettings.h"
#include "core/Ft8Apriori.h"
#include "gui/applets/Ft8Applet.h"
#include "models/Band.h"
#include "models/LogEntry.h"
#include "models/RadioModel.h"

#include <QPushButton>

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

/// So dekodieren, wie Ft8Stream es tut -- DERSELBE Weg, nicht
/// nachgebaut: beide Bloecke, zusammengefuehrt.
QVector<Ft8Decode> dekodiereWieDerStrom(const QVector<float>& fenster12k)
{
    return Ft8Stream::dekodiereFt2(fenster12k.constData(), fenster12k.size());
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
        // Und mit Laufzeit im eigenen Tonweg obendrauf. Die beiden
        // letzten fielen vor dem zweiten Block durch (Suchband bis
        // +496 ms) -- genau das war am Geraet zu sehen: kraeftige
        // FT2-Signale im Wasserfall, 0 Funde (FLEX-8400, 2026-11-17).
        QTest::newRow("300 ms + 100 ms Laufzeit") << 400 << 0;
        QTest::newRow("150 ms + 500 ms Laufzeit") << 650 << 1;
        QTest::newRow("300 ms + 600 ms Laufzeit") << 900 << 2;
    }

    void liestMshvsSignal()
    {
        QFETCH(int, versatzMs);
        QFETCH(int, folge);
        const Bezug& b = kMshvFolgen[folge];

        const QVector<float> welle = mshvWelle(folgeAus(b.toene), kRate, 1500.0);
        QCOMPARE(welle.size(), (kFt2Symbole + 2) * 288);
        // Nur so viel, wie im Betrieb vorliegt, wenn ausgewertet wird.
        QVector<float> f = fensterMit(welle, kRate, versatzMs);
        f.resize(Ft8Stream::kFt2DekodierProben);
        const auto funde = dekodiereWieDerStrom(f);
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

    // Die Grenze des Suchbandes: zweiter Block bei 520 ms, −10..+19
    // Symbole, also bis 520 + 456 = 976 ms. Bei 1100 ms kann nichts
    // mehr kommen -- das haelt fest, dass die Zahlen in Ft8Stream.h
    // stimmen und nicht nur „irgendwie" funktionieren. (Das Fenster
    // waere ohnehin zu kurz: 1100 + 2520 > 3500.)
    void ausserhalbDesSuchbandesNichts()
    {
        const QVector<float> welle =
            mshvWelle(folgeAus(kMshvFolgen[0].toene), kRate, 1500.0);
        QVector<float> f = fensterMit(welle, kRate, 1100);
        f.resize(Ft8Stream::kFt2DekodierProben);   // so viel liegt vor
        const auto funde = dekodiereWieDerStrom(f);
        QVERIFY2(funde.isEmpty(),
                 qPrintable(QStringLiteral("bei 1100 ms gefunden: %1")
                                .arg(wortlaute(funde).join(QLatin1String(" | ")))));
    }

    // Und der Zweck des zweiten Blocks, als Gegenprobe gefahren: nur
    // mit dem ersten Block (dem Stand vor dem Geraetetest) ist eine
    // Sendung 650 ms nach der Grenze unsichtbar.
    void ohneZweitenBlockUnsichtbar()
    {
        const QVector<float> welle =
            mshvWelle(folgeAus(kMshvFolgen[1].toene), kRate, 1500.0);
        const QVector<float> f = fensterMit(welle, kRate, 650);
        const int ab = kRate * Ft8Stream::kFt2BlockVersatzMs / 1000;
        const auto nurErster = Ft8Decoder::decodeBlock(
            f.constData() + ab, f.size() - ab, Ft8Decoder::Protocol::Ft2);
        QVERIFY2(nurErster.isEmpty(), "der erste Block allein fand sie doch");
        QVERIFY(wortlaute(dekodiereWieDerStrom(f))
                    .contains(QLatin1String(kMshvFolgen[1].text)));
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

    // ── Decodium sendet asynchron ──────────────────────────────────
    //
    // Eine Sendung, die 2,2 s nach einer Grenze beginnt, ist erst im
    // NAECHSTEN Fenster vollstaendig -- und dort nur mit dem Schluss des
    // vorigen. Vor dem Vorlauf kam sie nie an. Und eine Sendung kurz
    // nach der Grenze, die schon gemeldet war, darf im naechsten
    // Fenster nicht noch einmal erscheinen.
    void asynchroneSendungWirdGelesen()
    {
        // Zwei Fenster am Stueck, 48 kHz, wie WDSP liefert.
        const double rate = 48000.0;
        const int fenster = int(rate * kFensterMs / 1000);
        QVector<float> mono(2 * fenster, 0.0f);
        auto lege = [&](const Bezug& b, int abMs, double f0) {
            const QVector<float> w = mshvWelle(folgeAus(b.toene), rate, f0);
            const int ab = int(rate * abMs / 1000);
            for (int i = 0; i < w.size() && ab + i < mono.size(); ++i) {
                mono[ab + i] += 0.5f * w[i];
            }
        };
        lege(kMshvFolgen[0], 150, 1200.0);     // im Raster, Fenster 1
        lege(kMshvFolgen[1], 2200, 1800.0);    // asynchron, ueber die Grenze
        QVector<float> stereo;
        stereo.reserve(mono.size() * 2);
        for (float v : mono) { stereo << v << v; }

        Ft8Stream strom;
        strom.setProtocol(Ft8Decoder::Protocol::Ft2);
        qint64 jetzt = 999999990000LL;
        const qint64 grenze1 = jetzt;
        strom.setClock([&jetzt]() { return jetzt; });
        QSignalSpy spion(&strom, &Ft8Stream::decoded);
        const int stueck = 960;
        for (int pos = 0; pos + stueck <= mono.size(); pos += stueck) {
            jetzt = grenze1 + qint64(pos) * 1000 / 48000;
            strom.feedAudio(stereo.constData() + 2 * pos, stueck);
        }
        QCOMPARE(spion.count(), 2);

        const auto erstes  = spion.at(0).at(0).value<QVector<Longpath::Ft8Decode>>();
        const auto zweites = spion.at(1).at(0).value<QVector<Longpath::Ft8Decode>>();
        qInfo() << "Fenster 1:" << wortlaute(erstes) << "Fenster 2:" << wortlaute(zweites);
        QVERIFY(wortlaute(erstes).contains(QLatin1String(kMshvFolgen[0].text)));
        QVERIFY2(!wortlaute(erstes).contains(QLatin1String(kMshvFolgen[1].text)),
                 "die asynchrone Sendung war im ersten Fenster noch gar nicht fertig");
        QVERIFY2(wortlaute(zweites).contains(QLatin1String(kMshvFolgen[1].text)),
                 "die asynchrone Sendung fehlt -- ohne Vorlauf war sie nie zu hoeren");
        QVERIFY2(!wortlaute(zweites).contains(QLatin1String(kMshvFolgen[0].text)),
                 "die Sendung aus Fenster 1 kam ein zweites Mal");
        // Begann 2,2 s nach Grenze 1, also 1,55 s VOR Grenze 2: negativer
        // Zeitversatz, ab Grenze 2 + 40 ms, ein Symbol spaet gemeldet --
        // und um die Laufzeit des Umtasters (48 -> 12 kHz) spaeter: was
        // er an der Grenze noch zurueckhaelt, kommt erst im naechsten
        // Fenster heraus und zaehlt dort zum Anfang. Gemessen 2026-11-17:
        // 126 ms. Im ersten Fenster eines Laufs fehlt das (nichts war
        // zurueckgehalten); im Dauerbetrieb steckt es in JEDEM Fenster,
        // bei FT8 und FT4 genauso, und damit in allen Messungen am Band.
        for (const Ft8Decode& d : zweites) {
            if (d.text != QLatin1String(kMshvFolgen[1].text)) { continue; }
            const double ohneUmtaster =
                (2200 - kFensterMs - Ft8Stream::kFt2BlockVersatzMs) / 1000.0
                + FT2_SYMBOL_PERIOD;
            const double umtaster = double(d.dtSeconds) - ohneUmtaster;
            qInfo() << "Laufzeit des Umtasters:" << umtaster * 1000.0 << "ms";
            QVERIFY2(umtaster > 0.08 && umtaster < 0.17,
                     qPrintable(QStringLiteral("dt %1, Umtaster %2 s")
                                    .arg(double(d.dtSeconds)).arg(umtaster)));
        }
    }

    // ── Der Sendeweg, gegen den eigenen Empfaenger ─────────────────
    //
    // Die Tonquelle, die am Funkgeraet haengt (Ft8TxSource, 48 kHz),
    // nach dem Sendeplan gestartet -- und was herauskommt, durch
    // DENSELBEN Strom, der am Band hoert. Prueft Kodierer, Umtaster
    // 12 -> 48 kHz, Sprung ueber die Stille (probenAbMs) und Takt in
    // einem Lauf. Ein Fehler irgendwo darin heisst: niemand liest uns.
    void derSendewegWirdVomEigenenEmpfaengerGelesen()
    {
        const QString text = QStringLiteral("CQ OE5SOS JN78");
        const auto p = Ft8Decoder::Protocol::Ft2;
        Ft8TxSource quelle(48000);
        QVERIFY(quelle.vorbereiten(text, 1500.0f, p));
        // Ein FT2-Fenster bei 48 kHz: 3,75 s = 180 000 Proben -- abzueglich
        // des Anlaufs des Umtasters (r8brain, ~1700 Proben bei 12 kHz,
        // siehe Gedaechtnis „Umtaster schluckt"). Ob dabei die Zeitlage
        // verrutscht, prueft der Zeitversatz unten, nicht diese Zahl.
        QVERIFY2(quelle.probenImFenster() > 170000
                     && quelle.probenImFenster() <= 180000,
                 qPrintable(QString::number(quelle.probenImFenster())));

        const Ft8Tx::Sendeplan plan = Ft8Tx::sendeplan(
            p, Ft8Tx::dekoderFristMs(p), Ft8Tx::zielVersatzMs(p), 300);
        quelle.starten(plan.probenAbMs);

        // Getastet ab tastenNachMs nach der Grenze; davor Stille.
        const int rate = 48000;
        QVector<float> mono(rate * kFensterMs / 1000 * 2, 0.0f);
        int pos = rate * int(plan.tastenNachMs) / 1000;
        const int ende = pos + rate * int(plan.tastdauerMs) / 1000;
        QVector<float> puffer(960);
        while (quelle.laeuft() && pos < ende && pos + 960 <= mono.size()) {
            const int n = quelle.pullSamples(puffer.data(), 960);
            for (int i = 0; i < n; ++i) { mono[pos + i] = puffer[i]; }
            pos += n;
        }
        QVector<float> stereo;
        stereo.reserve(mono.size() * 2);
        for (float v : mono) { stereo << v << v; }

        Ft8Stream strom;
        strom.setProtocol(p);
        qint64 jetzt = 999999990000LL;
        const qint64 grenze = jetzt;
        strom.setClock([&jetzt]() { return jetzt; });
        QSignalSpy spion(&strom, &Ft8Stream::decoded);
        for (int i = 0; i + 960 <= mono.size(); i += 960) {
            jetzt = grenze + qint64(i) * 1000 / rate;
            strom.feedAudio(stereo.constData() + 2 * i, 960);
        }
        QVERIFY(spion.count() >= 1);
        const auto funde = spion.at(0).at(0).value<QVector<Longpath::Ft8Decode>>();
        qInfo() << "eigene Sendung gelesen:" << wortlaute(funde);
        QVERIFY2(wortlaute(funde).contains(text),
                 qPrintable(wortlaute(funde).join(QLatin1String(" | "))));
        // Und der Ton liegt dort, wo der Plan ihn hinlegt: 150 ms nach
        // der Grenze -> dt = 0,11 s + ein Symbol (ft8_lib meldet spaet).
        const double soll = (plan.tonBeiMs - Ft8Stream::kFt2BlockVersatzMs) / 1000.0
                            + FT2_SYMBOL_PERIOD;
        QVERIFY2(std::abs(double(funde.first().dtSeconds) - soll) < 0.03,
                 qPrintable(QStringLiteral("dt %1, Plan %2")
                                .arg(double(funde.first().dtSeconds)).arg(soll)));
    }

    // ── Ein ganzes QSO im Dekoderfenster, bis ins Logbuch ──────────────
    //
    // Derselbe Ablauf wie bei FT8/FT4 (tst_ft8_senden,
    // einQsoAusDerAnruferRolle), mit FT2 gewaehlt -- und der
    // Logbucheintrag traegt MFSK/FT2, wie WSJT-X Improved und Decodium.
    void einFt2QsoLandetAlsMfskImLogbuch()
    {
        RadioModel radio;
        radio.installFt8TxForTest();
        Ft8Applet applet(&radio);
        applet.setProtocol(Ft8Decoder::Protocol::Ft2);
        QCOMPARE(applet.protocol(), Ft8Decoder::Protocol::Ft2);
        AppSettings::instance().setValue(QStringLiteral("User/Callsign"),
                                         QStringLiteral("OE5SOS"));
        AppSettings::instance().setValue(QStringLiteral("User/GridSquare"),
                                         QStringLiteral("JN67VV"));

        QSignalSpy gemeldet(&applet, &Ft8Applet::qsoFertig);
        applet.cqRufenButtonForTest()->click();
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::RufeCq);

        const auto fund = [&applet](const char* text) {
            Ft8Decode d;
            d.text   = QString::fromLatin1(text);
            d.snrDb  = -5.0;
            d.freqHz = 1500.0;
            applet.fundForTest(d);
        };
        fund("OE5SOS SP6CA JO80");
        QCOMPARE(applet.ablaufForTest().partner(), QStringLiteral("SP6CA"));
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::SendeReport);
        fund("OE5SOS SP6CA R-09");
        QCOMPARE(applet.ablaufForTest().zustand(), Ft8Ablauf::Zustand::SendeRR73);
        applet.fensterZuEndeForTest();
        QCOMPARE(gemeldet.count(), 1);

        const auto e = gemeldet.first().first().value<LogEntry>();
        QCOMPARE(e.call, QStringLiteral("SP6CA"));
        QCOMPARE(e.mode, QStringLiteral("MFSK"));
        QCOMPARE(e.submode, QStringLiteral("FT2"));
        QCOMPARE(e.rstRcvd, QStringLiteral("-09"));
        applet.abbrechen();
    }

    // ── Das Vorwissen hilft auch bei FT2 (2026-11-18) ───────────────
    //
    // Die A-priori-Bits kommen als Klartext; FT4 und FT2 verwuerfeln
    // die Nachricht vor dem LDPC mit kFT4_XOR_sequence. Ohne Umrechnung
    // zeigte die Vorgabe dort auf falsche Werte, und das Vorwissen half
    // NIE. Gezaehlt ueber viele Rauschwuerfel an der Grenze des
    // Dekoders: mit Vorwissen muessen deutlich mehr Funde kommen.
    void dasVorwissenHilftAuchBeiFt2()
    {
        const QString text = QStringLiteral("OE5SOS SP6CA R-09");
        QVector<float> gemittet;
        QVERIFY(Ft8Decoder::encodeBlock(text, 1500.0f,
                                        Ft8Decoder::Protocol::Ft2, &gemittet));
        const int nutz   = int(0.5 + FT4_NN * FT2_SYMBOL_PERIOD * kRate);
        const int stille = (gemittet.size() - nutz) / 2;
        const QVector<float> signal = gemittet.mid(stille, nutz);
        QVector<float> f = fensterMit(signal, kRate, 150);
        f.resize(Ft8Stream::kFt2DekodierProben);
        const int von = kRate * 150 / 1000;

        const QVector<ApVorgabe> vorwissen = {
            baueApVorgabe(ApTyp::MeinRufUndSeiner, QStringLiteral("OE5SOS"),
                          QStringLiteral("SP6CA")) };
        QVERIFY(vorwissen.first().brauchbar);

        int ohne = 0, mit = 0;
        const int wuerfe = 16;
        for (int saat = 1; saat <= wuerfe; ++saat) {
            const QVector<float> ton = mitRauschen(f, von, nutz, -13.5, uint32_t(saat));
            if (wortlaute(Ft8Stream::dekodiereFt2(ton.constData(), ton.size()))
                    .contains(text)) { ++ohne; }
            const auto ap = Ft8Stream::dekodiereFt2(ton.constData(), ton.size(), 0,
                                                    vorwissen);
            if (wortlaute(ap).contains(text)) { ++mit; }
        }
        qInfo() << "FT2 -13,5 dB, von" << wuerfe << "Wuerfen: ohne Vorwissen"
                << ohne << "mit Vorwissen" << mit;
        QVERIFY2(mit >= ohne + 4,
                 qPrintable(QStringLiteral("ohne %1, mit %2").arg(ohne).arg(mit)));
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
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band12m, Digimodus::Ft2),
                 qint64(24'923'000));
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band2m, Digimodus::Ft2),
                 qint64(144'177'000));
        // 2 m hat (noch) keine FT8/FT4-Stelle -- der Eintrag darf keine
        // erfinden.
        QCOMPARE(DigiFrequenzen::anruffrequenz(Band::Band2m, Digimodus::Ft8),
                 qint64(0));

        // Die weiteren gueltigen Stellen: der Knopf stellt nicht dorthin,
        // aber wer dort steht, wird als FT2 erkannt.
        for (const qint64 weiter : { qint64(7'062'000), qint64(3'568'000),
                                     qint64(70'157'000), qint64(432'177'000),
                                     qint64(1'296'177'000) }) {
            QVERIFY2(DigiFrequenzen::stehtSchonDort(weiter, Digimodus::Ft2),
                     qPrintable(QStringLiteral("%1 nicht als FT2 erkannt").arg(weiter)));
            QVERIFY2(!DigiFrequenzen::stehtSchonDort(weiter, Digimodus::Ft8),
                     qPrintable(QStringLiteral("%1 ist auch FT8").arg(weiter)));
            QVERIFY2(!DigiFrequenzen::stehtSchonDort(weiter, Digimodus::Ft4),
                     qPrintable(QStringLiteral("%1 ist auch FT4").arg(weiter)));
        }
        // Und die FT4-Stellen bleiben FT4: das Decodium-Handbuch nennt
        // 14,080 / 28,180 / 50,318 als FT2 -- uebernommen haette das die
        // FT4-Erkennung auf drei Baendern zerstoert.
        for (const qint64 ft4 : { qint64(14'080'000), qint64(28'180'000),
                                  qint64(50'318'000) }) {
            QVERIFY(DigiFrequenzen::stehtSchonDort(ft4, Digimodus::Ft4));
            QVERIFY(!DigiFrequenzen::stehtSchonDort(ft4, Digimodus::Ft2));
        }
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
        QCOMPARE(urteil(0.60), Ft8Uhrversatz::Lage::Ok);
        QCOMPARE(urteil(0.75), Ft8Uhrversatz::Lage::Auffaellig);
        QCOMPARE(urteil(0.92), Ft8Uhrversatz::Lage::Schlecht);
    }
};

QTEST_MAIN(TstFt2)
#include "tst_ft2.moc"
