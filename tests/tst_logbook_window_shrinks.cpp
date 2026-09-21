// =================================================================
// tests/tst_logbook_window_shrinks.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. Betreiber 2026-09-21: "Logbuch laesst sich
// nicht sehr verkleinern" -- gemessen 1107 x 519 px Mindestgroesse, weil
// Knopf- und Filterzeile als QHBoxLayout die Summe ihrer Elemente
// verlangten. Jetzt FlowLayout: umbrechend, Mindestbreite = breitestes
// Element. Dazu das FlowLayout selbst: Umbruch, Hoehe je Breite,
// Mindestgroesse, Platzhalter ohne Wirkung.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QPushButton>
#include <QTemporaryDir>

#include "gui/LogbookWindow.h"
#include "gui/widgets/FlowLayout.h"

using namespace Longpath;

class TstLogbookWindowShrinks : public QObject {
    Q_OBJECT

private slots:
    void flowLayoutWrapsAndReportsItsHeight()
    {
        QWidget host;
        auto* flow = new FlowLayout(&host, 0, 4, 4);
        QList<QPushButton*> buttons;
        for (int i = 0; i < 5; ++i) {
            auto* b = new QPushButton(QStringLiteral("B%1").arg(i), &host);
            b->setFixedSize(100, 20);
            flow->addWidget(b);
            buttons << b;
        }
        flow->addItem(new QSpacerItem(0, 0, QSizePolicy::Expanding, QSizePolicy::Minimum));   // Platzhalter: keine Wirkung
        QCOMPARE(flow->count(), 6);
        // Mindestbreite ist ein Element, nicht die Summe.
        QCOMPARE(flow->minimumSize().width(), 100);
        // Eine Zeile bei genuegend Breite, zwei Zeilen bei 260 px
        // (2 x 100 + 4 passen, das dritte nicht), drei Zeilen bei 210.
        QCOMPARE(flow->heightForWidth(600), 20);
        QCOMPARE(flow->heightForWidth(260), 20 + 4 + 20 + 4 + 20);
        QCOMPARE(flow->heightForWidth(210), 20 + 4 + 20 + 4 + 20);
        QCOMPARE(flow->heightForWidth(100), 5 * 20 + 4 * 4);
        host.resize(260, 100);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        // Zeile 1: B0 B1; Zeile 2: B2 B3; Zeile 3: B4.
        QCOMPARE(buttons[0]->geometry().topLeft(), QPoint(0, 0));
        QCOMPARE(buttons[1]->geometry().topLeft(), QPoint(104, 0));
        QCOMPARE(buttons[2]->geometry().topLeft(), QPoint(0, 24));
        QCOMPARE(buttons[4]->geometry().topLeft(), QPoint(0, 48));
    }

    void logbookWindowShrinksWellBelowTheOldMinimum()
    {
        QTemporaryDir dir;
        LogbookWindow w(dir.filePath(QStringLiteral("log.adi")));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        // Vorher 1107 px; jetzt bestimmt der Splitter (Tabelle + Detail).
        QVERIFY2(w.minimumSizeHint().width() < 500,
                 qPrintable(QStringLiteral("Mindestbreite %1").arg(w.minimumSizeHint().width())));
        w.resize(520, 420);
        QTest::qWait(50);
        QCOMPARE(w.width(), 520);
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(w.grab().save(grabDir + QStringLiteral("/longpath-grab-LogbookWindow-520.png")));
            w.resize(1240, 620);
            QTest::qWait(50);
            QVERIFY(w.grab().save(grabDir + QStringLiteral("/longpath-grab-LogbookWindow-1240.png")));
            w.resize(520, 420);
            QTest::qWait(50);
        }
        // Kein Element ragt aus dem Fenster -- die Zeilen sind umgebrochen,
        // nicht abgeschnitten.
        for (QWidget* c : w.findChildren<QWidget*>()) {
            if (!c->isVisible() || c->parentWidget() != &w) { continue; }
            QVERIFY2(c->geometry().right() <= w.width(),
                     qPrintable(QStringLiteral("%1 ragt rechts hinaus (%2 > %3)")
                                    .arg(QString::fromLatin1(c->metaObject()->className()))
                                    .arg(c->geometry().right()).arg(w.width())));
        }
    }
};

QTEST_MAIN(TstLogbookWindowShrinks)
#include "tst_logbook_window_shrinks.moc"
