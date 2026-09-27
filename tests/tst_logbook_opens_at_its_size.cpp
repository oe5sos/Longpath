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
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::qWait(1300);
        qInfo() << "Mindestgroesse" << w.minimumSizeHint() << "Groesse" << w.size();
        // Voraussetzung: der Inhalt passt wirklich in 700 px Hoehe.
        QVERIFY(w.minimumSizeHint().height() <= 700);
        QCOMPARE(w.height(), 700);
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
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::qWait(10);
        const QSize min = w.minimumSizeHint();
        const QSize smaller(qMax(min.width(), 900), qMax(min.height(), 650));
        QVERIFY2(smaller.width() < 1000 || smaller.height() < 700,
                 "Inhalt braucht die ganze vorgesehene Groesse");
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
        QVERIFY(QTest::qWaitForWindowExposed(&w));
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
        QTest::qWait(1300);
        QCOMPARE(w.size(), smaller);
    }

    // Kleiner als der Inhalt braucht, wird es nie -- die Leisten duerfen
    // sich nicht ueberdecken.
    void neverSmallerThanTheContent()
    {
        LogbookWindow w(m_log);
        w.resize(1000, 200);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        QTest::qWait(1300);
        QVERIFY(w.height() >= w.minimumSizeHint().height());
    }
};

QTEST_MAIN(TstLogbookOpensAtItsSize)
#include "tst_logbook_opens_at_its_size.moc"
