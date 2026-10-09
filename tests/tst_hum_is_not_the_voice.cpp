// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_hum_is_not_the_voice.cpp  (Longpath)
// =================================================================
// Der Grundton einer tiefen Stimme ist kein Netzbrumm.
//
// Am 2026-10-09 am Geraet gemessen: eine Maennerstimme mit rund
// 110 Hz Grundton wurde als „Netzbrumm bei 50 Hz, 12,5 dB unter der
// Stimme" gemeldet -- und StripTuner kerbte daraufhin 50, 100 und
// 150 Hz mit bis zu -18 dB aus. Das traf den Grundton und die zweite
// Oberwelle der Stimme. „kein bass, es fehlt an allem."
//
// Die Ursache ist Arithmetik, kein Zufall: die Brummfenster sind
// plus/minus zwei Faecher breit, bei dieser FFT-Laenge rund 23 Hz.
// Das 100-Hz-Fenster reicht damit von 77 bis 123 Hz und faengt jede
// Stimme zwischen diesen Grenzen ein.
//
// Zwei Faelle. Der erste faellt gegen die alte Fassung durch, der
// zweite haelt in beiden -- er bewacht, dass die Behebung den
// wirklichen Brumm nicht mitnimmt. Ohne ihn waere „keine Brumm-
// meldung mehr" trivial zu erreichen und vollkommen wertlos.
//
// Modification history (Longpath):
//   2026-10-09 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/VoiceAnalyzer.h"

#include <cmath>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;

// Eine Stimme: Grundton plus Oberwellen, in Silben an- und abgeschaltet,
// damit der Analysator Sprache und Pausen auseinanderhalten kann.
std::vector<float> voice(double f0, double seconds, double humHz = 0.0,
                         double humAmp = 0.0)
{
    const int n = int(seconds * kRate);
    std::vector<float> x(static_cast<size_t>(n), 0.0f);
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / kRate;
        // Silben: 250 ms Ton, 150 ms Pause -- mit 20 ms weichen
        // Flanken. Ein hart geschalteter Ton ist ein Knall, und ein
        // Knall hat Energie bei 50 Hz. Ohne die Flanken pruefte
        // dieser Fall die Bandbreite meines Generators statt den
        // Brummdetektor (erst gesehen, als er -6 dB „Brumm" meldete,
        // wo keiner ist).
        const double phase = std::fmod(t, 0.4);
        double gate = 0.0;
        if (phase < 0.25) {
            const double ramp = 0.02;
            if (phase < ramp) {
                gate = 0.5 * (1.0 - std::cos(M_PI * phase / ramp));
            } else if (phase > 0.25 - ramp) {
                gate = 0.5 * (1.0 - std::cos(M_PI * (0.25 - phase) / ramp));
            } else {
                gate = 1.0;
            }
        }

        double v = 0.0;
        if (gate > 0.0) {
            // Ein glaubwuerdiger Stimmverlauf. Die Oberwellen muessen
            // bis ueber 3 kHz reichen: der Brummwert wird gegen die
            // Leistung zwischen 1 und 3 kHz gemessen, und eine
            // Kunststimme, die dort nichts hat, laesst jeden Wert
            // unten riesig aussehen. Mit vierzehn Oberwellen (bis
            // 1,5 kHz) meldete dieser Fall -7,8 dB „Brumm", und das
            // lag an meinem Generator, nicht am Detektor.
            for (int h = 1; h <= 40; ++h) {
                const double hz = f0 * h;
                if (hz > 5000.0) { break; }
                const double a = 0.45 / double(h);
                v += a * std::sin(2.0 * M_PI * hz * t);
            }
            v += 0.12 * std::sin(2.0 * M_PI * 1200.0 * t);
            v += 0.06 * std::sin(2.0 * M_PI * 2600.0 * t);
            v *= 0.35 * gate;
        }
        if (humAmp > 0.0) {
            for (int h = 1; h <= 3; ++h) {
                v += (humAmp / h) * std::sin(2.0 * M_PI * humHz * h * t);
            }
        }
        x[static_cast<size_t>(i)] = float(v);
    }
    return x;
}

} // namespace

class TstHumIsNotTheVoice : public QObject
{
    Q_OBJECT
private slots:

    // Eine tiefe Stimme, kein Brumm im Raum. Es darf keine
    // Brummmeldung geben -- und erst recht keine Kerbe.
    void aLowVoiceIsNotMistakenForMains()
    {
        const std::vector<float> x = voice(110.0, 8.0);
        const VoiceAnalysis a =
            VoiceAnalyzer::analyse(x.data(), int(x.size()), kRate);
        QVERIFY2(a.valid, qPrintable(a.problem));

        // Die Tonhoehe muss erkannt sein, sonst greift der Wachhund
        // gar nicht und der Fall prueft nichts.
        QVERIFY2(std::abs(a.pitchHz - 110.0) < 8.0,
                 qPrintable(QStringLiteral("Grundton als %1 Hz gemessen")
                                .arg(a.pitchHz)));

        QVERIFY2(a.humMaskedHarmonics > 0,
                 "die Stimme belegt kein Brummfenster -- dann prueft "
                 "dieser Fall nicht, was er zu pruefen behauptet");

        // Entweder gar nicht messbar, oder weit unter der Schwelle.
        // Die alte Fassung meldete hier rund -12 dB.
        QVERIFY2(!a.humMeasurable || a.humDb < -35.0,
                 qPrintable(QStringLiteral("Brumm mit %1 dB gemeldet, "
                                           "obwohl keiner da ist")
                                .arg(a.humDb)));

        // Und keine Brummzeile in den Befunden.
        for (const QString& f : a.findings) {
            QVERIFY2(!f.contains(QStringLiteral("hum"), Qt::CaseInsensitive),
                     qPrintable(QStringLiteral("Befund behauptet Brumm: %1")
                                    .arg(f)));
        }
    }

    // Die Gegenrichtung: eine hohe Stimme mit echtem 50-Hz-Brumm. Der
    // MUSS weiter gefunden werden, sonst hat die Behebung die Messung
    // nur abgeschaltet.
    void realMainsHumIsStillFound()
    {
        const std::vector<float> x = voice(210.0, 8.0, 50.0, 0.05);
        const VoiceAnalysis a =
            VoiceAnalyzer::analyse(x.data(), int(x.size()), kRate);
        QVERIFY2(a.valid, qPrintable(a.problem));

        QVERIFY2(a.humMeasurable,
                 "bei einer 210-Hz-Stimme belegt nichts die "
                 "Brummfenster -- die Messung muss moeglich sein");
        QCOMPARE(a.humBaseHz, 50);
        QVERIFY2(a.humDb > -40.0,
                 qPrintable(QStringLiteral("echter Brumm nur mit %1 dB "
                                           "gemessen -- zu leise, der "
                                           "wird uebersehen")
                                .arg(a.humDb)));
    }
};

QTEST_APPLESS_MAIN(TstHumIsNotTheVoice)
#include "tst_hum_is_not_the_voice.moc"
