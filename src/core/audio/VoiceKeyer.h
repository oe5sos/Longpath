#pragma once

// no-port-check: Nennt Thetis clsAudioRecordPlayback.cs (MW0LGE) als Verhaltensquelle fuer
// den Sprachspeicher; NereusSDR-original, kein Zeilenport. Siehe
// THETIS-PROVENANCE.md, Art 'reference'.

// =================================================================
// src/core/audio/VoiceKeyer.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, im Verhalten an Thetis angelehnt
// (clsAudioRecordPlayback.cs, MW0LGE — insbesondere MoxOnPlayback und
// die Trennung von Aufnahmequelle und Wiedergabeweg).
//
// Der Sprachspeicher: zehn Ansagen aufnehmen, mit Tasten abrufen, als
// CQ senden. Ansage des Betreibers am 2026-08-19: „sinn macht auch,
// dort 10 audio files aufnehmen zu koennen und diese dann mit
// shortcuts zu verbinden und als CQ CALL usw. abspielen zu lassen."
//
// ── Warum das hier NICHT AetherSDRs DVK ist ──────────────────────────
//
// AetherSDR hat einen DVK, aber er ist FUNKGERAETSEITIG: FlexRadio
// speichert die Ansagen im Geraet, und DvkWavTransfer schiebt WAVs per
// TCP hin und her (DvkWavTransfer.h [@0cd4559]). OpenHPSDR-Geraete haben
// keinen solchen Speicher — bei uns muss alles im Rechner liegen.
// Deshalb ist der Aufbau ein anderer, obwohl das Bedienbild dasselbe
// bleibt.
//
// ── Zwei Teile, absichtlich getrennt ────────────────────────────────
//
//   VoiceKeyerStore   verwaltet zehn Plaetze: Beschriftung, Datei,
//                     Tastenkuerzel, Dauer. Kennt kein Audio.
//   WavTxSource       spielt eine geladene Ansage in den Sendeweg,
//                     ueber TxMicRouter wie jede andere Mikrofonquelle.
//                     Kennt keine Plaetze.
//
// Die Trennung ist der Punkt: der Speicher laesst sich ohne Audio
// pruefen, die Quelle ohne Dateien. Zusammengeschweisst braeuchte jeder
// Test ein Funkgeraet.
//
// ── Was hier NICHT drin ist ──────────────────────────────────────────
//
// Das TASTEN des Senders. Thetis' MoxOnPlayback schaltet beim Abspielen
// auf Senden; bei uns gehoert das in MoxController, und der wird von
// hier aus NICHT gerufen. Grund: eine Klasse, die Audio abspielt UND
// den Sender tastet, kann man nicht mehr gefahrlos testen — und ein
// Fehler darin sendet.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/TxMicRouter.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

namespace NereusSDR {

// Zehn Plaetze, wie vom Betreiber verlangt. Nicht mehr: eine Taste je
// Platz, und mehr als zehn Tasten merkt sich niemand.
inline constexpr int kVoiceKeyerSlots = 10;

struct VoiceKeyerSlot {
    QString label;       // „CQ", „73", „Rufzeichen" …
    QString wavPath;     // leer = Platz frei
    QString shortcut;    // z. B. „F1"; leer = keine Taste
    double  seconds{0.0};

    bool isEmpty() const { return wavPath.isEmpty(); }
};

class VoiceKeyerStore : public QObject
{
    Q_OBJECT

public:
    explicit VoiceKeyerStore(QObject* parent = nullptr);

    // Ordner, in dem die Ansagen liegen. Wird bei Bedarf angelegt.
    QString folder() const { return m_folder; }
    void setFolder(const QString& path);

    const VoiceKeyerSlot& slot(int index) const;
    int slotCount() const { return kVoiceKeyerSlots; }

    // Eine fertige Aufnahme uebernehmen. Die Abtastwerte kommen von
    // TxAudioRecorder; hier werden sie unter einem festen Namen im
    // Ordner abgelegt, damit ein Platz nach dem Neustart wieder da ist.
    bool setRecording(int index, const QVector<float>& samples,
                      int sampleRate, QString* error = nullptr);

    // Eine vorhandene Datei uebernehmen (WAV von anderswo).
    bool importFile(int index, const QString& path, QString* error = nullptr);

    void setLabel(int index, const QString& label);
    void setShortcut(int index, const QString& keys);

    // Platz leeren. Loescht die Datei NICHT: wer sich verklickt, soll
    // seine Ansage nicht verlieren. Aufraeumen ist eine eigene
    // Entscheidung und gehoert dem Betreiber.
    void clearSlot(int index);

    void load();
    void save() const;

signals:
    void slotChanged(int index);

private:
    QString slotFileName(int index) const;

    QVector<VoiceKeyerSlot> m_slots;
    QString m_folder;
};

// ── Die Wiedergabe in den Sendeweg ──────────────────────────────────
//
// Eine Mikrofonquelle wie jede andere: TxChannel zieht Abtastwerte,
// hier kommen sie aus einer geladenen Ansage statt aus einem Mikrofon.
//
// pullSamples() laeuft im Audio-Faden. Deshalb: kein Sperren, kein
// Speicher anfordern, keine Datei anfassen. Geladen und umgetastet wird
// VORHER, in load().
class WavTxSource : public TxMicRouter
{
public:
    // Ansage laden und auf die Senderate bringen. Gibt false zurueck,
    // wenn die Datei nicht lesbar ist.
    bool load(const QString& path, int txSampleRate, QString* error = nullptr);

    void play();          // von vorn
    void stop();
    bool isPlaying() const { return m_playing.load(std::memory_order_relaxed); }

    // Wiederholen mit Pause dazwischen — das ist der CQ-Ruf im
    // Contest-Betrieb. Vorgabe aus: eine Ansage, die von selbst wieder
    // anfaengt, ueberrascht jeden beim ersten Mal.
    void setRepeat(bool on, double gapSeconds = 2.0);
    bool repeats() const { return m_repeat.load(std::memory_order_relaxed); }

    int sampleCount() const { return m_samples.size(); }
    double seconds() const;

    // TxMicRouter
    int pullSamples(float* dst, int n) override;

private:
    QVector<float>      m_samples;
    int                 m_rate{0};
    int                 m_gapSamples{0};
    std::atomic<int>    m_pos{0};
    std::atomic<bool>   m_playing{false};
    std::atomic<bool>   m_repeat{false};
};

} // namespace NereusSDR
