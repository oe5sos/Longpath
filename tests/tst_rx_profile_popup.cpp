// =================================================================
// tests/tst_rx_profile_popup.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. The receive-profile sheet (RxProfilePopup)
// over a RadioModel's manager and a slice:
//   * the list shows the profiles, the active one in bold
//   * Load / Save / Delete need a selected row, Save As a typed name
//   * Save As adds a profile from the slice and clears the field;
//     Enter in the field does the same
//   * Load writes the selected profile into the slice
//   * Delete asks inline (Yes removes, No keeps)
//   * the RX applet's gear opens it (hasExtendedSettings)
//   * the sheet renders to a PNG when LONGPATH_GRAB_DIR is set
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/RxProfileManager.h"
#include "core/WdspTypes.h"
#include "gui/applets/RxApplet.h"
#include "gui/widgets/RxProfilePopup.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

struct Rig {
    QTemporaryDir dir;
    AppSettings settings;
    RxProfileManager manager;
    SliceModel slice;

    Rig()
        : settings(dir.path() + QStringLiteral("/Longpath.settings"))
        , manager(settings)
    {
        slice.setAgcMode(AGCMode::Fast);
        slice.setAgcThreshold(-33);
        slice.setActiveNr(NrSlot::NR2);
        slice.setNbMode(NbMode::NB);
    }
};

} // namespace

class TestRxProfilePopup : public QObject {
    Q_OBJECT

private slots:
    void listShowsProfilesAndButtonsFollowTheSelection()
    {
        Rig rig;
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Ragchew"), &rig.slice));
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Contest"), &rig.slice));

        RxProfilePopup popup(&rig.manager, &rig.slice);
        QListWidget* list = popup.listForTest();
        QCOMPARE(list->count(), 2);
        QCOMPARE(list->item(0)->text(), QStringLiteral("Ragchew"));
        QCOMPARE(list->item(1)->text(), QStringLiteral("Contest"));
        QVERIFY(!list->item(0)->font().bold());
        QVERIFY(list->item(1)->font().bold());           // the active one
        // The active profile is preselected.
        QCOMPARE(list->currentItem()->text(), QStringLiteral("Contest"));
        QVERIFY(popup.loadButtonForTest()->isEnabled());
        QVERIFY(popup.saveButtonForTest()->isEnabled());
        QVERIFY(popup.deleteButtonForTest()->isEnabled());
        QVERIFY(!popup.saveAsButtonForTest()->isEnabled());   // no name typed
        QVERIFY(!popup.confirmRowForTest()->isVisible());

        list->clearSelection();
        QVERIFY(!popup.loadButtonForTest()->isEnabled());
        QVERIFY(!popup.saveButtonForTest()->isEnabled());
        QVERIFY(!popup.deleteButtonForTest()->isEnabled());

        popup.nameEditForTest()->setText(QStringLiteral("  "));
        QVERIFY(!popup.saveAsButtonForTest()->isEnabled());
        popup.nameEditForTest()->setText(QStringLiteral("DX"));
        QVERIFY(popup.saveAsButtonForTest()->isEnabled());

        // Without a slice nothing that reads or writes one is enabled.
        popup.setSlice(nullptr);
        list->setCurrentRow(0);
        QVERIFY(!popup.loadButtonForTest()->isEnabled());
        QVERIFY(!popup.saveButtonForTest()->isEnabled());
        QVERIFY(popup.deleteButtonForTest()->isEnabled());
        QVERIFY(!popup.saveAsButtonForTest()->isEnabled());
    }

    void saveAsAddsAndLoadApplies()
    {
        Rig rig;
        RxProfilePopup popup(&rig.manager, &rig.slice);
        QCOMPARE(popup.listForTest()->count(), 0);

        popup.nameEditForTest()->setText(QStringLiteral("Contest"));
        popup.saveAsNew();
        QCOMPARE(rig.manager.profileNames(), QStringList{QStringLiteral("Contest")});
        QCOMPARE(popup.listForTest()->count(), 1);
        QVERIFY(popup.nameEditForTest()->text().isEmpty());
        QCOMPARE(popup.listForTest()->currentItem()->text(), QStringLiteral("Contest"));

        // Enter in the field is Save As too.
        rig.slice.setAgcMode(AGCMode::Slow);
        popup.nameEditForTest()->setText(QStringLiteral("Ragchew"));
        QTest::keyClick(popup.nameEditForTest(), Qt::Key_Return);
        QCOMPARE(rig.manager.profileNames(),
                 (QStringList{QStringLiteral("Contest"), QStringLiteral("Ragchew")}));

        // Load "Contest" into the slice: Fast again.
        popup.listForTest()->setCurrentRow(0);
        QCOMPARE(popup.listForTest()->currentItem()->text(), QStringLiteral("Contest"));
        popup.loadSelected();
        QCOMPARE(rig.slice.agcMode(), AGCMode::Fast);
        QCOMPARE(rig.manager.activeProfileName(), QStringLiteral("Contest"));

        // Save overwrites the selected one with the slice of now.
        rig.slice.setAgcThreshold(-50);
        popup.saveSelected();
        QCOMPARE(rig.manager.profileValues(QStringLiteral("Contest")).value(QStringLiteral("agcThreshold")),
                 QStringLiteral("-50"));
    }

    void deleteAsksInline()
    {
        Rig rig;
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Old"), &rig.slice));
        RxProfilePopup popup(&rig.manager, &rig.slice);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));
        popup.listForTest()->setCurrentRow(0);

        popup.deleteSelected();
        QVERIFY(popup.confirmRowForTest()->isVisible());
        QCOMPARE(rig.manager.profileNames().size(), 1);           // nothing yet

        // No: keeps it, row folds away.
        for (QPushButton* b : popup.confirmRowForTest()->findChildren<QPushButton*>()) {
            if (b->text() == QStringLiteral("No")) { b->click(); }
        }
        QVERIFY(!popup.confirmRowForTest()->isVisible());
        QCOMPARE(rig.manager.profileNames().size(), 1);

        // Yes: gone.
        popup.deleteSelected();
        QVERIFY(popup.confirmRowForTest()->isVisible());
        popup.confirmYesForTest()->click();
        QVERIFY(!popup.confirmRowForTest()->isVisible());
        QVERIFY(rig.manager.profileNames().isEmpty());
        QCOMPARE(popup.listForTest()->count(), 0);
        popup.close();
    }

    void rxAppletGearOpensTheSheet()
    {
        SliceModel slice;
        RxApplet applet(&slice, nullptr);
        QVERIFY(applet.hasExtendedSettings());
        // Without a RadioModel there is no manager: nothing opens, no crash.
        applet.openExtendedSettings();
        QVERIFY(applet.findChildren<RxProfilePopup*>().isEmpty());
    }

    void sheetRendersToPng()
    {
        Rig rig;
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Ragchew"), &rig.slice));
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Contest CW"), &rig.slice));
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Weak-signal SSB"), &rig.slice));
        rig.manager.applyProfile(QStringLiteral("Contest CW"), &rig.slice);

        RxProfilePopup popup(&rig.manager, &rig.slice);
        popup.show();
        QVERIFY(QTest::qWaitForWindowExposed(&popup));
        popup.adjustSize();
        const QPixmap pm = popup.grab();
        QVERIFY(!pm.isNull());
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(pm.save(grabDir + QStringLiteral("/longpath-grab-RxProfilePopup.png")));
        }
        popup.close();
    }
};

QTEST_MAIN(TestRxProfilePopup)
#include "tst_rx_profile_popup.moc"
