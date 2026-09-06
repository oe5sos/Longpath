// src/core/DevAutomationServer.h
// In-process, agent-first introspection/capture bridge for Longpath.
//
// Phase 0 (this file): read-only. dumpTree walks every top-level QWidget and
// reports a JSON tree (class, objectName, accessibleName, enabled, visible,
// geometry, and a best-effort "value" for common controls). grab captures a
// single widget by objectName as a PNG, including GPU-rendered QRhiWidgets
// (SpectrumWidget, MeterWidget) via QRhiWidget::grabFramebuffer(). get reads
// a handful of live model properties (radio connection state, the active
// slice's frequency/mode/filter/band) directly, without a screenshot at
// all -- reclassified into Phase 0 on 2026-09-06 (the design doc originally
// filed it as Phase 1) because it carries zero interaction risk: it never
// writes to a model, never touches a widget, so it belongs with the other
// read-only verbs rather than waiting on Phase 1's click/setValue/tune
// machinery and TX-safety gating. get reads the RadioModel handed to
// setRadioModel() -- not discovered by scanning widgets -- see that
// method's own comment for why.
//
// Off by default: the server only starts when the LONGPATH_AUTOMATION
// environment variable is set (see main.cpp), so it adds no attack surface
// or overhead to a normal launch. No interaction verbs live here yet --
// no click/setValue/tune/connect, and certainly nothing that could key a
// transmitter. That is deliberately left for a later phase, gated behind
// its own, separate opt-in, matching the safety posture the AetherSDR
// reference below settled on after real use.

// =================================================================
// src/core/DevAutomationServer.h  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR -- GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       -- per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a structural derivative of AetherSDR's automation bridge
//   (docs/automation-bridge.md, src/core/AutomationServer.{h,cpp}, issue
//   #3646) -- the idea (an opt-in, in-process QLocalServer exposing a JSON
//   widget-tree dump and per-widget PNG capture, including QRhiWidget
//   readback via grabFramebuffer()) and the phase boundary (Phase 0
//   read-only introspection before any drive/interact verb) are carried
//   over. AetherSDR's own implementation is roughly 12,000 lines covering
//   25 verbs across four phases; this file implements only the Phase 0
//   subset, written fresh for Longpath's own widget vocabulary rather than
//   translated line-by-line -- there is no Thetis equivalent to port
//   (Thetis has no such tooling), so AetherSDR is the sole source here.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 SS5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-06 -- Created for Longpath (Phase 0: dumpTree + grab only),
//                 by OE5SOS with AI-assisted transformation via Anthropic
//                 Claude Code. Phases 1+ (invoke/interact, live model
//                 reads, TX-gated verbs, MCP wrapper) are future work --
//                 see docs/architecture/2026-09-06-dev-automation-bridge-design.md.
// =================================================================

#pragma once

#include <QObject>
#include <QPointer>
#include <QString>

class QLocalServer;
class QLocalSocket;
class QWidget;
class QJsonObject;

namespace Longpath {

class RadioModel;

// Serializes one widget's introspectable state into the shape dumpTree/grab
// report. Exposed standalone (not just as a private file-local helper) so
// tests can assert on it without spinning up a live socket.
QJsonObject describeAutomationWidget(const QWidget* w);

class DevAutomationServer : public QObject {
    Q_OBJECT

public:
    explicit DevAutomationServer(QObject* parent = nullptr);
    ~DevAutomationServer() override;

    // Starts listening on a QLocalServer named by LONGPATH_AUTOMATION_SOCKET
    // (default "longpath-automation"). Returns false (and logs why) if the
    // socket is already taken -- e.g. a previous instance's stale handle --
    // matching AetherSDR's own remove-stale-then-retry-once approach.
    bool start();
    void stop();

    // True once start() has succeeded and the server is actually listening.
    bool isListening() const;

    // Wires the one true RadioModel for `get` to read, handed in directly by
    // whoever owns it (main.cpp, from MainWindow::radioModelForTest()) rather
    // than discovered by scanning QApplication::topLevelWidgets() -- that
    // scan has no way to tell "the" MainWindow's RadioModel apart from a
    // stray one under some other top-level widget (a floated ToolWindow, or
    // an orphaned MainWindow left over in a test harness that runs several
    // in the same process; MainWindow::closeEvent() was bitten by exactly
    // this ambiguity once already, commit 6e7c1cad). QPointer so a RadioModel
    // destroyed without a matching setRadioModel(nullptr) call is read back
    // as null instead of dangling.
    void setRadioModel(RadioModel* radio);

private slots:
    void onNewConnection();
    void onReadyRead();

private:
    QJsonObject handleLine(const QByteArray& line);
    QJsonObject doDumpTree() const;
    QJsonObject doGrab(const QString& target) const;
    QJsonObject doGet(const QString& model, const QString& selector) const;
    QJsonObject doPing() const;

    QLocalServer* m_server{nullptr};
    QPointer<RadioModel> m_radioModel;
};

} // namespace Longpath
