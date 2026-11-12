#pragma once

// =================================================================
// src/core/CompanionRigController.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Die Verbindung gehoert nicht in das Fenster ─────────────────────
//
// Stufe 1 (2026-10-27) brachte den CAT-Client und die Einstellseite.
// Die Seite hatte ihren eigenen RigctldClient -- gut genug fuer den
// Knopf "Testen", und nur dafuer: eine `SetupPage` wird bei jedem
// Oeffnen des Fensters neu angelegt und beim Schliessen abgeraeumt.
// Eine Verbindung, die so lebt, ist beim ersten Blick auf eine andere
// Seite wieder weg.
//
// Genau derselbe Fehler steht bei RadioModel::rotor() als Kommentar:
// „ein Netzdienst, der an einem Fenster haengt, stirbt mit dem
// Fenster". Dort wurde der kleine, umkehrbare Schritt gewaehlt (eine
// Registrierung statt Besitz), weil die Aenderung ein Geraet am Mast
// betrifft. Hier gibt es diesen Grund nicht: das Begleitgeraet ist
// neu, es haengt an nichts, und der saubere Weg kostet nichts.
//
// Darum: diese Klasse gehoert RadioModel, lebt so lange wie das
// Programm, und die Einstellseite BEDIENT sie, statt eine zweite
// Verbindung aufzumachen. Zwei Clients auf demselben rigctld waere
// nicht nur doppelt, sondern falsch -- rigctld antwortet in der
// Reihenfolge der Befehle, und zwei Frager teilen sich die Antworten.
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Getastet wird nicht. RigctldClient kann es nicht (siehe dort), und
// diese Klasse fuegt es nicht hinzu. Sie liest, und sie stimmt ab --
// Abstimmen tastet nichts.
//
// Und: die Frequenz des fremden Geraets fuehrt NIE Longpaths eigene
// Scheibe. Das waere ein Kreis, sobald der Klick im Panadapter in die
// andere Richtung geht. Das Geraet treibt die MARKE, nichts weiter.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QString>
#include <memory>
#include <optional>

#include "core/RadioDiscovery.h"
#include "core/RigctldClient.h"
#include "core/Ts590Cat.h"

namespace Longpath {

class RigctldProcess;

class CompanionRigController : public QObject {
    Q_OBJECT
public:
    explicit CompanionRigController(QObject* parent = nullptr);
    ~CompanionRigController() override;

    // ── Einstellungen ───────────────────────────────────────────────

    /// Liest CompanionRig* und Transverter/* aus AppSettings und legt
    /// den Transverter-Versatz als prozessweite Fassung ab.
    ///
    /// Verbindet NICHT. Das ist eine eigene Entscheidung, siehe
    /// starteWennGewuenscht().
    void ladeEinstellungen();

    /// Nach dem Speichern auf der Einstellseite: neu lesen, den Versatz
    /// nachziehen, und eine stehende Verbindung an die neue Adresse
    /// umhaengen, falls sich dort etwas geaendert hat.
    void einstellungenGeaendert();

    // ── Verbindung ──────────────────────────────────────────────────

    /// Verbindet, wenn der Bediener das Begleitgeraet eingeschaltet hat
    /// UND dieser Lauf von selbst ins Netz darf.
    ///
    /// Der zweite Teil ist nicht Vorsicht, sondern eine Lehre: eine
    /// Sonde hat am 2026-09-17 mit dem Rufzeichen des Betreibers
    /// dessen eigene Cluster-Sitzung geworfen, weil nur die DATEIEN
    /// isoliert waren, das Netz nie. Seitdem fragt alles, was von
    /// selbst verbindet, AppSettings::darfVonSelbstVerbinden().
    ///
    /// Hier kommt dazu, dass ein rigctld, das wir selbst starten, einen
    /// seriellen Anschluss belegt -- in einem Pruefstandslauf womoeglich
    /// den, an dem gerade das echte Geraet des Betreibers haengt.
    void starteWennGewuenscht();

    /// Von Hand (Knopf "Verbinden" auf der Einstellseite). Startet
    /// rigctld mit, wenn so eingestellt.
    void verbinde();
    void trenne();

    // ── Betrieb ohne SDR ───────────────────────────────────

    /// Das eingestellte Geraet als Eintrag fuer das Verbinden-Fenster,
    /// oder nichts, wenn kein Geraet mit Anschluss eingestellt ist.
    static std::optional<RadioInfo> radioInfoAusEinstellungen();

    /// Das Geraet wird als eigenes Funkgeraet gefuehrt (ProtocolVersion::
    /// CatRig). Verbindet und startet rigctld, auch wenn das Folgen auf
    /// der Einstellseite aus ist. Beim Abschalten bleibt die Verbindung
    /// nur stehen, wenn das Folgen dort an ist.
    ///
    /// In dieser Rolle ist das Geraet die Anzeige: gefragt wird im
    /// kKatAbfrageMs-Takt statt im gemuetlichen Takt fuer die Marke, und
    /// am Zwischenspeicher von rigctld vorbei. Jede zweite Abfrage gilt
    /// der Frequenz, der VFO laeuft also mit rund 16 Schritten je Sekunde
    /// mit (Betreiber, 2026-11-07: "relativ zeitverzoegert").
    void setCatBetrieb(bool an);
    static constexpr int kKatAbfrageMs = 30;
    bool catBetrieb() const { return m_catBetrieb; }

    // ── Was das Geraet sagt ─────────────────────────────────────────

    RigctldClient* client() const { return m_client.get(); }
    /// Nur dann gelten die Zeilen aus Ts590Cat (Hamlib-Modell 2031).
    bool istTs590() const;

    bool    aktiv() const { return m_eingeschaltet; }
    bool    verbunden() const { return m_client && m_client->isConnected(); }

    /// Die ZF-Frequenz, wie das Geraet sie meldet -- also die Zahl, die
    /// auf der Achse des Panadapters liegt. 0, wenn nichts Frisches da
    /// ist.
    qint64  zfFrequenzHz() const;

    /// Dieselbe Frequenz, umgerechnet auf die Antenne. Fuer Anzeige und
    /// Logbuch.
    qint64  rfFrequenzHz() const;

    QString modus() const { return m_client ? m_client->mode() : QString{}; }

    /// Fuer die Statuszeile: "Kenwood TS-590S, 28.300.00 -> 144.300.00",
    /// oder der Grund, warum nichts da ist.
    QString beschreibung() const;

    // ── Abstimmen ───────────────────────────────────────────────────

    /// Stimmt das fremde Geraet auf eine ZF-Frequenz ab -- die Zahl aus
    /// dem Panadapter, nicht die von der Antenne.
    ///
    /// Tut nichts, wenn keine Verbindung steht oder der Bediener das
    /// Folgen nicht eingeschaltet hat. Beides stillschweigend: es ist
    /// ein Klick, kein Befehl, und ein Klick in einen Panadapter ohne
    /// Begleitgeraet soll keine Meldung ausloesen.
    void stimmeAbZf(qint64 ifHz);

    /// Darf ein Klick im Panadapter das fremde Geraet bewegen?
    bool darfAbstimmen() const;

    /// Was das Geraet zuletzt gemeldet hat. Leer, solange es kein TS-590S
    /// im Betrieb ohne SDR ist.
    const Ts590Cat::Zustand& catZustand() const { return m_catZustand; }

    /// Sendet das Geraet gerade? Es meldet das, Longpath loest es nie aus.
    bool sendet() const { return m_sendet; }

    /// Schickt eine Zeile aus core/Ts590Cat.h ans Geraet.
    void stelle(const QByteArray& zeile);

    /// Stellt die Betriebsart des fremden Geraets ("USB", "LSB", ...).
    /// Gleiche Bedingungen wie stimmeAbZf().
    void stelleModus(const QString& modus);

    /// Zeigt die eingestellte Adresse auf Longpaths EIGENEN
    /// rigctld-Server? Dann fragt Longpath sich selbst -- siehe den
    /// langen Kommentar in der Umsetzung.
    ///
    /// Steht hier und nicht unter private, weil der Pruefstand die
    /// Entscheidung einzeln pruefen soll: ueber verbinde() liesse sie
    /// sich nur daran ablesen, dass nichts passiert, und „nichts
    /// passiert" hat zu viele moegliche Gruende.
    bool zieltAufUnsSelbst() const;

signals:
    /// Das Geraet meldet eine andere ZF-Frequenz. Die Marke haengt hier.
    void zfFrequenzGeaendert(qint64 ifHz);

    /// catZustand() hat sich geaendert.
    void catZustandGeaendert();

    /// Das Geraet sendet, oder es hat aufgehoert.
    void sendetGeaendert(bool sendet);

    /// Das Geraet meldet eine andere Betriebsart.
    void modusGeaendert(const QString& modus);

    /// Verbindung auf oder zu. Wer die Marke malt, blendet sie hier aus.
    void verbindungGeaendert(bool verbunden);

    /// Der Versatz hat sich geaendert (Einstellseite). Wer Achsen oder
    /// Zahlen zeichnet, muss neu zeichnen.
    void versatzGeaendert();

    /// Etwas ging schief, mit Grund. Fuer die Statuszeile, nicht fuer
    /// einen Dialog.
    void problem(const QString& text);

private:
    void baueClient();
    void starteDaemonWennGewuenscht();
    void rohAntwortLesen(const QByteArray& antwort);
    void pruefeTransverterWege();
    void wacheSchlaegt();

public:
    /// Wie oft die Wache schaut, und nach wie vielen stillen Schlaegen sie
    /// rigctld neu startet. Fuer den Pruefstand oeffentlich.
    static constexpr int kWacheMs          = 2000;
    static constexpr int kStilleSchlaege   = 3;
    /// Nach jedem Start: so lange zaehlt Stille nicht. rigctld oeffnet den
    /// Anschluss, der Client wird beim ersten Versuch oft abgewiesen.
    static constexpr int kAnlaufMs         = 8000;
    void setWacheMsForTest(int ms) { m_wache.setInterval(ms); }
    bool ausfallForTest() const { return m_ausfall; }
    qint64 daemonPidForTest() const;

private:
    // Geht der 590 aus oder faellt der USB-Stecker, bleibt rigctld mit
    // einem toten Anschluss stehen (oder beendet sich), und der Client
    // verbindet sich endlos mit etwas, das nie antwortet. Der Daemon ist
    // unserer: die Wache startet ihn neu, bis wieder eine Frequenz kommt.
    QTimer m_wache;
    int    m_stilleSchlaege{0};
    bool   m_ausfall{false};
    QElapsedTimer m_seitStart;

    std::unique_ptr<RigctldProcess> m_proc;
    std::unique_ptr<RigctldClient>  m_client;

    bool    m_eingeschaltet{false};
    bool    m_catBetrieb{false};
    bool    m_sendet{false};
    bool    m_wegeWarnung{false};
    Ts590Cat::Zustand m_catZustand;
    bool    m_daemonSelbstStarten{false};
    int     m_modell{0};
    QString m_anschluss;
    int     m_baud{0};
    QString m_host;
    quint16 m_port{4532};
    QString m_geraeteName;
};

} // namespace Longpath
