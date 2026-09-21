// SPDX-License-Identifier: GPL-3.0-or-later
// tests/tst_logbook_toolbar_wraps.cpp  (Longpath)
//
// Longpath-original. No Thetis port.
// no-port-check: Longpath-original.
//
// Der Betreiber am 2026-09-21: „wenn ich logbook im longpath verkleinern
// will, geht das nicht komplett, hier duerfte eine mindestgroesse
// eingestellt sein, sodass die taskleiste oben nicht veraendert wird.
// diese kann aber anstatt 1-zeilig auch 2-zeilig werden."
//
// Elf Elemente in einem QHBoxLayout addierten sich zu ueber 1000 px
// Mindestbreite. Jetzt: FlowLayout — bei voller Breite eine Zeile, bei
// halber Breite zwei oder drei, und das Fenster laesst sich auf die
// Breite des breitesten Knopfs plus Detailpaneel ziehen.

#include <QtTest>

#include "gui/LogbookWindow.h"
#include "gui/QsoMapWindow.h"
#include "gui/widgets/GibsTileLayer.h"
#include "gui/widgets/FlowLayout.h"

#include <QDir>
#include <QLineEdit>
#include <QPushButton>
#include <QTemporaryDir>

using namespace Longpath;

class TstLogbookToolbarWraps : public QObject { Q_OBJECT
private slots:
    void theLayoutWrapsAndStretches()
    {
        QWidget host;
        auto* flow = new FlowLayout(&host, 6, 6);
        auto* search = new QLineEdit(&host);
        search->setMinimumWidth(200);
        flow->addWidget(search);
        for (int i = 0; i < 6; ++i) {
            auto* b = new QPushButton(QStringLiteral("Button %1").arg(i), &host);
            b->setFixedSize(90, 24);
            flow->addWidget(b);
        }
        // Breit: eine Zeile, das Suchfeld nimmt den Rest.
        host.resize(1200, 60);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        QCoreApplication::processEvents();
        QCOMPARE(flow->rowsLaidOut(), 1);
        QVERIFY2(search->width() > 500, qPrintable(QString::number(search->width())));

        // Halb so breit: mehr Zeilen, und die Hoehe waechst mit.
        const int h1 = flow->heightForWidth(1200);
        const int h2 = flow->heightForWidth(400);
        QVERIFY2(h2 > h1, qPrintable(QStringLiteral("%1 vs %2").arg(h1).arg(h2)));
        host.resize(400, 200);
        QCoreApplication::processEvents();
        QVERIFY2(flow->rowsLaidOut() >= 2, qPrintable(QString::number(flow->rowsLaidOut())));

        // Die Mindestbreite ist das breiteste Element, nicht die Summe.
        QVERIFY2(flow->minimumSize().width() <= 200 + 1,
                 qPrintable(QString::number(flow->minimumSize().width())));
    }

    void theLogbookWindowShrinksToHalf()
    {
        QTemporaryDir dir;
        LogbookWindow w(dir.path() + QStringLiteral("/log.adi"));
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const int minW = w.minimumSizeHint().width();
        qInfo() << "Logbuchfenster: Mindestbreite" << minW << "Voreinstellung" << w.width();
        QVERIFY2(minW < 700, qPrintable(QStringLiteral("Mindestbreite %1 px — die Leiste bricht nicht um").arg(minW)));

        w.resize(640, 500);
        QCoreApplication::processEvents();
        QTRY_VERIFY_WITH_TIMEOUT(w.width() <= 660, 3000);
        // Beide Leisten sind jetzt mehrzeilig.
        int wrapped = 0;
        for (FlowLayout* f : w.findChildren<FlowLayout*>()) {
            if (f->rowsLaidOut() >= 2) { ++wrapped; }
        }
        // findChildren findet Layouts nur ueber ihr Parent-Layout; zur
        // Sicherheit ueber die Widgets: das Suchfeld ist schmaler als das
        // Fenster minus alle Knoepfe — d. h. es teilt sich die Zeile nicht
        // mehr mit allen.
        QLineEdit* search = nullptr;
        for (QLineEdit* e : w.findChildren<QLineEdit*>()) {
            if (e->placeholderText().startsWith(QLatin1String("Search"))) { search = e; }
        }
        QVERIFY(search);
        qInfo() << "Suchfeld" << search->width() << "px breit, umgebrochene Leisten:" << wrapped;
        QVERIFY(search->width() >= 200);

        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QDir().mkpath(grabDir);
            QTest::qWait(200);
            w.grab().save(grabDir + QStringLiteral("/logbook-640.png"));
            w.resize(1240, 620);
            QTest::qWait(300);
            w.grab().save(grabDir + QStringLiteral("/logbook-1240.png"));
            w.resize(420, 500);
            QTest::qWait(300);
            w.grab().save(grabDir + QStringLiteral("/logbook-420.png"));
        }
    }

    // Dieselbe Leiste im Kartenfenster: zwanzig Elemente, vorher ueber
    // 1300 px Mindestbreite.
    void theMapWindowShrinksToo()
    {
        QsoMapWindow w;
        w.imagery()->setNetworkEnabled(false);
        w.show();
        QVERIFY(QTest::qWaitForWindowExposed(&w));
        const int minW = w.minimumSizeHint().width();
        qInfo() << "Kartenfenster: Mindestbreite" << minW;
        QVERIFY2(minW < 700, qPrintable(QString::number(minW)));
        w.resize(640, 480);
        QCoreApplication::processEvents();
        QTRY_VERIFY_WITH_TIMEOUT(w.width() <= 660, 3000);
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QTest::qWait(300);
            w.grab().save(grabDir + QStringLiteral("/map-640.png"));
        }
    }
};

QTEST_MAIN(TstLogbookToolbarWraps)
#include "tst_logbook_toolbar_wraps.moc"
