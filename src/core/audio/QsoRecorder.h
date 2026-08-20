#pragma once

// no-port-check: Verhalten an Thetis clsAudioRecordPlayback.cs angelehnt (Feldauswahl der
// Beschreibung, Stereo-Spurenaufteilung); KEIN Zeilenport, kein C#-Code
// uebernommen. Herkunft steht als Zeile 'reference' in THETIS-PROVENANCE.md.

// =================================================================
// src/core/audio/QsoRecorder.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, im Verhalten an Thetis angelehnt
// (clsAudioRecordPlayback.cs, MW0LGE — AudioRecordRxSource /
// AudioRecordTxSource und die JSON-Beschreibung neben der WAV).
//
// Ein QSO aufnehmen: was ankommt UND was man selbst sagt.
//
// Ansage des Betreibers (2026-08-19): „wichtig ist, dass man sich
// selbst aber auch die andere station hört!"
//
// ── Eine Datei, zwei Spuren ─────────────────────────────────────────
//
// Links die Gegenstation, rechts die eigene Stimme. Thetis schreibt je
// Quelle eine eigene Datei; eine gemeinsame Stereodatei haelt beide
// dagegen SYNCHRON, und genau darum geht es bei einem QSO — wer wann
// was gesagt hat. Zwei Dateien laufen beim Abspielen auseinander,
// sobald jemand eine davon schneidet.
//
// Wer die Spuren getrennt braucht, trennt sie in jedem Audioprogramm
// mit zwei Klicks. Wer sie synchron braucht, bekommt sie aus zwei
// Dateien nie wieder zusammen.
//
// ── Die eigentliche Schwierigkeit: die Ausrichtung ──────────────────
//
// Der Empfang laeuft dauernd, das Mikrofon nur beim Senden. Wer beide
// Spuren einfach hintereinander anhaengt und am Ende die kuerzere
// auffuellt, bekommt eine Aufnahme, in der die eigene Stimme GANZ AM
// ANFANG steht — egal wann sie gesprochen wurde.
//
// Deshalb ist der EMPFANG DIE UHR: kommt Mikrofonton, wird die
// Sprechspur vorher mit Stille bis zum aktuellen Empfangsstand
// aufgefuellt. Dann liegt die eigene Stimme dort, wo sie hingehoert.
//
// ── Was hier NICHT drin ist ─────────────────────────────────────────
//
// Das Anschalten der Abgriffe. Diese Klasse bekommt Abtastwerte
// gereicht und weiss nicht, woher — so laesst sie sich ohne Funkgeraet,
// ohne Audiogeraet und ohne WDSP pruefen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QDateTime>
#include <QString>
#include <QVector>

namespace Longpath {

// Was neben der Aufnahme steht. Feldauswahl aus Thetis
// RecordingDetails (clsAudioRecordPlayback.cs:88-108) — ohne die
// MP3-Felder, weil wir kein MP3 schreiben.
struct QsoRecordingInfo {
    QDateTime utcStart;
    QString   frequency;   // „14.205.000"
    QString   mode;        // „LSB"
    QString   band;        // „20m"
    QString   callsign;    // Gegenstation, falls bekannt
    int       sampleRate{0};
    double    seconds{0.0};
};

// Die Beschreibung neben einer Aufnahme wieder einlesen.
//
// Aus der Thetis-Durchsicht vom 2026-08-19 (`GetJSONDetailsFromFile`,
// clsAudioRecordPlayback.cs:1102 [v2.10.3.15-5-g852bf0e]): Thetis liest
// seine Beschreibungen zurueck, wir haben sie bisher nur geschrieben.
// Der Unterschied steht in der Liste der Aufnahmen — „19.08. 18:02 ·
// DL1ABC · 14.205.000 · LSB" statt „qso-20260819-180213".
//
// `wavPath` darf auf die WAV oder direkt auf die JSON zeigen. Bei
// Fehlern kommt ein leerer Datensatz zurueck (utcStart ungueltig); eine
// Aufnahme ohne Beschreibung ist kein Fehler, nur aermer.
QsoRecordingInfo readQsoDescription(const QString& wavPath);

class QsoRecorder
{
public:
    // 30 Minuten bei 48 kHz sind rund 330 MB in float32-Stereo. Das ist
    // die Grenze, ab der eine vergessene Aufnahme die Platte auffrisst.
    static constexpr int kMaxMinutes = 30;

    void setSampleRate(int hz);
    int  sampleRate() const { return m_rate; }

    void start(const QsoRecordingInfo& info);
    void stop();
    bool isRecording() const { return m_recording; }

    // Empfangston, verschachtelt Stereo, wie AudioEngine ihn fuehrt.
    // Wird auf einen Kanal gemischt: eine Aufnahme des QSOs ist keine
    // Musikproduktion, und die Haelfte des Platzes ist es wert.
    void feedRx(const float* interleavedStereo, int frames);

    // Mikrofonton, einkanalig, wie der Sendeabgriff ihn liefert.
    void feedTx(const float* mono, int frames);

    int rxFrames() const { return m_rx.size(); }
    int txFrames() const { return m_tx.size(); }
    double recordedSeconds() const;

    // Schreibt die Stereodatei (links Empfang, rechts eigene Stimme)
    // und daneben eine JSON-Beschreibung, wie Thetis es tut.
    //
    // 16 Bit mit Dither, es sei denn jemand will ausdruecklich float32
    // (siehe setSaveFloat32). Eine halbe Stunde Stereo sind in float32
    // 690 MB, in PCM16 noch 173 MB — und der Dynamikumfang eines
    // Sprach-QSOs liegt weit unter dem, was 16 Bit tragen.
    bool save(const QString& wavPath, QString* error = nullptr) const;

    // Fuer den Fall, dass jemand die Aufnahme weiterverarbeiten will
    // und keine Rundung im Weg haben moechte. Vorgabe ist 16 Bit.
    void setSaveFloat32(bool on) { m_saveFloat32 = on; }
    bool saveFloat32() const { return m_saveFloat32; }

    void clear();

    const QsoRecordingInfo& info() const { return m_info; }

private:
    void padTxToRx();

    QVector<float>   m_rx;
    QVector<float>   m_tx;
    QsoRecordingInfo m_info;
    int              m_rate{48000};
    bool             m_recording{false};
    bool             m_saveFloat32{false};
};

} // namespace Longpath
