// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/core/SotaGipfel.h  (Longpath)
// =================================================================
//
// ── Der eigene Standort vom Gipfel (2026-11-11) ─────────────────────
//
// Martin aktiviert Gipfel. Sein Log traegt in MY_GRIDSQUARE trotzdem
// JN67VV -- ein Tal auf 423 m --, auch fuer die QSOs, die er am
// 21.06. und 04.07.2026 auf OE/OO-106 Gmundnerberg (884 m) gefahren hat.
// Fuer Peilung und Entfernung macht das sieben Kilometer, fuer ein
// 2-m-Hoehenprofil den Unterschied zwischen "kein Sichtweg" und
// "Sichtweg" (Entwurfsblatt 5, werkzeug/logbuch-2026-11-09).
//
// Steht im QSO MY_SOTA_REF (bzw. SOTA_REF fuer die Gegenseite eines
// Gipfel-zu-Gipfel-QSOs), gelten Lage und Hoehe des Gipfels aus der
// SOTA-Datenbank:
//
//   GET https://api2.sota.org.uk/api/summits/<assoc>/<region-nnn>
//   -> {"summitCode":"OE/OO-106","name":"Gmundnerberg",
//       "latitude":47.9058,"longitude":13.7353,"altM":884, ...}
//   unbekannter Gipfel -> HTTP 400 "Not found"
//
// (am 2026-11-11 so abgefragt). Jeder Gipfel wird hoechstens EINMAL
// gefragt: die Antwort -- auch "gibt es nicht" -- liegt danach in
// <dataDir>/sota-summits.json, und ein Netzfehler wird in diesem Lauf
// nicht wiederholt. Die Abfragen gehen nacheinander, eine je Sekunde.
// User-Agent wie beim SotaClient: Programm und Rufzeichen, so wie es der
// Betreuer der SOTA-API verlangt (siehe SotaClient.h).
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include "core/HttpAbruf.h"
#include "models/LogEntry.h"

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUrl>
#include <QVector>

namespace Longpath {

struct SotaGipfel {
    QString ref;         ///< "OE/OO-106"
    QString name;        ///< "Gmundnerberg"
    double  lat{0.0};
    double  lon{0.0};
    int     altM{0};
    /// 6-stellig, gross: "JN67UV". Aus der API ("locator"), sonst aus
    /// der Lage gerechnet.
    QString locator;

    bool gueltig() const { return !ref.isEmpty(); }
};

class SotaGipfelVerzeichnis : public QObject {
    Q_OBJECT
public:
    explicit SotaGipfelVerzeichnis(const QString& cachePfad,
                                   QObject* parent = nullptr);

    /// Das eine Verzeichnis des Programms, Cache im Datenordner.
    static SotaGipfelVerzeichnis& instanz();

    /// "oe/oo-106 " -> "OE/OO-106"; leer, wenn es keine Gipfelreferenz ist.
    static QString normalisiere(const QString& ref);
    static QUrl anfrageUrl(const QString& ref);
    /// Eine Antwort der SOTA-API lesen. false, wenn sie keinen Gipfel
    /// mit Lage beschreibt.
    static bool leseAntwort(const QByteArray& json, SotaGipfel& aus);

    /// Fuer Pruefstaende: ein Transport, der nicht ins Netz geht.
    void setAbruf(HttpAbruf abruf);
    /// Pause zwischen zwei Abfragen (Vorgabe 1000 ms).
    void setTakt(int ms);

    /// Nur aus dem Cache -- nie ein Netzzugriff.
    bool finde(const QString& ref, SotaGipfel& aus) const;
    /// Die SOTA-API hat gesagt: diesen Gipfel gibt es nicht.
    bool istUnbekannt(const QString& ref) const;

    /// Einen Gipfel nachschlagen, wenn er weder im Cache steht noch in
    /// diesem Lauf schon gefragt wurde. Meldet gipfelBekannt().
    void anfragen(const QString& ref);
    /// Alle Gipfel eines Logs (MY_SOTA_REF und SOTA_REF), die fehlen.
    void anfragenFuer(const QVector<LogEntry>& log);
    /// Steht die Frage nach diesem Gipfel noch aus (Warteschlange oder
    /// unterwegs)? Dann kommt noch gipfelBekannt() oder gipfelFehler().
    bool wartet(const QString& ref) const;
    /// Wie viele Abfragen wirklich hinausgingen (fuer Pruefstaende).
    int abfragenGestellt() const { return m_gestellt; }

    // ── Bei SOTA gilt der Locator des Gipfels (Betreiber 2026-11-10) ──
    //
    // „bei sota muss immer der locator der sotareferenz angenommen
    // werden": Martins Logger schreibt auch auf dem Gipfel JN67VV. Ein QSO
    // mit MY_SOTA_REF bekommt den Locator des Gipfels als MY_GRIDSQUARE,
    // eines mit SOTA_REF den des anderen Gipfels als GRIDSQUARE.
    // Geschrieben wird das nur dort, wo der Betreiber bestaetigt: im
    // Nachtrag-Dialog ("Apply") und im Bearbeiten-Dialog ("Save"), wo das
    // Feld sichtbar ausgefuellt wird.
    /// Locator eines bekannten Gipfels, sonst leer.
    QString gipfelLocator(const QString& ref) const;
    /// Die Locatoren eines QSOs auf seine Gipfel setzen, wo diese bekannt
    /// sind. Liefert die Zahl der geaenderten Felder (0..2).
    int wendeGipfelLocatorAn(LogEntry& e) const;

    // ── Die beiden Enden eines QSOs ──────────────────────────────────
    bool eigenerGipfel(const LogEntry& e, SotaGipfel& aus) const;
    bool gegenGipfel(const LogEntry& e, SotaGipfel& aus) const;

    struct Ende {
        bool    gueltig{false};
        bool    gipfel{false};   ///< aus der SOTA-Datenbank
        double  lat{0.0};
        double  lon{0.0};
        QString locator;         ///< wenn aus dem Locator
        SotaGipfel g;
    };
    /// Eigener Standort: Gipfel (MY_SOTA_REF), sonst MY_GRIDSQUARE,
    /// sonst `ersatzLocator`.
    Ende eigenesEnde(const LogEntry& e, const QString& ersatzLocator = QString()) const;
    /// Gegenstation: Gipfel (SOTA_REF), sonst GRIDSQUARE.
    Ende gegenEnde(const LogEntry& e) const;

    /// distanceKm/bearingDeg neu rechnen, wo ein Ende ein bekannter
    /// Gipfel ist. Liefert die Zahl der geaenderten Eintraege. Schreibt
    /// nichts in die Datei -- beide Werte sind abgeleitet (LogEntry.h).
    int korrigiereStrecken(QVector<LogEntry>& log) const;

signals:
    void gipfelBekannt(const QString& ref);
    void gipfelFehler(const QString& ref, const QString& text);

private:
    void laden();
    void speichern() const;
    void naechste();
    void beantwortet(const QString& ref, const HttpAntwort& a);

    QString m_pfad;
    HttpAbruf m_abruf;
    QHash<QString, SotaGipfel> m_gipfel;
    QSet<QString> m_unbekannt;
    QSet<QString> m_gefragt;       // in diesem Lauf, auch fehlgeschlagene
    QStringList   m_warteschlange;
    QString       m_unterwegs;
    bool   m_laeuft{false};
    QTimer m_takt;
    int    m_taktMs{1000};
    int    m_gestellt{0};
};

} // namespace Longpath
