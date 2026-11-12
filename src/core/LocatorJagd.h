// no-port-check: Longpath-original, no Thetis logic (Thetis has neither a
// native FT8 decoder nor a QSO map).
//
// =================================================================
// src/core/LocatorJagd.h  (Longpath)
// =================================================================
//
// ── Locator-Jagd mit FT8 (2026-11-11) ───────────────────────────────
//
// Betreiber, nachts zum 11.11.: „besonders bei FT8 sehen, welche
// Locator ich noch nicht gearbeitet habe und welche gerade erreichbar
// sind, damit ich auf der Karte draufklicken kann und es probiere."
// (LOGBUCHLISTE.md, Abschnitt 9.)
//
// Alles dafuer war schon da -- der eigene Dekoder mit Ablauf, die
// Karten, die Grid-Ueberlagerung, das Logbuch. Was fehlte, war die
// Verbindung. Hier stehen die drei Rechnungen dazwischen, ohne Fenster
// und ohne Funkgeraet, damit sie im Pruefstand fahrbar sind:
//
//   * LocatorMosaik -- jedes vierstellige Feld je Band aus dem Logbuch:
//     bestaetigt, gearbeitet oder noch nie.
//   * LiveSchicht   -- wer in den letzten zwei Minuten MIT Locator
//     gehoert wurde, und was er fuer mich waere (neues Land, neues
//     Feld auf diesem Band, schon gearbeitet).
//   * die Texte    -- Zaehler und Hinweis, an EINER Stelle, damit
//     flache Karte und Azimutalkarte dasselbe sagen.
//
// Was hier NICHT steht: der Anruf. Ein Klick auf einen Punkt geht
// denselben Weg wie ein Doppelklick in der Fundliste
// (Ft8Applet::nimmFundAn) -- es gibt genau einen Ablauf, keinen zweiten.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "models/LogEntry.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

namespace Longpath {

/// Wie weit ein Feld im Logbuch ist. Die Reihenfolge ist die Aussage:
/// ein bestaetigtes QSO schlaegt jedes nur gearbeitete im selben Feld.
enum class FeldStand { Nie = 0, Gearbeitet = 1, Bestaetigt = 2 };

/// Jedes vierstellige Maidenhead-Feld aus dem Logbuch, je Band.
class LocatorMosaik {
public:
    /// Neu aufbauen. `nurFt8` laesst nur QSOs mit MODE=FT8 zu -- die
    /// Frage „welches Feld fehlt mir IN FT8".
    void baue(const QVector<LogEntry>& log, bool nurFt8);

    /// Der Stand eines Feldes auf einem Band. `band` leer heisst: ueber
    /// alle Baender. Ein unbekanntes Feld ist `Nie`, nicht „unbekannt"
    /// -- der Locator der Gegenstation ist ja bekannt, nur das Logbuch
    /// hat ihn nicht.
    FeldStand stand(const QString& feld, const QString& band) const;

    /// Alle gearbeiteten Felder eines Bandes mit ihrem Stand (leer =
    /// alle Baender). `Nie` steht nicht darin -- das ist der Rest der Welt.
    QHash<QString, FeldStand> felder(const QString& band) const;

    /// Stand das Rufzeichen auf diesem Band schon im Logbuch?
    bool gearbeitet(const QString& rufzeichen, const QString& band) const;

    /// Welche Baender kommen vor, nach Frequenz sortiert.
    QStringList baender() const;

    bool nurFt8() const { return m_nurFt8; }

    /// Das Grossfeld aus einem Locator, vier Stellen und gross, oder
    /// leer. Dieselbe Regel wie GridsGearbeitet::grossfeld (RR73 ist
    /// keines), dazu die Pruefung der Buchstaben A..R.
    static QString feld(const QString& locator);
    /// Der Schluessel eines Bandes: „20m", klein, ohne Rand.
    static QString bandSchluessel(const QString& band);

private:
    QHash<QString, QHash<QString, FeldStand>> m_jeBand;
    QHash<QString, FeldStand> m_alle;
    QHash<QString, QSet<QString>> m_rufeJeBand;
    bool m_nurFt8{false};
};

/// Eine gehoerte FT8/FT4-Station, so wie das FT8-Fenster sie meldet.
struct Ft8Gehoert {
    QString   rufzeichen;
    QString   locator;        ///< aus der Nachricht, sonst leer
    int       snrDb{0};
    double    dtSek{0.0};
    int       tonHz{0};
    double    freqMHz{0.0};
    QString   band;           ///< „20m", wie es ins Logbuch ginge
    QString   modus;          ///< „FT8" / „FT4"
    QDateTime zeit;           ///< Beginn der Periode, UTC
    QString   nachricht;      ///< der reine FT8-Satz
    /// Die Zeile, wie sie in der Fundliste steht. Ein Klick auf der
    /// Karte beantwortet GENAU diese Zeile -- wie ein Doppelklick dort.
    QString   zeile;
    QString   land;
    bool      neuesLand{false};   ///< DXCC noch nie im Logbuch
};

/// Was eine gehoerte Station fuer mich waere. Die Reihenfolge ist die
/// Rangfolge: ein neues Land zaehlt mehr als ein neues Feld, und beides
/// mehr als eine Station, die schon im Logbuch steht.
enum class LiveArt { NeuesLand, NeuesFeld, Gearbeitet, Gewoehnlich };

/// Ein Punkt auf der Karte: die Station, wo sie liegt, was sie ist.
struct LivePunkt {
    Ft8Gehoert g;
    QString    locator;       ///< zuletzt gehoerter Locator (auch wenn
                              ///< der letzte Satz keinen trug)
    QString    feld;          ///< vierstellig
    double     lat{0.0};
    double     lon{0.0};      ///< positiv Ost
    LiveArt    art{LiveArt::Gewoehnlich};
    bool       feldNeu{false};  ///< das Feld fehlt auf diesem Band --
                                ///< auch wenn `art` ein neues Land meldet
    double     km{-1.0};      ///< < 0: kein eigener Locator
    double     azimutGrad{-1.0};
    int        alterSek{0};
};

/// Wer in den letzten Minuten mit Locator gehoert wurde.
class LiveSchicht {
public:
    /// Zwei Minuten: vier FT8-Perioden je Seite. Wer laenger still ist,
    /// ruft gerade woanders -- oder ist weg.
    static constexpr int kVorgabeSekunden = 120;
    static constexpr int kMinSekunden = 15;
    static constexpr int kMaxSekunden = 3600;

    void setFensterSekunden(int sekunden);
    int  fensterSekunden() const { return m_fenster; }

    /// Einen Fund aufnehmen. Ohne Locator zaehlt er nur fuer ein
    /// Rufzeichen, das schon MIT Locator gehoert wurde: dann wird es
    /// aufgefrischt (Zeit, Rapport, Zeile), sein Feld bleibt.
    /// Ein Satz ohne Locator von einer unbekannten Station laesst sich
    /// nicht verorten und bleibt draussen.
    void nimm(const Ft8Gehoert& g);

    /// Alles vergessen, was aelter als das Fenster ist.
    void altere(const QDateTime& jetzt);

    /// Die Punkte, die jetzt zu zeigen sind -- eingeordnet gegen das
    /// Mosaik (Feld und Rufzeichen je auf dem Band, auf dem die Station
    /// gehoert wurde) und mit Entfernung/Richtung vom eigenen Locator.
    QVector<LivePunkt> punkte(const QDateTime& jetzt,
                              const LocatorMosaik& mosaik,
                              const QString& heimLocator) const;

    /// Der letzte Fund eines Rufzeichens, oder nullptr.
    const Ft8Gehoert* finde(const QString& rufzeichen) const;

    int  anzahl() const { return m_jeRuf.size(); }
    void leeren() { m_jeRuf.clear(); }

    /// Die Rangfolge, als reine Regel.
    static LiveArt art(bool neuesLand, bool neuesFeld, bool gearbeitet);

    /// Wie viele VERSCHIEDENE neue Felder unter den Punkten auf `band`
    /// liegen. Zwei Stationen im selben neuen Feld sind ein Feld.
    static int neueFelder(const QVector<LivePunkt>& punkte, const QString& band);

    /// „3 new squares heard now on 20m".
    static QString zaehlerText(int neue, const QString& band);

    /// Der Hinweis unter dem Mauszeiger: Rufzeichen, Rapport, Locator,
    /// Entfernung, Richtung, Alter -- und was die Station fuer mich ist.
    static QString hinweis(const LivePunkt& p, const QDateTime& jetzt);

    /// Die Art in Worten, fuer Hinweis und Legende.
    static QString artText(LiveArt a, const QString& band);

private:
    struct Eintrag {
        Ft8Gehoert letzter;
        QString    locator;
    };
    QHash<QString, Eintrag> m_jeRuf;
    int m_fenster{kVorgabeSekunden};
};

} // namespace Longpath
