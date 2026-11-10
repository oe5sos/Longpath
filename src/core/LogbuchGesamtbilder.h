// no-port-check: Longpath-original, no Thetis logic (Thetis' logbook has
// none of these views).
//
// =================================================================
// src/core/LogbuchGesamtbilder.h  (Longpath)
// =================================================================
//
// ── Drei Gesamtbilder aus dem eigenen Log (2026-11-10) ──────────────
//
// Betreiber waehlte am 09.11. aus fuenf Entwurfsblaettern (werkzeug/
// logbuch-2026-11-09/blaetter), aus seinem echten Log gerechnet. Hier
// die Rechnung fuer drei davon, ohne Oberflaeche -- dieselbe Teilung
// wie core/LogbuchBilder (Bedarf, Band x Stunde, Rose):
//
//   sonnenzyklus() -- QSOs je Monat neben dem Solar-Flux F10.7 (NOAA
//                     SWPC, Monatsmittel), roh und ueber fuenf Monate
//                     geglaettet.
//   kontinentUhr() -- QSOs je Kontinent und UTC-Stunde; jeder Ring auf
//                     seine eigene beste Stunde bezogen.
//   dxccWand()     -- jedes DXCC-Gebiet aus cty.dat als Kachel: welche
//                     Baender, wie viele QSOs, bestaetigt oder nicht.
//
// Kontinent und Gebiet kommen aus cty.dat, wie ueberall im Logbuch.

#pragma once

#include "models/LogEntry.h"

#include <QDate>
#include <QHash>
#include <QMap>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

namespace Longpath {

class CtyDatParser;

namespace LogbuchBilder {

// ── D  Log gegen den Sonnenzyklus ───────────────────────────────────

/// "yyyy-MM" -> F10.7-Monatsmittel (sfu). Nur gueltige Werte (> 0).
using FluxJeMonat = QMap<QString, double>;

/// Liest observed-solar-cycle-indices.json von NOAA SWPC: eine Liste
/// von Objekten mit "time-tag" ("2026-10") und "f10.7". Fehlende Werte
/// stehen dort als -1 und fallen weg. Kaputtes JSON -> leer.
FluxJeMonat fluxAusSwpc(const QByteArray& json);

struct Sonnenzyklus {
    QStringList     monate;     ///< "yyyy-MM", lueckenlos vom ersten QSO bis heute
    QVector<int>    qsos;       ///< je Monat
    QVector<double> flux;       ///< je Monat; NaN = kein Wert
    QVector<double> glatt;      ///< fuenf Monate, mittig; NaN ausserhalb der Daten
    int gesamt{0};
    int maxQsos{0};
    int besterMonat{-1};        ///< Index in monate
    bool hatFlux() const;
    /// Juengster Monat mit einem Flux-Wert, sonst leer.
    QString fluxBis() const;
};

/// `heute` bestimmt den letzten Monat (Pruefstaende geben ihn fest vor).
Sonnenzyklus sonnenzyklus(const QVector<LogEntry>& log, const FluxJeMonat& flux,
                          const QDate& heute);

/// Gleitendes Mittel ueber 2*halb+1 Werte, randrichtig: am Rand und an
/// Luecken mittelt es ueber die Werte, die es gibt (NaN zaehlt nicht),
/// statt mit Nullen aufzufuellen. Wo der Rohwert selbst fehlt, bleibt
/// das Mittel NaN -- die Linie endet, wo die Daten enden.
QVector<double> gleitendesMittel(const QVector<double>& werte, int halb);

// ── E  Kontinent-Uhr ────────────────────────────────────────────────

struct KontinentUhr {
    /// Die Ringe von innen nach aussen: EU, NA, AS, SA, AF, OC.
    static const QStringList& kontinente();
    QHash<QString, QVector<int>> anzahl;   ///< je Kontinent 24 Werte (UTC)
    int gesamt{0};                          ///< mit Kontinent und Zeit
    int ohneZuordnung{0};                   ///< Rufzeichen nicht in cty.dat
    int summe(const QString& kontinent) const;
    int maxWert(const QString& kontinent) const;
    int besteStunde(const QString& kontinent) const;   ///< -1 = leer
    /// 0..1: diese Stunde gegen die beste Stunde DIESES Kontinents.
    double anteil(const QString& kontinent, int stunde) const;
};
KontinentUhr kontinentUhr(const QVector<LogEntry>& log, const CtyDatParser* cty);

// ── F  DXCC-Wand ────────────────────────────────────────────────────

/// Die zehn Punkte einer Kachel: 160 · 80 · 40 · 30 · 20 · 17 · 15 ·
/// 12 · 10 · 6 m.
const QStringList& wandBaender();

/// Die WAE-Sondergebiete (cty.dat-Praefix mit "*") zaehlen fuer DXCC
/// zu ihrem Gebiet: Sizilien ist Italien, Shetland ist Schottland,
/// 4U1VIC ist Oesterreich. Leer = kein solches Gebiet.
QString dxccFuerWae(const QString& waePraefix);

struct DxccKachel {
    QString praefix;             ///< cty.dat-Hauptpraefix
    QString name;
    QString kontinent;
    QSet<QString> baender;       ///< klein geschrieben, alle Baender
    int qsos{0};
    bool bestaetigt{false};
    bool gearbeitet() const { return qsos > 0; }
};

struct DxccWand {
    /// Die Bloecke der Wand wie im Entwurfsblatt: alte Welt, dann neue
    /// Welt und Ozeanien -- EU, AS, AF, NA, SA, OC.
    static const QStringList& kontinente();
    QVector<DxccKachel> kacheln;   ///< nach Kontinent (kontinente()), dann Praefix
    int gearbeitet{0};
    int bestaetigt{0};
    int qsos{0};                   ///< einem Gebiet zugeordnet
    int ohneZuordnung{0};
    int gesamt() const { return int(kacheln.size()); }
    /// Gebiete und gearbeitete Gebiete eines Kontinents.
    void zaehle(const QString& kontinent, int* alle, int* davonGearbeitet) const;
};
DxccWand dxccWand(const QVector<LogEntry>& log, const CtyDatParser* cty);

} // namespace LogbuchBilder
} // namespace Longpath
