#pragma once

// =================================================================
// src/core/strip/StripLiveMonitor.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Sich selbst hoeren, waehrend man schraubt.
//
// Am 2026-08-11 gab es das schon einmal, als Haekchen "Hear myself"
// im Kanalzugfenster, und es flog wieder hinaus. Der Grund steht im
// Commit 49d10dc8 und ist wichtig genug, hier zu stehen, damit ihn
// niemand ein zweites Mal findet: der damalige Weg hing am
// Mikrofonstrom des FUNKGERAETS. Dessen eigene Taktmessung verurteilte
// ihn -- 3190 bis 3639 Pumpentakte je 5 s, wo 3755 hingehoeren, also
// 3 bis 15 % verlorene Bloecke. Ein Sample, das nie ankam, laesst sich
// nicht zurueckpuffern; es stotterte, egal was dahinter sass.
//
// Deshalb beruehrt diese Fassung das Funkgeraet mit keinem Finger:
//
//   Soundkarte ein  ->  eigener StripChain  ->  Soundkarte aus
//
// Ein Taktgeber, einer der des Rechners, und keine Netzstrecke
// dazwischen -- derselbe Grund, aus dem AetherSDRs Kette nie
// stotterte. Dass es dasselbe HOERT wie die Sendekette, liegt nicht
// an gemeinsamem Code auf dem Audiofaden, sondern daran, dass dieser
// Kanalzug aus derselben gespeicherten Einstellung wiederhergestellt
// wird: jede Schraube im Fenster schreibt, danach ruft das Fenster
// resync().
//
// Warum ein EIGENER Kanalzug und nicht der des Funkgeraets: dessen
// Stufen halten Filterzustaende und Verzoegerungsleitungen, und die
// Sendepumpe laeuft darauf auf ihrem eigenen Faden. Zwei Audiofaeden
// in denselben Zustaenden sind ein Datenrennen, und zwar eines, das
// nur im Betrieb und nur manchmal knackt. Parameter dagegen liegen in
// Atomaren mit Versionszaehler -- genau dafuer sind sie gebaut, und
// das Fenster schreibt auch in die Sendekette aus dem Bedienfaden.
//
// Ueber die Latenz wird hier nicht geschwindelt. Was dieses Modul
// kennt, ist der eigene Puffer auf beiden Seiten; was die Soundkarte
// und CoreAudio darunter noch dazulegen, sieht es nicht. latencyMs()
// sagt deshalb ausdruecklich nur den eigenen Anteil.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include <QAudioDevice>
#include <QAudioFormat>
#include <QObject>
#include <QString>

#include <atomic>
#include <memory>
#include <vector>

class QAudioSink;
class QAudioSource;
class QIODevice;
class QThread;

namespace Longpath {

class MicSpectrum;
class StripChain;

class StripLiveMonitor : public QObject {
    Q_OBJECT

public:
    // ── Wie knapp es laufen darf ────────────────────────────────────
    //
    // Das war einmal fest: 256 Rahmen, drei Bloecke Vorrat, 21 ms.
    // Dann fest auf 128 und zwei, 8 ms -- und beim Betreiber im
    // Kopfhoerer "komplettes krachen", ohne dass er etwas getan
    // haette. Das war kein Ausrutscher, sondern der vorhersehbare
    // Ausgang: wie knapp ein Rechner laufen KANN, haengt an seinen
    // Geraeten, seinen Treibern und daran, was sonst noch laeuft --
    // und nichts davon weiss der, der die Zahl in den Quelltext
    // schreibt.
    //
    // Also wird sie nicht mehr im Quelltext entschieden. Drei Stufen,
    // und die sichere ist die Vorgabe. Wer weniger will, nimmt sie
    // sich und sieht am Zaehler daneben sofort, ob sein Rechner sie
    // traegt.
    enum class Latency { Safe = 0, Low = 1, Lowest = 2 };

    static int blockFramesFor(Latency l)
    {
        switch (l) {
        case Latency::Lowest: return 128;
        case Latency::Low:    return 192;
        default:              return 256;
        }
    }
    // Drei. Nie zwei: einer wird gerade ausgegeben, einer ist
    // unterwegs, und der dritte ist der eigentliche Vorrat gegen zwei
    // Soundkartenuhren, die nie gleich schnell laufen. Mit zweien war
    // jeder Zeitgeberhuepfer ein Loch.
    static constexpr int kTargetBlocks = 3;

    // Der Puffer der Ausgabe ist absichtlich GROESSER als der Vorrat,
    // den wir halten wollen. Sonst kann write() nicht alles
    // unterbringen und schneidet mitten im Block ab -- und ein
    // abgeschnittener Block knackt lauter als ein ganzer, der fehlt.
    // Gesteuert wird die Verzoegerung dann nicht ueber die
    // Puffergroesse, sondern darueber, wie viel wir stehen lassen.
    static constexpr int kSinkBlocks = 8;

    explicit StripLiveMonitor(QObject* parent = nullptr);
    ~StripLiveMonitor() override;

    // Laeuft es gerade?
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    // Der eigene Anteil an der Verzoegerung, in Millisekunden.
    // Soundkarte und Treiber kommen oben drauf und sind von hier aus
    // nicht messbar.
    double latencyMs() const { return m_latencyMs.load(std::memory_order_acquire); }

    Latency latencyPreset() const { return m_preset; }
    // Im Lauf umschaltbar: haelt an, stellt um, laeuft wieder an.
    void setLatencyPreset(Latency l);

    // Wie viele Bloecke seit dem Start nachgelegt (Unterlauf) oder
    // weggeworfen (Ueberlauf) werden mussten. Null bleibt es nie ganz;
    // interessant wird es, wenn es waechst, waehrend man zuhoert.
    int underruns() const { return m_underruns.load(std::memory_order_acquire); }
    int overruns()  const { return m_overruns.load(std::memory_order_acquire); }

    // Der Kanalzug, durch den gehoert wird. Gehoert diesem Objekt.
    StripChain* chain() const { return m_chain.get(); }

    // Wohin das ROHE Mikrofonsignal zusaetzlich geschrieben wird.
    //
    // Dafuer: die Entzerrerkurve und ihr Peak-Hold. Die zeichnen aus
    // `StripChain::micSpectrum()`, und das fuellte bisher allein die
    // Sendepumpe -- ohne Funkgeraet also gar nichts, und eine
    // Entzerrerkurve ohne Spektrum darunter ist genau die Hilfe, die
    // fehlt, wenn man von Hand einstellt (Betreiber, 2026-10-09:
    // „beim eq seine eigene kurve mit hold visuell zu sehen um zu
    // erkennen, wo die eq kurve angehoben oder gesenkt werden muss").
    //
    // Roh und nicht bearbeitet, weil EqHost die Kurve ueber ein
    // Spektrum VOR dem Entzerrer legt -- eine Kurve auf ein Spektrum
    // zu richten, das sie selbst schon verbogen hat, heisst dem
    // eigenen Schwanz nachlaufen.
    void setSpectrumSink(MicSpectrum* sink) { m_spectrum = sink; }

    // ── Von aussen, vom Bedienfaden ──────────────────────────────────
    //
    // Alles hier reicht nur weiter: die Arbeit geschieht auf dem
    // eigenen Faden dieses Objekts. Das ist der Grund, warum die
    // Puffer so klein sein duerfen.
    void start();
    void stop();
    void resync();
    void setVolume(double v);
    void setStripBypassed(bool bypassed);
    bool stripBypassed() const { return m_bypass.load(std::memory_order_acquire); }

    // Genau der Schritt, den pump() mit jedem Block macht -- als
    // eigene Methode, damit er ohne Soundkarte geprueft werden kann.
    //
    // Gebraucht, weil „ich hoere keinen Unterschied, ob an oder aus"
    // zwei voellig verschiedene Ursachen haben kann: der Schalter
    // wirkt nicht, oder die Kette tut nichts. Das ist von aussen
    // nicht zu unterscheiden und von innen in drei Zeilen.
    void applyStrip(float* samples, int frames) noexcept;

signals:
    void started();
    void stopped();
    void failed(const QString& why);

private slots:
    void pump();
    void doStart();
    void doStop();
    void doResync();
    void doSetVolume(double v);
    void doSetLatencyPreset(int l);

private:
    void teardown();

    QThread* m_thread{nullptr};

    QAudioSource* m_src{nullptr};
    QAudioSink*   m_sink{nullptr};
    QIODevice*    m_srcIo{nullptr};
    QIODevice*    m_sinkIo{nullptr};

    std::unique_ptr<StripChain> m_chain;
    MicSpectrum* m_spectrum{nullptr};   // nicht besessen

    Latency m_preset{Latency::Safe};
    int m_blockFrames{256};
    int m_inRate{48000};
    int m_inChannels{1};
    int m_inFormat{0};          // QAudioFormat::SampleFormat
    int m_outChannels{2};
    int m_outFormat{0};
    int m_outBytesPerFrame{4};

    std::vector<float> m_mono;      // Arbeitsspeicher, nie im Lauf belegt
    std::vector<char>  m_outBytes;
    std::vector<float> m_carry;     // Rest unter einer Blockgroesse

    double m_volume{0.7};
    std::atomic<bool> m_bypass{false};
    std::atomic<bool> m_running{false};
    std::atomic<double> m_latencyMs{0.0};
    std::atomic<int> m_underruns{0};
    std::atomic<int> m_overruns{0};
};

} // namespace Longpath
