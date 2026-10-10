// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_clipper_buys_talk_power.cpp  (Longpath)
// =================================================================
// Die Stufe verspricht etwas Messbares: bei gleicher SPITZE mehr
// EFFEKTIVWERT. Genau das wird hier gemessen, und nicht, dass sie
// ohne Absturz durchläuft.
//
// Dazu der Punkt, an dem ein selbstgebauter Clipper üblicherweise
// scheitert: Abschneiden erzeugt Oberwellen bis ins Unendliche, und
// bei 48 kHz faltet sich alles über 24 kHz ZURÜCK ins Sprachband.
// Ein 3,1-kHz-Ton erzeugt abgeschnitten eine fünfzehnte Oberwelle
// bei 46,5 kHz -- die landet bei 1,5 kHz mitten in der Stimme, und
// kein Filter holt sie je wieder heraus. Der Prüfstand sucht genau
// dort danach.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/strip/ClientClipper.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

constexpr double kRate = 48000.0;

// Eine stimmhafte Stimme ist ein Pulszug: die Stimmlippen geben
// einen kurzen Schlag, und der Rest ist Nachschwingen. Zwanzig
// Harmonische von 120 Hz in Phase ergeben genau das -- ein spitzer
// Wellenzug mit rund 16 dB Scheitelfaktor, in der Grössenordnung
// einer ungefassten Stimme am Mikrofon.
//
// Erster Versuch waren sechs Harmonische mit 1/h: die kamen auf
// 5,5 dB und blieben beim Abschneiden unter der Schwelle, also
// zeigte der Prüfstand „nichts passiert" und hatte recht. Ein
// Prüfsignal, das die Stufe gar nicht erreicht, prüft sie nicht.
std::vector<float> peakyVoice(int n)
{
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        for (int h = 1; h <= 20; ++h) {
            s += std::cos(2.0 * M_PI * 120.0 * h * i / kRate);
        }
        v[size_t(i)] = float(s);
    }
    // Auf Spitze 0,9 bringen, damit die Stufe auch etwas zu tun hat.
    double p = 0.0;
    for (float x : v) { p = std::max(p, std::fabs(double(x))); }
    if (p > 1e-9) {
        for (float& x : v) { x = float(double(x) * 0.9 / p); }
    }
    return v;
}

std::vector<float> tone(int n, double hz, double amp)
{
    std::vector<float> v(static_cast<size_t>(n));
    for (int i = 0; i < n; ++i) {
        v[size_t(i)] = float(amp * std::sin(2.0 * M_PI * hz * i / kRate));
    }
    return v;
}

void run(ClientClipper& c, std::vector<float>& x)
{
    for (size_t at = 0; at + 240 <= x.size(); at += 240) {
        c.process(x.data() + at, 240, 1);
    }
}

double peak(const std::vector<float>& v, int from)
{
    double p = 0.0;
    for (int i = from; i < int(v.size()); ++i) {
        p = std::max(p, std::fabs(double(v[size_t(i)])));
    }
    return p;
}

double rms(const std::vector<float>& v, int from)
{
    double s = 0.0;
    for (int i = from; i < int(v.size()); ++i) {
        s += double(v[size_t(i)]) * double(v[size_t(i)]);
    }
    return std::sqrt(s / std::max(1, int(v.size()) - from));
}

double crestDb(const std::vector<float>& v, int from)
{
    return 20.0 * std::log10(peak(v, from) / std::max(rms(v, from), 1e-12));
}

// Goertzel: wieviel Energie steckt bei genau dieser Frequenz.
double magnitudeAt(const std::vector<float>& v, double hz, int from)
{
    const int n = int(v.size()) - from;
    const double w = 2.0 * M_PI * hz / kRate;
    const double cw = 2.0 * std::cos(w);
    double s1 = 0.0;
    double s2 = 0.0;
    for (int i = 0; i < n; ++i) {
        const double s = double(v[size_t(from + i)]) + cw * s1 - s2;
        s2 = s1;
        s1 = s;
    }
    return 2.0 * std::sqrt(s1 * s1 + s2 * s2 - cw * s1 * s2) / n;
}

} // namespace

class TstClipperBuysTalkPower : public QObject {
    Q_OBJECT

private slots:
    // Die eigentliche Zusage: weniger Scheitelfaktor, mehr
    // Effektivwert, gleiche Spitze.
    void itTradesPeakForAverage()
    {
        const auto src = peakyVoice(int(kRate));

        ClientClipper c;
        c.prepare(kRate);
        c.setEnabled(true);
        c.setDriveDb(6.0f);
        c.setKnee(0.2f);
        c.setLevelMatch(false);     // wie gesendet wird

        auto out = src;
        run(c, out);

        const int skip = 2000;            // Einschwingen der Filter
        const double crestIn  = crestDb(src, skip);
        const double crestOut = crestDb(out, skip);
        const double rmsGain  = 20.0 * std::log10(rms(out, skip)
                                                  / rms(src, skip));
        const double peakGain = 20.0 * std::log10(peak(out, skip)
                                                  / peak(src, skip));

        qInfo() << "Scheitelfaktor" << crestIn << "->" << crestOut
                << "dB; Effektivwert" << rmsGain
                << "dB; Spitze" << peakGain << "dB";

        // Gemessen: 16,0 -> 13,6 dB bei 6 dB Drive und Knie 0,2.
        QVERIFY2(crestOut < crestIn - 2.0,
                 qPrintable(QStringLiteral("nur %1 dB weniger")
                                .arg(crestIn - crestOut, 0, 'f', 1)));
        // Das ist die Zusage: mehr Effektivwert.
        QVERIFY2(rmsGain > 3.0,
                 qPrintable(QStringLiteral("Effektivwert nur +%1 dB")
                                .arg(rmsGain, 0, 'f', 1)));
        // Und die Spitze darf dabei nicht davonlaufen -- sonst ist
        // der Gewinn geliehen und die Endstufe zahlt ihn zurueck.
        // Etwas Ueberschwingen bleibt: das Filter hinter der Kante
        // macht aus einer abgeschnittenen Ecke wieder eine Rundung,
        // die ein Stueck ueber die Decke ragt. Dafuer steht der
        // Begrenzer dahinter.
        QVERIFY2(peakGain < 2.0,
                 qPrintable(QStringLiteral("die Spitze stieg um %1 dB")
                                .arg(peakGain, 0, 'f', 1)));
    }

    // Der Punkt, an dem ein Clipper ohne Ueberabtastung scheitert.
    void aliasingStaysOutOfTheVoiceBand()
    {
        auto x = tone(int(kRate), 3100.0, 0.9);

        ClientClipper c;
        c.prepare(kRate);
        c.setEnabled(true);
        c.setDriveDb(12.0f);              // kräftig abgeschnitten
        c.setKnee(0.0f);                  // harte Kante, der schlimmste Fall
        c.setLevelMatch(false);
        run(c, x);

        const int skip = 2000;
        const double fund  = magnitudeAt(x, 3100.0, skip);
        // Die fuenfzehnte Oberwelle von 3,1 kHz liegt bei 46,5 kHz
        // und faltet sich bei 48 kHz auf 1,5 kHz zurueck.
        const double alias = magnitudeAt(x, 1500.0, skip);
        const double db = 20.0 * std::log10(std::max(alias, 1e-12)
                                            / std::max(fund, 1e-12));

        qInfo() << "Grundton" << fund << "Faltprodukt bei 1,5 kHz"
                << alias << "=" << db << "dB";
        QVERIFY2(db < -40.0,
                 qPrintable(QStringLiteral("Faltprodukt bei %1 dB")
                                .arg(db, 0, 'f', 1)));
    }

    // Mit Abgleich bleibt die Lautstaerke, wo sie war -- dafuer ist
    // er da. Der Scheitelfaktor wird trotzdem besser, denn das ist
    // die Arbeit der Kante und nicht die der Verstaerkung.
    void levelMatchedKeepsTheLoudnessAndStillFlattensPeaks()
    {
        const auto src = peakyVoice(int(kRate) * 3);

        ClientClipper c;
        c.prepare(kRate);
        c.setEnabled(true);
        c.setDriveDb(6.0f);
        c.setKnee(0.2f);
        c.setLevelMatch(true);

        auto out = src;
        run(c, out);

        const int skip = int(kRate) * 2;     // Abgleich einschwingen
        const double rmsGain = 20.0 * std::log10(rms(out, skip)
                                                 / rms(src, skip));
        const double dCrest = crestDb(src, skip) - crestDb(out, skip);
        qInfo() << "abgeglichen: Effektivwert" << rmsGain
                << "dB, Scheitelfaktor" << dCrest << "dB besser";

        QVERIFY2(std::fabs(rmsGain) < 1.0,
                 qPrintable(QStringLiteral("Lautstaerke um %1 dB verschoben")
                                .arg(rmsGain, 0, 'f', 2)));
        QVERIFY2(dCrest > 2.0,
                 qPrintable(QStringLiteral("Scheitelfaktor nur %1 dB besser")
                                .arg(dCrest, 0, 'f', 1)));
    }

    // Aus, und ohne Drive: beides unveraendert. Eine Stufe, die auch
    // bei null etwas tut, ist beim Vergleichen nicht zu gebrauchen.
    void offAndZeroDriveAreUntouched()
    {
        const auto src = peakyVoice(4800);

        ClientClipper off;
        off.prepare(kRate);
        off.setEnabled(false);
        off.setDriveDb(12.0f);
        auto a = src;
        run(off, a);
        for (size_t i = 0; i < src.size(); ++i) { QCOMPARE(a[i], src[i]); }

        ClientClipper zero;
        zero.prepare(kRate);
        zero.setEnabled(true);
        zero.setDriveDb(0.0f);
        auto b = src;
        run(zero, b);
        for (size_t i = 0; i < src.size(); ++i) { QCOMPARE(b[i], src[i]); }
    }
};

QTEST_MAIN(TstClipperBuysTalkPower)
#include "tst_clipper_buys_talk_power.moc"
