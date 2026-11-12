// no-port-check: Longpath-original, no Thetis logic (Thetis' logbook knows
// nothing about the sun).
//
// =================================================================
// src/core/Grauzone.h  (Longpath)
// =================================================================
//
// ── Licht an beiden Enden eines QSOs (2026-11-10) ───────────────────
//
// Betreiber: das Logbuch soll ein QSO ERKLAEREN, nicht nur ablegen —
// „Grauzone und Ausbreitung je QSO, wenn es billig ist". Hier die
// Rechnung ohne Oberflaeche: wo stand die Sonne an meinem und am
// anderen Ende, als das QSO lief, und war eines der Enden in der
// Grauzone?
//
// Die Sonnenhoehe rechnet NICHT neu: sie kommt aus solarInfo()
// (core/SolarTimes.cpp, Sonnenaufgangsgleichung, gegen NOAA auf etwa
// zwei Minuten genau). Die Schattengrenze auf dem Globus kommt seit
// 2026-11-10 aus demselben Modell (subsolarPoint() in SolarTimes);
// vorher rechnete GlobeWidget eine eigene Naeherung, deren Zeitgleichung
// bis 17 Minuten danebenlag — bis 4,3° Sonnenhoehe am Horizont, so viel
// wie die halbe Grauzone. Globus und Logbuch sagen jetzt dasselbe.
//
// ── Woher die beiden Orte kommen ────────────────────────────────────
//
//   mein Ende:   MY_GRIDSQUARE des QSOs, sonst der Ersatz, den der
//                Aufrufer mitgibt (im Logbuch: das juengste
//                MY_GRIDSQUARE im Log, siehe LogbookWindow::meinFeld()).
//   anderes Ende: GRIDSQUARE, sonst die Landesmitte aus cty.dat.
//
// Laengen sind OST-positiv. cty.dat schreibt West-positiv, aber
// CtyDatParser dreht das beim Lesen schon um (CtyDatParser.cpp, „flip
// on read") — hier darf es KEIN zweites Minus geben. Genau das hatte
// LogbuchBilder::feldDerLandesmitte (im Zweig feat/logbuch-
// statistikbilder behoben): USA-Mitte landete in Asien. tst_grauzone
// prueft die Richtung deshalb an einem QSO in die USA, bei dem Tag und
// Nacht davon abhaengen.
//
// Es ist ein Urteil aus dem Log, nicht aus QRZ: die Detailkarte, der
// Filter „Grey line" und die Zahl in der Statistik muessen dasselbe
// sagen, und die beiden letzten haben keine Abfrage zur Hand.

#pragma once

#include "models/LogEntry.h"

#include <QDateTime>
#include <QString>

namespace Longpath {

class CtyDatParser;

namespace Grauzone {

// ── Die Grenze ───────────────────────────────────────────────────────
//
// Sonne zwischen −6° und +6° = Grauzone. −6° ist das Ende der
// buergerlichen Daemmerung; +6° dieselbe Breite auf der Tagseite, weil
// die D-Schicht sich nicht am Horizont ab- und aufbaut, sondern ueber
// diese Spanne — auf dem Hinweg wie auf dem Rueckweg. Beide Grenzen
// gehoeren zur Grauzone (|h| <= 6). Dieselbe Grenze wie
// SolarInfo::greyline in core/SolarTimes.cpp, damit der Globus und das
// Logbuch nicht zwei verschiedene Grauzonen zeigen.
inline constexpr double kGrenzeGrad = 6.0;

enum class Licht { Unbekannt, Tag, Grauzone, Nacht };

// Tag oberhalb +6°, Nacht unterhalb −6°, dazwischen Grauzone.
Licht licht(double sonnenhoeheGrad);

// Sonnenhoehe in Grad ueber dem (geometrischen) Horizont, aus
// solarInfo(). Laenge OST-positiv.
double sonnenhoehe(const QDateTime& utc, double lat, double lonOst);

enum class Herkunft { Keine, Locator, Landesmitte };

struct Ende {
    Herkunft herkunft{Herkunft::Keine};
    double   lat{0.0};
    double   lon{0.0};          // Grad, Ost positiv
    QString  woher;             // "JN67" oder "Germany" — fuer den Hinweis
    double   hoeheGrad{0.0};
    Licht    licht{Licht::Unbekannt};

    bool bekannt() const { return herkunft != Herkunft::Keine; }
};

struct QsoLicht {
    Ende hier;
    Ende dort;

    // Ein Grauzonen-QSO: mindestens ein Ende in der Grauzone.
    bool grauzonenQso() const
    {
        return hier.licht == Licht::Grauzone || dort.licht == Licht::Grauzone;
    }
};

// Beide Enden eines QSOs. `meinFeldErsatz` gilt, wenn das QSO kein
// gueltiges MY_GRIDSQUARE hat; `cty` darf null sein (dann gibt es fuer
// das andere Ende nur den Locator). Ohne gueltige Zeit bleibt alles
// Unbekannt.
QsoLicht fuer(const LogEntry& e, const QString& meinFeldErsatz,
              const CtyDatParser* cty);

// Der kurze Satz fuer die Detailkarte: „Grey line at their end",
// „Day here, night there" … Leer, wenn kein Ende bekannt ist.
QString kurztext(const QsoLicht& l);

// Der Hinweistext dazu: beide Sonnenhoehen und woher die Orte kommen.
QString hinweis(const QsoLicht& l);

} // namespace Grauzone
} // namespace Longpath
