// no-port-check: Longpath-original, see LocatorJagdZeichen.h.
//
// =================================================================
// src/gui/widgets/LocatorJagdZeichen.cpp  (Longpath)
// =================================================================
// Modification history (Longpath):
//   2026-11-11 — Created in C++20/Qt6 for Longpath by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude (Claude Code).
// =================================================================
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "gui/widgets/LocatorJagdZeichen.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"   // Style::hexRole()

#include <QFontMetricsF>
#include <QPainter>
#include <QPolygonF>

#include <QHash>

#include <algorithm>
#include <cmath>

namespace Longpath::LocatorJagdZeichen {

QColor feldFuellung(FeldStand s)
{
    // Gedaempft: das Mosaik ist der Hintergrund der Jagd, nicht ihr
    // Gegenstand. Bei voller Deckung waere Europa ein gruener Fleck, in
    // dem kein Punkt mehr zu sehen ist.
    switch (s) {
    // Erste Fassung (105/80) war auf dem Blue-Marble-Bild kaum zu sehen
    // -- am eigenen Bild nachgezogen (2026-11-11).
    case FeldStand::Bestaetigt: {
        QColor c(Style::kJagdFeldBestaetigt);
        c.setAlpha(150);
        return c;
    }
    case FeldStand::Gearbeitet: {
        QColor c(Style::kJagdFeldGearbeitet);
        c.setAlpha(125);
        return c;
    }
    case FeldStand::Nie:
        break;
    }
    return QColor(Qt::transparent);
}

QColor farbe(LiveArt a)
{
    switch (a) {
    case LiveArt::NeuesLand:
    case LiveArt::NeuesFeld:   return QColor(Style::kJagdNeu);
    case LiveArt::Gearbeitet:  return QColor(Style::kJagdGearbeitet);
    case LiveArt::Gewoehnlich: return QColor(Style::kJagdGewoehnlich);
    }
    return QColor(Style::kJagdGewoehnlich);
}

void punkt(QPainter& p, const QPointF& c, LiveArt a)
{
    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    const QColor f = farbe(a);
    const QColor grund(Style::hexRole(Style::kAppBg));
    switch (a) {
    case LiveArt::NeuesLand: {
        QColor schein = f;
        schein.setAlpha(80);
        p.setPen(Qt::NoPen);
        p.setBrush(schein);
        p.drawEllipse(c, 12.0, 12.0);
        QPolygonF raute;
        raute << c + QPointF(0, -8) << c + QPointF(8, 0)
              << c + QPointF(0, 8) << c + QPointF(-8, 0);
        p.setPen(QPen(QColor(Style::kJagdNeuesLandRand), 1.6));
        p.setBrush(f);
        p.drawPolygon(raute);
        break;
    }
    case LiveArt::NeuesFeld: {
        QColor schein = f;
        schein.setAlpha(80);
        p.setPen(Qt::NoPen);
        p.setBrush(schein);
        p.drawEllipse(c, 9.5, 9.5);
        // Warmweisser Ring: auf der Azimutalkarte traegt 20 m dieselbe
        // Bandfarbe (Bernstein), und ein neues Feld darf nicht wie einer
        // der hundert 20-m-QSO-Punkte aussehen (am Bild gefunden).
        p.setPen(QPen(QColor(Style::kJagdNeuesLandRand), 1.3));
        p.setBrush(f);
        p.drawEllipse(c, 5.0, 5.0);
        break;
    }
    case LiveArt::Gearbeitet:
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(grund, 3.0));
        p.drawEllipse(c, 4.5, 4.5);
        p.setPen(QPen(f, 1.5));
        p.drawEllipse(c, 4.5, 4.5);
        break;
    case LiveArt::Gewoehnlich:
        p.setPen(QPen(grund, 1.0));
        p.setBrush(f);
        p.drawEllipse(c, 3.4, 3.4);
        break;
    }
    p.restore();
}

QVector<QPointF> versaetze(const QVector<LivePunkt>& punkte)
{
    QHash<QString, int> jeFeld;
    for (const LivePunkt& lp : punkte) { ++jeFeld[lp.feld]; }
    QHash<QString, int> schon;
    QVector<QPointF> aus;
    aus.reserve(punkte.size());
    for (const LivePunkt& lp : punkte) {
        const int n = jeFeld.value(lp.feld);
        const int k = schon[lp.feld]++;
        if (n <= 1) { aus.append(QPointF(0, 0)); continue; }
        // Bis sechs auf einem Ring von 9 px, darueber ein zweiter.
        const int ring = k < 6 ? 0 : 1;
        const int imRing = ring == 0 ? std::min(n, 6) : n - 6;
        const int stelle = ring == 0 ? k : k - 6;
        const double r = ring == 0 ? 9.0 : 17.0;
        const double w = (2.0 * M_PI * stelle) / std::max(1, imRing) - M_PI / 2.0;
        aus.append(QPointF(r * std::cos(w), r * std::sin(w)));
    }
    return aus;
}

bool beschriften(LiveArt a, int anzahl)
{
    if (a == LiveArt::NeuesLand || a == LiveArt::NeuesFeld) { return true; }
    return anzahl <= 25;
}

double legende(QPainter& p, const QRectF& rahmen, bool mitMosaik, bool mitLive)
{
    struct Eintrag { int art; QString text; };   // art: 0/1 Feld, 2.. Live
    QVector<Eintrag> e;
    if (mitMosaik) {
        e.append({0, QStringLiteral("confirmed")});
        e.append({1, QStringLiteral("worked")});
    }
    if (mitLive) {
        e.append({2, QStringLiteral("new DXCC")});
        e.append({3, QStringLiteral("new square")});
        e.append({4, QStringLiteral("worked before")});
    }
    if (e.isEmpty()) { return 0.0; }

    p.save();
    p.setRenderHint(QPainter::Antialiasing);
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    const QFontMetricsF fm(f);
    const double zeile = fm.height() + 3.0;
    double breite = 0.0;
    for (const Eintrag& x : e) {
        breite += 14.0 + fm.horizontalAdvance(x.text) + 10.0;
    }
    const QRectF box(rahmen.left() + 8.0, rahmen.bottom() - zeile - 10.0,
                     breite + 6.0, zeile + 4.0);
    QColor bg(Style::hexRole(Style::kAppBg));
    bg.setAlpha(190);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(box, 3.0, 3.0);

    double x = box.left() + 6.0;
    const double mitte = box.center().y();
    for (const Eintrag& it : e) {
        const QPointF c(x + 4.0, mitte);
        if (it.art <= 1) {
            QColor fu = feldFuellung(it.art == 0 ? FeldStand::Bestaetigt
                                                 : FeldStand::Gearbeitet);
            fu.setAlpha(std::min(255, fu.alpha() + 70));
            p.setPen(Qt::NoPen);
            p.setBrush(fu);
            p.drawRect(QRectF(c.x() - 4.0, c.y() - 3.0, 8.0, 6.0));
        } else {
            const LiveArt a = it.art == 2 ? LiveArt::NeuesLand
                            : it.art == 3 ? LiveArt::NeuesFeld
                                          : LiveArt::Gearbeitet;
            p.save();
            p.translate(c);
            p.scale(0.6, 0.6);
            punkt(p, QPointF(0, 0), a);
            p.restore();
        }
        p.setPen(QColor(Style::kTextSecondary));
        p.drawText(QPointF(x + 12.0, mitte + fm.ascent() / 2.0 - 1.0), it.text);
        x += 14.0 + fm.horizontalAdvance(it.text) + 10.0;
    }
    p.restore();
    return box.height();
}

} // namespace Longpath::LocatorJagdZeichen
