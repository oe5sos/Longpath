// no-port-check: Longpath-original, no Thetis logic (Thetis' logbook has
// none of these views).
//
// =================================================================
// src/core/LogbuchBilder.h  (Longpath)
// =================================================================
//
// ── Drei Bilder aus dem eigenen Log (2026-11-08) ────────────────────
//
// Betreiber waehlte aus neun Entwurfsblaettern (werkzeug/logbuch-
// 2026-11-08/blaetter): „Alle drei". Hier die Rechnung, ohne Oberflaeche:
//
//   bedarf()     -- Band x CW/PHONE/DATA fuer das Land eines Rufzeichens:
//                   gearbeitet, bestaetigt, dieses Rufzeichen; dazu das
//                   Urteil fuer das Band und die Betriebsart, auf denen
//                   der Betreiber gerade steht.
//   bandStunde() -- QSOs je Band und UTC-Stunde.
//   rose()       -- Peilung und Entfernung je QSO, Anteil je 10 Grad.
//
// Die Betriebsartgruppen sind die von WorkedBefore::modeGroup (DXCC-
// Regel: RTTY und FT8 sind dasselbe "DATA"). Das Land ist der cty.dat-
// Hauptpraefix, wie ueberall im Logbuch.

#pragma once

#include "models/LogEntry.h"

#include <QDateTime>
#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

#include <functional>

namespace Longpath {

class CtyDatParser;

namespace LogbuchBilder {

/// Die Baender der Bilder, in dieser Reihenfolge.
const QStringList& baender();
/// Die Betriebsartgruppen (WorkedBefore::modeGroup).
const QStringList& gruppen();
/// "20m|PHONE" -- der Schluessel einer Zelle.
QString zelle(const QString& band, const QString& gruppe);
/// Gruppe fuer eine Betriebsart, auch in der Schreibweise des Funkgeraets
/// (CWU/CWL sind CW, SAM/DSB Phone) -- sonst WorkedBefore::modeGroup.
QString gruppeFuer(const QString& mode);

// ── A  Bedarf ───────────────────────────────────────────────────────

struct Bedarf {
    QString call;
    QString land;                ///< cty.dat-Hauptpraefix; leer = unbekannt
    QString landName;
    QString kontinent;
    double  km{0.0};
    double  grad{0.0};
    bool    lageBekannt{false};
    int     qsosLand{0};
    int     qsosCall{0};
    QDateTime zuletzt;           ///< dieses Rufzeichen
    QString zuletztBand;
    QString zuletztMode;
    QSet<QString> gearbeitet;    ///< Zellen des Landes
    QSet<QString> bestaetigt;
    QSet<QString> diesesCall;
    QString jetzt;               ///< Zelle, auf der der Betreiber steht

    enum class Urteil { Unbekannt, NeuesLand, NeuesBand, NeueBetriebsart,
                        NeuerPlatz, Unbestaetigt, Bestaetigt };
    Urteil urteil{Urteil::Unbekannt};
    QString urteilText() const;
};

/// `landJeEintrag` (optional, gleich lang wie `log`) spart die cty-
/// Aufloesung je Tastendruck; leer heisst: selbst aufloesen.
Bedarf bedarf(const QVector<LogEntry>& log, const QString& call,
              const QString& band, const QString& mode,
              const CtyDatParser* cty, const QString& meinFeld,
              const QVector<QString>& landJeEintrag = {});

// ── B  Band x UTC-Stunde ────────────────────────────────────────────

struct BandStunde {
    QStringList zeilen;                       ///< Baender mit Daten
    QHash<QString, QVector<int>> anzahl;      ///< je Band 24 Werte
    int maxWert{0};
    int gesamt{0};
    int besteStunde(const QString& band) const;
};
BandStunde bandStunde(const QVector<LogEntry>& log,
                      const std::function<bool(const LogEntry&)>& nimm = {});

// ── C  Richtungsrose ────────────────────────────────────────────────

struct Rose {
    struct Punkt { double grad; double km; };
    QVector<Punkt> punkte;
    QVector<int>   sektor = QVector<int>(36, 0);   ///< je 10 Grad
    int ohneOrt{0};       ///< weder Locator noch Land
    int ausLand{0};       ///< ohne Locator, an der Landesmitte
    int besterSektor() const;
    /// Laengste Folge leerer Sektoren: Anfang (Sektor) und Laenge.
    void groessteLuecke(int* anfang, int* laenge) const;
};
/// Auf UKW (2 m und hoeher) zaehlt nur ein echter Locator -- die Mitte
/// eines Landes ist dort sinnlos (FM ueber Relais: alles "Austria").
Rose rose(const QVector<LogEntry>& log, const CtyDatParser* cty,
          const QString& meinFeld);

/// Liegt das Band bei 2 m oder hoeher?
bool istUkw(const QString& band);

} // namespace LogbuchBilder
} // namespace Longpath
