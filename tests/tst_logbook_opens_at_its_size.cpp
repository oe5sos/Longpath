// SPDX-License-Identifier: GPL-3.0-or-later
// no-port-check: Longpath-original regression test.
//
// Das Logbuch geht in der Groesse auf, die vorgesehen war (2026-09-26).
//
// Beim ersten Zeigen kannten die umbrechenden Leisten ihre wirkliche
// Breite noch nicht: die Knopfzeilen der Karte standen fuer einen
// Durchgang in einer schmalen Spalte, brauchten viele Zeilen, und das
// Logbuch meldete bis zu 931 px Mindesthoehe. Qt zog das Fenster darauf
// auf; einen Takt spaeter war die Mindesthoehe wieder unter 600 -- aber
// ein Fenster schrumpft nie von selbst zurueck. Jede gespeicherte Hoehe
// ging verloren (live in der Sandbox: 1000x700 gespeichert, 1000x817
// aufgegangen).

#include <QtTest/QtTest>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "gui/LogbookWindow.h"
#include "FensterFrist.h"

using namespace Longpath;

class TstLogbookOpensAtItsSize : public QObject {
    Q_OBJECT

    QTemporaryDir m_dir;
    QString m_log;

private slots:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        QVERIFY(m_dir.isValid());
        m_log = m_dir.filePath(QStringLiteral("logbook.adi"));
        // Zwoelf Baender und acht Betriebsarten wie im Betreiber-Log:
        // genug Knoepfe, dass die Zeile in einer schmalen Spalte umbricht.
        const char* bands[] = {"160m", "80m", "40m", "30m", "20m", "17m",
                               "15m", "12m", "10m", "6m", "2m", "70cm"};
        const char* modes[] = {"SSB", "CW", "FT8", "FT4", "RTTY", "PSK", "AM", "FM"};
        QByteArray out = "t\n<EOH>\n";
        auto f = [&out](const QByteArray& n, const QByteArray& v) {
            out += '<' + n + ':' + QByteArray::number(v.size()) + '>' + v + ' ';
        };
        for (int i = 0; i < 400; ++i) {
            f("CALL", "DL" + QByteArray::number(i) + "AB");
            f("QSO_DATE", QDate(2025, 1, 1).addDays(i).toString("yyyyMMdd").toUtf8());
            f("TIME_ON", "120000");
            f("BAND", bands[i % 12]);
            f("MODE", modes[(i / 12) % 8]);
            f("GRIDSQUARE", "JO" + QByteArray::number(40 + i % 50));
            out += "<EOR>\n";
        }
        QFile file(m_log);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(out);
        AppSettings::instance().setValue(QStringLiteral("LogbookShowMap"), QStringLiteral("True"));
        AppSettings::instance().setValue(QStringLiteral("LogbookShowStats"), QStringLiteral("True"));
    }

    void theIntendedSizeHoldsAfterTheFirstShow()
    {
        LogbookWindow w(m_log);
        w.resize(1000, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        // Nicht eine feste Zeit absitzen: wie lange das Einschwingen
        // braucht, haengt an der Auslastung der Maschine. Im Durchgang
        // vom 2026-10-16 war die Hoehe nach 1300 ms noch 780 — und das
        // war schon die Behebung wert, aber die feste Wartezeit hat
        // sie zum Wuerfelspiel gemacht.
        QTRY_COMPARE_WITH_TIMEOUT(w.height(), 700, 5000);
        qInfo() << "Mindestgroesse" << w.minimumSizeHint() << "Groesse" << w.size();
        // Voraussetzung: der Inhalt passt wirklich in 700 px Hoehe.
        QVERIFY(w.minimumSizeHint().height() <= 700);
        // Die Breite haelt, soweit der Inhalt es zulaesst (eine starre
        // Knopfzeile der Karte erzwang bis #112 ueber 1400 px).
        QCOMPARE(w.width(), qMax(1000, w.minimumSizeHint().width()));
    }

    // Wer das Fenster in der ersten Sekunde kleiner zieht, behaelt es so:
    // die Nachtakte nehmen nur zurueck, was Qt aufgezogen hat
    // (2026-09-27; vorher stellte der naechste Takt die vorgesehene
    // Groesse wieder her -- auf der CI in tst_logbook_toolbar_wraps).
    void aShrinkInTheFirstSecondStays()
    {
        LogbookWindow w(m_log);
        w.resize(1000, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QTest::qWait(10);

        // ── Erst einschwingen lassen (2026-10-15) ────────────────────
        //
        // Hier wurde die Mindestgroesse 10 ms nach show() genommen. Zu
        // frueh: auf diesem Mac meldet das Fenster da noch 561x720
        // (die Werkzeugleiste ist noch umgebrochen) und erst beim
        // naechsten Takt die wirklichen 561x617 — gemessen in
        // 50-ms-Schritten.
        //
        // Aus dem fruehen Wert wurde `smaller` mit der HOEHE 720
        // gebildet. Damit verlangte der Lauf am Ende eine
        // VERGROESSERUNG von 700 auf 720 und prueste das Gegenteil
        // dessen, was in seinem Namen steht. Auf den CI-Laeufern
        // schwingt es schneller ein, darum fiel es dort nie auf.
        QTRY_VERIFY_WITH_TIMEOUT(w.minimumSizeHint().height() <= 650, 1000);
        const QSize min = w.minimumSizeHint();
        const QSize smaller(qMax(min.width(), 900), qMax(min.height(), 650));
        // UND, nicht ODER: ist eine der beiden Kanten nicht wirklich
        // kleiner, verkleinert dieser Lauf in dieser Richtung nichts
        // und bezeugt sie auch nicht.
        QVERIFY2(smaller.width() < 1000 && smaller.height() < 700,
                 qPrintable(QStringLiteral("Inhalt braucht die ganze "
                                           "vorgesehene Groesse: %1x%2")
                                .arg(smaller.width()).arg(smaller.height())));
        // Erst verkleinern, wenn die wirkliche Mindestgroesse es zulaesst:
        // unter Last auf cocoa war sie nach 10 ms noch 698 hoch, und Qt
        // klemmte das resize sofort ab (der Wunsch kam nie als Ereignis
        // an). Noch in der Einschwingzeit -- die Nachtakte laufen noch.
        QTRY_VERIFY_WITH_TIMEOUT(w.minimumSize().width() <= smaller.width()
                                 && w.minimumSize().height() <= smaller.height(), 250);
        w.resize(smaller);
        QTest::qWait(1300);
        QCOMPARE(w.size(), smaller.expandedTo(w.minimumSizeHint()));
    }

    // Zieht Qt das Fenster NACH einer Verkleinerung noch einmal auf,
    // gehen die Nachtakte auf die Verkleinerung zurueck -- nicht nur auf
    // die vorgesehene Groesse (2026-09-27).
    void aGrowthAfterAShrinkGoesBackToTheShrink()
    {
        LogbookWindow w(m_log);
        w.resize(1000, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        // Eine echte Verkleinerung (700 -> 650), sobald die wirkliche
        // Mindestgroesse sie zulaesst; minimumSizeHint() springt beim
        // Einschwingen (offscreen kurz 820) und taugt hier nicht.
        const QSize smaller(900, 650);
        QTRY_VERIFY_WITH_TIMEOUT(w.minimumSize().width() <= smaller.width()
                                 && w.minimumSize().height() <= smaller.height(), 250);
        w.resize(smaller);
        QCOMPARE(w.size(), smaller);
        // Wie eine voruebergehende Mindesthoehe beim Einschwingen: Qt zieht
        // auf, und von selbst schrumpft das Fenster nie zurueck.
        w.setMinimumHeight(smaller.height() + 48);
        QCOMPARE(w.height(), smaller.height() + 48);
        w.setMinimumHeight(0);
        QTRY_COMPARE_WITH_TIMEOUT(w.size(), smaller, 5000);
    }

    // ── Der Fall, der am 2026-10-16 umfiel ──────────────────────────
    //
    // Bis dahin korrigierte LogbookWindow::setVisible an drei festen
    // Punkten (50, 300, 1000 ms) und nahm dabei nur zurueck bis
    // m_settleTarget.expandedTo(minimumSizeHint()). War die
    // Mindesthoehe beim LETZTEN davon noch nicht eingeschwungen, war
    // diese Grenze selbst der aufgezogene Wert: die Korrektur
    // unterblieb, und danach lief kein Takt mehr.
    //
    // Im vollen Durchgang stand das Fenster dadurch unter -j8-Last auf
    // 780 statt 700; allein gefahren war derselbe Lauf gruen. Hier
    // wird dieselbe Lage ohne Last hergestellt: eine Mindesthoehe, die
    // erst NACH dem letzten alten Takt zurueckgenommen wird. Mit den
    // drei festen Punkten bleibt das Fenster aufgezogen, mit dem
    // laufenden Takt geht es zurueck.
    void einSpaetEingeschwungenerInhaltWirdZurueckgenommen()
    {
        LogbookWindow w(m_log);
        w.resize(1000, 700);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));

        // Wie eine Leiste, die lange umgebrochen bleibt: Qt zieht auf,
        // und von selbst schrumpft das Fenster nie zurueck.
        w.setMinimumHeight(780);
        QTRY_COMPARE_WITH_TIMEOUT(w.height(), 780, 1000);

        // Erst JETZT geben wir sie frei — nach dem letzten alten Takt
        // (1000 ms), aber vor der Obergrenze des laufenden (3000 ms).
        QTest::qWait(1500);
        w.setMinimumHeight(0);

        QTRY_COMPARE_WITH_TIMEOUT(w.height(), 700, 3000);
        qInfo() << "nach spaeter Freigabe:" << w.size();
    }

    // Kleiner als der Inhalt braucht, wird es nie -- die Leisten duerfen
    // sich nicht ueberdecken.
    void neverSmallerThanTheContent()
    {
        LogbookWindow w(m_log);
        w.resize(1000, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));
        QTest::qWait(1300);
        QVERIFY(w.height() >= w.minimumSizeHint().height());
    }
};

QTEST_MAIN(TstLogbookOpensAtItsSize)
#include "tst_logbook_opens_at_its_size.moc"
