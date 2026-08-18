// Verify AppletPanelWidget::setAppletVisible toggles wrapper visibility
// while keeping the applet's position in the stack layout intact.

#include <QApplication>
#include <QtTest/QtTest>
#include <QVBoxLayout>
#include <QScrollArea>
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/GridCellWidget.h"
#include "gui/applets/AppletWidget.h"

using namespace NereusSDR;

namespace {
class FakeApplet : public AppletWidget {
public:
    explicit FakeApplet(const QString& title)
        : AppletWidget(nullptr), m_title(title) {}
    QString appletId() const override { return m_title; }
    QString appletTitle() const override { return m_title; }
    void syncFromModel() override {}
private:
    QString m_title;
};
} // anon

class TstAppletPanelSetVisible : public QObject {
    Q_OBJECT
private slots:
    void hides_wrapper_without_changing_layout_index();
    void shows_wrapper_again_at_same_index();
    void null_applet_is_noop();
    void unknown_applet_is_noop();
};

// ── Zugriff ueber die Zusicherung, nicht ueber die Bauart ───────────
//
// Diese beiden Helfer griffen bis 2026-08-18 auf das QVBoxLayout im
// Rollbereich zu und lasen dort den Index ab. Mit Schritt 1 des freien
// Rasters ist daraus ein AppletGrid geworden (dieselbe Reihenfolge,
// dasselbe Bild) — und der Test wurde rot, ohne dass sich an dem, was
// er zusichert, etwas geaendert haette.
//
// Er prueft jetzt, was er immer meinte: das Feld eines Applets ist
// versteckt, und seine STELLE hat sich dabei nicht geaendert. Die
// Stelle kommt aus AppletPanelWidget::appletPosition — der oeffentlichen
// Antwort auf genau diese Frage.

static QWidget* wrapperFor(AppletPanelWidget& panel, QWidget* applet)
{
    for (auto* cell : panel.findChildren<NereusSDR::GridCellWidget*>()) {
        if (cell && cell->isAncestorOf(applet)) { return cell; }
    }
    return nullptr;
}

void TstAppletPanelSetVisible::hides_wrapper_without_changing_layout_index()
{
    AppletPanelWidget panel;
    auto* a = new FakeApplet(QStringLiteral("A"));
    auto* b = new FakeApplet(QStringLiteral("B"));
    panel.addApplet(a);
    panel.addApplet(b);

    QWidget* wrapperB = wrapperFor(panel, b);
    QVERIFY(wrapperB);
    const int idxBefore = panel.appletPosition(b);

    panel.setAppletVisible(b, false);

    QVERIFY(!wrapperB->isVisible());
    QCOMPARE(panel.appletPosition(b), idxBefore);
}

void TstAppletPanelSetVisible::shows_wrapper_again_at_same_index()
{
    AppletPanelWidget panel;
    panel.show();
    auto* a = new FakeApplet(QStringLiteral("A"));
    auto* b = new FakeApplet(QStringLiteral("B"));
    panel.addApplet(a);
    panel.addApplet(b);

    QWidget* wrapperB = wrapperFor(panel, b);
    QVERIFY(wrapperB);
    const int idxBefore = panel.appletPosition(b);

    panel.setAppletVisible(b, false);
    panel.setAppletVisible(b, true);

    QVERIFY(wrapperB->isVisible());
    QCOMPARE(panel.appletPosition(b), idxBefore);
}

void TstAppletPanelSetVisible::null_applet_is_noop()
{
    AppletPanelWidget panel;
    panel.setAppletVisible(nullptr, false);
    panel.setAppletVisible(nullptr, true);
}

void TstAppletPanelSetVisible::unknown_applet_is_noop()
{
    AppletPanelWidget panel;
    auto* orphan = new FakeApplet(QStringLiteral("Orphan"));
    panel.setAppletVisible(orphan, false);
    delete orphan;
}

QTEST_MAIN(TstAppletPanelSetVisible)
#include "tst_applet_panel_set_visible.moc"
