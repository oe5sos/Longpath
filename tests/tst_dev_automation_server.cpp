// SPDX-License-Identifier: GPL-3.0-or-later
//
// Phase 0 of the dev automation bridge (src/core/DevAutomationServer.*):
// describeAutomationWidget()'s JSON shape, and the live ping/dumpTree/grab
// round-trip over a real QLocalSocket connection.

#include <QtTest>
#include <QCheckBox>
#include <QElapsedTimer>
#include <functional>
#include <QComboBox>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QLocalSocket>
#include <QPushButton>
#include <QSlider>
#include <QWidget>

#include "core/DevAutomationServer.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

using namespace Longpath;

namespace {
// One request/response round trip over a fresh connection, matching how a
// real client would use the bridge: connect, send one line, read one line,
// disconnect.
// QLocalSocket::waitForX() only pumps that socket's own engine, not the
// general Qt event loop -- so it never gives a same-process QLocalServer a
// turn to accept the connection and reply. Poll via processEvents() instead,
// which does dispatch the server side's signals too.
bool pumpUntil(const std::function<bool()>& predicate, int timeoutMs)
{
    QElapsedTimer t;
    t.start();
    while (!predicate()) {
        if (t.elapsed() > timeoutMs) { return false; }
        QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
    }
    return true;
}

QJsonObject sendCommand(const QString& socketName, const QString& command)
{
    QLocalSocket sock;
    sock.connectToServer(socketName);
    if (!pumpUntil([&]{ return sock.state() == QLocalSocket::ConnectedState
                             || sock.state() == QLocalSocket::UnconnectedState; }, 2000)
        || sock.state() != QLocalSocket::ConnectedState) {
        return QJsonObject{{"ok", false}, {"error", "connect failed: " + sock.errorString()}};
    }
    sock.write((command + QStringLiteral("\n")).toUtf8());
    if (!pumpUntil([&]{ return sock.bytesToWrite() == 0; }, 2000)) {
        return QJsonObject{{"ok", false}, {"error", "write did not flush"}};
    }
    if (!pumpUntil([&]{ return sock.canReadLine(); }, 2000)) {
        return QJsonObject{{"ok", false}, {"error", "no reply"}};
    }
    const QByteArray line = sock.readLine();
    return QJsonDocument::fromJson(line).object();
}
}

class TstDevAutomationServer : public QObject
{
    Q_OBJECT

private slots:
    void describePushButtonReportsCheckedAndText()
    {
        QPushButton btn(QStringLiteral("NR2"));
        btn.setObjectName(QStringLiteral("nr2Button"));
        btn.setCheckable(true);
        btn.setChecked(true);
        btn.setEnabled(true);

        const QJsonObject o = describeAutomationWidget(&btn);
        QCOMPARE(o.value(QStringLiteral("class")).toString(), QStringLiteral("QPushButton"));
        QCOMPARE(o.value(QStringLiteral("objectName")).toString(), QStringLiteral("nr2Button"));
        QCOMPARE(o.value(QStringLiteral("text")).toString(), QStringLiteral("NR2"));
        QVERIFY(o.value(QStringLiteral("checked")).toBool());
        QVERIFY(o.value(QStringLiteral("enabled")).toBool());
    }

    void describeLineEditReportsValue()
    {
        QLineEdit edit(QStringLiteral("14.200000"));
        edit.setObjectName(QStringLiteral("freqEdit"));
        const QJsonObject o = describeAutomationWidget(&edit);
        QCOMPARE(o.value(QStringLiteral("value")).toString(), QStringLiteral("14.200000"));
    }

    void describeComboReportsItemsAndCurrentValue()
    {
        QComboBox combo;
        combo.setObjectName(QStringLiteral("modeCombo"));
        combo.addItems({QStringLiteral("USB"), QStringLiteral("LSB"), QStringLiteral("CWL")});
        combo.setCurrentIndex(1);

        const QJsonObject o = describeAutomationWidget(&combo);
        QCOMPARE(o.value(QStringLiteral("value")).toString(), QStringLiteral("LSB"));
        const QJsonArray items = o.value(QStringLiteral("items")).toArray();
        QCOMPARE(items.size(), 3);
        QCOMPARE(items.at(0).toString(), QStringLiteral("USB"));
    }

    void describeSliderReportsRangeAndValue()
    {
        QSlider slider(Qt::Horizontal);
        slider.setRange(-8, 32);
        slider.setValue(12);
        const QJsonObject o = describeAutomationWidget(&slider);
        QCOMPARE(o.value(QStringLiteral("value")).toString(), QStringLiteral("12"));
        const QJsonObject range = o.value(QStringLiteral("range")).toObject();
        QCOMPARE(range.value(QStringLiteral("min")).toInt(), -8);
        QCOMPARE(range.value(QStringLiteral("max")).toInt(), 32);
    }

    void liveServerAnswersPingDumpTreeAndGrab()
    {
        const QString socketName = QStringLiteral("longpath-automation-test-%1")
                                        .arg(QCoreApplication::applicationPid());

        DevAutomationServer server;
        qputenv("LONGPATH_AUTOMATION_SOCKET", socketName.toUtf8());
        QVERIFY2(server.start(), "DevAutomationServer failed to start on the test socket");
        QVERIFY(server.isListening());

        auto* probe = new QPushButton(QStringLiteral("Probe Button"));
        probe->setObjectName(QStringLiteral("automationProbeButton"));
        probe->resize(120, 30);
        probe->show();
        QVERIFY(QTest::qWaitForWindowExposed(probe));

        const QJsonObject pong = sendCommand(socketName, QStringLiteral("ping"));
        QVERIFY2(pong.value(QStringLiteral("ok")).toBool(),
                 qPrintable(QStringLiteral("ping did not reply ok:true, got: ") +
                            QString::fromUtf8(QJsonDocument(pong).toJson(QJsonDocument::Compact))));

        const QJsonObject tree = sendCommand(socketName, QStringLiteral("dumpTree"));
        QVERIFY(tree.value(QStringLiteral("ok")).toBool());
        const QString treeText = QString::fromUtf8(QJsonDocument(tree).toJson(QJsonDocument::Compact));
        QVERIFY2(treeText.contains(QStringLiteral("automationProbeButton")),
                  "dumpTree did not mention the live probe button by objectName");
        QVERIFY2(treeText.contains(QStringLiteral("Probe Button")),
                  "dumpTree did not report the probe button's text");

        const QJsonObject grabbed = sendCommand(socketName,
            QStringLiteral("grab automationProbeButton"));
        QVERIFY2(grabbed.value(QStringLiteral("ok")).toBool(),
                  qPrintable(grabbed.value(QStringLiteral("error")).toString()));
        const QString path = grabbed.value(QStringLiteral("path")).toString();
        QVERIFY2(!path.isEmpty(), "grab did not report an output path");
        QVERIFY2(QFile::exists(path), qPrintable(QStringLiteral("grab PNG missing at ") + path));
        QVERIFY(grabbed.value(QStringLiteral("width")).toInt() > 0);
        QVERIFY(grabbed.value(QStringLiteral("height")).toInt() > 0);
        QFile::remove(path);

        const QJsonObject missing = sendCommand(socketName,
            QStringLiteral("grab thisObjectNameDoesNotExist"));
        QVERIFY2(!missing.value(QStringLiteral("ok")).toBool(),
                  "grab on an unknown objectName should fail, not silently succeed");

        // Widgets with no objectName of their own (the live panadapter is
        // exactly this case) must still be reachable by class name.
        const QJsonObject byClass = sendCommand(socketName, QStringLiteral("grab QPushButton"));
        QVERIFY2(byClass.value(QStringLiteral("ok")).toBool(),
                  qPrintable(byClass.value(QStringLiteral("error")).toString()));
        QFile::remove(byClass.value(QStringLiteral("path")).toString());

        // No RadioModel exists in this test window at all -- get must say
        // so plainly, not crash and not fabricate a reply.
        const QJsonObject noRadio = sendCommand(socketName, QStringLiteral("get radio"));
        QVERIFY2(!noRadio.value(QStringLiteral("ok")).toBool(),
                  "get radio should fail cleanly with no RadioModel present");

        delete probe;
        qunsetenv("LONGPATH_AUTOMATION_SOCKET");
    }

    void objectNameMatchWinsGloballyOverClassNameFallback()
    {
        const QString socketName = QStringLiteral("longpath-automation-test-priority-%1")
                                        .arg(QCoreApplication::applicationPid());
        qputenv("LONGPATH_AUTOMATION_SOCKET", socketName.toUtf8());

        DevAutomationServer server;
        QVERIFY(server.start());

        // Two top-level windows. One has a QLineEdit findable only via the
        // (spurious) class-name fallback; the other has a QLineEdit whose
        // objectName is literally the string "QLineEdit". An exact
        // objectName match must win over any class-name fallback match no
        // matter which top-level window Qt happens to enumerate first --
        // a per-window search (the original, buggy resolveAutomationTarget)
        // could let the class-name-only window shadow this one if it were
        // enumerated first.
        auto* classNameOnlyWidget = new QLineEdit();
        classNameOnlyWidget->resize(30, 16);
        classNameOnlyWidget->show();
        QVERIFY(QTest::qWaitForWindowExposed(classNameOnlyWidget));

        auto* objectNameMatchWidget = new QLineEdit();
        objectNameMatchWidget->setObjectName(QStringLiteral("QLineEdit"));
        objectNameMatchWidget->resize(400, 200);
        objectNameMatchWidget->show();
        QVERIFY(QTest::qWaitForWindowExposed(objectNameMatchWidget));

        const QJsonObject grabbed = sendCommand(socketName, QStringLiteral("grab QLineEdit"));
        QVERIFY2(grabbed.value(QStringLiteral("ok")).toBool(),
                  qPrintable(grabbed.value(QStringLiteral("error")).toString()));
        // The class-name-only widget is 30x16; the objectName match is
        // 400x200. Any width comfortably above the small widget's rules out
        // having grabbed the wrong one.
        QVERIFY2(grabbed.value(QStringLiteral("width")).toInt() > 100,
                  "grab QLineEdit returned the class-name-only widget instead of the exact objectName match");

        QFile::remove(grabbed.value(QStringLiteral("path")).toString());
        delete classNameOnlyWidget;
        delete objectNameMatchWidget;
        qunsetenv("LONGPATH_AUTOMATION_SOCKET");
    }

    void getUnknownModelReportsUnknownRegardlessOfRadioPresence()
    {
        const QString socketName = QStringLiteral("longpath-automation-test-unknown-%1")
                                        .arg(QCoreApplication::applicationPid());
        qputenv("LONGPATH_AUTOMATION_SOCKET", socketName.toUtf8());

        // Deliberately no setRadioModel() call -- an unsupported model name
        // (e.g. the not-yet-shipped "pan") must be reported as such even
        // when no RadioModel exists yet, not masked by the unrelated
        // "no RadioModel set" error.
        DevAutomationServer server;
        QVERIFY(server.start());

        const QJsonObject reply = sendCommand(socketName, QStringLiteral("get pan"));
        QVERIFY2(!reply.value(QStringLiteral("ok")).toBool(),
                  "get with an unsupported model name must fail");
        QVERIFY2(reply.value(QStringLiteral("error")).toString().contains(QStringLiteral("unknown get model")),
                  qPrintable(QStringLiteral("expected an 'unknown get model' error even with no RadioModel set, got: ") +
                             reply.value(QStringLiteral("error")).toString()));

        qunsetenv("LONGPATH_AUTOMATION_SOCKET");
    }

    void getRadioAndSliceReportRealModelState()
    {
        const QString socketName = QStringLiteral("longpath-automation-test-get-%1")
                                        .arg(QCoreApplication::applicationPid());
        qputenv("LONGPATH_AUTOMATION_SOCKET", socketName.toUtf8());

        DevAutomationServer server;

        // get reads whichever RadioModel setRadioModel() was handed -- not
        // one discovered by scanning widgets, which has no way to prefer
        // "the" real RadioModel over one under some unrelated top-level
        // widget. RadioModel itself doesn't need to live under a QWidget at
        // all for this to work; a plain QObject parent is enough.
        auto* host = new QObject();
        auto* radio = new RadioModel(host);
        server.setRadioModel(radio);
        QVERIFY(server.start());

        const int sliceId = radio->addSlice();
        SliceModel* slice = radio->sliceById(sliceId);
        QVERIFY(slice);
        slice->setFrequency(14.195e6);
        radio->setActiveSliceById(sliceId);

        const QJsonObject radioInfo = sendCommand(socketName, QStringLiteral("get radio"));
        QVERIFY2(radioInfo.value(QStringLiteral("ok")).toBool(),
                  qPrintable(radioInfo.value(QStringLiteral("error")).toString()));

        const QJsonObject sliceInfo = sendCommand(socketName, QStringLiteral("get slice active"));
        QVERIFY2(sliceInfo.value(QStringLiteral("ok")).toBool(),
                  qPrintable(sliceInfo.value(QStringLiteral("error")).toString()));
        QCOMPARE(sliceInfo.value(QStringLiteral("frequencyHz")).toDouble(), 14.195e6);
        QCOMPARE(sliceInfo.value(QStringLiteral("band")).toString(), QStringLiteral("20m"));

        delete host;
        qunsetenv("LONGPATH_AUTOMATION_SOCKET");
    }
};

QTEST_MAIN(TstDevAutomationServer)
#include "tst_dev_automation_server.moc"
