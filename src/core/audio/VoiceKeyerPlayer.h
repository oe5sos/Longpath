// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/audio/VoiceKeyerPlayer.h  (Longpath)
// =================================================================
//
// Longpath-original. Der Sprachspeicher im Betrieb: eine Ansage senden,
// den CQ-Ruf wiederholen, eine Ansage aufnehmen -- per Knopf im Panel
// oder per Taste (F1…F10 senden, Strg+Umschalt+F1…F10 nimmt auf, Esc
// bricht ab, wie in N1MM Logger+).
//
// ── Warum es das gibt ───────────────────────────────────────────────
//
// VoiceKeyerStore verwaltet die zehn Plaetze, WavTxSource spielt eine
// Ansage in den Sendeweg; beide tasten absichtlich NICHT (Begruendung
// in VoiceKeyer.h). Bis heute hat niemand die beiden zusammengefuehrt:
// der Abspielknopf im Panel war gesperrt mit „needs the transmit path,
// which is the next step". Das ist dieser Schritt.
//
// ── Wie es tastet ───────────────────────────────────────────────────
//
// Wie das eigene FT8 (Ft8Applet): erst den Weg legen (Mikrofon-Router
// auf MicSource::Dvk), dann tasten (MoxController::setMox), Ablehnung
// des Bandplans abfangen und sagen. Nach der Ansage 300 ms Nachlauf --
// derselbe Zuschlag wie bei FT8, damit der letzte Laut draussen ist --,
// dann los und den Weg zurueck.
//
// Der Spieler selbst kennt weder Router noch MoxController: beides
// kommt als Hooks herein. So laesst er sich ohne Funkgeraet pruefen,
// und ein Fehler im Pruefstand sendet nichts.
//
// ── Die Regeln ──────────────────────────────────────────────────────
//
// * Eine neue Ansage waehrend einer laufenden: die laufende endet, die
//   neue beginnt, der Sender bleibt dabei an (N1MM).
// * Wiederholen: nach der Ansage geht der Sender AUS, die Pause ist
//   Empfang, dann kommt der Ruf wieder. Bei WavTxSource::setRepeat
//   liefe die Pause als Stille auf Sendung -- fuer einen CQ-Ruf genau
//   falsch, man will in der Pause hoeren, ob jemand antwortet.
// * Wer in der Pause selbst tastet (Sprechtaste, MOX), beendet das
//   Wiederholen. Wer waehrend der Ansage den Sender ausmacht, beendet
//   die Ansage.
// * Ist der Sender schon an, ohne dass der Spieler ihn getastet hat
//   (gehaltene Sprechtaste, TUNE), wird nicht gesendet: sonst schaltete
//   das Ende der Ansage einen fremden Sender ab.
// * Aufnehmen tastet nie. Es liest am Mikrofonabgriff mit
//   (preStripAudioReady), wie bisher das Panel.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude. Die Aufnahme
//                 ist aus DvkApplet hierher umgezogen, damit sie auch
//                 ohne offenes Panel per Taste geht.
// =================================================================

#include "core/audio/VoiceKeyer.h"

#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QTimer>

#include <atomic>
#include <functional>

namespace Longpath {

class TxAudioRecorder;
class TxWorkerThread;

class VoiceKeyerPlayer : public QObject
{
    Q_OBJECT

public:
    enum class State { Idle, Playing, Gap, Recording };
    Q_ENUM(State)

    // Rate des Sendewegs, in der die Ansage abgespielt wird, und des
    // Mikrofonabgriffs, an dem aufgenommen wird. Dieselbe Zahl wie der
    // Sprach-Selbsttest und die FT8-Quelle.
    static constexpr int kRateHz = 48000;
    // Nachlauf nach dem letzten Abtastwert, bevor der Sender abschaltet.
    // Wie Ft8Applet (dort gemessen am 2026-10-22/30).
    static constexpr int kTailMs = 300;
    // So oft wird nachgesehen, ob die Ansage zu Ende ist.
    static constexpr int kPollMs = 20;

    struct Hooks {
        // Den Mikrofon-Router auf den Sprachspeicher legen. false: es
        // gibt keinen (Flex, TS-590) -- dann geht Senden nicht.
        std::function<bool()> routeToVoiceKeyer;
        std::function<void()> restoreRoute;
        // Den Sender tasten oder los. Leer bei Erfolg, sonst der Grund
        // der Ablehnung (Bandplan, falsche Betriebsart …).
        std::function<QString(bool on)> key;
        // Ist der Sender gerade an (MOX, Sprechtaste, TUNE)?
        std::function<bool()> keyed;
        // Fuer die Aufnahme; nullptr, solange keine Verbindung steht.
        std::function<TxWorkerThread*()> txWorker;
    };

    explicit VoiceKeyerPlayer(VoiceKeyerStore* store, QObject* parent = nullptr);
    ~VoiceKeyerPlayer() override;

    void setHooks(Hooks hooks) { m_hooks = std::move(hooks); }

    // Die Quelle, die am Mikrofon-Router haengt. Lebt so lange wie der
    // Spieler.
    WavTxSource* source() { return &m_source; }

    // Eine Ansage senden. Leer bei Erfolg, sonst ein Satz fuer den
    // Betreiber, warum nicht.
    QString play(int slot);
    // Ansage, Wiederholung und Aufnahme beenden. Eine Aufnahme wird
    // dabei gespeichert, nicht verworfen.
    void stop();
    // Aufnahme beginnen. Laeuft schon eine auf DIESEM Platz, wird sie
    // beendet und gespeichert -- dieselbe Taste startet und stoppt.
    QString record(int slot);

    State state() const { return m_state; }
    int activeSlot() const { return m_slot; }
    bool busy() const { return m_state != State::Idle; }

    void setRepeat(bool on);
    bool repeat() const { return m_repeat; }
    void setRepeatSeconds(int seconds);
    int repeatSeconds() const { return m_repeatSeconds; }

    // Spitzenpegel der laufenden Aufnahme seit der letzten Abfrage,
    // 0…1. Setzt sich beim Lesen zurueck.
    float takeRecordPeak();

public slots:
    // Vom MoxController: der Sender ist an- oder ausgegangen.
    void onKeyedChanged(bool on);

signals:
    void stateChanged(Longpath::VoiceKeyerPlayer::State state, int slot);
    void repeatChanged(bool on, int seconds);
    // Etwas, das der Betreiber wissen muss, ohne dass er gefragt hat
    // (Aufnahme voll, Speichern fehlgeschlagen).
    void notice(const QString& text);

private:
    void setState(State state, int slot);
    void poll();
    void finishPlayback();
    void releaseKey();
    void finishRecording();
    void loadSettings();
    void saveSettings() const;

    VoiceKeyerStore* m_store;
    WavTxSource m_source;
    Hooks m_hooks;

    State m_state{State::Idle};
    int m_slot{-1};
    bool m_keyedByUs{false};
    bool m_routed{false};
    // Waehrend der Spieler selbst tastet oder loslaesst, ist ein
    // Zustandswechsel des Senders seiner und kein fremder Eingriff.
    bool m_ownKeyChange{false};

    bool m_repeat{false};
    int m_repeatSeconds{5};

    QTimer m_pollTimer;
    QTimer m_tailTimer;
    QTimer m_gapTimer;

    TxAudioRecorder* m_recorder{nullptr};
    QPointer<TxWorkerThread> m_tapWorker;
    QMetaObject::Connection m_micTap;
    std::atomic<float> m_recordPeak{0.0f};
};

} // namespace Longpath
