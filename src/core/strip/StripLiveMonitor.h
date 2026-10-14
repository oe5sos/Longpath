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
// sagt deshalb ausdruecklich nur den Anteil, den Longpath kennt --
// den eigenen Puffer UND die Verzoegerungsleitung des Entrauschens,
// die mit 1440 Abtastwerten mehr als das Dreifache des Puffers
// ausmacht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include <QAudioDevice>
#include <QAudioFormat>
#include "core/strip/BandLimit.h"
#include "core/strip/ClientFinalLimiter.h"

#include <QObject>
#include <QByteArray>
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
    // Eigener Puffer plus die Stufen, die eine Verzoegerung
    // mitbringen -- derzeit nur das Entrauschen. Live gerechnet und
    // nicht beim Start gemerkt: der Schalter des Entrauschens kann
    // sich jederzeit aendern, und eine Zahl, die dann stehenbleibt,
    // ist falscher als keine.
    double latencyMs() const;

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

    // ── Vorher und nachher, zum Ansehen ─────────────────────────────
    //
    // Zwei eigene Ringe: was hereinkommt und was nach der Kette
    // herausgeht. Daraus zeichnet StripTransferCurve die Antwort auf
    // die Frage, die diesen ganzen Tag getragen hat -- nicht „was
    // habe ich eingestellt", sondern WAS MACHT DIE KETTE.
    //
    // Eigene Ringe und nicht der des Kanalzugs: der traegt das rohe
    // Mikrofonsignal fuer die Entzerrerkurve und darf nicht zweimal
    // etwas anderes bedeuten.
    MicSpectrum&       inputRing()        { return *m_inRing; }
    MicSpectrum&       outputRing()       { return *m_outRing; }

    // Die Kette, von der abgeschrieben wird.
    //
    // Ohne sie holt sich resync() die Einstellung aus den
    // GESPEICHERTEN Werten -- und gespeichert wird erst, wenn die
    // Maus losgelassen wird. „leider hoere ich beim EQ erst den
    // unterschied, wenn ich per drag and drop ausgelassen habe."
    //
    // Mit ihr wird direkt von Kette zu Kette kopiert, im
    // Arbeitsspeicher, und zwar waehrend des Ziehens. Nicht besessen;
    // das Fenster setzt sie und raeumt den Mithoerer vor ihr ab.
    void setMirrorSource(const StripChain* live) { m_mirror = live; }

    // ── Von aussen, vom Bedienfaden ──────────────────────────────────
    //
    // Alles hier reicht nur weiter: die Arbeit geschieht auf dem
    // eigenen Faden dieses Objekts. Das ist der Grund, warum die
    // Puffer so klein sein duerfen.
    void start();
    void stop();
    void resync();
    void setVolume(double v);

    // ── Rückkopplungswache ──────────────────────────────────────────
    //
    // Am 2026-10-14 hat der Betreiber dreimal den Kopfhörer abreissen
    // müssen. Im Protokoll stand, warum: Eingang „Externes Mikrofon",
    // Ausgang „Externe Kopfhörer" -- beides dieselbe 3,5-mm-Buchse,
    // und die Kette legt +12 dB dazwischen. Das ist eine Schleife, und
    // eine Schleife wird lauter, bis etwas sie beendet. Bis dahin war
    // dieses Etwas sein Ohr.
    //
    // Also: bleibt der Ausgang laenger als anderthalb Sekunden ueber
    // -6 dBFS, schaltet der Mithoerer sich selbst stumm. Sprache
    // kommt dort in Spitzen hin, aber nicht dauerhaft -- eine
    // Schleife schon, und genau das ist der Unterschied.
    bool feedbackCut() const noexcept
    { return m_feedbackCut.load(std::memory_order_acquire); }
    void clearFeedbackCut() noexcept
    { m_feedbackCut.store(false, std::memory_order_release); m_hotBlocks = 0; }
    void setStripBypassed(bool bypassed);

    // ── Durch den Kanal hoeren ───────────────────────────────────────
    //
    // -1 = roh, sonst ein Index in BandLimit::channels(). Steht hinter
    // der Kette UND hinter dem Messabgriff: das Messbild sagt weiter,
    // was die KETTE tut, und das Ohr hoert, was der KANAL davon
    // uebriglaesst. Zwei verschiedene Fragen, zwei verschiedene
    // Antworten, beide gleichzeitig sichtbar.
    // Die Abtastrate, mit der der Mithoerer gerade laeuft.
    int  inputRate() const { return m_inRate; }

    QString inputName()  const { return m_inName; }
    QString outputName() const { return m_outName; }

    // ── Eigene Geraetewahl ───────────────────────────────────────────
    //
    // Bis zum 2026-10-10 nahm der Mithoerer stur die SYSTEMVORGABE.
    // Am Geraet des Betreibers hiess das: ein Apollo Twin X haengt
    // am Thunderbolt, laeuft auf 48 kHz, und Longpath hoerte
    // trotzdem das eingebaute MacBook-Mikrofon -- weil die Vorgabe
    // nie umgestellt war. Jede Beurteilung der Kette lief damit
    // ueber das falsche Mikrofon, ohne dass irgendwo etwas davon
    // stand.
    //
    // Leer heisst weiterhin „Systemvorgabe". Wirkt beim naechsten
    // Start; das Fenster haelt dafuer an und laeuft wieder an.
    void setInputDeviceId(const QByteArray& id);
    void setOutputDeviceId(const QByteArray& id);
    QByteArray inputDeviceId()  const { return m_wunschIn; }
    QByteArray outputDeviceId() const { return m_wunschOut; }

    void setMonitorChannel(int index);
    int  monitorChannel() const;
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
    // Der Kanal, durch den gehoert wird. Hinter der Kette.
    QString m_inName;
    QString m_outName;
    QByteArray m_wunschIn;
    QByteArray m_wunschOut;

    BandLimit m_band;
    // ── Der Begrenzer HINTER dem Kanal ───────────────────────────
    //
    // Clippen erzeugt Energie weit ausserhalb des Kanals, und das
    // Kanalfilter nimmt sie wieder weg -- genau die Energie, die die
    // Spitzen flach gehalten hat. Der Begrenzer in der Kette sieht
    // also eine andere Wellenform als die, die herauskommt.
    //
    // Am Geraet gemessen (2026-10-10, echte Sprache durch die
    // Empfehlung, Kanal 50-4050):
    //
    //   Kette -> Kanal                     Spitze +3,35 dBFS · -16,8 LUFS
    //   Kette ohne Begrenzer -> Kanal
    //     -> Begrenzer                     Spitze -2,00 dBFS · -12,3 LUFS
    //
    // Die zweite Reihenfolge haelt die Spitze UND ist 4,5 dB dichter.
    // Sie ist ausserdem die ehrlichere Nachbildung: im Sender steht
    // hinter dem Bandfilter auch eine Regelung, nicht davor.
    //
    // Nur aktiv, wenn ein Kanal gewaehlt ist. Bei „roh" haelt der
    // Begrenzer der Kette selbst, und ein zweiter waere eine Stufe
    // ohne Aufgabe.
    ClientFinalLimiter m_kanalBegrenzer;

    // ── Der rohe Weg wartet genauso lange ────────────────────────
    //
    // Beim Umschalten auf „roh" sprang bisher die Verzögerung weg:
    // die Kette braucht 30 ms (das Entrauschen), der ueberbrueckte
    // Weg keine. Wer so vergleicht, vergleicht nicht die Kette gegen
    // roh, sondern 30 ms verzoegerte eigene Stimme gegen
    // unverzoegerte -- und die verzoegerte klingt IMMER schlechter,
    // weil man sich selbst mit Nachhall hoert. Das ist kein
    // Klangunterschied, das ist ein Messfehler im Vergleich, und er
    // hat drei Tage lang jedes Urteil mitentschieden.
    //
    // Also wartet der rohe Weg genauso lange. Der Vergleich wird
    // dadurch nicht besser oder schlechter -- er wird ehrlich.
    static constexpr int kMaxVerzug = 1440;   // drei Rahmen
    std::vector<float> m_rohVerzug;
    int m_rohPos{0};
    int m_rohLaenge{0};
    void verzoegereRoh(float* samples, int frames) noexcept;

public:
    /// Welcher Eingang des Geraets gehoert wird. -1 legt alle
    /// zusammen -- richtig bei einem Mikrofon, falsch bei einem
    /// Mehrkanalwandler. Siehe m_inPick.
    void setInputPick(int index) noexcept;
    int  inputPick() const noexcept;

private:
    std::unique_ptr<MicSpectrum> m_inRing;
    std::unique_ptr<MicSpectrum> m_outRing;
    const StripChain* m_mirror{nullptr};   // nicht besessen
    int m_mirrorTick{0};
    // Der Uhrenausgleich: ein gleitendes Leseverhaeltnis statt
    // gestanzter Loecher. Siehe pump().
    double m_ratio{1.0};
    double m_readPos{0.0};
    float  m_lastOut{0.0f};
    std::vector<float> m_resampled;
    int m_logFrames{0};

    Latency m_preset{Latency::Safe};
    int m_blockFrames{256};
    int m_inRate{48000};
    int m_inChannels{1};

    // ── Welcher Eingang die Stimme traegt ────────────────────────
    //
    // Bis zum 2026-10-14 wurden ALLE Kanäle des Geräts gemittelt.
    // Bei einem Headset an der Klinke (ein Kanal) faellt das nicht
    // auf. An einem Apollo Twin mit sechzehn Eingaengen schon: das
    // Mikrofon liegt auf einem davon, die anderen fuenfzehn sind
    // still -- und die Division durch sechzehn nimmt der Stimme
    // 24 dB. Gemessen hat Longpath danach -55 dBFS, wo der
    // Vorverstaerker sauber ausgesteuert war.
    //
    // -1 heisst „zusammenlegen" (richtig fuer ein- und zweikanalige
    // Geraete), sonst der Index des einen Kanals. Steht hinter
    // StripMonitorInChannel.
    int m_inPick{-1};

    // Der tatsaechlich gehoerte Kanal, wenn keiner vorgegeben ist.
    // Faengt bei null an und wandert einmal weiter, wenn dort drei
    // Sekunden lang nichts war und anderswo schon -- sonst sitzt man
    // vor einem stillen Kopfhoerer und sucht den Fehler in der Kette.
    int m_autoPick{0};
    int m_stilleSek{0};

    // Je Kanal die Spitze der letzten Sekunde, fuer das Protokoll.
    // Ohne diese Zeile ist „der falsche Kanal" von „der Vorverstaerker
    // steht zu leise" nicht zu unterscheiden.
    std::vector<float> m_kanalSpitze;

    // Die Spitze der laufenden Sekunde, ein- und ausgangsseitig.
    // Siehe die Begruendung an der Messstelle in der .cpp.
    float m_sekSpitzeIn{0.0f};
    float m_sekSpitzeOut{0.0f};
    int m_inFormat{0};          // QAudioFormat::SampleFormat
    int m_outChannels{2};
    int m_outFormat{0};
    int m_outBytesPerFrame{4};

    std::vector<float> m_mono;      // Arbeitsspeicher, nie im Lauf belegt
    std::vector<char>  m_outBytes;
    std::vector<float> m_carry;     // Rest unter einer Blockgroesse

    double m_volume{0.7};

    // Wache und Einblendung. m_fade läuft nach jedem Start von 0 auf 1
    // -- ein Pegel, der im ersten Block voll da ist, ist ein Schlag
    // auf das Ohr, auch wenn er danach richtig wäre.
    std::atomic<bool> m_feedbackCut{false};
    int   m_hotBlocks{0};
    float m_fade{0.0f};
    std::atomic<bool> m_bypass{false};
    std::atomic<bool> m_running{false};
    std::atomic<double> m_latencyMs{0.0};
    std::atomic<int> m_underruns{0};
    std::atomic<int> m_overruns{0};
};

} // namespace Longpath
