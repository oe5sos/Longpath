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

#include <QObject>
#include <QString>
#include <QTimer>

#include <memory>

namespace Longpath {

class CwMithoerton;
class IambicKeyer;
class MidiKeyInput;

/// Was die Tastung vom Geraet braucht. Vier Fragen, mehr nicht.
class CwTastziel {
public:
    virtual ~CwTastziel() = default;
    /// Kann dieser Tastweg das Geraet jetzt erreichen (verbunden, in
    /// CW)? `warum` bekommt den Grund, wenn nicht.
    virtual bool bereit(QString* warum = nullptr) const = 0;
    /// Sendet das Geraet gerade (MOX steht)?
    virtual bool sendet() const = 0;
    /// Senden anfordern oder zurueckgeben -- ueber die Sendefreigabe des
    /// Programms, nie an ihr vorbei.
    virtual void fordereSenden(bool an) = 0;
    /// Die Taste selbst.
    virtual void taste(bool unten) = 0;
};

class CwTastung : public QObject {
    Q_OBJECT
public:
    enum class Art { JambusA = 0, JambusB = 1, Handtaste = 2 };

    /// Die Wache gegen die haengende Taste. 15 s: laenger haelt niemand
    /// absichtlich einen Traeger mit der Morsetaste (zum Abstimmen gibt
    /// es TUNE), kuerzer wuerde einen langen Strich bei 5 WpM stutzen.
    static constexpr int kHoechstensUntenMs = 15000;

    explicit CwTastung(QObject* eltern = nullptr);
    ~CwTastung() override;

    void setZiel(CwTastziel* ziel) { m_ziel = ziel; }

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

signals:
    void zustandGeaendert();
    void tasteGeaendert(bool unten);

private:
    void starteOderStoppe();
    void kanteVomFaden(bool unten);
    void nachlaufAbgelaufen();
    void wacheAbgelaufen();
    void melde(const QString& text);

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

    bool m_tasteUnten{false};        ///< `cw key 1` ist draussen
    bool m_selbstAngefordert{false}; ///< das Senden gehoert uns
    QString m_letzteMeldung;

    QTimer m_nachlauf;
    QTimer m_wache;
};

} // namespace Longpath
