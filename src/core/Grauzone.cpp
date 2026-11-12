// no-port-check: Longpath-original — see Grauzone.h.
//
// =================================================================
// src/core/Grauzone.cpp  (Longpath)
// =================================================================
//
// Longpath-original — siehe Grauzone.h.

#include "core/Grauzone.h"

#include "core/CtyDatParser.h"
#include "core/Maidenhead.h"
#include "core/SolarTimes.h"

#include <cmath>

namespace Longpath::Grauzone {

Licht licht(double sonnenhoeheGrad)
{
    if (std::isnan(sonnenhoeheGrad)) { return Licht::Unbekannt; }
    if (sonnenhoeheGrad > kGrenzeGrad)  { return Licht::Tag; }
    if (sonnenhoeheGrad < -kGrenzeGrad) { return Licht::Nacht; }
    return Licht::Grauzone;
}

double sonnenhoehe(const QDateTime& utc, double lat, double lonOst)
{
    // Wiederverwendet: solarInfo() aus core/SolarTimes.cpp. Keine zweite
    // Sonnenrechnung im Programm.
    return solarInfo(utc, lat, lonOst).elevationDeg;
}

namespace {

void ausFeld(Ende& ende, const QString& feld)
{
    double lat = 0.0, lon = 0.0;
    calculateLatLonFromGridSquare(feld, lat, lon);
    ende.herkunft = Herkunft::Locator;
    ende.lat = lat;
    ende.lon = lon;
    ende.woher = feld.toUpper();
}

void belichten(Ende& ende, const QDateTime& utc)
{
    if (!ende.bekannt() || !utc.isValid()) { return; }
    ende.hoeheGrad = sonnenhoehe(utc, ende.lat, ende.lon);
    ende.licht = licht(ende.hoeheGrad);
}

const char* wort(Licht l)
{
    switch (l) {
    case Licht::Tag:       return "day";
    case Licht::Nacht:     return "night";
    case Licht::Grauzone:  return "grey line";
    case Licht::Unbekannt: break;
    }
    return "";
}

QString endeHinweis(const char* wer, const Ende& e)
{
    if (!e.bekannt()) {
        return QStringLiteral("%1: position unknown").arg(QLatin1String(wer));
    }
    const QString quelle = e.herkunft == Herkunft::Locator
        ? QStringLiteral("locator %1").arg(e.woher)
        : QStringLiteral("country centre of %1").arg(e.woher);
    return QStringLiteral("%1: sun %2° (%3), from %4")
        .arg(QLatin1String(wer))
        .arg(e.hoeheGrad, 0, 'f', 1)
        .arg(QLatin1String(wort(e.licht)), quelle);
}

} // namespace

QsoLicht fuer(const LogEntry& e, const QString& meinFeldErsatz,
              const CtyDatParser* cty)
{
    QsoLicht l;

    // ── Mein Ende ────────────────────────────────────────────────────
    const QString mein = e.myGridSquare.trimmed();
    if (isValidGridSquare(mein)) {
        ausFeld(l.hier, mein);
    } else if (isValidGridSquare(meinFeldErsatz.trimmed())) {
        ausFeld(l.hier, meinFeldErsatz.trimmed());
    }

    // ── Das andere Ende ──────────────────────────────────────────────
    const QString dort = e.gridSquare.trimmed();
    if (isValidGridSquare(dort)) {
        ausFeld(l.dort, dort);
    } else if (cty && cty->isLoaded()) {
        const DxccEntity* d = cty->entityByPrefix(cty->resolvePrimaryPrefix(e.call));
        if (d && d->hasLatLon) {
            l.dort.herkunft = Herkunft::Landesmitte;
            l.dort.lat = d->latitude;
            // Schon OST-positiv (CtyDatParser dreht beim Lesen). KEIN
            // zweites Minus — siehe Grauzone.h.
            l.dort.lon = d->longitude;
            l.dort.woher = d->name;
        }
    }

    const QDateTime utc = e.timeOn.toUTC();
    belichten(l.hier, utc);
    belichten(l.dort, utc);
    return l;
}

QString kurztext(const QsoLicht& l)
{
    const Licht h = l.hier.licht;
    const Licht d = l.dort.licht;
    if (h == Licht::Unbekannt && d == Licht::Unbekannt) { return {}; }

    const bool gh = h == Licht::Grauzone;
    const bool gd = d == Licht::Grauzone;
    if (gh && gd) { return QStringLiteral("Grey line at both ends"); }
    if (gd)       { return QStringLiteral("Grey line at their end"); }
    if (gh)       { return QStringLiteral("Grey line at your end"); }

    if (h == Licht::Unbekannt) {
        return d == Licht::Tag ? QStringLiteral("Day at their end")
                               : QStringLiteral("Night at their end");
    }
    if (d == Licht::Unbekannt) {
        return h == Licht::Tag ? QStringLiteral("Day at your end")
                               : QStringLiteral("Night at your end");
    }
    if (h == Licht::Tag && d == Licht::Tag) {
        return QStringLiteral("Daylight at both ends");
    }
    if (h == Licht::Nacht && d == Licht::Nacht) {
        return QStringLiteral("Dark at both ends");
    }
    return h == Licht::Tag ? QStringLiteral("Day here, night there")
                           : QStringLiteral("Night here, day there");
}

QString hinweis(const QsoLicht& l)
{
    if (!l.hier.bekannt() && !l.dort.bekannt()) { return {}; }
    return endeHinweis("Your end", l.hier) + QLatin1Char('\n')
         + endeHinweis("Their end", l.dort) + QLatin1Char('\n')
         + QStringLiteral("Grey line: sun between −%1° and +%1°.")
               .arg(kGrenzeGrad, 0, 'f', 0);
}

} // namespace Longpath::Grauzone
