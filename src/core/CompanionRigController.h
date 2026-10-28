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
#include <QString>
#include <memory>

#include "core/RigctldClient.h"

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

    // ── Was das Geraet sagt ─────────────────────────────────────────

    RigctldClient* client() const { return m_client.get(); }

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

    std::unique_ptr<RigctldProcess> m_proc;
    std::unique_ptr<RigctldClient>  m_client;

    bool    m_eingeschaltet{false};
    bool    m_daemonSelbstStarten{false};
    int     m_modell{0};
    QString m_anschluss;
    int     m_baud{0};
    QString m_host;
    quint16 m_port{4532};
    QString m_geraeteName;
};

} // namespace Longpath
