// no-port-check: Longpath-original render test for Setup > CAT & Network >
// TCP/IP CAT (2026-10-11).
//
// The page carries two live server groups now (rigctld and Kenwood CAT).
// This test builds it, renders it, and checks the things a reader of the
// page depends on: both groups present, every control reachable, and the
// status lines saying what they are told to say. It also writes the
// rendered PNG to the test's temp dir so the look can be eyeballed
// without starting the whole application -- the house-style rule is that
// a feature is not done until it comes out in the house language, and
// that has to be seen at least once.

#include <QtTest/QtTest>
#include <QtGui/QPixmap>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpinBox>

#include "gui/setup/CatNetworkSetupPages.h"

using namespace Longpath;

class TestCatPageRender : public QObject {
    Q_OBJECT

private slots:
    void page_has_both_server_groups()
    {
        CatTcpIpPage page;
        const auto groups = page.findChildren<QGroupBox*>();
        QStringList titles;
        for (const QGroupBox* g : groups) {
            titles << g->title();
        }
        QVERIFY2(titles.contains(QStringLiteral("rigctld (Hamlib network CAT)")),
                 qPrintable(titles.join(QStringLiteral(" | "))));
        QVERIFY2(titles.contains(QStringLiteral("CAT (Kenwood dialect)")),
                 qPrintable(titles.join(QStringLiteral(" | "))));
    }

    void every_control_is_live_not_a_placeholder()
    {
        // The page used to be four disabled widgets with "NYI" tooltips.
        // Nothing on it may be disabled now, or the operator cannot reach
        // the server the rest of this work built.
        CatTcpIpPage page;
        for (const QCheckBox* c : page.findChildren<QCheckBox*>()) {
            QVERIFY2(c->isEnabled(), qPrintable(c->text()));
            QVERIFY2(!c->toolTip().contains(QStringLiteral("NYI")),
                     qPrintable(c->text()));
        }
        for (const QLineEdit* e : page.findChildren<QLineEdit*>()) {
            QVERIFY(e->isEnabled());
            QVERIFY(!e->toolTip().contains(QStringLiteral("NYI")));
        }
        for (const QSpinBox* s : page.findChildren<QSpinBox*>()) {
            QVERIFY(s->isEnabled());
            QVERIFY(!s->toolTip().contains(QStringLiteral("NYI")));
        }
    }

    void both_transmit_gates_start_closed()
    {
        // What the page shows must match what the servers do: both gates
        // default to off.
        CatTcpIpPage page;
        int gateBoxes = 0;
        for (const QCheckBox* c : page.findChildren<QCheckBox*>()) {
            if (c->text().contains(QStringLiteral("transmit"), Qt::CaseInsensitive)) {
                ++gateBoxes;
                QVERIFY2(!c->isChecked(), qPrintable(c->text()));
            }
        }
        QCOMPARE(gateBoxes, 2);
    }

    void default_ports_are_the_expected_ones()
    {
        CatTcpIpPage page;
        QList<int> ports;
        for (const QSpinBox* s : page.findChildren<QSpinBox*>()) {
            ports << s->value();
        }
        // 4532 is what every Hamlib client tries first; 13013 is Thetis's
        // own TCP CAT port.
        QVERIFY2(ports.contains(4532), qPrintable(QStringLiteral("ports: %1")
                     .arg(ports.size())));
        QVERIFY2(ports.contains(13013), qPrintable(QStringLiteral("ports: %1")
                     .arg(ports.size())));
    }

    void status_lines_report_what_they_are_told()
    {
        CatTcpIpPage page;
        page.setRigctldStatus(true, 4532, 0);
        page.setCatStatus(true, 13013, 2);

        QStringList texts;
        for (const QLabel* l : page.findChildren<QLabel*>()) {
            texts << l->text();
        }
        QVERIFY2(texts.contains(QStringLiteral("Status: listening on port 4532, no client")),
                 qPrintable(texts.join(QStringLiteral(" | "))));
        QVERIFY2(texts.contains(QStringLiteral("Status: listening on port 13013, 2 clients")),
                 qPrintable(texts.join(QStringLiteral(" | "))));

        page.setRigctldStatus(false, 0, 0);
        texts.clear();
        for (const QLabel* l : page.findChildren<QLabel*>()) {
            texts << l->text();
        }
        QVERIFY(texts.contains(QStringLiteral("Status: stopped")));
    }

    void the_groups_sit_at_the_top_not_at_the_bottom()
    {
        // SetupPage puts a stretch at the end of its content layout and
        // inserts sections BEFORE it. A plain addWidget lands AFTER the
        // stretch, which pushes everything to the bottom edge -- how this
        // page looked from the day it was created. The first group must
        // start in the upper third of the page.
        CatTcpIpPage page;
        page.resize(520, 620);
        page.show();
        QTest::qWait(100);

        const auto groups = page.findChildren<QGroupBox*>();
        QVERIFY(!groups.isEmpty());
        int topMost = page.height();
        for (const QGroupBox* g : groups) {
            topMost = std::min(topMost, g->mapTo(&page, QPoint(0, 0)).y());
        }
        QVERIFY2(topMost < page.height() / 3,
                 qPrintable(QStringLiteral("first group starts at y=%1 of %2")
                                .arg(topMost).arg(page.height())));
    }

    void page_renders_and_the_png_is_kept_for_eyeballing()
    {
        CatTcpIpPage page;
        page.setRigctldStatus(true, 4532, 1);
        page.setCatStatus(true, 13013, 0);
        page.resize(520, 620);
        page.show();
        QTest::qWait(100);

        const QPixmap shot = page.grab();
        QVERIFY(!shot.isNull());
        QVERIFY(shot.width() > 200);
        QVERIFY(shot.height() > 200);

        const QString out = QDir(QStandardPaths::writableLocation(
                                     QStandardPaths::TempLocation))
                                .filePath(QStringLiteral("longpath-cat-page.png"));
        QVERIFY2(shot.save(out, "PNG"), qPrintable(out));
        qInfo() << "rendered page written to" << out;
    }
};

QTEST_MAIN(TestCatPageRender)
#include "tst_cat_page_render.moc"
