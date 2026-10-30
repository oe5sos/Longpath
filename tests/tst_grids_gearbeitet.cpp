// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// tests/tst_grids_gearbeitet.cpp  (Longpath)
// =================================================================
//
// Welche Grossfelder im Logbuch stehen. Betreiber am 2026-10-30:
// „interessant waere zu sehen, welche grids ich noch nicht habe".
// =================================================================

#include <QtTest/QtTest>
#include <QTemporaryDir>

#include "core/GridsGearbeitet.h"

using namespace Longpath;

class TstGridsGearbeitet : public QObject
{
    Q_OBJECT

private slots:
    // ── Was ein Grossfeld ist und was nicht ───────────────────────
    void dasGrossfeldSindVierStellen()
    {
        QCOMPARE(GridsGearbeitet::grossfeld(QStringLiteral("JN67")),
                 QStringLiteral("JN67"));
        // Sechs Stellen werden auf vier gekuerzt -- sonst waeren
        // JN67 und JN67VV verschiedene Felder, und jedes QSO waere
        // ein neues.
        QCOMPARE(GridsGearbeitet::grossfeld(QStringLiteral("JN67vv")),
                 QStringLiteral("JN67"));
        QCOMPARE(GridsGearbeitet::grossfeld(QStringLiteral("io83")),
                 QStringLiteral("IO83"));

        // Kein Locator: RR73 steht in FT8 an derselben Stelle.
        QVERIFY(GridsGearbeitet::grossfeld(QStringLiteral("RR73")).isEmpty());
        QVERIFY(GridsGearbeitet::grossfeld(QStringLiteral("-12")).isEmpty());
        QVERIFY(GridsGearbeitet::grossfeld(QStringLiteral("R-09")).isEmpty());
        QVERIFY(GridsGearbeitet::grossfeld(QString()).isEmpty());
        // Buchstaben nur bis R.
        QVERIFY(GridsGearbeitet::grossfeld(QStringLiteral("ZZ99")).isEmpty());
    }

    // RR73 ist formal ein Feld (R-R, 7-3) und darf trotzdem keines
    // sein -- siehe `Ft8Ablauf::wieLocator`, wo derselbe Fall seit
    // Wochen steht.
    void rr73IstKeinFeld()
    {
        GridsGearbeitet g;
        g.merke(QStringLiteral("RR73"));
        QCOMPARE(g.anzahl(), 0);
    }

    void neuHeisstNichtImLogbuch()
    {
        GridsGearbeitet g;
        g.merke(QStringLiteral("JN67"));
        g.merke(QStringLiteral("IO83WJ"));

        QVERIFY(!g.istNeu(QStringLiteral("JN67")));
        QVERIFY(!g.istNeu(QStringLiteral("JN67AB")));   // dasselbe Feld
        QVERIFY(!g.istNeu(QStringLiteral("IO83")));
        QVERIFY(g.istNeu(QStringLiteral("KP20")));

        // Ohne Locator ist nichts neu: „unbekannt" ist kein Fund.
        QVERIFY2(!g.istNeu(QString()),
                 "ein fehlender Locator darf nicht als neues Feld "
                 "durchgehen");
        QVERIFY(!g.istNeu(QStringLiteral("RR73")));
    }

    // ── Aus einer ADIF-Datei lesen ────────────────────────────────
    void ausEinerAdifDatei()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString pfad = dir.filePath(QStringLiteral("log.adi"));
        {
            QFile f(pfad);
            QVERIFY(f.open(QIODevice::WriteOnly | QIODevice::Text));
            QTextStream out(&f);
            out << "Longpath test log\n<EOH>\n"
                << "<CALL:6>HB9ZHK<GRIDSQUARE:4>JN47<MY_GRIDSQUARE:4>JN67<EOR>\n"
                << "<CALL:5>PD4RJ<GRIDSQUARE:6>JO33AB<EOR>\n"
                << "<call:4>9A6T<eor>\n";
        }

        GridsGearbeitet g;
        QVERIFY(g.liesAus(pfad) > 0);
        QVERIFY(!g.istNeu(QStringLiteral("JN47")));
        QVERIFY(!g.istNeu(QStringLiteral("JO33")));

        // ── MY_GRIDSQUARE darf NICHT mitzaehlen ───────────────
        //
        // Das ist der Unterschied zwischen „wen habe ich gearbeitet"
        // und „wo stand ich dabei". Zaehlte es mit, waere Martins
        // eigenes Feld JN67 fuer immer „gearbeitet" -- und eine
        // Station aus seinem eigenen Grossfeld nie mehr blau.
        QVERIFY2(g.istNeu(QStringLiteral("JN67")),
                 "MY_GRIDSQUARE wurde als gearbeitetes Feld gezaehlt");
    }

    void eineFehlendeDateiIstKeinBefund()
    {
        GridsGearbeitet g;
        QCOMPARE(g.liesAus(QStringLiteral("/gibt/es/nicht.adi")), 0);
        QVERIFY(g.leer());
        // Und dann ist auch nichts neu -- ohne Logbuch waere SONST
        // jede Station blau, und die Markierung sagte nichts mehr.
        QVERIFY2(g.istNeu(QStringLiteral("KP20")),
                 "ohne Logbuch ist formal alles neu — das muss der "
                 "Aufrufer wissen (siehe leer())");
    }
};

QTEST_APPLESS_MAIN(TstGridsGearbeitet)
#include "tst_grids_gearbeitet.moc"
