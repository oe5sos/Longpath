// no-port-check: Longpath-original, see LogbuchBilder.h.
//
// src/core/LogbuchBilder.cpp  (Longpath)

#include "core/LogbuchBilder.h"

#include "core/CtyDatParser.h"
#include "core/Maidenhead.h"
#include "core/QsoConfirmation.h"
#include "core/WorkedBefore.h"

#include <algorithm>
#include <cmath>

namespace Longpath::LogbuchBilder {

namespace {
double norm360(double a) { double r = std::fmod(a, 360.0); return r < 0 ? r + 360.0 : r; }

QString feldDerLandesmitte(const CtyDatParser* cty, const QString& land)
{
    if (!cty || land.isEmpty()) { return {}; }
    const DxccEntity* d = cty->entityByPrefix(land);
    if (!d) { return {}; }
    // cty.dat schreibt Laengen positiv nach WESTEN.
    return gridSquareFromLatLon(d->latitude, -d->longitude);
}
} // namespace

const QStringList& baender()
{
    static const QStringList b = {QStringLiteral("160m"), QStringLiteral("80m"),
        QStringLiteral("60m"), QStringLiteral("40m"), QStringLiteral("30m"),
        QStringLiteral("20m"), QStringLiteral("17m"), QStringLiteral("15m"),
        QStringLiteral("12m"), QStringLiteral("10m"), QStringLiteral("6m"),
        QStringLiteral("2m")};
    return b;
}

const QStringList& gruppen()
{
    static const QStringList g = {QStringLiteral("CW"), QStringLiteral("PHONE"),
                                  QStringLiteral("DATA")};
    return g;
}

QString zelle(const QString& band, const QString& gruppe)
{
    return band.toLower() + QLatin1Char('|') + gruppe;
}

QString gruppeFuer(const QString& mode)
{
    const QString m = mode.trimmed().toUpper();
    if (m.startsWith(QLatin1String("CW"))) { return QStringLiteral("CW"); }
    if (m == QLatin1String("SAM") || m == QLatin1String("DSB")
        || m == QLatin1String("NFM") || m == QLatin1String("WFM")) {
        return QStringLiteral("PHONE");
    }
    return WorkedBefore::modeGroup(m);
}

bool istUkw(const QString& band)
{
    const QString b = band.toLower();
    return b == QLatin1String("2m") || b == QLatin1String("1.25m")
        || b == QLatin1String("70cm") || b == QLatin1String("33cm")
        || b == QLatin1String("23cm") || b.endsWith(QLatin1String("mm"))
        || (b.endsWith(QLatin1String("cm")));
}

QString Bedarf::urteilText() const
{
    switch (urteil) {
    case Urteil::NeuesLand:       return QStringLiteral("NEW ONE");
    case Urteil::NeuesBand:       return QStringLiteral("NEW BAND");
    case Urteil::NeueBetriebsart: return QStringLiteral("NEW MODE");
    case Urteil::NeuerPlatz:      return QStringLiteral("NEW SLOT");
    case Urteil::Unbestaetigt:    return QStringLiteral("NOT YET CONFIRMED");
    case Urteil::Bestaetigt:      return QStringLiteral("CONFIRMED HERE");
    case Urteil::Unbekannt:       break;
    }
    return {};
}

Bedarf bedarf(const QVector<LogEntry>& log, const QString& call,
              const QString& band, const QString& mode,
              const CtyDatParser* cty, const QString& meinFeld,
              const QVector<QString>& landJeEintrag)
{
    Bedarf b;
    b.call = call.trimmed().toUpper();
    if (b.call.isEmpty() || !cty) { return b; }
    b.land = cty->resolvePrimaryPrefix(b.call);
    if (b.land.isEmpty()) { return b; }
    if (const DxccEntity* d = cty->entityByPrefix(b.land)) {
        b.landName = d->name;
        b.kontinent = d->continent;
    }
    const QString mitte = feldDerLandesmitte(cty, b.land);
    if (isValidGridSquare(meinFeld) && !mitte.isEmpty()) {
        b.km = calculateDistanceKm(meinFeld, mitte);
        b.grad = calculateBearingInDegrees(meinFeld, mitte);
        b.lageBekannt = true;
    }
    const QString gruppe = gruppeFuer(mode);
    b.jetzt = band.isEmpty() ? QString() : zelle(band, gruppe);

    const bool cache = landJeEintrag.size() == log.size();
    for (int i = 0; i < log.size(); ++i) {
        const LogEntry& e = log.at(i);
        const QString l = cache ? landJeEintrag.at(i) : cty->resolvePrimaryPrefix(e.call);
        if (l != b.land) { continue; }
        ++b.qsosLand;
        const QString k = zelle(e.band, WorkedBefore::modeGroup(e.mode, e.submode));
        b.gearbeitet.insert(k);
        if (QsoConfirmation::isConfirmed(e)) { b.bestaetigt.insert(k); }
        if (e.call.compare(b.call, Qt::CaseInsensitive) == 0) {
            b.diesesCall.insert(k);
            ++b.qsosCall;
            if (!b.zuletzt.isValid() || e.timeOn > b.zuletzt) {
                b.zuletzt = e.timeOn;
                b.zuletztBand = e.band;
                b.zuletztMode = e.mode;
            }
        }
    }

    if (b.qsosLand == 0) { b.urteil = Bedarf::Urteil::NeuesLand; return b; }
    if (b.jetzt.isEmpty()) { return b; }
    bool bandBekannt = false, gruppeBekannt = false;
    for (const QString& k : b.gearbeitet) {
        if (k.startsWith(band.toLower() + QLatin1Char('|'))) { bandBekannt = true; }
        if (k.endsWith(QLatin1Char('|') + gruppe)) { gruppeBekannt = true; }
    }
    if (!b.gearbeitet.contains(b.jetzt)) {
        b.urteil = !bandBekannt ? Bedarf::Urteil::NeuesBand
                 : !gruppeBekannt ? Bedarf::Urteil::NeueBetriebsart
                 : Bedarf::Urteil::NeuerPlatz;
    } else {
        b.urteil = b.bestaetigt.contains(b.jetzt) ? Bedarf::Urteil::Bestaetigt
                                                  : Bedarf::Urteil::Unbestaetigt;
    }
    return b;
}

int BandStunde::besteStunde(const QString& band) const
{
    const QVector<int> v = anzahl.value(band);
    if (v.size() != 24) { return -1; }
    return int(std::max_element(v.cbegin(), v.cend()) - v.cbegin());
}

BandStunde bandStunde(const QVector<LogEntry>& log,
                      const std::function<bool(const LogEntry&)>& nimm)
{
    BandStunde s;
    for (const LogEntry& e : log) {
        if (nimm && !nimm(e)) { continue; }
        const QString b = e.band.toLower();
        if (!baender().contains(b) || !e.timeOn.isValid()) { continue; }
        QVector<int>& v = s.anzahl[b];
        if (v.isEmpty()) { v.fill(0, 24); }
        s.maxWert = std::max(s.maxWert, ++v[e.timeOn.toUTC().time().hour()]);
        ++s.gesamt;
    }
    for (const QString& b : baender()) {
        if (s.anzahl.contains(b)) { s.zeilen << b; }
    }
    return s;
}

int Rose::besterSektor() const
{
    return int(std::max_element(sektor.cbegin(), sektor.cend()) - sektor.cbegin());
}

void Rose::groessteLuecke(int* anfang, int* laenge) const
{
    int a = -1, l = 0;
    if (!punkte.isEmpty()) {
        for (int s = 0; s < 36; ++s) {
            int n = 0;
            while (n < 36 && sektor[(s + n) % 36] == 0) { ++n; }
            if (n > l) { l = n; a = s; }
        }
    }
    if (anfang) { *anfang = a; }
    if (laenge) { *laenge = l; }
}

Rose rose(const QVector<LogEntry>& log, const CtyDatParser* cty, const QString& meinFeld)
{
    Rose r;
    if (!isValidGridSquare(meinFeld)) { return r; }
    for (const LogEntry& e : log) {
        QString feld;
        if (isValidGridSquare(e.gridSquare)) {
            feld = e.gridSquare;
        } else if (!istUkw(e.band)) {
            feld = feldDerLandesmitte(cty, cty ? cty->resolvePrimaryPrefix(e.call) : QString());
            if (!feld.isEmpty()) { ++r.ausLand; }
        }
        if (feld.isEmpty()) { ++r.ohneOrt; continue; }
        const double km = calculateDistanceKm(meinFeld, feld);
        if (km < 1.0) { continue; }                 // eigenes Feld: keine Richtung
        const double g = norm360(calculateBearingInDegrees(meinFeld, feld));
        r.punkte.push_back({g, km});
        ++r.sektor[int(g / 10.0) % 36];
    }
    return r;
}

} // namespace Longpath::LogbuchBilder
