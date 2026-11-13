// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbuch_karte_pfeil.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Pfeil „↗" NEBEN dem 3D-Knopf in der eingebetteten Logbuch-Karte
// (Live-Befund 2026-11-13: „neben dem 3d ist ein pfeil, der funktioniert
// auch nicht").
//
// Warum ein eigener Lauf, obwohl tst_logbook_durchgang das Herausloesen
// schon kennt: dort steht
//
//     emit map->popOutRequested();
//
// Der Prueflauf sendet das Signal SELBST. Ob der Knopf es ueberhaupt
// aussendet, hat damit nie jemand gemessen -- genau die Luecke, durch die
// ein Knopf faellt, der unsichtbar, abgeschaltet, verdeckt oder gar nicht
// verdrahtet ist. Dieselbe Klasse wie der 3D-Knopf nebenan (2026-11-10)
// und wie „Eigene Bewegung ist kein Ziehen": ein Prueflauf, der die
// Bedienung umgeht, belegt die Bedienung nicht.
//
// Hier klickt die Maus auf den echten Knopf im echten Logbuchfenster.

#include <QtTest>
#include <QLayout>

#include "core/AppSettings.h"
#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"

#include <QApplication>
#include <QDir>
#include <QPushButton>
#include <QTableWidget>
#include <QTemporaryDir>
#include "FensterFrist.h"

using namespace Longpath;

namespace {

QString writeAdif(const QDir& dir)
{
    const QString path = dir.filePath(QStringLiteral("log.adi"));
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) { return {}; }
    f.write("Longpath test\n<EOH>\n"
            "<CALL:5>K1ABC <QSO_DATE:8>20260902 <TIME_ON:6>143710 <BAND:3>20m <MODE:3>SSB "
            "<GRIDSQUARE:4>FN42 <MY_GRIDSQUARE:6>JN67UT <EOR>\n"
            "<CALL:6>DL1XYZ <QSO_DATE:8>20261013 <TIME_ON:6>081500 <BAND:2>2m <MODE:3>FT8 "
            "<GRIDSQUARE:6>JO62QM <MY_GRIDSQUARE:6>JN67UT <EOR>\n");
    f.close();
    return path;
}

// Der Pfeil traegt keinen Namen, nur sein Zeichen -- gesucht wird er
// darum in der Leiste der eingebetteten Karte.
QPushButton* pfeilIn(QsoMapWindow* karte)
{
    for (QPushButton* b : karte->findChildren<QPushButton*>()) {
        if (b->text() == QString::fromUtf8("↗")) { return b; }
    }
    return nullptr;
}

// Jedes eigene Kartenfenster ausser der eingebetteten Spalte.
QsoMapWindow* eigenesFenster(QWidget* unter, QsoMapWindow* ausser)
{
    for (QsoMapWindow* m : unter->findChildren<QsoMapWindow*>()) {
        if (m != ausser && m->isWindow() && m->isVisible()) { return m; }
    }
    return nullptr;
}

} // namespace

class TstLogbuchKartePfeil : public QObject { Q_OBJECT
private slots:
    void init()
    {
        AppSettings::instance().setValue(QStringLiteral("LogbookMode"),
                                        QStringLiteral("Log"));
    }

    // ── Der Pfeil ist da, sichtbar, bedienbar und frei ───────────────
    //
    // Vier Fragen, die ein „tut nichts" erklaeren koennen, bevor man im
    // Verhalten sucht: gibt es ihn, sieht man ihn, kann man ihn
    // druecken, und liegt nichts darueber.
    void pfeil_ist_erreichbar()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY2(karte, "keine eingebettete Karte im Logbuch");
        QTRY_VERIFY(karte->isVisible());

        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY2(pfeil, "kein Pfeil-Knopf in der Kartenleiste");
        QTRY_VERIFY2(pfeil->isVisible(),
                     "der Pfeil ist unsichtbar — setEmbedded() blendet ihn ein");
        QVERIFY2(pfeil->isEnabled(), "der Pfeil ist abgeschaltet");

        // Und nichts liegt darueber: ein verdeckter Knopf nimmt den Klick
        // nicht an, sieht aber bedienbar aus.
        QWidget* getroffen = w.childAt(pfeil->mapTo(&w, pfeil->rect().center()));
        QVERIFY2(getroffen == pfeil, "ueber dem Pfeil liegt etwas");
    }

    // ── Ein echter Mausklick oeffnet das eigene Kartenfenster ────────
    void mausklick_oeffnet_das_eigene_fenster()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        QTRY_VERIFY(karte->isVisible());
        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY(pfeil);
        QTRY_VERIFY(pfeil->isVisible());

        QVERIFY2(!eigenesFenster(&w, karte),
                 "ein eigenes Kartenfenster stand schon offen");

        // Die Maus, nicht das Signal.
        QTest::mouseClick(pfeil, Qt::LeftButton, Qt::NoModifier,
                          pfeil->rect().center());
        QCoreApplication::processEvents();

        QsoMapWindow* gross = nullptr;
        QTRY_VERIFY2((gross = eigenesFenster(&w, karte)) != nullptr,
                     "der Klick auf den Pfeil oeffnete kein eigenes Fenster");
        QVERIFY(QTest::qWaitForWindowExposed(gross, kFensterFrist));

        // Und die eingebettete Spalte bleibt, wo sie war -- der Pfeil
        // loest heraus, er verschiebt nicht.
        QVERIFY2(karte->isVisible(), "die Kartenspalte verschwand beim Herausloesen");

        gross->close();
        QCoreApplication::processEvents();
    }

    // ── Zweimal hintereinander ───────────────────────────────────────
    //
    // Das Fenster wird beim ersten Mal angelegt und danach
    // wiederverwendet; ein zweiter Druck nach dem Schliessen muss es
    // wieder zeigen.
    void zweiter_druck_zeigt_es_wieder()
    {
        QTemporaryDir dir;
        LogbookWindow w(writeAdif(QDir(dir.path())));
        w.resize(1400, 760);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, kFensterFrist));

        QsoMapWindow* karte = w.mapPanelForTest();
        QVERIFY(karte);
        QTRY_VERIFY(karte->isVisible());
        QPushButton* pfeil = pfeilIn(karte);
        QVERIFY(pfeil);
        QTRY_VERIFY(pfeil->isVisible());

        for (int runde = 1; runde <= 2; ++runde) {
            QTest::mouseClick(pfeil, Qt::LeftButton, Qt::NoModifier,
                              pfeil->rect().center());
            QCoreApplication::processEvents();
            QsoMapWindow* gross = nullptr;
            QTRY_VERIFY2((gross = eigenesFenster(&w, karte)) != nullptr,
                         qPrintable(QStringLiteral("Runde %1: kein eigenes Fenster")
                                        .arg(runde)));
            QVERIFY(QTest::qWaitForWindowExposed(gross, kFensterFrist));
            gross->close();
            QCoreApplication::processEvents();
            QTRY_VERIFY(!eigenesFenster(&w, karte));
        }
    }
};

QTEST_MAIN(TstLogbuchKartePfeil)
#include "tst_logbuch_karte_pfeil.moc"
