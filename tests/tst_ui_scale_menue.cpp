// Das Menü "Darstellungsgröße" im echten Hauptfenster.
//
// `tst_ui_scale` prueft die Regeln dahinter -- Stufen, Grenzen,
// Speichern, die Kopplung zu main.cpp. Hier geht es um das, was vorher
// tatsaechlich kaputt war: **das Menue war tot.** Jeder Eintrag stand
// auf `setEnabled(false)` mit dem Tooltip "NYI — Phase X", die
// Darstellungsgroesse liess sich also ueberhaupt nicht einstellen; sie
// haette von Hand in die Einstellungsdatei gemusst.
//
// Ein Prueffall, der nur die Regeln prueft, haette das nie bemerkt:
// `UiScale::store()` haette es gegeben, und niemand haette es gerufen.
// Darum baut dieser Fall das echte Hauptfenster auf und sieht im Menue
// nach, so wie eine Hand es tut.

#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QtTest>

#include "core/UiScale.h"
#include "gui/MainWindow.h"

using namespace Longpath;

class TstUiScaleMenue : public QObject
{
    Q_OBJECT

private slots:
    void dasMenueIstDaUndAnklickbar();
};

void TstUiScaleMenue::dasMenueIstDaUndAnklickbar()
{
    // Bewusst nicht abgeraeumt, aus demselben Grund wie in
    // tst_real_mainwindow_detach: MainWindow startet Arbeitsfaeden,
    // deren geordnetes Ende an einer laufenden Ereignisschleife haengt.
    auto* mwp = new MainWindow();
    MainWindow& mw = *mwp;
    mw.resize(1280, 800);
    mw.show();
    QVERIFY2(QTest::qWaitForWindowExposed(&mw),
             "das Hauptfenster muss ueberhaupt erscheinen");
    QTest::qWait(200);

    // NICHT `mw.menuBar()`: nach `buildMenuBar()` haengt Longpath die
    // Leiste in einen eigenen Titelstreifen um
    // (`m_titleBar->setMenuBar(menuBar())`, Phase 3O). `menuBar()`
    // liefert danach eine NEUE, leere Leiste -- der erste Versuch dieses
    // Falls fand darum null Menues. Die bestueckte Leiste ist die, die
    // irgendwo unter dem Fenster haengt.
    QMenuBar* leiste = nullptr;
    for (QMenuBar* mb : mw.findChildren<QMenuBar*>()) {
        if (mb && !mb->actions().isEmpty()) { leiste = mb; break; }
    }
    QVERIFY2(leiste, "es muss eine bestueckte Menueleiste geben");

    // Das Untermenue so suchen, wie man es im Menue sucht: ueber den
    // sichtbaren Namen.
    QMenu* groesse = nullptr;
    for (QMenu* m : leiste->findChildren<QMenu*>()) {
        if (m && m->title().contains(QStringLiteral("Darstellungsgröße"))) {
            groesse = m;
            break;
        }
    }
    QVERIFY2(groesse, "das Menue \"Darstellungsgröße\" fehlt");

    const QList<QAction*> eintraege = groesse->actions();
    QVERIFY2(!eintraege.isEmpty(), "das Menue ist leer");

    // Jede Stufe aus UiScale muss als Eintrag dastehen -- und zwar
    // ANKLICKBAR. Genau hier lag der Fehler.
    for (int pct : UiScale::steps()) {
        QAction* gefunden = nullptr;
        for (QAction* a : eintraege) {
            if (a && a->data().toInt() == pct) { gefunden = a; break; }
        }
        QVERIFY2(gefunden,
                 qPrintable(QStringLiteral("die Stufe %1 % fehlt im Menue").arg(pct)));
        QVERIFY2(gefunden->isEnabled(),
                 qPrintable(QStringLiteral("die Stufe %1 % ist grau -- genau so war "
                                           "das Menue vorher: da, aber tot").arg(pct)));
        QVERIFY2(gefunden->isCheckable(),
                 qPrintable(QStringLiteral("die Stufe %1 % laesst sich nicht "
                                           "anhaken").arg(pct)));
        QVERIFY2(!gefunden->toolTip().contains(QStringLiteral("NYI")),
                 qPrintable(QStringLiteral("die Stufe %1 % traegt noch den "
                                           "NYI-Hinweis").arg(pct)));
    }

    // Genau eine Stufe ist angehakt, und es ist die geltende.
    int angehakt = 0;
    int angehakteStufe = -1;
    for (QAction* a : eintraege) {
        if (a && a->isChecked()) { ++angehakt; angehakteStufe = a->data().toInt(); }
    }
    QCOMPARE(angehakt, 1);
    QCOMPARE(angehakteStufe, UiScale::current());
}

QTEST_MAIN(TstUiScaleMenue)
#include "tst_ui_scale_menue.moc"
