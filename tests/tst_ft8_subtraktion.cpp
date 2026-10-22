// no-port-check: Longpath-eigener Pruefstand.
// =================================================================
// tests/tst_ft8_subtraktion.cpp  (Longpath)
// =================================================================
//
// Zieht die Subtraktion wirklich ein Signal heraus -- und legt sie
// darunter etwas frei?
//
// Das ist der ganze Zweck: WSJT-X und JTDX dekodieren mehrfach und
// ziehen nach jedem Durchgang die Funde ab. Darunter werden schwaechere
// Stationen ueberhaupt erst auffindbar. Zwei Fragen also, und die
// zweite ist die eigentliche:
//
//   1. Wird das abgezogene Signal wirklich leiser?
//   2. Findet der Dekoder DANACH etwas, das er vorher nicht fand?
//
// Ohne Fenster, ohne Funkgeraet, ohne Tondatei -- zwei erzeugte
// Signale, uebereinandergelegt.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>
#include <QElapsedTimer>

#include <cmath>
#include <vector>

#include "core/Ft8Decoder.h"
#include "core/Ft8Subtract.h"

extern "C" {
#include "common/gfsk.h"
#include "ft8/constants.h"
#include "ft8/encode.h"
#include "ft8/message.h"
}

using namespace Longpath;

namespace {

constexpr int kRate   = Ft8Subtract::kSampleRate;      // 12000
constexpr int kFenster = 15 * kRate;                   // 180000
constexpr int kNframe = Ft8Subtract::kSymbols
                      * Ft8Subtract::kSamplesPerSymbol; // 151680

/// Die 79 Toene zu einer Nachricht. Leer, wenn ft8_lib sie nicht packt.
std::vector<uint8_t> toeneFuer(const char* text)
{
    ftx_message_t m{};
    ftx_message_init(&m);
    if (ftx_message_encode(&m, nullptr, text) != FTX_MESSAGE_RC_OK) {
        return {};
    }
    std::vector<uint8_t> t(FT8_NN, 0);
    ft8_encode(m.payload, t.data());
    return t;
}

/// Ein Signal in das Fenster legen: Toene, Tonlage, Zeitversatz, Pegel.
void legeHinein(std::vector<float>* fenster, const std::vector<uint8_t>& toene,
                double f0Hz, double dtSek, float pegel)
{
    std::vector<float> rahmen(kNframe, 0.0f);
    synth_gfsk(toene.data(), FT8_NN, static_cast<float>(f0Hz),
               2.0f, FT8_SYMBOL_PERIOD, kRate, rahmen.data());
    const int start = static_cast<int>(dtSek * kRate + 1.0) - 1;
    for (int i = 0; i < kNframe; ++i) {
        const int j = start + i;
        if (j >= 0 && j < int(fenster->size())) {
            (*fenster)[size_t(j)] += pegel * rahmen[size_t(i)];
        }
    }
}

/// Effektivwert ueber den Rahmen, ab `start`.
double effektivwert(const std::vector<float>& x, int start, int n)
{
    double s = 0.0;
    int z = 0;
    for (int i = start; i < start + n && i < int(x.size()); ++i) {
        if (i < 0) { continue; }
        s += double(x[size_t(i)]) * double(x[size_t(i)]);
        ++z;
    }
    return z > 0 ? std::sqrt(s / double(z)) : 0.0;
}

bool enthaelt(const QVector<Ft8Decode>& funde, const char* teil)
{
    for (const Ft8Decode& d : funde) {
        if (d.text.contains(QString::fromLatin1(teil))) { return true; }
    }
    return false;
}

} // namespace

class TstFt8Subtraktion : public QObject
{
    Q_OBJECT

private slots:

    void dasAbgezogeneSignalWirdLeise()
    {
        const auto toene = toeneFuer("CQ OE5SOS JN67");
        QVERIFY2(!toene.empty(), "ft8_lib packt die Nachricht nicht");

        std::vector<float> fenster(kFenster, 0.0f);
        legeHinein(&fenster, toene, 1000.0, 0.5, 1.0f);

        const double vorher = effektivwert(fenster, kRate / 2, kNframe);
        QVERIFY2(vorher > 0.1, qPrintable(QStringLiteral("nichts im Fenster: %1").arg(vorher)));

        QVERIFY(Ft8Subtract::zieheAb(fenster.data(), int(fenster.size()),
                                     toene.data(), int(toene.size()),
                                     1000.0, 0.5));

        const double nachher = effektivwert(fenster, kRate / 2, kNframe);
        const double daempfungDb = 20.0 * std::log10(vorher / std::max(nachher, 1e-12));
        qInfo().noquote() << QStringLiteral("Effektivwert vorher %1, nachher %2 — %3 dB leiser")
                                 .arg(vorher, 0, 'f', 4)
                                 .arg(nachher, 0, 'f', 4)
                                 .arg(daempfungDb, 0, 'f', 1);
        QVERIFY2(daempfungDb > 20.0,
                 qPrintable(QStringLiteral("nur %1 dB leiser — das traegt nicht")
                                .arg(daempfungDb, 0, 'f', 1)));
    }

    void ungueltigeGroessenLassenDieAufnahmeInRuhe()
    {
        // Falsch abziehen waere schlimmer als nicht abziehen: es
        // erzeugte ein Stoersignal, wo vorher ein echtes stand.
        const auto toene = toeneFuer("CQ OE5SOS JN67");
        QVERIFY(!toene.empty());
        std::vector<float> fenster(kFenster, 0.25f);
        const std::vector<float> vorher = fenster;

        QVERIFY(!Ft8Subtract::zieheAb(nullptr, kFenster, toene.data(), 79, 1000.0, 0.0));
        QVERIFY(!Ft8Subtract::zieheAb(fenster.data(), kFenster, toene.data(), 78, 1000.0, 0.0));
        QVERIFY(!Ft8Subtract::zieheAb(fenster.data(), 1000, toene.data(), 79, 1000.0, 0.0));
        QVERIFY(!Ft8Subtract::zieheAb(fenster.data(), kFenster, toene.data(), 79, 0.0, 0.0));
        QCOMPARE(fenster, vorher);
    }

    void wieGenauSchaetztDerDekoder()
    {
        // Messung, kein Waechter: der Abzug steht und faellt damit,
        // wie genau Tonlage und Zeitversatz gemeldet werden. Ein
        // Fehler von 3 Hz ueber 12,6 s sind 38 volle Umdrehungen --
        // damit laesst sich nichts kohaerent abziehen.
        const auto toene = toeneFuer("CQ OE5SOS JN67");
        QVERIFY(!toene.empty());
        std::vector<float> fenster(kFenster, 0.0f);
        legeHinein(&fenster, toene, 1000.0, 0.5, 1.0f);

        const QVector<Ft8Decode> d = Ft8Decoder::decodeBlock(
            fenster.data(), int(fenster.size()),
            Ft8Decoder::Protocol::Ft8, 1);
        QVERIFY(!d.isEmpty());
        for (const Ft8Decode& x : d) {
            qInfo().noquote()
                << QStringLiteral("gemeldet f=%1 Hz (wahr 1000,000), dt=%2 s "
                                  "(wahr 0,5000) -> %3")
                       .arg(double(x.freqHz), 0, 'f', 3)
                       .arg(double(x.dtSeconds), 0, 'f', 4)
                       .arg(x.text);
        }
    }

    void derDekoderMachtDasVonSelbst()
    {
        // Der Einbau: `decodeBlock` geht jetzt zweimal ueber das
        // Fenster und zieht dazwischen ab. Ohne Zutun des Aufrufers.
        const auto stark   = toeneFuer("CQ OE5SOS JN67");
        const auto schwach = toeneFuer("CQ DL1ABC JO31");
        QVERIFY(!stark.empty() && !schwach.empty());

        std::vector<float> fenster(kFenster, 0.0f);
        legeHinein(&fenster, stark,   1000.0, 0.5, 1.00f);
        legeHinein(&fenster, schwach, 1000.0, 0.9, 0.25f);

        // Eine Runde -- so war es bis zum 2026-10-21.
        const QVector<Ft8Decode> einmal = Ft8Decoder::decodeBlock(
            fenster.data(), int(fenster.size()),
            Ft8Decoder::Protocol::Ft8, 1);
        QVERIFY2(!enthaelt(einmal, "DL1ABC"),
                 "die schwache Station wird schon in einer Runde "
                 "gefunden — dann belegt dieser Lauf nichts");

        // Zwei Runden -- die Vorgabe.
        const QVector<Ft8Decode> zweimal = Ft8Decoder::decodeBlock(
            fenster.data(), int(fenster.size()));
        qInfo().noquote() << "eine Runde:" << einmal.size()
                          << "— zwei Runden:" << zweimal.size();
        QVERIFY2(enthaelt(zweimal, "OE5SOS"), "die starke fehlt");
        QVERIFY2(enthaelt(zweimal, "DL1ABC"),
                 "die schwache kommt auch mit zwei Runden nicht");
    }

    void dieRechenzeitBeiVollemBand()
    {
        // Entscheidend fuer die Frage, ob das ueberhaupt laufen darf:
        // am 2026-10-21 standen auf 20 m 27 Zeilen je Durchgang. Jeder
        // Fund wird abgezogen, jeder Abzug kostet FFTs ueber 180 000
        // Punkte. Die Decke ist das Fenster selbst (15 s) -- aber weit
        // darunter, denn es laeuft neben dem Empfang.
        const char* rufe[] = {
            "CQ OE5SOS JN67", "CQ DL1ABC JO31", "CQ G8BCG IO70",
            "CQ PA3DJV JO33", "CQ SP9GL KO02",  "CQ IK0CHU JN62",
            "CQ EA3BDE IN80", "CQ F4HBV IN95",  "CQ OZ4ABH JO45",
            "CQ LA9RMA JO28", "CQ SM5BVV JO89", "CQ R6NW LN05",
            "CQ 9A5ZD JN85",  "CQ TA2LA KN30",  "CQ VK4MDP QG62",
        };
        std::vector<float> fenster(kFenster, 0.0f);
        int gelegt = 0;
        for (int i = 0; i < int(sizeof(rufe) / sizeof(rufe[0])); ++i) {
            const auto t = toeneFuer(rufe[i]);
            if (t.empty()) { continue; }
            legeHinein(&fenster, t, 500.0 + 150.0 * i, 0.4 + 0.02 * i, 0.5f);
            ++gelegt;
        }
        QVERIFY(gelegt >= 10);

        QElapsedTimer uhr;
        uhr.start();
        const QVector<Ft8Decode> funde =
            Ft8Decoder::decodeBlock(fenster.data(), int(fenster.size()));
        const qint64 ms = uhr.elapsed();
        qInfo().noquote()
            << QStringLiteral("%1 Signale gelegt, %2 gefunden, %3 ms")
                   .arg(gelegt).arg(funde.size()).arg(ms);

        // Eine Sekunde ist die Grenze: darueber waere es im Faden der
        // Oberflaeche nicht mehr vertretbar, und das Fenster davor
        // haette dann auch noch Rechenzeit gebraucht.
        QVERIFY2(ms < 1000,
                 qPrintable(QStringLiteral("%1 ms fuer ein volles Band").arg(ms)));
    }

    void dieAufnahmeDesAufrufersBleibtUnberuehrt()
    {
        // Der Abzug laeuft auf einer Kopie. Wer uns seine Aufnahme
        // leiht, bekommt sie unveraendert zurueck -- sonst zoege der
        // Wasserfall daneben plotzlich andere Linien.
        const auto toene = toeneFuer("CQ OE5SOS JN67");
        QVERIFY(!toene.empty());
        std::vector<float> fenster(kFenster, 0.0f);
        legeHinein(&fenster, toene, 1000.0, 0.5, 1.0f);
        const std::vector<float> vorher = fenster;

        Ft8Decoder::decodeBlock(fenster.data(), int(fenster.size()));
        QCOMPARE(fenster, vorher);
    }

    void darunterKommtEinZweitesSignalZumVorschein()
    {
        // Der eigentliche Punkt. Zwei Stationen auf DERSELBEN Tonlage,
        // eine deutlich schwaecher und zeitlich versetzt -- so wie es
        // auf einem vollen Band zugeht. Die starke verdeckt die
        // schwache; erst wenn sie heraus ist, wird die schwache
        // auffindbar.
        const auto stark   = toeneFuer("CQ OE5SOS JN67");
        const auto schwach = toeneFuer("CQ DL1ABC JO31");
        QVERIFY(!stark.empty() && !schwach.empty());

        std::vector<float> fenster(kFenster, 0.0f);
        legeHinein(&fenster, stark,   1000.0, 0.5, 1.00f);
        legeHinein(&fenster, schwach, 1000.0, 0.9, 0.25f);

        const QVector<Ft8Decode> vorher =
            Ft8Decoder::decodeBlock(fenster.data(), int(fenster.size()));
        qInfo().noquote() << "vor dem Abziehen:" << vorher.size() << "Funde";
        for (const Ft8Decode& d : vorher) { qInfo().noquote() << "   " << d.text; }

        QVERIFY2(enthaelt(vorher, "OE5SOS"),
                 "schon die starke Station wird nicht gefunden");

        QVERIFY(Ft8Subtract::zieheAb(fenster.data(), int(fenster.size()),
                                     stark.data(), int(stark.size()),
                                     1000.0, 0.5));

        const QVector<Ft8Decode> nachher =
            Ft8Decoder::decodeBlock(fenster.data(), int(fenster.size()));
        qInfo().noquote() << "nach dem Abziehen:" << nachher.size() << "Funde";
        for (const Ft8Decode& d : nachher) { qInfo().noquote() << "   " << d.text; }

        QVERIFY2(enthaelt(nachher, "DL1ABC"),
                 "die schwache Station bleibt auch nach dem Abziehen "
                 "verborgen — dann traegt das Verfahren nicht");
    }
};

QTEST_APPLESS_MAIN(TstFt8Subtraktion)
#include "tst_ft8_subtraktion.moc"
