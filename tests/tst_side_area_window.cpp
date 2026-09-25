// no-port-check: Longpath-original test, no Thetis logic.
//
// SideAreaWindow: der Seitenbereich mit der Leiste am rechten Rand
// (Betreiber 2026-09-25, Variante 2 „Symbolleiste am Rand").

#include <QtTest/QtTest>
#include <QApplication>
#include <QLabel>
#include <QPointer>
#include <QSignalSpy>

#include "gui/SideAreaWindow.h"

using namespace Longpath;

class TestSideAreaWindow : public QObject {
    Q_OBJECT
private slots:
    void firstPageBecomesActive()
    {
        SideAreaWindow w;
        w.addPage(QStringLiteral("WinRotorLog"), QStringLiteral("Rotor / Log"),
                  new QLabel(QStringLiteral("rotor")));
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"),
                  new QLabel(QStringLiteral("rx")));
        QCOMPARE(w.activeId(), QStringLiteral("WinRotorLog"));
        QCOMPARE(w.pageIds(), (QStringList{QStringLiteral("WinRotorLog"),
                                           QStringLiteral("Rx")}));
        QCOMPARE(w.titleForTest(), QStringLiteral("Rotor / Log"));
    }

    void samePageTwiceIsIgnored()
    {
        SideAreaWindow w;
        auto* a = new QLabel;
        auto* b = new QLabel;
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"), a);
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"), b);
        QCOMPARE(w.pageIds().size(), 1);
        QCOMPARE(w.pageContent(QStringLiteral("Rx")), a);
        delete b;
    }

    // Klick auf ein anderes Symbol zeigt dessen Seite; Klick auf das
    // aktive klappt zu, nochmal auf.
    void railClickSwitchesThenToggles()
    {
        SideAreaWindow w;
        w.setGeometry(1000, 100, 330, 600);
        w.addPage(QStringLiteral("WinRotorLog"), QStringLiteral("Rotor / Log"), new QLabel);
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"), new QLabel);
        QSignalSpy active(&w, &SideAreaWindow::activeChanged);

        w.railClicked(QStringLiteral("Rx"));
        QCOMPARE(w.activeId(), QStringLiteral("Rx"));
        QCOMPARE(active.count(), 1);
        QVERIFY(!w.isCollapsed());

        w.railClicked(QStringLiteral("Rx"));
        QVERIFY(w.isCollapsed());
        w.railClicked(QStringLiteral("Rx"));
        QVERIFY(!w.isCollapsed());
    }

    // Zuklappen: nur die Leiste bleibt, der RECHTE Rand steht still, und
    // die freie Breite wird gemeldet. Aufklappen gibt sie zurück.
    void collapseKeepsRightEdgeAndReportsDelta()
    {
        SideAreaWindow w;
        w.setGeometry(1000, 100, 330, 600);
        w.addPage(QStringLiteral("WinRotorLog"), QStringLiteral("Rotor / Log"), new QLabel);
        const int right = w.geometry().x() + w.geometry().width();
        QSignalSpy spy(&w, &SideAreaWindow::collapsedChanged);

        w.setCollapsed(true);
        QCOMPARE(w.geometry().width(), SideAreaWindow::kRailW);
        QCOMPARE(w.geometry().x() + w.geometry().width(), right);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(spy.at(0).at(1).toInt(), 330 - SideAreaWindow::kRailW);
        QCOMPARE(w.expandedWidth(), 330);

        w.setCollapsed(false);
        QCOMPARE(w.geometry().width(), 330);
        QCOMPARE(w.geometry().x() + w.geometry().width(), right);
        QCOMPARE(spy.at(1).at(1).toInt(), 330 - SideAreaWindow::kRailW);
    }

    // Eine Seite auswählen klappt einen zugeklappten Bereich auf — „das
    // aktive geht immer auf".
    void choosingAPageExpands()
    {
        SideAreaWindow w;
        w.setGeometry(1000, 100, 330, 600);
        w.addPage(QStringLiteral("WinRotorLog"), QStringLiteral("Rotor / Log"), new QLabel);
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"), new QLabel);
        w.setCollapsed(true);
        w.railClicked(QStringLiteral("Rx"));
        QVERIFY(!w.isCollapsed());
        QCOMPARE(w.activeId(), QStringLiteral("Rx"));
    }

    // Herausnehmen gibt den Inhalt unbeschädigt und ohne Elternteil
    // zurück, und die nächste Seite wird aktiv.
    void takePageReturnsContentIntact()
    {
        SideAreaWindow w;
        auto* rotor = new QLabel(QStringLiteral("rotor"));
        auto* rx = new QLabel(QStringLiteral("rx"));
        w.addPage(QStringLiteral("WinRotorLog"), QStringLiteral("Rotor / Log"), rotor);
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"), rx);
        QPointer<QLabel> guard(rotor);

        QWidget* back = w.takePage(QStringLiteral("WinRotorLog"));
        QCOMPARE(back, static_cast<QWidget*>(rotor));
        QVERIFY(back->parent() == nullptr);
        QCOMPARE(w.activeId(), QStringLiteral("Rx"));
        QCOMPARE(w.pageIds(), QStringList{QStringLiteral("Rx")});

        // Auch nach dem Aufräumen der Hülle (deleteLater) lebt es noch.
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        QVERIFY(!guard.isNull());
        QCOMPARE(guard->text(), QStringLiteral("rotor"));
        delete back;

        QVERIFY(w.takePage(QStringLiteral("gibt-es-nicht")) == nullptr);
    }

    void dropHighlightToggles()
    {
        SideAreaWindow w;
        QVERIFY(w.acceptDrops());
        QVERIFY(!w.dropHighlight());
        w.setDropHighlight(true);
        QVERIFY(w.dropHighlight());
        w.setDropHighlight(false);
        QVERIFY(!w.dropHighlight());
    }

    // Für das Profil zählt die AUFGEKLAPPTE Lage — auch wenn gerade zu.
    void capturedStateIsTheExpandedGeometry()
    {
        SideAreaWindow w;
        w.setGeometry(1000, 100, 330, 600);
        w.addPage(QStringLiteral("WinRotorLog"), QStringLiteral("Rotor / Log"), new QLabel);
        w.addPage(QStringLiteral("Rx"), QStringLiteral("RX"), new QLabel);
        w.setActive(QStringLiteral("Rx"));
        w.setCollapsed(true);

        const QVariantMap s = w.captureState();
        QCOMPARE(s.value(QStringLiteral("pages")).toStringList(),
                 (QStringList{QStringLiteral("WinRotorLog"), QStringLiteral("Rx")}));
        QCOMPARE(s.value(QStringLiteral("active")).toString(), QStringLiteral("Rx"));
        QCOMPARE(s.value(QStringLiteral("collapsed")).toBool(), true);
        QCOMPARE(s.value(QStringLiteral("x")).toInt(), 1000);
        QCOMPARE(s.value(QStringLiteral("w")).toInt(), 330);
        QCOMPARE(s.value(QStringLiteral("h")).toInt(), 600);
    }
};

QTEST_MAIN(TestSideAreaWindow)
#include "tst_side_area_window.moc"
