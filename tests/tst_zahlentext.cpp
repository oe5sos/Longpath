// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-eigen, kein Thetis-Bezug.
//
// Ein Trennzeichen fuer die ganze Oberflaeche (2026-10-13).
//
// Der Betreiber hat am 2026-10-09 notiert, dass EIN Feld zwei
// Schreibweisen zeigt: der Balken schreibt "1,3", die Beschriftung
// daneben "47.5V". Die Ursache war, dass es zwei Wege gab --
// QLocale::system() in HGauge und im FreeDV-Melder, QString::arg(double)
// an vierzehn Stellen in den Applets. QString::arg kennt keine Locale
// und schreibt IMMER einen Punkt.
//
// Jetzt gibt es zwei Funktionen mit sprechenden Namen, und dieser
// Prueffall haelt beide auseinander. Das ist der eigentliche Zweck:
// nicht "alles Komma", sondern dass man an der Aufrufstelle SIEHT, ob
// eine Zahl gelesen oder gespeichert wird.

#include <QtTest/QtTest>

#include "gui/Zahlentext.h"

using namespace Longpath;

class TstZahlentext : public QObject {
    Q_OBJECT

private slots:
    void anzeigeZahlFolgtDemSystem()
    {
        // Welches Zeichen das ist, entscheidet das System -- der
        // Prueffall darf darum nicht auf das Komma festnageln, sonst
        // faellt er auf einem englischen Rechner (und in der CI).
        QLocale l = QLocale::system();
        l.setNumberOptions(QLocale::OmitGroupSeparator);
        QCOMPARE(anzeigeZahl(1.3, 1), l.toString(1.3, 'f', 1));
        QCOMPARE(anzeigeZahl(47.5, 1), l.toString(47.5, 'f', 1));
    }

    void anzeigeZahlSetztKEINETausenderpunkte()
    {
        // DIE Falle dieses Durchgangs. Bei QLocale ist die Gruppierung
        // standardmaessig AN: aus 1296 MHz wuerde auf einem deutschen
        // System "1.296,000000" -- in einer Zeile mit Komma-Dezimalen
        // nicht mehr eindeutig zu lesen. Haette ich die vierzehn Stellen
        // einfach auf QLocale::system() umgestellt, waere genau das
        // eingeschleppt worden.
        const QString s = anzeigeZahl(1296.0, 6);
        QVERIFY2(!s.contains(QLatin1Char(' ')),
                 qPrintable(QStringLiteral("Gruppierung mit Leerzeichen: %1").arg(s)));
        // Genau EIN Trennzeichen darf vorkommen -- das Dezimaltrennzeichen.
        QLocale l = QLocale::system();
        const int dez = s.count(l.decimalPoint());
        QCOMPARE(dez, 1);
        const int grp = s.count(l.groupSeparator());
        // Ist das Gruppenzeichen dasselbe wie das Dezimalzeichen (kommt
        // in keiner gaengigen Locale vor), waere die Zaehlung oben schon
        // der Beleg; sonst darf es NICHT vorkommen.
        if (l.groupSeparator() != l.decimalPoint()) { QCOMPARE(grp, 0); }
        // Und die Ziffern stimmen noch.
        QVERIFY(s.startsWith(QStringLiteral("1296")));
    }

    void datenZahlNimmtIMMERDenPunkt()
    {
        // Unabhaengig vom System: das geht in Dateien, die andere
        // Programme lesen (ADIF, Beschreibungsdateien, Protokolle).
        QCOMPARE(datenZahl(14.225, 6), QStringLiteral("14.225000"));
        QCOMPARE(datenZahl(1296.0, 6), QStringLiteral("1296.000000"));
        QCOMPARE(datenZahl(1.3, 1), QStringLiteral("1.3"));
        // Nie ein Komma, nie eine Gruppierung.
        for (double v : {0.5, 1.3, 47.5, 1296.0, 1234567.0}) {
            const QString s = datenZahl(v, 3);
            QVERIFY2(!s.contains(QLatin1Char(',')),
                     qPrintable(QStringLiteral("datenZahl(%1) enthaelt ein Komma: %2")
                                    .arg(v).arg(s)));
        }
    }

    void dieBeidenSindNichtDasselbe()
    {
        // Auf einem deutschen System muessen sie sich unterscheiden --
        // sonst waere die ganze Unterscheidung wirkungslos und niemand
        // wuerde es merken. Auf einem englischen sind sie gleich, und
        // das ist richtig.
        QLocale l = QLocale::system();
        if (l.decimalPoint() == QLatin1Char(',')) {
            QVERIFY(anzeigeZahl(1.3, 1) != datenZahl(1.3, 1));
        } else {
            QCOMPARE(anzeigeZahl(1.3, 1), datenZahl(1.3, 1));
        }
    }

    void nullStellenGehenAuch()
    {
        // Eine der umgestellten Stellen nutzt 0 Stellen ("%1 dB" im
        // QSO-Recorder).
        QVERIFY(!anzeigeZahl(12.0, 0).contains(QLocale::system().decimalPoint()));
        QCOMPARE(datenZahl(12.0, 0), QStringLiteral("12"));
    }
};

QTEST_MAIN(TstZahlentext)
#include "tst_zahlentext.moc"
