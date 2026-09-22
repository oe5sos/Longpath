// Die Applet-Spalte ist so breit wie ihr breitestes Feld -- und der
// Container darum herum uebernimmt das. Frisches Profil bei 1280
// Punkten, 2026-09-22: die Spalte stand auf ihren 260 (ein festes
// setMinimumWidth an Panel und Container, das Qt jeden Layout-Hinweis
// vergessen laesst), das Raster brauchte 317 (DvkApplet), und RX-"MUTE"
// samt TX-Knoepfen war rechts abgeschnitten. Jetzt liefert
// AppletPanelWidget::minimumSizeHint() das Maximum aus kAppletPanelW und
// Rasterbreite, ContainerWidget::refreshMinimumWidth() folgt ihm.
// no-port-check: Longpath-original.

#include <QApplication>
#include <QtTest/QtTest>

#include "gui/StyleConstants.h"
#include "gui/applets/AppletPanelWidget.h"
#include "gui/applets/AppletWidget.h"
#include "gui/containers/ContainerWidget.h"

using namespace Longpath;

namespace {

class WideApplet : public AppletWidget {
public:
    explicit WideApplet(const QString& title, int minWidth)
        : AppletWidget(nullptr), m_title(title)
    {
        setMinimumWidth(minWidth);
    }
    QString appletId() const override { return m_title; }
    QString appletTitle() const override { return m_title; }
    void syncFromModel() override {}
private:
    QString m_title;
};

} // namespace

class TstAppletPanelMinWidth : public QObject {
    Q_OBJECT
private slots:
    void empty_panel_keeps_the_260_floor();
    void a_wide_applet_widens_the_hint_and_removal_narrows_it_again();
    void a_hidden_applet_does_not_count();
    void the_container_follows_its_content();
};

void TstAppletPanelMinWidth::empty_panel_keeps_the_260_floor()
{
    AppletPanelWidget panel;
    QCOMPARE(panel.minimumSizeHint().width(), Style::kAppletPanelW);
    // Kein festes Minimum mehr -- sonst gaelte der Hinweis nicht.
    QCOMPARE(panel.minimumWidth(), 0);
}

void TstAppletPanelMinWidth::a_wide_applet_widens_the_hint_and_removal_narrows_it_again()
{
    AppletPanelWidget panel;
    auto* narrow = new WideApplet(QStringLiteral("schmal"), 100);
    auto* wide = new WideApplet(QStringLiteral("breit"), 400);
    panel.addApplet(narrow);
    QCOMPARE(panel.minimumSizeHint().width(), Style::kAppletPanelW);
    panel.addApplet(wide);
    // 400 plus Feldrahmen und der 8-px-Rollbalkenrand des Rasters.
    QVERIFY2(panel.minimumSizeHint().width() >= 400 + 8,
             qPrintable(QString::number(panel.minimumSizeHint().width())));
    panel.removeApplet(wide);
    delete wide;
    QCOMPARE(panel.minimumSizeHint().width(), Style::kAppletPanelW);
}

void TstAppletPanelMinWidth::a_hidden_applet_does_not_count()
{
    AppletPanelWidget panel;
    panel.show();
    auto* wide = new WideApplet(QStringLiteral("breit"), 400);
    panel.addApplet(wide);
    // Ein Feld, das in ein schon gezeigtes Panel kommt, wird vom Layout
    // erst mit dem naechsten Ereignis gezeigt (ShowToParent).
    QCoreApplication::processEvents();
    QVERIFY(panel.minimumSizeHint().width() >= 408);
    panel.setAppletVisible(wide, false);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCOMPARE(panel.minimumSizeHint().width(), Style::kAppletPanelW);
    panel.setAppletVisible(wide, true);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QVERIFY(panel.minimumSizeHint().width() >= 408);
}

void TstAppletPanelMinWidth::the_container_follows_its_content()
{
    ContainerWidget container;
    auto* panel = new AppletPanelWidget;
    container.setContent(panel);
    container.show();
    QCOMPARE(container.minimumWidth(), ContainerWidget::kMinContainerWidth);

    auto* wide = new WideApplet(QStringLiteral("breit"), 400);
    panel->addApplet(wide);
    // addApplet -> updateGeometry -> LayoutRequest am Container.
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    QVERIFY2(container.minimumWidth() >= 408, qPrintable(QString::number(container.minimumWidth())));
    QVERIFY(container.minimumWidth() >= panel->minimumSizeHint().width());

    panel->removeApplet(wide);
    delete wide;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
    QCoreApplication::processEvents();
    QCOMPARE(container.minimumWidth(), ContainerWidget::kMinContainerWidth);
}

QTEST_MAIN(TstAppletPanelMinWidth)
#include "tst_applet_panel_min_width.moc"
