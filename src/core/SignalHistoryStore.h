#pragma once

// =================================================================
// src/core/SignalHistoryStore.h  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/MainWindow.cpp at 0cd4559 — struct SHistoryEntry
//   (MainWindow.h:1023-1046), rebuildSHistoryForPan (:9568-9676),
//   expireSHistoryMarkers (:9732-9755) und die Zusammenfuehrung der
//   Treffer aus onSpectrumReadyForSHistory (:9830-9877).
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// Die zweite Haelfte des „S-Verlaufs": der Erkenner
// (VoiceSignalDetector) sagt, was in EINEM Bild steht, diese Klasse
// sagt, was ueber ZEIT daraus wird — welche Signale bleiben, welche
// nur aufblitzen, und welches davon eine Stoerung ist.
//
// VIER ABWEICHUNGEN vom Vorbild, alle mit Grund:
//
// 1. EIGENE KLASSE statt MainWindow-Innenleben. Bei AetherSDR liegen
//    Struktur und Logik in einer 20.000-Zeilen-MainWindow. Hier ist es
//    eine Klasse ohne Qt-Widget, ohne Zeitgeber und ohne Uhr — die Zeit
//    kommt als Parameter herein. Genau deshalb ist das Verhalten
//    pruefbar, ohne 6 Sekunden zu warten (tst_signal_history_store).
//
// 2. KEIN carrierScore. Die CNN-Ueberstimmung (`cnnSaysCarrier`,
//    Schwelle 0.70) haengt an AetherSDRs SignalClassifier. Der ist
//    nicht portiert, weil sein Modell nirgends existiert — Begruendung
//    in VoiceSignalDetector.h. Ohne Modell steht der Wert dort fest auf
//    0.5, und 0.5 > 0.70 ist falsch: der Zweig ist beim Vorbild also
//    ebenfalls tot. Ein toter Zweig wird nicht mitportiert, er wird
//    hier erklaert.
//
// 3. EINE LISTE statt QHash<panId, ...>. Bis Phase 3F gibt es genau ein
//    Panadapter. Dieselbe Begruendung wie bei den Anzeigeschaltern vom
//    selben Tag.
//
// 4. SCHWELLEN ALS FELDER statt AppSettings-Zugriffe mitten in der
//    Rechnung. Das Vorbild liest „SHistoryQrmGateS" und
//    „SHistoryLifetimeS" pro Durchlauf aus den Einstellungen. Diese
//    Klasse kennt die Einstellungen nicht — wer sie setzt, ist Sache
//    des Aufrufers. Sonst braeuchte jeder Test eine
//    Einstellungsdatei.
//
// Alle Zeitkonstanten und Schwellen sind unveraendert uebernommen.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QVector>
#include <QtGlobal>

#include "core/VoiceSignalDetector.h"

namespace NereusSDR {

// Ein ueber die Zeit verfolgtes Signal.
// Feldnamen wie AetherSDR SHistoryEntry (MainWindow.h:1023).
struct SignalHistoryEntry {
    double          freqMhz{0.0};
    float           peakDbm{-200.0f};
    QString         mode;
    qint64          firstDetectedMs{0};
    qint64          lastSeenMs{0};
    double          widthHz{0.0};
    bool            suspectQrm{false};
    // Hit timestamps for the last 10 seconds. Used to detect both
    // qualification streaks and QRM persistence (>90% occupancy).
    QVector<qint64> hitTimestamps;
    bool            visible{false};
    bool            confirmedVoice{false}; // true once shown as a gold voice marker
    qint64          lastGapMs{0};          // last time a ≥1 s gap was detected (epoch ms)
    // Last time a voice-width (1.8–8 kHz) signal was detected while this
    // entry was already QRM-classified.  Drives the "voice over QRM"
    // double-marker — shows both a red QRM marker and a gold voice marker.
    qint64          voiceOverQrmLastMs{0};
};

class SignalHistoryStore
{
public:
    // Treffer eines FFT-Bildes einarbeiten: bekannte Eintraege
    // auffrischen, unbekannte anlegen.
    //
    // Der Parameter heisst detections und nicht signals wie im Vorbild:
    // `signals` ist bei Qt ein Makro fuer `public`, und ein so
    // benannter Parameter uebersetzt nicht.
    void ingest(const QVector<DetectedVoiceSignal>& detections, qint64 nowMs);

    // Der Qualifizierungsdurchlauf: Zeitstempel altern lassen, Luecken
    // suchen, Stoerungen einordnen, ueber Sichtbarkeit entscheiden.
    //
    // observedFps: gemessene Bildrate. Das Vorbild leitet daraus ab, wie
    // viele Treffer 70 % Belegung entsprechen — eine feste Zahl waere
    // bei 10 fps und bei 60 fps verschieden falsch.
    // suppressUntilMs: bis dahin wird nichts neu sichtbar (Bandwechsel).
    void rebuild(qint64 nowMs, float observedFps, qint64 suppressUntilMs = 0);

    // Eintraege wegwerfen, die laenger als die Lebensdauer nicht mehr
    // gesehen wurden.
    void expire(qint64 nowMs);

    const QVector<SignalHistoryEntry>& entries() const { return m_entries; }
    QVector<SignalHistoryEntry> visibleEntries() const;
    void clear() { m_entries.clear(); }

    // Sekunden, die ein schmales oder breites Signal ununterbrochen
    // stehen muss, bis es als Stoerung gilt. Vorbild: Regler „QRM Gate",
    // Vorgabe 6 s, begrenzt auf 3..30.
    void setQrmGateSeconds(int s);
    int  qrmGateSeconds() const { return m_qrmGateS; }

    // Sekunden ohne Sichtung, nach denen ein Eintrag verschwindet.
    // Vorbild: Regler „Lifetime", Vorgabe 60 s, begrenzt auf 15..300.
    void setLifetimeSeconds(int s);
    int  lifetimeSeconds() const { return m_lifetimeS; }

private:
    QVector<SignalHistoryEntry> m_entries;
    int m_qrmGateS{6};
    int m_lifetimeS{60};
};

} // namespace NereusSDR
