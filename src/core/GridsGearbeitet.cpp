// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/GridsGearbeitet.cpp  (Longpath)
// =================================================================
// Siehe GridsGearbeitet.h fuer den Anlass und die Entscheidung,
// nur vier Stellen zu fuehren.
// =================================================================

#include "core/GridsGearbeitet.h"

#include <QFile>
#include <QRegularExpression>
#include <QTextStream>

namespace Longpath {

QString GridsGearbeitet::grossfeld(const QString& locator)
{
    const QString s = locator.trimmed().toUpper();
    if (s.size() < 4) { return {}; }

    // ── „RR73" ist KEIN Feldkenner ────────────────────────────
    //
    // Formal ist es einer: Feld RR, Quadrat 73 -- ein Punkt mitten im
    // Pazifik, und die Regel darunter liesze ihn durch. Im
    // FT8-Protokoll steht RR73 aber an genau der Stelle, an der sonst
    // der Feldkenner steht, und bedeutet dort „verstanden, Ende".
    //
    // Dieselbe Ausnahme steht seit dem 2026-10-16 in
    // `Ft8Ablauf::wieLocator`, mit derselben Begruendung. Ich habe sie
    // hier zuerst vergessen und sie im eigenen Prueflauf
    // wiedergefunden -- `rr73IstKeinFeld` war rot, bevor diese Zeilen
    // standen.
    if (s.startsWith(QStringLiteral("RR73"))) { return {}; }
    // Maidenhead: zwei Buchstaben A..R, zwei Ziffern.
    const QChar a = s.at(0), b = s.at(1), c = s.at(2), d = s.at(3);
    if (a < QLatin1Char('A') || a > QLatin1Char('R')) { return {}; }
    if (b < QLatin1Char('A') || b > QLatin1Char('R')) { return {}; }
    if (!c.isDigit() || !d.isDigit()) { return {}; }
    return s.left(4);
}

int GridsGearbeitet::liesAus(const QString& adifPfad)
{
    QFile f(adifPfad);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return 0; }

    // ADIF ist ein Strom aus <feld:laenge>wert. Hier interessiert
    // genau eines, und ein zeilenweiser Durchlauf genuegt: die Datei
    // waechst mit den QSOs, nicht mit der Zeit, und 60 000 Zeilen
    // sind in Millisekunden gelesen.
    //
    // GRIDSQUARE, nicht MY_GRIDSQUARE -- und die Unterscheidung ist
    // der Unterschied zwischen „wen habe ich gearbeitet" und „wo
    // stand ich dabei". Der Wortanfang `(?<![_A-Z])` haelt
    // MY_GRIDSQUARE heraus.
    static const QRegularExpression re(
        QStringLiteral("(?<![_A-Z])GRIDSQUARE:(\\d+)(?::[^>]*)?>"),
        QRegularExpression::CaseInsensitiveOption);

    int gefunden = 0;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString zeile = in.readLine();
        auto it = re.globalMatch(zeile);
        while (it.hasNext()) {
            const auto m = it.next();
            const int laenge = m.captured(1).toInt();
            const int ab = m.capturedEnd(0);
            if (laenge <= 0 || ab + laenge > zeile.size()) { continue; }
            const QString feld = grossfeld(zeile.mid(ab, laenge));
            if (feld.isEmpty()) { continue; }
            m_felder.insert(feld);
            ++gefunden;
        }
    }
    return gefunden;
}

void GridsGearbeitet::merke(const QString& grid)
{
    const QString feld = grossfeld(grid);
    if (!feld.isEmpty()) { m_felder.insert(feld); }
}

bool GridsGearbeitet::istNeu(const QString& grid) const
{
    const QString feld = grossfeld(grid);
    // Kein Locator in der Nachricht heisst nicht „neues Feld". Es
    // heisst, dass niemand es gesagt hat -- und eine Markierung auf
    // Verdacht waere schlimmer als keine.
    if (feld.isEmpty()) { return false; }
    return !m_felder.contains(feld);
}

}  // namespace Longpath
