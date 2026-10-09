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
    // Blockgroesse des Durchlaufs. 128 Rahmen sind bei 48 kHz 2,67 ms.
    //
    // Es waren 256 bei drei Bloecken Vorrat, also rund 21 ms eigener
    // Anteil. Der Betreiber am 2026-10-09: „arbeite auch daran, dass
    // latenz noch geringer wird." Halbiert und der Vorrat von drei
    // auf zwei -- das sind rund 8 ms, und es geht nur, weil dieses
    // Objekt seit derselben Aenderung auf einem EIGENEN Faden laeuft.
    // Auf dem Bedienfaden war das nicht zu machen: dort haengt die
    // Antwortzeit an Zeichenarbeit und 100-ms-Zeitgebern, und ein
    // Vorrat von 5 ms waere regelmaessig leergelaufen.
    static constexpr int kBlockFrames = 128;

    // Soviel Vorrat haelt die Ausgabe. Zwei Bloecke sind 5,3 ms Spiel
    // gegen den Auseinanderlauf zweier Soundkartenuhren -- Eingang und
    // Ausgang koennen verschiedene Geraete sein und laufen dann
    // garantiert nicht gleich schnell. Weniger als zwei geht nicht:
    // einer wird gerade ausgegeben, der zweite ist der Vorrat.
    static constexpr int kTargetBlocks = 2;

    explicit StripLiveMonitor(QObject* parent = nullptr);
    ~StripLiveMonitor() override;

    // Laeuft es gerade?
    bool isRunning() const { return m_running.load(std::memory_order_acquire); }

    // Der eigene Anteil an der Verzoegerung, in Millisekunden.
    // Soundkarte und Treiber kommen oben drauf und sind von hier aus
    // nicht messbar.
    double latencyMs() const { return m_latencyMs.load(std::memory_order_acquire); }

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

private:
    void teardown();

    QThread* m_thread{nullptr};

    QAudioSource* m_src{nullptr};
    QAudioSink*   m_sink{nullptr};
    QIODevice*    m_srcIo{nullptr};
    QIODevice*    m_sinkIo{nullptr};

    std::unique_ptr<StripChain> m_chain;
    MicSpectrum* m_spectrum{nullptr};   // nicht besessen

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
