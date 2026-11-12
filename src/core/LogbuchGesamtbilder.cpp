// no-port-check: Longpath-original, see LogbuchGesamtbilder.h.
//
// src/core/LogbuchGesamtbilder.cpp  (Longpath)

#include "core/LogbuchGesamtbilder.h"

#include "core/CtyDatParser.h"
#include "core/QsoConfirmation.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <cmath>
#include <limits>

namespace Longpath::LogbuchBilder {

namespace {

constexpr double kNaN = std::numeric_limits<double>::quiet_NaN();

QString monatVon(const QDate& d) { return d.toString(QStringLiteral("yyyy-MM")); }

// Gebiet eines QSOs: cty.dat-Hauptpraefix, WAE auf DXCC gefaltet.
QString gebietVon(const LogEntry& e, const CtyDatParser* cty)
{
    if (!cty) { return {}; }
    const QString p = cty->resolvePrimaryPrefix(e.call.trimmed().toUpper());
    if (!p.startsWith(QLatin1Char('*'))) { return p; }
    return dxccFuerWae(p);
}

} // namespace

// ── D  Sonnenzyklus ─────────────────────────────────────────────────

FluxJeMonat fluxAusSwpc(const QByteArray& json)
{
    FluxJeMonat f;
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isArray()) { return f; }
    for (const QJsonValue& v : doc.array()) {
        const QJsonObject o = v.toObject();
        const QString monat = o.value(QStringLiteral("time-tag")).toString().left(7);
        const double wert = o.value(QStringLiteral("f10.7")).toDouble(-1.0);
        if (monat.size() == 7 && monat.at(4) == QLatin1Char('-') && wert > 0.0) {
            f.insert(monat, wert);
        }
    }
    return f;
}

QVector<double> gleitendesMittel(const QVector<double>& werte, int halb)
{
    QVector<double> aus(werte.size(), kNaN);
    for (int i = 0; i < werte.size(); ++i) {
        if (std::isnan(werte[i])) { continue; }
        double summe = 0.0;
        int n = 0;
        for (int j = std::max(0, i - halb); j <= std::min<int>(werte.size() - 1, i + halb); ++j) {
            if (!std::isnan(werte[j])) { summe += werte[j]; ++n; }
        }
        aus[i] = summe / n;     // n >= 1: werte[i] selbst zaehlt
    }
    return aus;
}

bool Sonnenzyklus::hatFlux() const
{
    return std::any_of(flux.cbegin(), flux.cend(), [](double v) { return !std::isnan(v); });
}

QString Sonnenzyklus::fluxBis() const
{
    for (int i = int(flux.size()) - 1; i >= 0; --i) {
        if (!std::isnan(flux[i])) { return monate.value(i); }
    }
    return {};
}

Sonnenzyklus sonnenzyklus(const QVector<LogEntry>& log, const FluxJeMonat& flux,
                          const QDate& heute)
{
    Sonnenzyklus s;
    QHash<QString, int> jeMonat;
    QDate erster, letzter;
    for (const LogEntry& e : log) {
        if (!e.timeOn.isValid()) { continue; }
        const QDate d = e.timeOn.toUTC().date();
        ++jeMonat[monatVon(d)];
        ++s.gesamt;
        if (!erster.isValid() || d < erster) { erster = d; }
        if (!letzter.isValid() || d > letzter) { letzter = d; }
    }
    if (!erster.isValid()) { return s; }
    // Bis heute, damit eine Pause am Ende als Pause zu sehen ist; ein
    // QSO mit einem Datum in der Zukunft verlaengert bis zu sich.
    const QDate ende = heute.isValid() && heute > letzter ? heute : letzter;
    for (QDate m(erster.year(), erster.month(), 1); m <= ende; m = m.addMonths(1)) {
        const QString k = monatVon(m);
        s.monate << k;
        const int n = jeMonat.value(k);
        s.qsos << n;
        s.flux << flux.value(k, kNaN);
        if (n > s.maxQsos) { s.maxQsos = n; s.besterMonat = int(s.monate.size()) - 1; }
    }
    s.glatt = gleitendesMittel(s.flux, 2);
    return s;
}

// ── E  Kontinent-Uhr ────────────────────────────────────────────────

const QStringList& KontinentUhr::kontinente()
{
    static const QStringList k = {QStringLiteral("EU"), QStringLiteral("NA"),
        QStringLiteral("AS"), QStringLiteral("SA"), QStringLiteral("AF"),
        QStringLiteral("OC")};
    return k;
}

int KontinentUhr::summe(const QString& kontinent) const
{
    const QVector<int> v = anzahl.value(kontinent);
    int s = 0;
    for (int n : v) { s += n; }
    return s;
}

int KontinentUhr::maxWert(const QString& kontinent) const
{
    const QVector<int> v = anzahl.value(kontinent);
    return v.isEmpty() ? 0 : *std::max_element(v.cbegin(), v.cend());
}

int KontinentUhr::besteStunde(const QString& kontinent) const
{
    const QVector<int> v = anzahl.value(kontinent);
    if (v.size() != 24 || maxWert(kontinent) == 0) { return -1; }
    return int(std::max_element(v.cbegin(), v.cend()) - v.cbegin());
}

double KontinentUhr::anteil(const QString& kontinent, int stunde) const
{
    const QVector<int> v = anzahl.value(kontinent);
    const int m = maxWert(kontinent);
    if (m == 0 || stunde < 0 || stunde >= v.size()) { return 0.0; }
    return double(v[stunde]) / m;
}

KontinentUhr kontinentUhr(const QVector<LogEntry>& log, const CtyDatParser* cty)
{
    KontinentUhr u;
    for (const QString& k : KontinentUhr::kontinente()) { u.anzahl[k] = QVector<int>(24, 0); }
    for (const LogEntry& e : log) {
        if (!e.timeOn.isValid()) { continue; }
        const DxccEntity* d = cty ? cty->entityByPrefix(cty->resolvePrimaryPrefix(e.call.trimmed().toUpper()))
                                  : nullptr;
        const auto it = d ? u.anzahl.find(d->continent) : u.anzahl.end();
        if (it == u.anzahl.end()) { ++u.ohneZuordnung; continue; }
        ++(*it)[e.timeOn.toUTC().time().hour()];
        ++u.gesamt;
    }
    return u;
}

// ── F  DXCC-Wand ────────────────────────────────────────────────────

const QStringList& wandBaender()
{
    static const QStringList b = {QStringLiteral("160m"), QStringLiteral("80m"),
        QStringLiteral("40m"), QStringLiteral("30m"), QStringLiteral("20m"),
        QStringLiteral("17m"), QStringLiteral("15m"), QStringLiteral("12m"),
        QStringLiteral("10m"), QStringLiteral("6m")};
    return b;
}

QString dxccFuerWae(const QString& waePraefix)
{
    // Dieselbe Tabelle wie LogbookStats (foldNonDxcc), die Kacheln der
    // Uebersicht und der Wand zaehlen also gleich.
    static const QHash<QString, QString> kFalten = {
        {QStringLiteral("*IT9"),  QStringLiteral("I")},    // Sicily -> Italy
        {QStringLiteral("*IG9"),  QStringLiteral("I")},    // African Italy -> Italy
        {QStringLiteral("*GM/S"), QStringLiteral("GM")},   // Shetland -> Scotland
        {QStringLiteral("*JW/B"), QStringLiteral("JW")},   // Bear Island -> Svalbard
        {QStringLiteral("*TA1"),  QStringLiteral("TA")},   // European Turkey -> Turkey
        {QStringLiteral("*4U1V"), QStringLiteral("OE")},   // Vienna Intl Ctr -> Austria
    };
    return kFalten.value(waePraefix.toUpper());
}

const QStringList& DxccWand::kontinente()
{
    static const QStringList k = {QStringLiteral("EU"), QStringLiteral("AS"),
        QStringLiteral("AF"), QStringLiteral("NA"), QStringLiteral("SA"),
        QStringLiteral("OC")};
    return k;
}

void DxccWand::zaehle(const QString& kontinent, int* alle, int* davonGearbeitet) const
{
    int a = 0, g = 0;
    for (const DxccKachel& k : kacheln) {
        if (k.kontinent != kontinent) { continue; }
        ++a;
        if (k.gearbeitet()) { ++g; }
    }
    if (alle) { *alle = a; }
    if (davonGearbeitet) { *davonGearbeitet = g; }
}

DxccWand dxccWand(const QVector<LogEntry>& log, const CtyDatParser* cty)
{
    DxccWand w;
    if (!cty) { w.ohneZuordnung = int(log.size()); return w; }
    const QStringList& reihe = DxccWand::kontinente();
    for (const DxccEntity& d : cty->allEntities()) {
        if (d.primaryPrefix.startsWith(QLatin1Char('*'))) { continue; }
        DxccKachel k;
        k.praefix = d.primaryPrefix;
        k.name = d.name;
        k.kontinent = d.continent;
        w.kacheln << k;
    }
    std::sort(w.kacheln.begin(), w.kacheln.end(), [&](const DxccKachel& a, const DxccKachel& b) {
        const int ia = int(reihe.indexOf(a.kontinent)), ib = int(reihe.indexOf(b.kontinent));
        // Unbekannter Kontinent (gibt es in cty.dat nicht) ans Ende.
        const int ra = ia < 0 ? 99 : ia, rb = ib < 0 ? 99 : ib;
        if (ra != rb) { return ra < rb; }
        return a.praefix < b.praefix;
    });
    QHash<QString, int> index;
    for (int i = 0; i < w.kacheln.size(); ++i) { index.insert(w.kacheln[i].praefix, i); }

    for (const LogEntry& e : log) {
        const auto it = index.constFind(gebietVon(e, cty));
        if (it == index.constEnd()) { ++w.ohneZuordnung; continue; }
        DxccKachel& k = w.kacheln[it.value()];
        ++k.qsos;
        ++w.qsos;
        const QString band = e.band.trimmed().toLower();
        if (!band.isEmpty()) { k.baender.insert(band); }
        if (QsoConfirmation::isConfirmed(e)) { k.bestaetigt = true; }
    }
    for (const DxccKachel& k : w.kacheln) {
        if (k.gearbeitet()) { ++w.gearbeitet; }
        if (k.bestaetigt) { ++w.bestaetigt; }
    }
    return w;
}

} // namespace Longpath::LogbuchBilder
