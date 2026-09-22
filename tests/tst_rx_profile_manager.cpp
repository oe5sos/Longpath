// =================================================================
// tests/tst_rx_profile_manager.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. RxProfileManager against a temporary
// AppSettings file:
//   * every property in profileProperties() exists on SliceModel
//   * save captures a slice (bools, ints, doubles, enums as text);
//     apply writes them into another slice, enums included
//   * names are trimmed and lose their commas; empty is refused
//   * the manifest lists, rename moves the values, delete drops them
//     and clears the active name
//   * a value the profile lacks leaves the slice alone
//   * settings not in the list (frequency, mode, filter) are untouched
//
// =================================================================
// Modification history (Longpath):
//   2026-09-20 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QMetaProperty>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "core/AppSettings.h"
#include "core/RxProfileManager.h"
#include "core/WdspTypes.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {

struct Rig {
    QTemporaryDir dir;
    AppSettings settings;
    RxProfileManager manager;

    Rig()
        : settings(dir.path() + QStringLiteral("/Longpath.settings"))
        , manager(settings)
    {
    }
};

// A slice with recognisable, non-default receive settings.
void dressUp(SliceModel& s)
{
    s.setAgcMode(AGCMode::Fast);
    s.setAgcThreshold(-37);
    s.setAgcHang(250);
    s.setAutoAgcEnabled(true);
    s.setAutoAgcOffset(3.5);
    s.setActiveNr(NrSlot::NR2);
    s.setNr2Position(NrPosition::PostAgc);
    s.setNr2Post2Level(-25.0);
    s.setNr1Taps(128);
    s.setNbMode(NbMode::NB2);
    s.setNb1Threshold(45);
    s.setSnbEnabled(true);
    s.setAnfEnabled(true);
    s.setSsqlEnabled(true);
    s.setSsqlThresh(0.42);
    s.setBinauralEnabled(true);
    s.setApfTuneHz(650);
}

} // namespace

class TestRxProfileManager : public QObject {
    Q_OBJECT

private slots:
    void everyListedPropertyExists()
    {
        SliceModel slice;
        const QMetaObject* meta = slice.metaObject();
        QVERIFY(RxProfileManager::profileProperties().size() > 60);
        for (const QString& prop : RxProfileManager::profileProperties()) {
            const int idx = meta->indexOfProperty(prop.toLatin1().constData());
            QVERIFY2(idx >= 0, qPrintable(QStringLiteral("no SliceModel property: %1").arg(prop)));
            QVERIFY2(meta->property(idx).isWritable(),
                     qPrintable(QStringLiteral("not writable: %1").arg(prop)));
        }
        // Nothing that belongs to the frequency memories.
        for (const char* forbidden : {"frequency", "dspMode", "filterLow", "filterHigh",
                                      "afGain", "rfGain", "rxAntenna", "locked", "muted"}) {
            QVERIFY(!RxProfileManager::profileProperties().contains(QLatin1String(forbidden)));
        }
    }

    void saveCapturesAndApplyRestores()
    {
        Rig rig;
        SliceModel a;
        dressUp(a);
        a.setFrequency(7'100'000.0);

        QSignalSpy listSpy(&rig.manager, &RxProfileManager::profileListChanged);
        QSignalSpy activeSpy(&rig.manager, &RxProfileManager::activeProfileChanged);
        QVERIFY(rig.manager.saveProfile(QStringLiteral("  Contest, 40m "), &a));
        QCOMPARE(rig.manager.profileNames(), QStringList{QStringLiteral("Contest 40m")});
        QCOMPARE(rig.manager.activeProfileName(), QStringLiteral("Contest 40m"));
        QCOMPARE(listSpy.count(), 1);
        QCOMPARE(activeSpy.count(), 1);

        // Stored as text, the enums as their integer.
        const QHash<QString, QString> v = rig.manager.profileValues(QStringLiteral("Contest 40m"));
        QCOMPARE(v.size(), RxProfileManager::profileProperties().size());
        QCOMPARE(v.value(QStringLiteral("agcMode")), QString::number(int(AGCMode::Fast)));
        QCOMPARE(v.value(QStringLiteral("agcThreshold")), QStringLiteral("-37"));
        QCOMPARE(v.value(QStringLiteral("autoAgcEnabled")), QStringLiteral("True"));
        QCOMPARE(v.value(QStringLiteral("autoAgcOffset")), QStringLiteral("3.5"));
        QCOMPARE(v.value(QStringLiteral("activeNr")), QString::number(int(NrSlot::NR2)));
        QCOMPARE(v.value(QStringLiteral("nr2Position")), QString::number(int(NrPosition::PostAgc)));
        QCOMPARE(v.value(QStringLiteral("nbMode")), QString::number(int(NbMode::NB2)));
        QCOMPARE(v.value(QStringLiteral("ssqlThresh")), QStringLiteral("0.42"));
        QVERIFY(!v.contains(QStringLiteral("frequency")));

        // Into a fresh slice: the receive settings arrive, the rest stays.
        SliceModel b;
        b.setFrequency(14'200'000.0);
        b.setDspMode(DSPMode::USB);
        QVERIFY(rig.manager.applyProfile(QStringLiteral("Contest 40m"), &b));
        QCOMPARE(b.agcMode(), AGCMode::Fast);
        QCOMPARE(b.agcThreshold(), -37);
        QCOMPARE(b.agcHang(), 250);
        QVERIFY(b.autoAgcEnabled());
        QCOMPARE(b.autoAgcOffset(), 3.5);
        QCOMPARE(b.activeNr(), NrSlot::NR2);
        QCOMPARE(b.nr2Position(), NrPosition::PostAgc);
        QCOMPARE(b.nr2Post2Level(), -25.0);
        QCOMPARE(b.nr1Taps(), 128);
        QCOMPARE(b.nbMode(), NbMode::NB2);
        QCOMPARE(b.nb1Threshold(), 45);
        QVERIFY(b.snbEnabled());
        QVERIFY(b.anfEnabled());
        QVERIFY(b.ssqlEnabled());
        QCOMPARE(b.ssqlThresh(), 0.42);
        QVERIFY(b.binauralEnabled());
        QCOMPARE(b.apfTuneHz(), 650);
        QCOMPARE(b.frequency(), 14'200'000.0);
        QCOMPARE(b.dspMode(), DSPMode::USB);

        // Every property round-trips exactly.
        const QMetaObject* meta = a.metaObject();
        for (const QString& prop : RxProfileManager::profileProperties()) {
            const QMetaProperty mp = meta->property(meta->indexOfProperty(prop.toLatin1().constData()));
            QVERIFY2(RxProfileManager::valueToText(mp.read(&a)) == RxProfileManager::valueToText(mp.read(&b)),
                     qPrintable(QStringLiteral("%1 differs: %2 vs %3").arg(prop,
                         RxProfileManager::valueToText(mp.read(&a)),
                         RxProfileManager::valueToText(mp.read(&b)))));
        }
    }

    void refusalsAndUnknownNames()
    {
        Rig rig;
        SliceModel s;
        QVERIFY(!rig.manager.saveProfile(QString(), &s));
        QVERIFY(!rig.manager.saveProfile(QStringLiteral(" , "), &s));
        QVERIFY(!rig.manager.saveProfile(QStringLiteral("x"), nullptr));
        QVERIFY(!rig.manager.applyProfile(QStringLiteral("nothing"), &s));
        QVERIFY(!rig.manager.deleteProfile(QStringLiteral("nothing")));
        QVERIFY(!rig.manager.renameProfile(QStringLiteral("nothing"), QStringLiteral("x")));
        QVERIFY(rig.manager.profileNames().isEmpty());
        QVERIFY(rig.manager.activeProfileName().isEmpty());
        QVERIFY(!rig.manager.hasProfile(QStringLiteral("nothing")));
    }

    void renameMovesValuesAndDeleteDropsThem()
    {
        Rig rig;
        SliceModel s;
        dressUp(s);
        QVERIFY(rig.manager.saveProfile(QStringLiteral("A"), &s));
        s.setAgcMode(AGCMode::Slow);
        QVERIFY(rig.manager.saveProfile(QStringLiteral("B"), &s));
        QCOMPARE(rig.manager.profileNames(), (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
        QCOMPARE(rig.manager.activeProfileName(), QStringLiteral("B"));

        // Rename A -> Ragchew: values move, order stays, active untouched.
        QVERIFY(!rig.manager.renameProfile(QStringLiteral("A"), QStringLiteral("B")));   // taken
        QVERIFY(rig.manager.renameProfile(QStringLiteral("A"), QStringLiteral("Ragchew")));
        QCOMPARE(rig.manager.profileNames(), (QStringList{QStringLiteral("Ragchew"), QStringLiteral("B")}));
        QVERIFY(rig.manager.profileValues(QStringLiteral("A")).isEmpty());
        QCOMPARE(rig.manager.profileValues(QStringLiteral("Ragchew")).value(QStringLiteral("agcMode")),
                 QString::number(int(AGCMode::Fast)));
        QCOMPARE(rig.manager.activeProfileName(), QStringLiteral("B"));
        // Renaming the active one follows it.
        QVERIFY(rig.manager.renameProfile(QStringLiteral("B"), QStringLiteral("Contest")));
        QCOMPARE(rig.manager.activeProfileName(), QStringLiteral("Contest"));

        // Delete the active one: gone from the list, the values, the name.
        QVERIFY(rig.manager.deleteProfile(QStringLiteral("Contest")));
        QCOMPARE(rig.manager.profileNames(), QStringList{QStringLiteral("Ragchew")});
        QVERIFY(rig.manager.profileValues(QStringLiteral("Contest")).isEmpty());
        QVERIFY(rig.manager.activeProfileName().isEmpty());
        QVERIFY(!rig.settings.contains(QStringLiteral("RxProfile/Contest/agcMode")));
        QVERIFY(rig.settings.contains(QStringLiteral("RxProfile/Ragchew/agcMode")));

        QVERIFY(rig.manager.deleteProfile(QStringLiteral("Ragchew")));
        QVERIFY(rig.manager.profileNames().isEmpty());
        QVERIFY(!rig.settings.contains(QStringLiteral("RxProfile/_names")));
    }

    void missingValueLeavesTheSliceAlone()
    {
        Rig rig;
        SliceModel s;
        dressUp(s);
        QVERIFY(rig.manager.saveProfile(QStringLiteral("Old"), &s));
        // A profile from before a setting joined the list: that key is
        // absent. Also a value nobody can read.
        rig.settings.remove(QStringLiteral("RxProfile/Old/agcThreshold"));
        rig.settings.setValue(QStringLiteral("RxProfile/Old/agcHang"), QStringLiteral("not a number"));

        SliceModel t;
        t.setAgcThreshold(-99);
        t.setAgcHang(777);
        QVERIFY(rig.manager.applyProfile(QStringLiteral("Old"), &t));
        QCOMPARE(t.agcThreshold(), -99);
        QCOMPARE(t.agcHang(), 777);
        QCOMPARE(t.agcMode(), AGCMode::Fast);     // the rest arrived
    }

    void textConversions()
    {
        QCOMPARE(RxProfileManager::valueToText(QVariant(true)), QStringLiteral("True"));
        QCOMPARE(RxProfileManager::valueToText(QVariant(false)), QStringLiteral("False"));
        QCOMPARE(RxProfileManager::valueToText(QVariant(-12)), QStringLiteral("-12"));
        QCOMPARE(RxProfileManager::valueToText(QVariant(0.125)), QStringLiteral("0.125"));
        QCOMPARE(RxProfileManager::valueToText(QVariant::fromValue(AGCMode::Fast)),
                 QString::number(int(AGCMode::Fast)));

        QCOMPARE(RxProfileManager::textToValue(QStringLiteral("True"), QMetaType::Bool).toBool(), true);
        QCOMPARE(RxProfileManager::textToValue(QStringLiteral("no"), QMetaType::Bool).toBool(), false);
        QCOMPARE(RxProfileManager::textToValue(QStringLiteral("-7"), QMetaType::Int).toInt(), -7);
        QVERIFY(!RxProfileManager::textToValue(QStringLiteral("x"), QMetaType::Int).isValid());
        QCOMPARE(RxProfileManager::textToValue(QStringLiteral("2.5"), QMetaType::Double).toDouble(), 2.5);
        const QVariant e = RxProfileManager::textToValue(
            QString::number(int(NrSlot::NR3)), QMetaType::fromType<NrSlot>().id());
        QVERIFY(e.isValid());
        QCOMPARE(e.value<NrSlot>(), NrSlot::NR3);
        QVERIFY(!RxProfileManager::textToValue(QStringLiteral("abc"),
                                               QMetaType::fromType<NrSlot>().id()).isValid());
    }
};

QTEST_GUILESS_MAIN(TestRxProfileManager)
#include "tst_rx_profile_manager.moc"
