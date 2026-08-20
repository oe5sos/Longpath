// =================================================================
// tests/tst_applet_reorder.cpp  (NereusSDR)
// =================================================================
//
// Das Verschieben der Applets im rechten Stapel.
//
// Geprüft wird nicht, ob sich etwas hübsch anfühlt — das kann kein
// Test. Geprüft wird die eine Eigenschaft, an der so etwas scheitert:
// dass die sichtbare Reihenfolge und die Liste, die gespeichert wird,
// dasselbe sagen. Laufen die beiden auseinander, richtet man sein
// Fenster ein und bekommt nach dem Neustart ein anderes.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QtTest>
#include <QSignalSpy>

#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletWidget.h"

using namespace Longpath;

namespace {

/// Das kleinste Applet, das die Basisklasse zulässt.
class StubApplet : public AppletWidget {
public:
    // Ohne RadioModel. Die Basisklasse baut daraus nur Titelleiste und
    // Regler; für die Reihenfolge im Stapel spielt sie keine Rolle, und
    // ein halbes Funkgerät aufzubauen, um zwei Widgets zu tauschen,
    // wäre der Grund, warum solche Tests sonst nicht geschrieben werden.
    explicit StubApplet(QString id)
        : AppletWidget(nullptr), m_id(std::move(id)) {}
    QString appletId() const override { return m_id; }
    QString appletTitle() const override { return m_id.toUpper(); }
    void syncFromModel() override {}

private:
    QString m_id;
};

QStringList idsOf(const AppletPanelWidget& panel)
{
    QStringList out;
    for (AppletWidget* a : panel.applets()) {
        if (a) { out << a->appletId(); }
    }
    return out;
}

} // namespace

class TestAppletReorder : public QObject
{
    Q_OBJECT

private slots:
    void addingKeepsTheOrderTheyCameIn()
    {
        AppletPanelWidget panel;
        panel.addApplet(new StubApplet(QStringLiteral("rx")));
        panel.addApplet(new StubApplet(QStringLiteral("tx")));
        panel.addApplet(new StubApplet(QStringLiteral("eq")));
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("rx"),
                                            QStringLiteral("tx"),
                                            QStringLiteral("eq")}));
    }

    void movingToTheFront()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("tx"));
        auto* eq = new StubApplet(QStringLiteral("eq"));
        panel.addApplet(rx);
        panel.addApplet(tx);
        panel.addApplet(eq);

        QVERIFY(panel.moveApplet(eq, 0));
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("eq"),
                                            QStringLiteral("rx"),
                                            QStringLiteral("tx")}));
    }

    void movingToTheBack()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.addApplet(new StubApplet(QStringLiteral("tx")));
        panel.addApplet(new StubApplet(QStringLiteral("eq")));

        QVERIFY(panel.moveApplet(rx, 2));
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("tx"),
                                            QStringLiteral("eq"),
                                            QStringLiteral("rx")}));
    }

    // ── Der Zug, der zu weit geht ────────────────────────────────────
    //
    // Hinter dem letzten Applet steht im Layout die Dehnung. Landet ein
    // Widget dahinter, klebt es unten fest, während alle anderen oben
    // zusammenrücken — und herausziehen kann man es nicht mehr, weil es
    // dann schon außerhalb der Reihe liegt.
    void draggingPastTheEndStopsAtTheEnd()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.addApplet(new StubApplet(QStringLiteral("tx")));

        panel.moveApplet(rx, 99);
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("tx"),
                                            QStringLiteral("rx")}));

        panel.moveApplet(rx, -5);
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("rx"),
                                            QStringLiteral("tx")}));
    }

    void movingToWhereItAlreadyIsChangesNothing()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        panel.addApplet(rx);
        panel.addApplet(new StubApplet(QStringLiteral("tx")));

        QVERIFY(!panel.moveApplet(rx, 0));
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("rx"),
                                            QStringLiteral("tx")}));
    }

    void anUnknownAppletIsRefused()
    {
        AppletPanelWidget panel;
        panel.addApplet(new StubApplet(QStringLiteral("rx")));
        StubApplet stranger(QStringLiteral("fremd"));
        QVERIFY(!panel.moveApplet(&stranger, 0));
        QVERIFY(!panel.moveApplet(nullptr, 0));
    }

    // ── Was nach einem Update passiert ───────────────────────────────
    void restoringAnOrderThatMissesOneKeepsIt()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("tx"));
        auto* neu = new StubApplet(QStringLiteral("neu"));
        panel.addApplet(rx);
        panel.addApplet(tx);
        panel.addApplet(neu);

        // Die gespeicherte Liste stammt von vor dem Update und kennt
        // „neu" nicht. Es darf trotzdem nicht verschwinden.
        panel.setAppletOrder({tx, rx});
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("tx"),
                                            QStringLiteral("rx"),
                                            QStringLiteral("neu")}));
    }

    void restoringIgnoresStrangers()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("tx"));
        panel.addApplet(rx);
        panel.addApplet(tx);

        StubApplet stranger(QStringLiteral("weg"));
        panel.setAppletOrder({&stranger, tx, rx});
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("tx"),
                                            QStringLiteral("rx")}));
    }

    // setAppletOrder ist das Herstellen einer gespeicherten Anordnung,
    // kein Zug des Betreibers. Würde es das Signal auslösen, schriebe
    // der Empfänger beim Start dieselbe Liste zurück, die er gerade
    // gelesen hat — harmlos, bis eine Kennung fehlt und die Liste sich
    // dabei still verkürzt.
    void restoringDoesNotAnnounceAReorder()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("tx"));
        panel.addApplet(rx);
        panel.addApplet(tx);

        QSignalSpy spy(&panel, &AppletPanelWidget::appletsReordered);
        panel.setAppletOrder({tx, rx});
        QCOMPARE(spy.count(), 0);
    }

    void hidingDoesNotChangeTheOrder()
    {
        AppletPanelWidget panel;
        auto* rx = new StubApplet(QStringLiteral("rx"));
        auto* tx = new StubApplet(QStringLiteral("tx"));
        panel.addApplet(rx);
        panel.addApplet(tx);

        panel.setAppletVisible(rx, false);
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("rx"),
                                            QStringLiteral("tx")}));
        panel.setAppletVisible(rx, true);
        QCOMPARE(idsOf(panel), QStringList({QStringLiteral("rx"),
                                            QStringLiteral("tx")}));
    }
};

QTEST_MAIN(TestAppletReorder)
#include "tst_applet_reorder.moc"
