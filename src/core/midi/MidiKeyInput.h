#pragma once

// =================================================================
// src/core/midi/MidiKeyInput.h  (Longpath)
// =================================================================
//
// Longpath-original. Uses RtMidi (third_party/rtmidi, MIT-style).
//
// ── Eine Morsetaste, die als MIDI-Geraet kommt ───────────────────────
//
// Betreiber, 2026-11-17: „Halikey Midi sollte funktionieren, bei Aether
// funktioniert das bereits, informiere dich und baue!"
//
// Das HaliKey MIDI (Halibut Electronics) ist ein USB-MIDI-Geraet: an
// seiner Buchse haengt ein Paddle oder eine Handtaste, und jeder
// Kontakt kommt als Note On / Note Off. Laut Handbuch
// (halibut-electronics.github.io/HaliKey/User%20Guide-MIDI.pdf):
//
//     Note 20   linkes Paddle (Punkt) -- und die Handtaste an der Spitze
//     Note 21   rechtes Paddle (Strich)
//     Note 30   Handtaste
//     Note 31   PTT
//
// AetherSDR loest dasselbe ueber eine allgemeine MIDI-Zuordnung mit
// „Learn" (MidiControlManager, Bindungen vom Typ Gate). Das ist ein
// Werkzeug fuer beliebige Regler; hier geht es um EINE Sache -- die
// Taste --, darum kein Lernen und keine Bindungstabelle, sondern vier
// Notennummern mit den Vorgaben des Handbuchs, einstellbar.
//
// ── Zwei Wege hinaus, mit Absicht ────────────────────────────────────
//
// RtMidi ruft auf SEINEM Faden. Fuer die Taste zaehlt jede Millisekunde,
// und der Weg ueber die Ereignisschleife kostet einige davon mit
// Streuung. Darum gibt es die Senken (`setPaddleSenke` …): sie werden
// direkt auf dem MIDI-Faden gerufen und muessen fadenfest sein
// (IambicKeyer::setPaddleState ist es). Daneben dieselben Ereignisse als
// Qt-Signale -- fuer Anzeigen, die es nicht eilig haben.
//
// `deute()` ist eine reine Funktion ueber den Bytes einer Botschaft:
// am Schreibtisch pruefbar, ohne Geraet.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>
#include <memory>
#include <vector>

class RtMidiIn;

namespace Longpath {

class MidiKeyInput : public QObject {
    Q_OBJECT
public:
    /// Welche Note was bedeutet. Vorgaben: das HaliKey-Handbuch.
    struct Noten {
        int punkt{20};
        int strich{21};
        int handtaste{30};
        int ptt{31};
    };

    enum class Ereignis {
        Keins,
        PunktAn, PunktAus,
        StrichAn, StrichAus,
        TasteAn, TasteAus,
        PttAn, PttAus,
    };

    /// Eine MIDI-Botschaft deuten. Note On mit Anschlag 0 ist ein Note
    /// Off (so schicken es viele Geraete); der Kanal ist gleichgueltig --
    /// das Handbuch nennt keinen, und eine Taste auf dem „falschen" Kanal
    /// zu ueberhoeren waere der schlechtere Fehler.
    static Ereignis deute(const std::vector<unsigned char>& botschaft,
                          const Noten& noten);

    /// Den Port waehlen: der gewuenschte Name, wenn es ihn gibt; sonst
    /// der erste, der „HaliKey" im Namen traegt; sonst keiner (-1). Es
    /// wird NICHT einfach der erste genommen -- ein Klavier am selben
    /// Rechner ist keine Morsetaste.
    static int waehlePort(const QStringList& ports, const QString& wunsch);

    explicit MidiKeyInput(QObject* eltern = nullptr);
    ~MidiKeyInput() override;

    /// Die Namen der MIDI-Eingaenge, wie das System sie nennt.
    QStringList ports() const;

    /// Oeffnen. `wunsch` leer = selbst suchen (siehe waehlePort).
    bool oeffne(const QString& wunsch = QString());
    void schliesse();
    bool istOffen() const { return m_offen.load(); }
    QString portName() const { return m_portName; }
    QString letzterFehler() const { return m_fehler; }

    void setNoten(const Noten& n);
    Noten noten() const;

    // ── Die schnellen Senken (MIDI-Faden!) ───────────────────────────
    void setPaddleSenke(std::function<void(bool punkt, bool strich)> senke);
    void setTastenSenke(std::function<void(bool unten)> senke);

    /// Eine Botschaft einspeisen, als kaeme sie vom Geraet. Der Weg, den
    /// der RtMidi-Rueckruf nimmt -- und der, den der Pruefstand nimmt.
    void verarbeite(const std::vector<unsigned char>& botschaft);

signals:
    /// Der Stand beider Paddle-Kontakte (wahr = gedrueckt).
    void paddle(bool punkt, bool strich);
    void handtaste(bool unten);
    void ptt(bool an);
    void geoeffnet(const QString& portName);
    void geschlossen();

private:
    std::unique_ptr<RtMidiIn> m_eingang;
    std::atomic<bool> m_offen{false};
    QString m_portName;
    QString m_fehler;

    std::atomic<int> m_notePunkt{20};
    std::atomic<int> m_noteStrich{21};
    std::atomic<int> m_noteTaste{30};
    std::atomic<int> m_notePtt{31};

    std::atomic<bool> m_punkt{false};
    std::atomic<bool> m_strich{false};

    std::function<void(bool, bool)> m_paddleSenke;
    std::function<void(bool)> m_tastenSenke;
};

} // namespace Longpath
