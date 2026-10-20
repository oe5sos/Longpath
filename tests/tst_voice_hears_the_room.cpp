// no-port-check: Longpath-original test file.
// =================================================================
// tests/tst_voice_hears_the_room.cpp  (Longpath)
// =================================================================
// Die drei Zahlen, bei denen Software aufhört zu helfen: Nachhall,
// Poppen, Mikrofonabstand. Ein Entzerrer legt keinen Raum trocken
// und ein Kompressor repariert kein Poppen -- beide machen es
// schlimmer, weil sie anheben. Deshalb muss der Befund stimmen.
//
// Geprüft wird gegen Signale mit BEKANNTER Antwort: ein künstlicher
// Nachhall mit eingestellter Abklingzeit, und ein Stoss unter 80 Hz
// an einer bekannten Stelle.
//
// Modification history (Longpath):
//   2026-10-10 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QtTest>

#include "core/VoiceAnalyzer.h"

#include <cmath>
#include <random>
#include <vector>

using namespace Longpath;

namespace {

constexpr int kRate = 48000;

// Sprachähnlich: Silben von 300 ms mit 200 ms Pause dazwischen, ein
// Oberwellensatz statt echter Sprache. Für die Hüllkurve, um die es
// hier geht, ist das genug -- gemessen wird, was NACH der Silbe
// passiert.
std::vector<float> syllables(double seconds, double noiseAmp)
{
    const int n = int(seconds * kRate);
    std::vector<float> v(static_cast<size_t>(n), 0.0f);
    std::mt19937 rng(7);
    std::normal_distribution<float> nd(0.0f, float(noiseAmp));
    for (int i = 0; i < n; ++i) {
        const double t = double(i) / kRate;
        // 0,3 s Silbe, 0,7 s Pause. Erster Versuch waren 0,2 s
        // Pause: der Nachhall kam darin nie unten an, zwischen
        // Grundgeräusch und Sprache lagen nur 10,8 dB, und die
        // Messung fand zu Recht kein Abklingen von 10 dB. Das Signal
        // war zu kurz atmend, nicht die Messung zu stumpf.
        const bool on = std::fmod(t, 1.0) < 0.3;
        double s = 0.0;
        if (on) {
            for (int h = 1; h <= 8; ++h) {
                s += std::sin(2.0 * M_PI * 150.0 * h * t) / h;
            }
            s *= 0.12;
        }
        v[size_t(i)] = float(s) + nd(rng);
    }
    return v;
}

// Ein Raum, nachgebaut: jede Silbe klingt exponentiell aus. tau wird
// so gewählt, dass der Pegel in rt60 Sekunden um 60 dB fällt.
void addDecay(std::vector<float>& v, double rt60)
{
    const double per = std::pow(10.0, -3.0 / (rt60 * kRate));  // je Wert
    float tail = 0.0f;
    for (size_t i = 0; i < v.size(); ++i) {
        tail = float(double(tail) * per);
        if (std::fabs(v[i]) > std::fabs(tail)) { tail = v[i]; }
        v[i] = tail;
    }
}

} // namespace

class TstVoiceHearsTheRoom : public QObject {
    Q_OBJECT

private slots:
    // Ein trockenes Signal darf nicht nach Halle aussehen, ein
    // halliges muss es. Und die Zahl muss in der richtigen
    // Grössenordnung liegen -- auf zwanzig Prozent genau wäre bei
    // einer Schätzung aus Sprachenden gelogen.
    void itTellsADryRoomFromALiveOne()
    {
        auto dry = syllables(12.0, 0.0004);
        const auto a = VoiceAnalyzer::analyse(dry.data(), int(dry.size()),
                                              kRate);
        QVERIFY2(a.valid || a.problem.isEmpty(), qPrintable(a.problem));

        auto wet = syllables(12.0, 0.0004);
        addDecay(wet, 0.8);
        const auto b = VoiceAnalyzer::analyse(wet.data(), int(wet.size()),
                                              kRate);

        qInfo() << "trocken:" << a.reverbMs << "ms (gueltig" << a.reverbValid
                << ") · hallig:" << b.reverbMs << "ms (gueltig"
                << b.reverbValid << ")";

        QVERIFY2(b.reverbValid, "im halligen Signal wurde kein Abklingen "
                                "gefunden");
        // Eingestellt sind 800 ms. Aus Sprachenden geschätzt, mit
        // einem Grundgeräusch darunter, ist das keine Messung nach
        // Norm -- die Spanne ist entsprechend weit gefasst.
        QVERIFY2(b.reverbMs > 450.0 && b.reverbMs < 1400.0,
                 qPrintable(QStringLiteral("%1 ms statt rund 800")
                                .arg(b.reverbMs, 0, 'f', 0)));
        if (a.reverbValid) {
            QVERIFY2(b.reverbMs > a.reverbMs * 1.8,
                     qPrintable(QStringLiteral("trocken %1 ms, hallig %2 ms "
                                               "-- kaum zu unterscheiden")
                                    .arg(a.reverbMs, 0, 'f', 0)
                                    .arg(b.reverbMs, 0, 'f', 0)));
        }
    }

    // Ein Stoss unter 80 Hz wird gefunden, und ohne ihn wird keiner
    // erfunden.
    void itFindsAPopAndDoesNotInventOne()
    {
        auto clean = syllables(12.0, 0.0004);
        const auto a = VoiceAnalyzer::analyse(clean.data(),
                                              int(clean.size()), kRate);

        auto popped = clean;
        // Drei Stösse mitten in Silben: 40 Hz, 60 ms, kräftig.
        for (const double at : {1.1, 3.1, 5.1}) {
            const int from = int(at * kRate);
            for (int i = 0; i < kRate * 6 / 100; ++i) {
                const double t = double(i) / kRate;
                const double env = std::exp(-t * 40.0);
                popped[size_t(from + i)] +=
                    float(0.5 * env * std::sin(2.0 * M_PI * 40.0 * t));
            }
        }
        const auto b = VoiceAnalyzer::analyse(popped.data(),
                                              int(popped.size()), kRate);

        qInfo() << "ohne Stoss:" << a.plosiveCount << "mit:"
                << b.plosiveCount << "staerkster" << b.plosiveDb << "dB";

        QVERIFY2(b.plosiveCount >= 3,
                 qPrintable(QStringLiteral("nur %1 Stösse gefunden")
                                .arg(b.plosiveCount)));
        QVERIFY2(b.plosiveDb > 12.0,
                 qPrintable(QStringLiteral("staerkster nur %1 dB")
                                .arg(b.plosiveDb, 0, 'f', 1)));
        QVERIFY2(a.plosiveCount == 0,
                 qPrintable(QStringLiteral("im sauberen Signal %1 Stösse "
                                           "erfunden").arg(a.plosiveCount)));
    }
};

QTEST_MAIN(TstVoiceHearsTheRoom)
#include "tst_voice_hears_the_room.moc"
