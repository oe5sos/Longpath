// SPDX-License-Identifier: GPL-3.0-or-later
//
// no-port-check: Longpath-eigener Prueflauf, kein Port.
//
// ── Die Spalten muessen wirklich untereinander stehen ──────────────────
//
// Betreiber 2026-10-24 zum FT8-Fenster: „uebersichtlich, professionell,
// einfach, high end". Dazu gehoert eine Spaltenueberschrift -- bis dahin
// standen dort vier Zahlenfelder ohne Beschriftung, und man musste
// raten, welches der Rapport ist und welches die Tonlage.
//
// Eine Ueberschrift, die um zwei Zeichen verrutscht ist, ist aber
// schlimmer als keine: sie behauptet eine Ordnung, die nicht stimmt.
// Darum prueft dieser Lauf nicht, DASS es eine Ueberschrift gibt,
// sondern dass ihre Spalten an denselben Stellen anfangen wie die
// Zahlen darunter.
//
// Gegenprobe (2026-10-24): mit einer von Hand geschriebenen
// Ueberschrift ("UTC   dB    DT    Hz   Message", wie man sie
// naheliegend tippt) faellt der Lauf -- die Felder liegen dann zwei
// bzw. drei Zeichen daneben.

#include <QtTest>

#include "gui/applets/Ft8Applet.h"

using namespace Longpath;

namespace {

// Wo faengt das n-te durch Leerzeichen getrennte Feld an?
QList<int> feldAnfaenge(const QString& zeile)
{
    QList<int> raus;
    bool inFeld = false;
    for (int i = 0; i < zeile.size(); ++i) {
        const bool leer = zeile.at(i).isSpace();
        if (!leer && !inFeld) { raus << i; inFeld = true; }
        if (leer) { inFeld = false; }
    }
    return raus;
}

} // namespace

class TstFt8Spaltenkopf : public QObject
{
    Q_OBJECT

private slots:
    void kopfUndZahlenStehenUntereinander()
    {
        const QString kopf = Ft8Applet::spaltenkopf();
        // Eine Zeile, wie sie das Fenster wirklich baut.
        const QString zeile = Ft8Applet::zeilentext(
            QStringLiteral("114215"), -12, 0.1, 920,
            QStringLiteral("OE5SOS G0XYZ IO91"), QString());

        const QList<int> k = feldAnfaenge(kopf);
        const QList<int> z = feldAnfaenge(zeile);

        QCOMPARE(k.size(), 5);
        QVERIFY2(z.size() >= 5,
                 qPrintable(QStringLiteral("Zeile hat nur %1 Felder: '%2'")
                                .arg(z.size()).arg(zeile)));

        // dB, DT und Hz sind rechtsbuendige Zahlenfelder -- ihre
        // Ueberschrift darf nicht WEITER LINKS anfangen als die Zahl,
        // sonst steht sie ueber der Nachbarspalte. Gleich oder weiter
        // rechts ist richtig.
        for (int i = 1; i <= 3; ++i) {
            QVERIFY2(k.at(i) >= z.at(i) - 1,
                     qPrintable(QStringLiteral(
                         "Spalte %1: Kopf bei %2, Zahl bei %3\n  '%4'\n  '%5'")
                         .arg(i).arg(k.at(i)).arg(z.at(i)).arg(kopf).arg(zeile)));
        }

        // Die Nachrichtenspalte ist linksbuendig -- hier muss es auf
        // das Zeichen stimmen, denn beide fangen am selben Ort an.
        QCOMPARE(k.at(4), z.at(4));
    }

    // Beide Zeilen sind gleich lang aufgebaut -- die Ueberschrift darf
    // die Liste nicht breiter machen als ihre breiteste Zeile.
    void derKopfIstNichtBreiterAlsNoetig()
    {
        const QString kopf = Ft8Applet::spaltenkopf();
        QVERIFY2(kopf.size() <= 40,
                 qPrintable(QStringLiteral("Kopf ist %1 Zeichen: '%2'")
                                .arg(kopf.size()).arg(kopf)));
        // Englisch, wie der Rest des Fensters (tst_oberflaeche_englisch
        // haelt eine Decke von neun deutschen Texten).
        QVERIFY(kopf.contains(QStringLiteral("Message")));
        QVERIFY(!kopf.contains(QStringLiteral("Nachricht")));
    }
};

QTEST_MAIN(TstFt8Spaltenkopf)
#include "tst_ft8_spaltenkopf.moc"
