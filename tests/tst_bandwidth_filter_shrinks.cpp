// SPDX-License-Identifier: GPL-3.0-or-later
//
// Das Bandfilter-Fenster laesst sich KLEINER ziehen, ohne dass der
// Inhalt abgeschnitten wird.
//
// Der Betreiber am 2026-08-22: "weiters verändert sich das fenster
// bandfilter und der inhalt nicht automatisch, sobald ich die größe
// verändere" — und gleich nachgeschoben: "vor allem verkleinert!"
//
// Genau so war es. Die Bedienzeile stand in EINER Reihe: drei
// Wortmarken, drei Zahlenfelder mit setFixedWidth(92), dazu VAR1,
// VAR2 und Zentrieren. Zusammen ein harter Boden von rund 700
// Punkten. Wer schmaler zog, bekam einen Rollbalken und sah den Rest
// nicht mehr — der Inhalt passte sich nicht an, er verschwand.

#include <QtTest>
#include <QLabel>

#include "gui/applets/AppletFloatingWindow.h"
#include "gui/applets/BandwidthFilterApplet.h"
#include "models/RadioModel.h"

using namespace Longpath;

class TstBandwidthFilterShrinks : public QObject
{
    Q_OBJECT

private slots:
    void itFitsIntoANarrowWindow()
    {
        RadioModel model;
        BandwidthFilterApplet applet(&model);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));

        const int floorW = applet.minimumSizeHint().width();
        qInfo() << "Untergrenze der Breite:" << floorW;

        // 700 war der alte Boden. Wer ein Applet neben den Panadapter
        // stellen will, hat selten mehr als 400 Punkte uebrig.
        QVERIFY2(floorW <= 400,
                 qPrintable(QStringLiteral(
                     "Das Bandfilter verlangt mindestens %1 Punkte "
                     "Breite — darunter wird der Inhalt abgeschnitten "
                     "statt verkleinert").arg(floorW)));
    }

    void theFloatingWindowShrinksToo()
    {
        // Der Betreiber am 2026-08-22, nach der ersten Behebung: "das
        // fenster des bandfilter passt noch immer nicht."
        //
        // Das Applet allein zu verkleinern reicht nicht — geprueft
        // wird, was der Bediener anfasst: das SCHWEBEFENSTER.
        RadioModel model;
        auto* applet = new BandwidthFilterApplet(&model);
        AppletFloatingWindow win(applet, QStringLiteral("test"), 0);
        win.show();
        QVERIFY(QTest::qWaitForWindowExposed(&win));
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }

        win.resize(340, 240);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(50);
        qInfo() << "Fenster nach dem Verkleinern:"
                << win.width() << "x" << win.height();

        QVERIFY2(win.width() <= 380,
                 qPrintable(QStringLiteral(
                     "Das Bandfilter-FENSTER bleibt bei %1 Punkten "
                     "stehen, obwohl 340 verlangt waren")
                     .arg(win.width())));
    }

    void theButtonGroupsWrapFirst()
    {
        // Wenn es eng wird, rutschen die Knopfgruppen (Memory, Span) in
        // eine zweite Reihe — die Zahlenfelder bleiben, und ihre
        // Versalzeilen ("LOW", "WIDTH", "HIGH") bleiben mit ihnen: neun
        // Punkte hoch, sie kosten keine Breite. Bis zum 2026-09-17
        // wichen stattdessen die fetten Wortmarken NEBEN den Feldern;
        // seit dem Umbau auf "Glas & Tiefe" steht jede Bedienung in
        // einer benannten Gruppe (Hausstil Regel 1), und eine
        // Ueberschrift, die bei Enge verschwindet, waere keine.
        RadioModel model;
        BandwidthFilterApplet applet(&model);
        applet.resize(800, 260);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        for (int i = 0; i < 4; ++i) { QCoreApplication::processEvents(); }

        auto lowLabel = [&]() -> QLabel* {
            for (QLabel* l : applet.findChildren<QLabel*>()) {
                if (l->text() == QStringLiteral("Low")) { return l; }
            }
            return nullptr;
        };
        auto* memory = applet.findChild<QWidget*>(QStringLiteral("bwFilterMemoryGroup"));
        auto* lowBox = applet.findChild<QWidget*>(QStringLiteral("bwFilterLow"));
        QVERIFY(lowLabel());
        QVERIFY(memory);
        QVERIFY(lowBox);
        QVERIFY2(lowLabel()->isVisible(), "Breit fehlt die Ueberschrift schon");
        // Breit: Memory steht in derselben Reihe wie die Felder.
        const int yWide = memory->mapTo(&applet, QPoint(0, 0)).y();
        const int yBoxWide = lowBox->mapTo(&applet, QPoint(0, 0)).y();
        QVERIFY2(qAbs(yWide - yBoxWide) < 30, "Breit steht Memory nicht neben den Feldern");

        applet.resize(360, 260);
        for (int i = 0; i < 6; ++i) { QCoreApplication::processEvents(); }
        QTest::qWait(50);
        qInfo() << "Breite nach dem Verkleinern:" << applet.width();
        QVERIFY2(lowLabel()->isVisible(),
                 "Eng ist die Ueberschrift weg — eine Gruppe ohne Namen");
        const int yNarrow = memory->mapTo(&applet, QPoint(0, 0)).y();
        const int yBoxNarrow = lowBox->mapTo(&applet, QPoint(0, 0)).y();
        QVERIFY2(yNarrow > yBoxNarrow + 20,
                 "Eng steht Memory noch neben den Feldern und draengt "
                 "sie aus dem Bild");
    }
};

QTEST_MAIN(TstBandwidthFilterShrinks)
#include "tst_bandwidth_filter_shrinks.moc"
