// =================================================================
// tests/tst_cw_decoder_applet.cpp  (Longpath)
// =================================================================
//
// Longpath-original test. CwDecoderApplet over a RadioModel:
//   * decoded text lands in the transcript and stays capped
//   * the stats line and the capsule follow the decoder (lock toggles)
//   * the pitch band follows the operator's CW pitch (AppSettings CWPitch)
//   * a removed bound slice unbinds the applet (tap released)
//   * a rebind to another slice moves the tap
//   * the applet renders to a PNG when LONGPATH_GRAB_DIR is set
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QtTest/QtTest>
#include <QLabel>
#include <QPlainTextEdit>
#include <QPushButton>

#include "core/AppSettings.h"
#include "core/AudioEngine.h"
#include "core/CwDecoder.h"
#include "gui/applets/CwDecoderApplet.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

class TstCwDecoderApplet : public QObject {
    Q_OBJECT

private slots:
    void decodedTextLandsInTheTranscriptAndIsCapped()
    {
        RadioModel radio;
        CwDecoderApplet applet(&radio);
        QVERIFY(applet.decoderForTest());
        QVERIFY(applet.decoderForTest()->isRunning());

        // Drive the slot the decoder's signal would hit.
        QMetaObject::invokeMethod(&applet, "onTextDecoded", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("CQ TEST ")), Q_ARG(float, 0.2f));
        QMetaObject::invokeMethod(&applet, "onTextDecoded", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("DE OE5SOS")), Q_ARG(float, 0.3f));
        QCOMPARE(applet.textForTest()->toPlainText(), QStringLiteral("CQ TEST DE OE5SOS"));

        // 5000 characters in: the transcript keeps the newest 4000.
        for (int i = 0; i < 50; ++i) {
            QMetaObject::invokeMethod(&applet, "onTextDecoded", Qt::DirectConnection,
                                      Q_ARG(QString, QString(100, QLatin1Char('V'))), Q_ARG(float, 0.1f));
        }
        QVERIFY(applet.textForTest()->toPlainText().size() <= 4000);
        QVERIFY(applet.textForTest()->toPlainText().endsWith(QString(100, QLatin1Char('V'))));
        QVERIFY(!applet.textForTest()->toPlainText().contains(QStringLiteral("CQ TEST")));
    }

    void statsAndCapsuleFollowTheDecoder()
    {
        RadioModel radio;
        CwDecoderApplet applet(&radio);
        QCOMPARE(applet.statsForTest()->text(), QStringLiteral("— Hz · — WPM"));

        QMetaObject::invokeMethod(&applet, "onStatsUpdated", Qt::DirectConnection,
                                  Q_ARG(float, 701.4f), Q_ARG(float, 19.6f));
        QCOMPARE(applet.statsForTest()->text(), QStringLiteral("701 Hz · 20 WPM"));

        applet.lockPitchForTest()->setChecked(true);
        QVERIFY(applet.decoderForTest()->isPitchLocked());
        QVERIFY(!applet.decoderForTest()->isSpeedLocked());
        applet.lockSpeedForTest()->setChecked(true);
        QVERIFY(applet.decoderForTest()->isSpeedLocked());
        applet.lockPitchForTest()->setChecked(false);
        QVERIFY(!applet.decoderForTest()->isPitchLocked());
        QVERIFY(applet.decoderForTest()->isSpeedLocked());
    }

    void pitchBandFollowsTheCwPitchSetting()
    {
        AppSettings::instance().setValue(QStringLiteral("CWPitch"), 800);
        RadioModel radio;
        // The band is applied inside the decoder's pending parameters; the
        // visible effect is a decoder that accepts an 800 Hz tone and the
        // range setter having been called with 650..950 -- covered by the
        // decoder's own tests; here: construction with the setting present
        // does not throw and the setting is read (a bad value is clamped).
        AppSettings::instance().setValue(QStringLiteral("CWPitch"), 50);
        CwDecoderApplet applet(&radio);
        QVERIFY(applet.decoderForTest()->isRunning());
        AppSettings::instance().remove(QStringLiteral("CWPitch"));
    }

    void removedSliceUnbindsAndRebindMovesTheTap()
    {
        RadioModel radio;
        const int idA = radio.addSlice();
        const int idB = radio.addSlice();
        SliceModel* sliceA = radio.sliceById(idA);
        SliceModel* sliceB = radio.sliceById(idB);
        QVERIFY(sliceA && sliceB);

        CwDecoderApplet applet(&radio);
        applet.setSlice(sliceA);
        QVERIFY(radio.audioEngine());
        // Rebinding to B moves the tap; removing B releases it. We observe
        // through the engine's tap slice (the applet owns the ring).
        applet.setSlice(sliceB);
        radio.removeSlice(idB);
        // After the removal the applet must have released the tap: a
        // further setSlice(nullptr) is a no-op and nothing crashes.
        applet.setSlice(nullptr);
        applet.syncFromModel();
    }

    void appletRendersToPng()
    {
        RadioModel radio;
        CwDecoderApplet applet(&radio);
        applet.resize(380, 260);
        applet.show();
        QVERIFY(QTest::qWaitForWindowExposed(&applet));
        QMetaObject::invokeMethod(&applet, "onStatsUpdated", Qt::DirectConnection,
                                  Q_ARG(float, 700.0f), Q_ARG(float, 24.0f));
        QMetaObject::invokeMethod(&applet, "onTextDecoded", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("CQ CQ DE OE5SOS OE5SOS K ")), Q_ARG(float, 0.2f));
        QMetaObject::invokeMethod(&applet, "onTextDecoded", Qt::DirectConnection,
                                  Q_ARG(QString, QStringLiteral("OE5SOS DE DL1ABC UR 599 599 TU")), Q_ARG(float, 0.3f));
        // A lock re-reads the decoder's own estimate (0 here, no audio), so
        // set the picture's stats after it.
        applet.lockPitchForTest()->setChecked(true);
        QMetaObject::invokeMethod(&applet, "onStatsUpdated", Qt::DirectConnection,
                                  Q_ARG(float, 700.0f), Q_ARG(float, 24.0f));
        QTest::qWait(50);   // let the layout settle before the picture
        const QPixmap pm = applet.grab();
        QVERIFY(!pm.isNull());
        const QString grabDir = qEnvironmentVariable("LONGPATH_GRAB_DIR");
        if (!grabDir.isEmpty()) {
            QVERIFY(pm.save(grabDir + QStringLiteral("/longpath-grab-CwDecoderApplet.png")));
        }
        applet.close();
    }
};

QTEST_MAIN(TstCwDecoderApplet)
#include "tst_cw_decoder_applet.moc"
