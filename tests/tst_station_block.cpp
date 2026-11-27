// tests/tst_station_block.cpp
#include <QtTest/QtTest>
#include <QSignalSpy>

#include <QLabel>
#include "gui/widgets/StationBlock.h"

using namespace Longpath;

class TstStationBlock : public QObject {
    Q_OBJECT

private slots:
    void defaultsToDisconnectedAppearance() {
        StationBlock s;
        QVERIFY(!s.isConnectedAppearance());
        QVERIFY(s.radioName().isEmpty());
    }

    void setRadioNameSwitchesToConnected() {
        StationBlock s;
        s.setRadioName(QStringLiteral("ANAN-G2 (Saturn)"));
        QVERIFY(s.isConnectedAppearance());
        QCOMPARE(s.radioName(), QStringLiteral("ANAN-G2 (Saturn)"));
        // Entwurf A: brass border while on.
        QVERIFY(s.styleSheet().contains(QStringLiteral("#c9a04e")));
    }

    void emptyNameRevertsToDisconnected() {
        StationBlock s;
        s.setRadioName(QStringLiteral("X"));
        s.setRadioName(QString());
        QVERIFY(!s.isConnectedAppearance());
        // Off: quiet slate border, no dashed red any more (2026-11-27).
        QVERIFY(s.styleSheet().contains(QStringLiteral("#3a4656")));
        QVERIFY(!s.styleSheet().contains(QStringLiteral("dashed")));
    }

    void leftClickEmitsClickedInBothAppearances() {
        StationBlock s;
        s.resize(180, 22);
        QSignalSpy spy(&s, &StationBlock::clicked);

        // Disconnected
        QTest::mouseClick(&s, Qt::LeftButton);
        QCOMPARE(spy.count(), 1);

        // Connected
        s.setRadioName(QStringLiteral("ANAN-G2"));
        QTest::mouseClick(&s, Qt::LeftButton);
        QCOMPARE(spy.count(), 2);
    }

    void rightClickEmitsContextMenuOnlyWhenConnected() {
        StationBlock s;
        s.resize(180, 22);
        QSignalSpy spy(&s, &StationBlock::contextMenuRequested);

        // Disconnected — right-click should NOT emit
        QTest::mouseClick(&s, Qt::RightButton);
        QCOMPARE(spy.count(), 0);

        // Connected — right-click SHOULD emit
        s.setRadioName(QStringLiteral("ANAN-G2"));
        QTest::mouseClick(&s, Qt::RightButton);
        QCOMPARE(spy.count(), 1);
    }

    void hardwareLineJoinsModelAndFirmware() {
        StationBlock b;
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        QCOMPARE(b.hardwareLine(), QStringLiteral("ANAN-G2 · v27"));
    }

    void modelAloneOmitsTheSeparator() {
        StationBlock b;
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QString());
        QCOMPARE(b.hardwareLine(), QStringLiteral("ANAN-G2"));
    }

    void firmwareAloneOmitsTheSeparator() {
        StationBlock b;
        b.setHardwareLine(QString(), QStringLiteral("v27"));
        QCOMPARE(b.hardwareLine(), QStringLiteral("v27"));
    }

    void bothEmptyGivesAnEmptyLine() {
        StationBlock b;
        // Seed a non-default state first. m_hardwareLine starts empty by
        // default construction, so asserting empty afterward with no seed
        // would still pass even if setHardwareLine()'s entire body were
        // deleted (final-fix-wave finding 9) -- it would just be
        // re-confirming the untouched default.
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        QVERIFY(!b.hardwareLine().isEmpty());
        b.setHardwareLine(QString(), QString());
        QCOMPARE(b.hardwareLine(), QString());
    }

    // Entwurf A (2026-11-27): name and status sit beside the power button,
    // left-aligned as a pair. Both rows must share one alignment so the
    // name never hangs off the line beneath it (the 2026-08-03 report).
    void bothRowsShareOneAlignment() {
        StationBlock b;
        b.setRadioName(QStringLiteral("ANAN-G2E"));
        b.setHardwareLine(QStringLiteral("HermesC10"), QStringLiteral("v110"));
        const QList<QLabel*> labels = b.findChildren<QLabel*>();
        QCOMPARE(labels.size(), 2);
        QCOMPARE(labels.at(0)->alignment(), labels.at(1)->alignment());
    }

    void offWithATargetShowsItsNameAndHowToSwitchOn() {
        StationBlock b;
        b.setTargetName(QStringLiteral("FLEX-8400 (Martin)"));
        QVERIFY(!b.isConnectedAppearance());
        auto* name = b.findChild<QLabel*>(QStringLiteral("StationBlock_Label"));
        QVERIFY(name);
        QCOMPARE(name->text(), QStringLiteral("FLEX-8400 (Martin)"));
        QCOMPARE(b.statusText(), QStringLiteral("off · click to switch on"));
    }

    void offWithoutATargetStillSaysClickToConnect() {
        StationBlock b;
        auto* name = b.findChild<QLabel*>(QStringLiteral("StationBlock_Label"));
        QVERIFY(name);
        QCOMPARE(name->text(), QStringLiteral("Click to connect"));
        QVERIFY(b.statusText().isEmpty());
    }

    void connectingIsDarkBrassAndSaysSo() {
        StationBlock b;
        b.setTargetName(QStringLiteral("FLEX-8400 (Martin)"));
        b.setConnecting(true);
        QCOMPARE(b.statusText(), QStringLiteral("connecting…"));
        QVERIFY(b.styleSheet().contains(QStringLiteral("#8a6a30")));
        // Arriving ends the connecting state on its own.
        b.setRadioName(QStringLiteral("FLEX-8400 (Martin)"));
        QVERIFY(!b.isConnecting());
        QVERIFY(b.styleSheet().contains(QStringLiteral("#c9a04e")));
    }

    void onShowsTheHardwareLineAfterOn() {
        StationBlock b;
        b.setRadioName(QStringLiteral("ANAN-G2"));
        QCOMPARE(b.statusText(), QStringLiteral("on · click to switch off"));
        b.setHardwareLine(QStringLiteral("Saturn"), QStringLiteral("v27"));
        QCOMPARE(b.statusText(), QStringLiteral("on · Saturn · v27"));
    }

    void switchingOffKeepsTheNameAsTheTarget() {
        StationBlock b;
        b.setRadioName(QStringLiteral("FLEX-8400 (Martin)"));
        b.setRadioName(QString());
        QCOMPARE(b.targetName(), QStringLiteral("FLEX-8400 (Martin)"));
        QCOMPARE(b.statusText(), QStringLiteral("off · click to switch on"));
    }

    // Werkzeug, kein Pruefpunkt: mit LONGPATH_RENDER_DIR legt es die drei
    // Zustaende als PNG ab (Abgleich mit dem Entwurfsblatt A, 2026-11-27).
    void renderStatesForTheSheet() {
        const QString dir = qEnvironmentVariable("LONGPATH_RENDER_DIR");
        if (dir.isEmpty()) {
            QSKIP("LONGPATH_RENDER_DIR not set");
        }
        const auto render = [&](const QString& file, auto setup) {
            StationBlock b;
            b.resize(260, 44);
            setup(b);
            b.ensurePolished();
            b.grab().save(dir + QLatin1Char('/') + file);
        };
        render(QStringLiteral("aus.png"), [](StationBlock& b) {
            b.setTargetName(QStringLiteral("FLEX-8400 (Martin)"));
        });
        render(QStringLiteral("verbindet.png"), [](StationBlock& b) {
            b.setTargetName(QStringLiteral("FLEX-8400 (Martin)"));
            b.setConnecting(true);
        });
        render(QStringLiteral("ein.png"), [](StationBlock& b) {
            b.setRadioName(QStringLiteral("FLEX-8400 (Martin)"));
        });
    }

    void clearingTheNameAlsoClearsTheHardwareLine() {
        StationBlock b;
        b.setRadioName(QStringLiteral("ANAN G2"));
        b.setHardwareLine(QStringLiteral("ANAN-G2"), QStringLiteral("v27"));
        b.setRadioName(QString());
        QCOMPARE(b.hardwareLine(), QString());
        QVERIFY(!b.isConnectedAppearance());
    }
};

QTEST_MAIN(TstStationBlock)
#include "tst_station_block.moc"
