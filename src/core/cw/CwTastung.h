#pragma once

// =================================================================
// src/core/cw/CwTastung.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Von der Taste bis zum Geraet ─────────────────────────────────────
//
// Betreiber, 2026-11-17: „Halikey Midi sollte funktionieren, bei Aether
// funktioniert das bereits, informiere dich und baue!"
//
// Die Kette, wie AetherSDR sie baut und wie sie hier steht:
//
//     MIDI-Note ─► MidiKeyInput ─► IambicKeyer ─► Tastkante ─► Geraet
//                                       │
//                                       └────────► Mithoerton (lokal)
//
// Die Flex kennt keinen Paddle-Befehl: sie nimmt `cw key 1` und
// `cw key 0`, und die Jambus-Zeit macht der Klient (AetherSDR,
// RadioModel::sendCwPaddle: „FlexLib only ever sends cw key 1 or
// cw key 0"). Darum laeuft der Geber hier, nicht im Geraet.
//
// ── Zwei Schalter, und der zweite ist der wichtige ───────────────────
//
// `eingeschaltet`  -- die Taste wird gelesen, der Geber laeuft, der
//                     Mithoerton klingt. Damit kann man UEBEN: es geht
//                     nichts zum Geraet.
// `senderTasten`   -- die Tastkanten gehen hinaus. Ab Werk AUS.
//
// Ein Programm, das beim Anstecken einer Taste von selbst sendebereit
// ist, ist eines, das einmal im falschen Augenblick sendet.
//
// ── Die Sendefreigabe ist nicht unsere ────────────────────────────────
//
// Getastet wird nur, waehrend das Geraet SENDET -- und ob es senden
// darf, entscheidet MoxController mit seiner BandPlanGuard-Vorpruefung,
// nicht diese Klasse. Der erste Tastendruck fordert das Senden an
// (`CwTastziel::fordereSenden`); wird es verweigert, geht keine einzige
// Kante hinaus. Nach der letzten Kante laeuft ein Nachlauf, dann gibt
// die Tastung das Senden zurueck -- aber nur, wenn sie es selbst
// angefordert hat. Faellt das Senden von aussen (MOX aus, Sperre), geht
// die Taste sofort hoch.
//
// Dazu eine Wache gegen die haengende Taste: ein Note Off, das nie
// kommt (abgezogenes Kabel), haelt den Traeger sonst bis jemand es
// merkt. Nach `kHoechstensUntenMs` geht die Taste von selbst hoch.
// Sie sieht auf zwei Stellen: auf die Taste am Geraet (Handtaste) und
// auf den Kontakt am Eingang -- ein haengendes Paddle gibt endlos
// Punkte, und die Taste am Geraet ist dabei nie lange unten.
//
// Und nach einem Ende von aussen wird erst wieder getastet, wenn die
// Taste fuer die Dauer des Nachlaufs los war: wer MOX ausschaltet,
// waehrend die Hand noch gibt, will nicht einen Punkt spaeter wieder
// auf Sendung sein.
//
// ── Der Mithoerton kommt vom Faden, nicht von hier ────────────────────
//
// Den Ton schaltet die Kante dort, wo sie entsteht (Geber- oder
// MIDI-Faden), atomar und sofort. `kante()` laeuft spaeter auf dem
// Hauptfaden und fasst den Ton NICHT an: kaeme sie nach dem Ende eines
// kurzen Punkts an, schaltete sie ihn sonst noch einmal ein (AetherSDR
// #4976, derselbe Fehler dort).
//
// ── Der Fusstaster (PTT) ─────────────────────────────────────────────
//
// Betreiber, 2026-11-17, mitten im Bau: „Ich benötige HaliKey für den
// Fußtaster!!!" Das ist damit die Hauptsache, nicht die Zugabe.
//
// Der Fusstaster haelt das Senden, solange er gedrueckt ist -- in JEDER
// Betriebsart und an jedem Geraet, genau wie „Hold to talk" auf der
// Leertaste (MoxController::onSpacePtt: setMox(gedrueckt)). Er geht
// durch dieselbe Freigabe, und nur bei `senderTasten`. Er kommt auf zwei
// Wegen: als Note 31 (der PTT-Eingang des HaliKey) in jeder Art, und in
// der Art `Fusstaster` auch ueber die Tastenbuchse (Note 20 und 30) --
// je nachdem, wo der Stecker sitzt. In dieser Art laeuft kein Geber und
// klingt kein Ton.
//
// Auch er hat eine Wache (`kPttHoechstensMs`), aus demselben Grund wie
// die Taste, nur viel laenger: einen Durchgang haelt man absichtlich.
//
// `CwTastziel` ist die schmale Stelle zum Geraet -- damit die ganze
// Logik hier ohne Funkgeraet pruefbar ist. Die Flex-Fassung steht in
// src/models/FlexCwTastziel.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "core/midi/MidiKeyInput.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <atomic>
#include <memory>

namespace Longpath {

class CwMithoerton;
class IambicKeyer;

/// Was die Tastung vom Geraet braucht. Fuenf Fragen, mehr nicht.
class CwTastziel {
public:
    virtual ~CwTastziel() = default;
    /// Kann dieser Tastweg das Geraet jetzt erreichen (verbunden, in
    /// CW)? `warum` bekommt den Grund, wenn nicht.
    virtual bool bereit(QString* warum = nullptr) const = 0;
    /// Sendet das Geraet gerade so, dass eine Tastkante hinaus darf?
    virtual bool sendet() const = 0;
    /// Steht die Sendefreigabe (MOX) -- gleich in welcher Betriebsart
    /// und an welchem Geraet? Die Frage des Fusstasters.
    virtual bool freigabeSteht() const = 0;
    /// Senden anfordern oder zurueckgeben -- ueber die Sendefreigabe des
    /// Programms, nie an ihr vorbei.
    virtual void fordereSenden(bool an) = 0;
    /// Die Taste selbst.
    virtual void taste(bool unten) = 0;
};

class CwTastung : public QObject {
    Q_OBJECT
public:
    enum class Art { JambusA = 0, JambusB = 1, Handtaste = 2, Fusstaster = 3 };

    /// Die Wache gegen die haengende Taste. 15 s: laenger haelt niemand
    /// absichtlich einen Traeger mit der Morsetaste (zum Abstimmen gibt
    /// es TUNE), kuerzer wuerde einen langen Strich bei 5 WpM stutzen.
    static constexpr int kHoechstensUntenMs = 15000;
    /// Die Wache des Fusstasters. Zehn Minuten: laenger ist kein
    /// Durchgang, und ein Sender, der nach abgezogenem Kabel stehen
    /// bleibt, soll nicht bis zum naechsten Blick auf die Anzeige senden.
    static constexpr int kPttHoechstensMs = 10 * 60 * 1000;

    explicit CwTastung(QObject* eltern = nullptr);
    ~CwTastung() override;

    void setZiel(CwTastziel* ziel) { m_ziel = ziel; }
    /// Vor dem Abbau des Geraets: Taste hoch, Senden zurueck, Ziel weg.
    /// Der Destruktor fasst das Ziel nicht mehr an.
    void trenneZiel();

    // ── Einstellungen (AppSettings, „CwKey/…") ───────────────────────
    void lade();
    void sichere() const;

    bool eingeschaltet() const { return m_eingeschaltet; }
    void setEingeschaltet(bool an);
    bool senderTasten() const { return m_senderTasten; }
    void setSenderTasten(bool an);
    Art art() const { return m_art; }
    void setArt(Art a);
    int wpm() const { return m_wpm; }
    void setWpm(int wpm);
    bool vertauscht() const { return m_vertauscht; }
    void setVertauscht(bool an);
    int nachlaufMs() const { return m_nachlaufMs; }
    void setNachlaufMs(int ms);
    bool mithoerton() const { return m_mithoerton; }
    void setMithoerton(bool an);
    int tonhoeheHz() const;
    void setTonhoeheHz(int hz);
    int tonLautstaerke() const;
    void setTonLautstaerke(int prozent);
    QString portWunsch() const { return m_portWunsch; }
    void setPortWunsch(const QString& name);
    MidiKeyInput::Noten noten() const;
    void setNoten(const MidiKeyInput::Noten& n);

    /// Die MIDI-Eingaenge des Rechners neu lesen und den Port neu
    /// oeffnen (Taste erst nach dem Programmstart angesteckt).
    QStringList lesePortsNeu();

    /// Fuer Pruefstaende: weder MIDI-Port noch Tonausgabe oeffnen. Der
    /// Geber und die ganze Logik laufen trotzdem. Vor `setEingeschaltet`
    /// oder `lade()` setzen.
    void setOhneGeraete(bool an) { m_ohneGeraete = an; }
    /// Fuer Pruefstaende: die Wache frueher greifen lassen.
    void setWacheMsFuerPruefstand(int ms) { m_wacheMs = ms; }
    void setPttWacheMsFuerPruefstand(int ms) { m_pttWacheMs = ms; }
    bool pttGehalten() const { return m_pttGehalten; }
    /// Nach einem Ende von aussen: gesperrt, bis die Taste los war.
    bool gesperrt() const { return m_sperre; }

    MidiKeyInput* eingang() const { return m_eingang.get(); }
    CwMithoerton* ton() const { return m_ton.get(); }

    /// Ein Satz fuer die Einstellseite: was laeuft, und was fehlt.
    QString zustandsText() const;
    bool tasteUnten() const { return m_tasteUnten; }

    /// Das Senden ist von aussen gefallen (MOX aus, Sperre): Taste hoch,
    /// und es gehoert uns nicht mehr.
    void sendenBeendet();

    /// Eine Tastkante, als kaeme sie vom Geber. Der Weg, den der Geber
    /// nimmt -- und der, den der Pruefstand nimmt.
    void kante(bool unten);

    /// Der Fussschalter (Note 31). Wie `kante`: der Weg des Geraets und
    /// der des Pruefstands.
    void pttGeaendert(bool an);

    /// Die Grenze der Einstellwerte -- eine Stelle fuer Seite und Lader.
    static constexpr int kWpmMin = 5;
    static constexpr int kWpmMax = 60;
    static constexpr int kNachlaufMinMs = 100;
    static constexpr int kNachlaufMaxMs = 3000;

signals:
    void zustandGeaendert();
    void tasteGeaendert(bool unten);

private:
    void starteOderStoppe();
    void kanteVomFaden(bool unten);
    void nachlaufAbgelaufen();
    void wacheAbgelaufen();
    void melde(const QString& text);
    void wendeAn();
    bool fordereSendenAn();
    void lasseLos(bool sendenZurueck);
    void vonAussenBeendet();
    void eingangBewegt();
    void pttVomFaden(bool an);
    void pttWacheAbgelaufen();

    CwTastziel* m_ziel{nullptr};
    std::unique_ptr<MidiKeyInput> m_eingang;
    std::unique_ptr<IambicKeyer> m_geber;
    std::unique_ptr<CwMithoerton> m_ton;

    bool m_eingeschaltet{false};
    bool m_senderTasten{false};
    Art  m_art{Art::JambusB};
    int  m_wpm{20};
    bool m_vertauscht{false};
    int  m_nachlaufMs{600};
    bool m_mithoerton{true};
    QString m_portWunsch;
    bool m_ohneGeraete{false};

    // Was die Senken auf dem MIDI-Faden lesen -- darum atomar.
    std::atomic<bool> m_aktiv{false};         ///< = m_eingeschaltet
    std::atomic<int>  m_artFaden{int(Art::JambusB)}; ///< = m_art
    std::atomic<bool> m_tonAn{true};          ///< = m_mithoerton

    bool m_tasteUnten{false};        ///< `cw key 1` ist draussen
    bool m_selbstAngefordert{false}; ///< das Senden gehoert uns
    bool m_pttGehalten{false};       ///< der Fussschalter ist unten
    bool m_sperre{false};            ///< Ende von aussen: erst loslassen
    bool m_paddleGedrueckt{false};   ///< Stand am Eingang (Hauptfaden)
    bool m_handtasteGedrueckt{false};
    int  m_wacheMs{kHoechstensUntenMs};
    int  m_pttWacheMs{kPttHoechstensMs};
    std::atomic<bool> m_leitung{false}; ///< letzte Kante, die hinausging
    QString m_letzteMeldung;

    QTimer m_nachlauf;
    QTimer m_wache;          ///< die Taste am Geraet ist zu lange unten
    QTimer m_eingangsWache;  ///< ein Kontakt steht zu lange unveraendert
    QTimer m_entsperren;
    QTimer m_pttWache;       ///< der Fusstaster steht zu lange
};

} // namespace Longpath
