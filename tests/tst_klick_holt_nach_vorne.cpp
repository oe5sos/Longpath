// SPDX-License-Identifier: GPL-3.0-or-later
//
// „wir hatten vereinbart, dass jeder klick auf ein fenster diese in den
// vordergrund rueckt" (Betreiber, 2026-11-07).
//
// Ein Applet in der Spalte hat kein Fenster, das man heben koennte: die
// Spalte gehoert zum Hauptfenster, und das kommt nie ueber die schwebenden
// Fenster. Liegt es verdeckt, muss der Klick es als eigenes Fenster holen.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Claude Code).
// =================================================================

#include <QPushButton>
#include <QScrollArea>
#include <QSignalSpy>
#include <QtTest>

#include "core/AppSettings.h"
#include "gui/MainWindow.h"
#include "gui/applets/CatRigApplet.h"
#include "FensterFrist.h"

using namespace Longpath;

namespace {
// Die Spalte scrollt; weit unten waere das Feld gar nicht zu sehen.
void rolleInsBild(QWidget* feld)
{
    for (QWidget* w = feld->parentWidget(); w; w = w->parentWidget()) {
        if (auto* flaeche = qobject_cast<QScrollArea*>(w)) {
            flaeche->ensureWidgetVisible(feld);
            QCoreApplication::processEvents();
            return;
        }
    }
}

// Die Startfenster eines frischen Ordners stehen sonst davor.
void schliesseStartfenster(QWidget* fenster)
{
    const auto oben = QApplication::topLevelWidgets();
    for (QWidget* w : oben) {
        if (w != fenster) { w->hide(); }
    }
}
}  // namespace

class TstKlickHoltNachVorne : public QObject
{
    Q_OBJECT

private slots:
    // Ein freies Applet in der Spalte bleibt, und die Taste schaltet.
    void klick_auf_das_freie_feld_schaltet_einfach()
    {
        AppSettings::instance().setValue(QStringLiteral("AppletCatRigVisible"),
                                         QStringLiteral("True"));
        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, kFensterFrist));
        auto* feld = fenster->findChild<CatRigApplet*>();
        QVERIFY(feld);
        QTRY_VERIFY_WITH_TIMEOUT(feld->isVisible(), 5000);
        QCOMPARE(feld->window(), static_cast<QWidget*>(fenster));
        rolleInsBild(feld);
        auto* taste = feld->findChild<QPushButton*>(QStringLiteral("catRigAtt"));
        QVERIFY(taste);
        schliesseStartfenster(fenster);
        QSignalSpy gedrueckt(taste, &QPushButton::clicked);
        QTest::mouseClick(taste, Qt::LeftButton);
        QCOMPARE(gedrueckt.size(), 1);
        QCOMPARE(feld->window(), static_cast<QWidget*>(fenster));
        fenster->hide();   // Es wuerde sonst den naechsten Fall verdecken.
    }

    void klick_auf_das_verdeckte_feld_holt_es_nach_vorne()
    {
        AppSettings::instance().setValue(QStringLiteral("AppletCatRigVisible"),
                                         QStringLiteral("True"));
        auto* fenster = new MainWindow();   // bewusst nicht abgeraeumt
        fenster->resize(1280, 800);
        fenster->show();
        QVERIFY(QTest::qWaitForWindowExposed(fenster, kFensterFrist));

        auto* feld = fenster->findChild<CatRigApplet*>();
        QVERIFY(feld);
        QTRY_VERIFY_WITH_TIMEOUT(feld->isVisible(), 5000);
        QCOMPARE(feld->window(), static_cast<QWidget*>(fenster));

        rolleInsBild(feld);
        schliesseStartfenster(fenster);

        QWidget deckel;
        deckel.setGeometry(QRect(feld->mapToGlobal(QPoint(0, 0)), feld->size()));
        deckel.show();
        auto* taste = feld->findChild<QPushButton*>(QStringLiteral("catRigAtt"));
        QVERIFY(taste);
        QSignalSpy gedrueckt(taste, &QPushButton::clicked);

        QTest::mouseClick(taste, Qt::LeftButton);
        QTRY_VERIFY2_WITH_TIMEOUT(feld->window() != fenster,
                                  "der Klick liess das Feld unter dem Fenster", 3000);
        QVERIFY(feld->isVisible());
        QCOMPARE(gedrueckt.size(), 0);   // Der Klick holt, er schaltet nichts.
    }
};

QTEST_MAIN(TstKlickHoltNachVorne)
#include "tst_klick_holt_nach_vorne.moc"
