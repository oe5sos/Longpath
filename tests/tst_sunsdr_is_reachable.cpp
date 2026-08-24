// SPDX-License-Identifier: GPL-3.0-or-later
//
// Der SunSDR ist ueber die Oberflaeche ERREICHBAR.
//
// 2026-08-24. TciClient (Schritt 1) und der Ton-Haken in AudioEngine
// (Schritt 2a) sind fertig und einzeln geprueft, aber genau das war
// beim KiwiSDR auch schon der Fall, als er "vollstaendig gebaut und
// vollstaendig UNERREICHBAR" war (siehe tst_kiwi_is_reachable.cpp) --
// Protokoll, Verwaltung, Ton standen, es gab nur keinen Weg in der
// Oberflaeche hinein. Dieselbe Pruefung fuer denselben Fehlertyp, hier
// fuer SunSDR.
//
// Wie beim Kiwi-Vorbild: das Menue, nicht die API.

#include <QtTest>
#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "gui/MainWindow.h"

using namespace Longpath;

namespace {

// Zeichengetreu aus tst_kiwi_is_reachable.cpp -- derselbe Grund gilt
// unveraendert: auf macOS ist die Menueleiste nativ, QMenu-Objekte
// haengen als Kinder am Fenster, nicht an ihr.
QStringList allEntries(QWidget* root)
{
    QStringList out;
    for (QMenu* m : root->findChildren<QMenu*>()) {
        for (QAction* a : m->actions()) {
            if (!a->text().isEmpty()) { out << a->text(); }
        }
    }
    return out;
}

} // namespace

class TstSunSdrIsReachable : public QObject
{
    Q_OBJECT

private slots:
    void dasMenueFuehrtZumSunSdr()
    {
        auto* mw = new MainWindow();
        MainWindow& w = *mw;
        w.resize(1200, 800);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w, 15000));

        const QStringList entries = allEntries(&w);
        QVERIFY2(!entries.isEmpty(),
                 "Es wurde ueberhaupt kein Menueeintrag gefunden — "
                 "dann prueft der Rest dieser Pruefung nichts");
        const QString all = entries.join(QStringLiteral(" | "));

        // Der Sammelpunkt.
        QVERIFY2(all.contains(QStringLiteral("SunSDR")), qPrintable(all));

        // Und die beiden Wege hinein. Ein Menue, das nur den Namen
        // traegt, ist dieselbe Sackgasse wie gar keines.
        QVERIFY2(all.contains(QStringLiteral("Verbinden")), qPrintable(all));
        QVERIFY2(all.contains(QStringLiteral("Trennen")), qPrintable(all));

        qInfo().noquote() << "gefunden:"
                          << entries.filter(QStringLiteral("SunSDR")).join(", ")
                          << "/"
                          << entries.filter(QStringLiteral("Verbinden")).join(", ")
                          << "/"
                          << entries.filter(QStringLiteral("Trennen")).join(", ");
        mw->close();
    }
};

QTEST_MAIN(TstSunSdrIsReachable)
#include "tst_sunsdr_is_reachable.moc"
