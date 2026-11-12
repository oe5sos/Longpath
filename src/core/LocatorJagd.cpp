// no-port-check: Longpath-original, see LocatorJagd.h.
//
// =================================================================
// src/core/LocatorJagd.cpp  (Longpath)
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/LocatorJagd.h"

#include "core/AdifLog.h"
#include "core/GridsGearbeitet.h"
#include "core/Maidenhead.h"
#include "core/QsoConfirmation.h"

#include <algorithm>
#include <cmath>

namespace Longpath {

// ── Mosaik ──────────────────────────────────────────────────────────

QString LocatorMosaik::feld(const QString& locator)
{
    // Dieselbe Regel wie beim FT8-Fenster -- dort steht auch, warum
    // „RR73" keines ist. Dazu die Grenze A..R: „ZZ99" hat die Form,
    // liegt aber nicht auf der Erde, und ein Feld ausserhalb der Karte
    // waere ein Punkt, den niemand findet.
    const QString f = GridsGearbeitet::grossfeld(locator);
    if (f.size() != 4) { return {}; }
    if (f.at(0) > QLatin1Char('R') || f.at(1) > QLatin1Char('R')) { return {}; }
    return f;
}

QString LocatorMosaik::bandSchluessel(const QString& band)
{
    return band.trimmed().toLower();
}

void LocatorMosaik::baue(const QVector<LogEntry>& log, bool nurFt8)
{
    m_jeBand.clear();
    m_alle.clear();
    m_rufeJeBand.clear();
    m_nurFt8 = nurFt8;

    const auto hebe = [](QHash<QString, FeldStand>& tafel, const QString& f,
                         FeldStand s) {
        auto it = tafel.find(f);
        if (it == tafel.end()) {
            tafel.insert(f, s);
        } else if (static_cast<int>(s) > static_cast<int>(it.value())) {
            it.value() = s;
        }
    };

    for (const LogEntry& e : log) {
        if (nurFt8 && e.mode.trimmed().toUpper() != QLatin1String("FT8")) {
            continue;
        }
        const QString band = bandSchluessel(e.band);
        const QString ruf  = e.call.trimmed().toUpper();
        if (!ruf.isEmpty()) { m_rufeJeBand[band].insert(ruf); }

        const QString f = feld(e.gridSquare);
        if (f.isEmpty()) { continue; }
        const FeldStand s = QsoConfirmation::isConfirmed(e)
                                ? FeldStand::Bestaetigt
                                : FeldStand::Gearbeitet;
        hebe(m_jeBand[band], f, s);
        hebe(m_alle, f, s);
    }
}

FeldStand LocatorMosaik::stand(const QString& f, const QString& band) const
{
    const QString k = feld(f);
    if (k.isEmpty()) { return FeldStand::Nie; }
    const QString b = bandSchluessel(band);
    if (b.isEmpty()) { return m_alle.value(k, FeldStand::Nie); }
    return m_jeBand.value(b).value(k, FeldStand::Nie);
}

QHash<QString, FeldStand> LocatorMosaik::felder(const QString& band) const
{
    const QString b = bandSchluessel(band);
    return b.isEmpty() ? m_alle : m_jeBand.value(b);
}

bool LocatorMosaik::gearbeitet(const QString& rufzeichen, const QString& band) const
{
    const QString r = rufzeichen.trimmed().toUpper();
    if (r.isEmpty()) { return false; }
    const QString b = bandSchluessel(band);
    if (b.isEmpty()) {
        for (auto it = m_rufeJeBand.cbegin(); it != m_rufeJeBand.cend(); ++it) {
            if (it.value().contains(r)) { return true; }
        }
        return false;
    }
    return m_rufeJeBand.value(b).contains(r);
}

QStringList LocatorMosaik::baender() const
{
    QStringList b = m_rufeJeBand.keys();
    b.removeAll(QString());
    std::sort(b.begin(), b.end(), [](const QString& x, const QString& y) {
        return AdifLog::bandSortKeyMHz(x) < AdifLog::bandSortKeyMHz(y);
    });
    return b;
}

// ── Live-Schicht ────────────────────────────────────────────────────

void LiveSchicht::setFensterSekunden(int sekunden)
{
    m_fenster = std::clamp(sekunden, kMinSekunden, kMaxSekunden);
}

void LiveSchicht::nimm(const Ft8Gehoert& g)
{
    const QString ruf = g.rufzeichen.trimmed().toUpper();
    if (ruf.isEmpty()) { return; }

    const QString loc = LocatorMosaik::feld(g.locator).isEmpty()
                            ? QString() : g.locator.trimmed().toUpper();
    auto it = m_jeRuf.find(ruf);
    if (it == m_jeRuf.end()) {
        // Ohne Locator laesst sich eine neue Station nicht verorten.
        // Sie zu raten (Landesmitte) waere hier falsch: die Frage ist
        // gerade, in WELCHEM Feld sie sitzt.
        if (loc.isEmpty()) { return; }
        Eintrag e;
        e.letzter = g;
        e.letzter.rufzeichen = ruf;
        e.locator = loc;
        m_jeRuf.insert(ruf, e);
        return;
    }

    // Eine aeltere Periode ueberschreibt keine juengere -- die Funde
    // kommen in der Regel in Zeitfolge, aber nicht garantiert.
    if (it->letzter.zeit.isValid() && g.zeit.isValid()
        && g.zeit < it->letzter.zeit) {
        if (it->locator.isEmpty() && !loc.isEmpty()) { it->locator = loc; }
        return;
    }
    it->letzter = g;
    it->letzter.rufzeichen = ruf;
    if (!loc.isEmpty()) { it->locator = loc; }
}

void LiveSchicht::altere(const QDateTime& jetzt)
{
    for (auto it = m_jeRuf.begin(); it != m_jeRuf.end();) {
        const qint64 alter = it->letzter.zeit.isValid()
                                 ? it->letzter.zeit.secsTo(jetzt) : 0;
        if (alter > m_fenster) {
            it = m_jeRuf.erase(it);
        } else {
            ++it;
        }
    }
}

const Ft8Gehoert* LiveSchicht::finde(const QString& rufzeichen) const
{
    auto it = m_jeRuf.constFind(rufzeichen.trimmed().toUpper());
    return it == m_jeRuf.cend() ? nullptr : &it->letzter;
}

LiveArt LiveSchicht::art(bool neuesLand, bool neuesFeld, bool gearbeitet)
{
    if (neuesLand)  { return LiveArt::NeuesLand; }
    if (neuesFeld)  { return LiveArt::NeuesFeld; }
    if (gearbeitet) { return LiveArt::Gearbeitet; }
    return LiveArt::Gewoehnlich;
}

QVector<LivePunkt> LiveSchicht::punkte(const QDateTime& jetzt,
                                       const LocatorMosaik& mosaik,
                                       const QString& heimLocator) const
{
    QVector<LivePunkt> aus;
    aus.reserve(m_jeRuf.size());
    const bool hatHeim = isValidGridSquare(heimLocator);

    for (auto it = m_jeRuf.cbegin(); it != m_jeRuf.cend(); ++it) {
        const Eintrag& e = it.value();
        const qint64 alter = e.letzter.zeit.isValid()
                                 ? e.letzter.zeit.secsTo(jetzt) : 0;
        if (alter > m_fenster) { continue; }
        const QString f = LocatorMosaik::feld(e.locator);
        if (f.isEmpty()) { continue; }

        LivePunkt p;
        p.g = e.letzter;
        p.locator = e.locator;
        p.feld = f;
        p.alterSek = static_cast<int>(std::max<qint64>(0, alter));
        // Die Mitte des Feldes, das die Station gesendet hat. Mehr als
        // vier Stellen schickt FT8 nicht.
        calculateLatLonFromGridSquare(f, p.lat, p.lon);

        // Gegen das Band, auf dem sie gehoert wurde -- nicht gegen das,
        // das gerade auf der Karte gewaehlt ist. „Neu auf 20 m" ist eine
        // Aussage ueber 20 m.
        const bool neuesFeld =
            mosaik.stand(f, e.letzter.band) == FeldStand::Nie;
        const bool schonGearbeitet =
            mosaik.gearbeitet(e.letzter.rufzeichen, e.letzter.band);
        p.feldNeu = neuesFeld;
        p.art = art(e.letzter.neuesLand, neuesFeld, schonGearbeitet);

        if (hatHeim) {
            p.km = calculateDistanceKm(heimLocator, f);
            double az = calculateBearingInDegrees(heimLocator, f);
            az = std::fmod(az + 360.0, 360.0);
            p.azimutGrad = az;
        }
        aus.append(p);
    }

    // Feste Reihenfolge: die wichtigsten zuletzt (obenauf gemalt), bei
    // Gleichstand nach Rufzeichen -- sonst waere jedes Bild anders, je
    // nachdem, wie QHash gerade sortiert.
    std::sort(aus.begin(), aus.end(), [](const LivePunkt& a, const LivePunkt& b) {
        if (a.art != b.art) {
            return static_cast<int>(a.art) > static_cast<int>(b.art);
        }
        return a.g.rufzeichen < b.g.rufzeichen;
    });
    return aus;
}

int LiveSchicht::neueFelder(const QVector<LivePunkt>& punkte, const QString& band)
{
    const QString b = LocatorMosaik::bandSchluessel(band);
    QSet<QString> felder;
    for (const LivePunkt& p : punkte) {
        if (!b.isEmpty() && LocatorMosaik::bandSchluessel(p.g.band) != b) {
            continue;
        }
        // Ein neues Land in einem neuen Feld ist AUCH ein neues Feld --
        // die Art zeigt nur das Wichtigere an. Darum zaehlt hier das
        // Feld selbst, nicht die Art.
        if (p.feldNeu) { felder.insert(p.feld); }
    }
    return felder.size();
}

QString LiveSchicht::zaehlerText(int neue, const QString& band)
{
    const QString b = band.trimmed().isEmpty() ? QStringLiteral("this band")
                                               : band.trimmed();
    return neue == 1
        ? QStringLiteral("1 new square heard now on %1").arg(b)
        : QStringLiteral("%1 new squares heard now on %2").arg(neue).arg(b);
}

QString LiveSchicht::artText(LiveArt a, const QString& band)
{
    const QString b = band.trimmed().isEmpty() ? QStringLiteral("this band")
                                               : band.trimmed();
    switch (a) {
    case LiveArt::NeuesLand:   return QStringLiteral("new DXCC");
    case LiveArt::NeuesFeld:   return QStringLiteral("new square on %1").arg(b);
    case LiveArt::Gearbeitet:  return QStringLiteral("worked before on %1").arg(b);
    case LiveArt::Gewoehnlich: return QStringLiteral("square already in the log");
    }
    return {};
}

QString LiveSchicht::hinweis(const LivePunkt& p, const QDateTime& jetzt)
{
    const int alter = p.g.zeit.isValid()
                          ? static_cast<int>(std::max<qint64>(0, p.g.zeit.secsTo(jetzt)))
                          : p.alterSek;
    const QString snr = QStringLiteral("%1%2 dB")
                            .arg(p.g.snrDb < 0 ? QLatin1Char('-') : QLatin1Char('+'))
                            .arg(std::abs(p.g.snrDb), 2, 10, QLatin1Char('0'));
    QStringList zeile1;
    zeile1 << p.g.rufzeichen << p.locator << snr;
    if (!p.g.band.isEmpty()) { zeile1 << QStringLiteral("%1 %2").arg(p.g.band, p.g.modus).trimmed(); }

    QStringList zeile2;
    if (p.km >= 0.0) {
        zeile2 << QStringLiteral("%1 km").arg(qRound(p.km))
               << QStringLiteral("%1°").arg(qRound(p.azimutGrad) % 360);
    }
    zeile2 << (alter < 60 ? QStringLiteral("heard %1 s ago").arg(alter)
                          : QStringLiteral("heard %1 min %2 s ago")
                                .arg(alter / 60).arg(alter % 60));

    QString zeile3 = artText(p.art, p.g.band);
    if (!p.g.land.isEmpty()) { zeile3 = p.g.land + QStringLiteral(" — ") + zeile3; }

    const QString sep = QStringLiteral("  ·  ");
    return zeile1.join(sep) + QLatin1Char('\n') + zeile2.join(sep)
           + QLatin1Char('\n') + zeile3
           + QStringLiteral("\nClick to call — the same as a double-click in the FT8 list.");
}

} // namespace Longpath
