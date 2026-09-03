// =================================================================
// src/gui/MainWindow.cpp  (NereusSDR)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/dsp.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Signal-routing hub, double-height status-bar layout, and
//                 TitleBar feature-request dialog ported from AetherSDR
//                 (ten9876/AetherSDR, GPLv3) src/gui/MainWindow.{h,cpp} and
//                 src/gui/TitleBar.{h,cpp}. AetherSDR has no per-file
//                 headers; project-level citation per docs/attribution/
//                 HOW-TO-PORT.md rule 6.
// =================================================================

/*  MeterManager.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

/*  wdsp.cs

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2013-2017 Warren Pratt, NR0V

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at  

warren@wpratt.com

*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//=================================================================
// setup.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "MainWindow.h"
#include "gui/styles/ThemeQss.h"
#include "ConnectionPanel.h"
#include "NetworkDiagnosticsDialog.h"
#include "SupportDialog.h"
#include "AboutDialog.h"
#include "SpectrumWidget.h"
// Phase 3F Sub-Epic D Task 10: +PAN bottom-bar dropdown reads slice
// state + drives PanadapterStack layout/float actions.
#include "PanadapterStack.h"
#include "PanadapterApplet.h"
#include "PanFloatingWindow.h"
#include "MacFloatingWindowBehavior.h"
#include "PanLayoutDialog.h"

#include <QColorDialog>
#include "core/KiwiSdrManager.h"
#include "core/FFTRouter.h"
#include "StyleConstants.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "widgets/CommandBar.h"
#include "widgets/ProfileRail.h"
#include "widgets/WidgetPicker.h"
#include "gui/LayoutProfiles.h"
#include "gui/widgets/WorldTexture.h"
#include "applets/StripWindow.h"
#include "widgets/RotorLogbookPanel.h"
#include "widgets/RotorDialWidget.h"
#include "gui/ToolWindow.h"
#include "gui/WindowChrome.h"
#include "widgets/SwrSweepPanel.h"
#include "core/Maidenhead.h"
#include "core/CredentialStore.h"
#include "core/QrzClient.h"
#include "gui/AntennaWindow.h"
#include "core/QrzLogbookUploader.h"
#include "core/CloudlogUploader.h"
#include "core/AdifNetworkUploader.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include "widgets/RxDashboard.h"
#include "widgets/AntennaSwitchToast.h"
#include "widgets/StatusToast.h"
#include "widgets/FilterPolicyDialog.h"
#include "widgets/TxBoundConfirmDialog.h"
#include "core/RxChannel.h"
#include "core/TxChannel.h"  // H.2: setTxChannel wiring
#include "core/ReceiverManager.h"
#include "core/AppSettings.h"
#include "core/BuildIdentity.h"
#include "core/PaTempUnit.h"
#include "core/RadioStatus.h"
#include "core/RadioDiscovery.h"
#include "core/WdspEngine.h"
#include "core/FFTEngine.h"
#include "core/NbFamily.h"
#include "core/ClarityController.h"
#include "core/StepAttenuatorController.h"
#include "core/MoxController.h"  // 3M-1a G.1: F.2 connect (hardwareFlipped → onMoxHardwareFlipped)
#include "core/NoiseFloorTracker.h"
#include "core/BoardCapabilities.h"
#include "core/TxSliceArbiter.h"  // Phase 3F Sub-Epic C Task 9: TX-handoff routing
#include "models/PanadapterModel.h"
#include "models/Band.h"
#include "models/TransmitModel.h"
#include "core/LogCategories.h"
#include "containers/ContainerManager.h"
#include "containers/ContainerWidget.h"
#include "containers/ContainerSettingsDialog.h"
#include "meters/MeterWidget.h"
#include "meters/MeterItem.h"
#include "meters/ItemGroup.h"
#include "meters/MeterPoller.h"
#include "meters/VfoDisplayItem.h"  // 3M-1c L.3 — TX badge routing
#include "applets/AppletFloatingWindow.h"
#include "applets/AppletKeys.h"
#include "applets/AppletPanelWidget.h"
#include "applets/FrequencyApplet.h"
#include "applets/InstrumentApplet.h"
#include "gui/WindowPlacement.h"
#include "applets/AmpApplet.h"
#include "applets/Rf2ksApplet.h"
#include "applets/AppletVisibilityController.h"
#include "applets/RxApplet.h"
#include "core/PgxlConnection.h"
#include "core/TgxlConnection.h"
#include "core/Rf2ksConnection.h"
#include "core/SmartSdrApiListener.h"
#include "applets/TxApplet.h"
#include "applets/TxEqDialog.h"
// Phase 3J-2 H1: Tools menu modeless singletons (Spot Hub + FreeDV Reporter).
#include "SpotHubDialog.h"
#include "FreeDVReporterDialog.h"
// Phase 3F Sub-Epic G T4: bench-minimum Diversity dialog (Tools menu).
#include "DiversityDialog.h"
#include "models/SpotModel.h"
#include "models/NotchModel.h"
#include "models/FreeDVStationModel.h"
#include "core/DxccColorProvider.h"
#include "core/FreeDVReporterClient.h"
#include "core/DxClusterClient.h"
#include "core/WsjtxClient.h"
#include "core/SpotCollectorClient.h"
#include "core/PotaClient.h"
#include "core/SotaClient.h"
#include "core/PskReporterClient.h"
#include "PsForm.h"
#include "PsaIndicatorWidget.h"
#include "core/PureSignal.h"
#include "core/TwoToneController.h"
#include "applets/PhoneCwApplet.h"
#include "applets/RadeApplet.h"
#include "applets/EqApplet.h"
#include "applets/VaxApplet.h"
#include "applets/DigitalApplet.h"
#include "applets/PureSignalApplet.h"
#include "applets/DiversityApplet.h"
#include "applets/CwxApplet.h"
#include "applets/DvkApplet.h"
#include "applets/QsoRecorderApplet.h"
#include "applets/KiwiSdrApplet.h"
#include "KiwiWaterfallPanel.h"
#include "applets/AsrApplet.h"
#include "asr/AsrService.h"
#include "asr/RemoteAsrBackend.h"
#include "core/audio/AudioTapRing.h"
#include "KiwiPublicReceiverPicker.h"
#include "applets/BandwidthFilterApplet.h"
#include "applets/CatApplet.h"
#include "applets/TunerApplet.h"
// Phase 23: TCI server + applets (guarded so non-WebSocket builds still compile)
#ifdef HAVE_WEBSOCKETS
#  include "applets/TciApplet.h"
#  include "applets/ClientChainApplet.h"
#  include "core/TciServer.h"
#  include "setup/TciLogWindow.h"  // Phase 3J-1 closeout Item 2 (2026-05-12)
#  include <QWebSocket>
#endif
#include "SpectrumOverlayPanel.h"
#include "SetupDialog.h"
#include "setup/DspSetupPages.h"   // NrAnfSetupPage::selectSubtab
#include "TitleBar.h"
#include "VaxFirstRunDialog.h"
#if defined(Q_OS_LINUX)
#  include "VaxLinuxFirstRunDialog.h"
#endif
#include "widgets/MasterOutputWidget.h"
#include "widgets/StationBlock.h"
#include "widgets/StatusBadge.h"
#include "widgets/AdcOverloadBadge.h"
#include "widgets/OverflowChip.h"
#include "widgets/SystemTile.h"
#include "gui/chrome/ChromeBarController.h"
#include "gui/styles/PopupMenuStyle.h"
#include "gui/chrome/ChromeBarItems.h"
#include "gui/chrome/TxSwitchBar.h"
#include "core/AudioDeviceConfig.h"
#include "core/RadioLinkKind.h"

#include <QNetworkInterface>
#include "core/AudioEngine.h"
#include "core/ClientPuduMonitor.h"
#include "core/TxWorkerThread.h"
#include "core/strip/StripChain.h"
#include "core/strip/StripSettings.h"

#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSource>
#include <QMediaDevices>

#include <algorithm>
#include <cmath>
#include <vector>
#include "core/audio/VirtualCableDetector.h"
#include "core/audio/RealtimeAudioPriority.h"

#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QSlider>
#include <QCloseEvent>
#include <QResizeEvent>
#include <QEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QToolTip>
#include <QMenuBar>
#include <QMenu>
#include <QMetaMethod>
#include <QMetaProperty>
#include <QAction>
#include <QSignalBlocker>
#include <QActionGroup>
#include <QStatusBar>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QSplitter>
#include <QDateTime>
#include <QPainter>
#include <QPixmap>
#include <QProgressDialog>
#include <QInputDialog>
#include <QMessageBox>
#include <QDialog>
#include <QDir>
#include <QDockWidget>
#include <QFileDialog>
#include <QFormLayout>
#include <QLineEdit>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QThread>
#include <QFile>          // /proc/stat reader for Linux system-CPU path
#include <QPushButton>
#include <QCursor>
#include <QClipboard>
#include <QDesktopServices>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>
#include <QVersionNumber>
#include <QPointer>
#include <QShortcut>

#include <cstdlib>

// Cross-platform CPU usage readers — see readProcessCpuPercent and
// readSystemCpuPercent below. POSIX side (macOS / Linux) shares
// getrusage for process CPU; macOS adds host_processor_info for system
// CPU; Linux reads /proc/stat; Windows uses GetProcessTimes /
// GetSystemTimes. Each branch is gated by Q_OS_*.
#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
#include <sys/resource.h>
#endif
#ifdef Q_OS_MAC
#include <mach/mach.h>
#include <mach/mach_host.h>
#include <mach/processor_info.h>
#endif
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Longpath {

namespace {
// First-run/rescan wants the "relevant" virtual cables for the current
// platform — 3rd-party cables on Windows (BYO), our own NereusSdrVax
// entries on Mac/Linux (native HAL plugin / pipe-source). Centralising
// the platform split here keeps checkVaxFirstRun() focused on
// scenario-selection + dialog wiring.
QVector<DetectedCable> detectedForFirstRun()
{
#if defined(Q_OS_WIN)
    return VirtualCableDetector::scanThirdPartyOnly();
#else
    QVector<DetectedCable> out;
    for (const auto& c : VirtualCableDetector::scan()) {
        if (c.product == VirtualCableProduct::NereusSdrVax) {
            out.push_back(c);
        }
    }
    return out;
#endif
}
} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_radioModel(new RadioModel(this))
{
    // ── 2026-08-10: WSJT-X auto-logging ──────────────────────────────
    // When WSJT-X logs a QSO it broadcasts the finished ADIF record;
    // route it into the Rotor/Log panel's logbook. Connected here in
    // the constructor rather than where the Spot Hub wires its other
    // WSJT-X signals, because the listener can auto-start with the app
    // (WsjtxAutoStart) long before any dialog is opened — a contact
    // logged in the first minute must not be lost to lazy wiring. The
    // panel itself is created on demand inside the lambda.
    if (auto* wsjtx = m_radioModel->wsjtx()) {
        connect(wsjtx, &WsjtxClient::qsoLogged,
                this, [this](const LogEntry& e) {
            if (RotorLogbookPanel* panel = ensureRotorPanel()) {
                panel->logExternalQso(e);
            }
        });
    }

    // ── Phase 23 (bench fix 2026-05-10): TCI Server BEFORE buildUI ───────────
    // TciApplet + ClientChainApplet are constructed by populateDefaultMeter()
    // (called from buildUI), gated on `if (m_tciServer)`. The original Phase
    // 23 wiring constructed TciServer AFTER buildUI, so the gate evaluated
    // false and the applets never appeared in the right-side panel.
    // RadioModel is already constructed via the member initializer list, so
    // it's safe to instantiate TciServer here. Auto-start is deferred to
    // after buildUI so the indicator + applets are ready to receive signals.
#ifdef HAVE_WEBSOCKETS
    {
        m_tciServer = new TciServer(m_radioModel, this);
        connect(m_tciServer, &TciServer::serverStarted,
                this, [this](quint16) { m_tciServerRunning = true;  updateTciIndicator(); });
        connect(m_tciServer, &TciServer::serverStopped,
                this, [this]()        { m_tciServerRunning = false; updateTciIndicator(); });
        connect(m_tciServer, &TciServer::clientConnected,
                this, [this](QWebSocket*) { ++m_tciClientCount; updateTciIndicator(); });
        connect(m_tciServer, &TciServer::clientDisconnected,
                this, [this](QWebSocket*) {
                    if (m_tciClientCount > 0) { --m_tciClientCount; }
                    updateTciIndicator();
                });
        connect(m_tciServer, &TciServer::txAudioActiveClientChanged,
                this, [this](QWebSocket* owner) {
                    m_tciHasTxClient = (owner != nullptr);
                    updateTciIndicator();
                    // Phase 3J-1 bench fix (2026-05-10): gate the TxWorkerThread
                    // mic-source pump while TCI is providing audio so it does
                    // not race feedTxAudioFromTci's dispatch.  See
                    // TxChannel::m_tciAudioActive doc-comment for the full
                    // narrative on why the two-source race corrupts on-air
                    // audio at the I/Q ring buffer.
                    if (auto* wdsp = m_radioModel ? m_radioModel->wdspEngine() : nullptr) {
                        if (auto* txCh = wdsp->txChannel(WdspEngine::kTxChannelId)) {
                            txCh->setTciAudioActive(owner != nullptr);
                        }
                    }
                });
    }
#endif

    buildUI();
    buildMenuBar();

    // Phase 3O Sub-Phase 10 Task 10c — host the menu bar + master-output
    // controls in a custom TitleBar strip. Must run AFTER buildMenuBar()
    // so the menu is fully populated with actions before we re-parent it.
    //
    // setMenuWidget() hands ownership to QMainWindow and installs the
    // strip at the top of the window. On macOS this does NOT by itself
    // disable Qt's promotion of the menu bar to the native global bar —
    // that claim, written here in April, was never true; setMenuWidget()
    // only changes which widget occupies the QMainWindow's menu-area
    // LAYOUT slot, and says nothing about QMenuBar::isNativeMenuBar(),
    // which macOS defaults to true regardless. Left alone, the QMenuBar
    // ends up neither properly promoted (it is no longer a direct
    // QMainWindow-adjacent menu bar once TitleBar::setMenuBar() reparents
    // it into m_hbox) nor rendered in-window (native mode suppresses its
    // own widget painting) -- it goes missing from BOTH places, which is
    // exactly the "Kopfleiste ist verschwunden" bug reported 2026-09-02.
    // Explicit setNativeMenuBar(false) is the actual missing half of the
    // "menus render in-window" design choice this comment always claimed
    // was already in effect.
    menuBar()->setNativeMenuBar(false);
    m_titleBar = new TitleBar(m_radioModel->audioEngine(), this);
    m_titleBar->setMenuBar(menuBar());
    setMenuWidget(m_titleBar);

    // Wire the MasterOutputWidget device picker → AudioEngine so picking
    // an output device rebuilds the speakers bus. Task 10b exposes only
    // the deviceName; we load the rest of the persisted speakers config
    // (sampleRate / channels / bufferSamples / exclusiveMode / etc.) so
    // the user's tuned values are preserved across a device change.  A
    // bare default-constructed AudioDeviceConfig with only deviceName
    // set would otherwise clobber persisted fields, defeating the
    // Setup → Devices page entirely.
    connect(m_titleBar->masterOutput(), &MasterOutputWidget::outputDeviceChanged,
            this, [this](const QString& name) {
        AudioDeviceConfig cfg = AudioDeviceConfig::loadFromSettings(
            QStringLiteral("audio/Speakers"));
        cfg.deviceName = name;
        if (auto* engine = m_radioModel->audioEngine()) {
            engine->setSpeakersConfig(cfg);
        }
    });

    // Phase 3O Sub-Phase 10 Task 10d — the 💡 feature-request button
    // now lives inside TitleBar (consolidated from the old featureBar
    // QToolBar). Wire its click signal to the existing slot.
    connect(m_titleBar, &TitleBar::featureRequestClicked,
            this, &MainWindow::showFeatureRequestDialog);

    // ── Phase 3Q Sub-PR-4 D.2: ConnectionSegment wiring ────────────────────
    {
        auto* seg = m_titleBar->connectionSegment();

        // 1. State dot + pulse: driven by connectionStateChanged.
        connect(m_radioModel, &RadioModel::connectionStateChanged,
                seg, &ConnectionSegment::setState);

        // ── Woran haengt das Geraet? (2026-08-23) ────────────────────
        //
        // Bestimmt beim Verbinden, weil erst dann die Adresse feststeht.
        // Der Hinweis steht neben der Verlustanzeige und beantwortet
        // genau deren Frage: der Betreiber sah 0,17 % bis 2,87 %
        // Verlust, der Kern hatte KEIN Paket wegen vollen Puffers
        // verworfen, und der Weg lief ueber WLAN. Ohne den Hinweis
        // sucht man den Fehler im Programm.
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [this, seg](ConnectionState st) {
            if (st != ConnectionState::Connected) {
                seg->setRadioLinkKind(RadioLinkKind::Unknown);
                return;
            }
            const QHostAddress ip =
                m_radioModel->connection()
                    ? m_radioModel->connection()->radioInfo().address
                    : QHostAddress();
            const RadioLinkKind kind =
                radioLinkKindFor(ip, QNetworkInterface::allInterfaces());
            seg->setRadioLinkKind(kind);
            const QString warn = radioLinkWarning(kind);
            if (!warn.isEmpty()) {
                qCInfo(lcConnection).noquote() << warn;
            }
        });

        // 2. frameTick: forwarded from RadioModel so we never need to
        //    re-wire when m_connection is recreated.
        connect(m_radioModel, &RadioModel::frameReceived,
                seg, &ConnectionSegment::frameTick);

        // 3. 1 Hz rate refresh — polls connection().{tx,rx}ByteRate(1000).
        auto* rateTimer = new QTimer(this);
        rateTimer->setInterval(1000);
        connect(rateTimer, &QTimer::timeout, this, [this, seg]() {
            if (auto* conn = m_radioModel->connection()) {
                // setRates(rxMbps, txMbps) — first arg names the "radio→client"
                // direction (m_rxMbps), second names "client→radio" (m_txMbps).
                // Earlier revisions passed these reversed, which made the ▲/▼
                // glyphs read in radio perspective rather than the client's.
                // Spec §Affordances reads the segment from the operator's
                // (client's) point of view: ▲ = NereusSDR uploading to radio
                // (commands), ▼ = radio downloading to NereusSDR (I/Q).
                seg->setRates(conn->rxByteRate(1000), conn->txByteRate(1000));
            }
        });
        rateTimer->start();

        // 4. RTT — wire from RadioConnection::pingRttMeasured.
        //    Re-wired on every Connecting/Probing transition so the segment
        //    always follows the live connection object (RadioModel recreates
        //    RadioConnection on each connect cycle).
        auto wireRtt = [this, seg]() {
            if (auto* conn = m_radioModel->connection()) {
                connect(conn, &RadioConnection::pingRttMeasured,
                        seg, &ConnectionSegment::setRttMs,
                        Qt::UniqueConnection);
                // Paketverlust auf demselben Weg — dieselbe Neuverdrahtung
                // bei jedem Verbindungsversuch, weil RadioModel das
                // Verbindungsobjekt jedes Mal neu anlegt.
                connect(conn, &RadioConnection::iqPacketLoss,
                        seg, &ConnectionSegment::onIqPacketLoss,
                        Qt::UniqueConnection);
            }
        };
        wireRtt();
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [wireRtt](ConnectionState s) {
                    if (s == ConnectionState::Connecting ||
                        s == ConnectionState::Probing) {
                        wireRtt();
                    }
                });

        // 5. Audio flow-state → ♪ pip color.
        if (auto* engine = m_radioModel->audioEngine()) {
            connect(engine, &AudioEngine::flowStateChanged,
                    seg, &ConnectionSegment::setAudioFlowState);
        }

        // 6. Click affordances.
        // The segment's mousePressEvent (TitleBar.cpp:382-387) routes
        // anywhere-click in the disconnected state to rttClicked so a
        // single signal covers both "click for diagnostics" (when
        // connected) and "click to connect" (when disconnected). Branch
        // here on the live connection state to honor the "Click to
        // connect" affordance the segment paints — without this branch,
        // a disconnected click trapped the user in NetworkDiagnostics
        // instead of opening the connection panel (Codex P2 review
        // against PR #158, MainWindow.cpp:482).
        connect(seg, &ConnectionSegment::rttClicked, this, [this]() {
            const auto state = m_radioModel->connectionState();
            if (state == ConnectionState::Disconnected
                || state == ConnectionState::LinkLost) {
                showConnectionPanel();
                return;
            }
            auto* dlg = new NetworkDiagnosticsDialog(
                m_radioModel, m_radioModel->audioEngine(), this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
        connect(seg, &ConnectionSegment::audioPipClicked, this, [this]() {
            // Audio pip click also opens diagnostics — audio section
            // is the most relevant panel for pip trouble-shooting.
            auto* dlg = new NetworkDiagnosticsDialog(
                m_radioModel, m_radioModel->audioEngine(), this);
            dlg->setAttribute(Qt::WA_DeleteOnClose);
            dlg->show();
        });
        connect(seg, &ConnectionSegment::contextMenuRequested,
                this, &MainWindow::showSegmentContextMenu);

        // 7. D.3: Hover tooltip — event filter delivers QHelpEvent.
        seg->setToolTip(QString());   // suppress Qt's own tooltip; we intercept
        seg->setAttribute(Qt::WA_AlwaysShowToolTips, false);
        seg->installEventFilter(this);

        // Seed with current state (Disconnected at launch).
        seg->setState(m_radioModel->connectionState());
    }

    buildStatusBar();
    applyDarkTheme();

    // Wire connection state changes to status bar
    connect(m_radioModel, &RadioModel::connectionStateChanged,
            this, &MainWindow::onConnectionStateChanged);

    // Issue #118 — show a transient status-bar message when a band-button
    // click short-circuits (locked slice, XVTR without transverter config).
    // Prevents silent failure — the user sees why nothing happened.
    connect(m_radioModel, &RadioModel::bandClickIgnored,
            this, [this](Band /*band*/, const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 3000);
    });

    // Jeder gescheiterte Verbindungsversuch sagt jetzt, WARUM — auch der
    // von Hand angestossene. 14 Sekunden, weil der Text zwei Saetze hat
    // und der Bediener in dem Moment ohnehin ratlos ist.
    //
    // Review-Fund 2026-08-28: eigene Wache, unabhaengig von der beim
    // Verbinden-Dialog (m_connectionPanelAutoOpenedThisEpisode) -- ohne
    // sie stapelt ein unbegrenzter automatischer Wiederholversuch
    // (P2-Stillewaechter) einen 14-Sekunden-Hinweis nach dem anderen,
    // solange die Stoerung anhaelt. Einmal je Ausfall reicht.
    connect(m_radioModel, &RadioModel::connectAttemptFailed,
            this, [this](Longpath::ConnectFailure, const QString& detail) {
        if (detail.isEmpty()) { return; }
        if (m_connectFailedToastShownThisEpisode) { return; }
        m_connectFailedToastShownThisEpisode = true;
        showToast(detail, ToastSeverity::Warning, 14000);
    });

    // WDSP wisdom progress dialog — shown as a modal window during first-run
    // wisdom generation. Pattern from AetherSDR MainWindow::enableNr2WithWisdom().
    connect(m_radioModel->wdspEngine(), &WdspEngine::wisdomProgress,
            this, [this](int percent, const QString& status) {
        // Create dialog on first progress signal
        if (!m_wisdomDialog && percent < 100) {
            m_wisdomDialog = new QProgressDialog(this);
            m_wisdomDialog->setWindowTitle(QStringLiteral("Longpath — FFTW Wisdom"));
            m_wisdomDialog->setLabelText(
                QStringLiteral("Optimizing FFT plans for DSP engine...\n\n"
                               "This only happens on first run."));
            m_wisdomDialog->setRange(0, 100);
            m_wisdomDialog->setValue(0);
            m_wisdomDialog->setCancelButton(nullptr);
            m_wisdomDialog->setAutoClose(false);
            m_wisdomDialog->setMinimumWidth(500);
            m_wisdomDialog->setMinimumDuration(0);
            m_wisdomDialog->setWindowModality(Qt::ApplicationModal);
            m_wisdomDialog->setStyleSheet(Style::themed(QStringLiteral(
                "QProgressDialog { background: #0f0f1a; }"
                "QLabel { color: #c8d8e8; font-size: 13px; }"
                "QProgressBar {"
                "  text-align: center; font-size: 13px;"
                "  font-weight: bold; color: #c8d8e8;"
                "  background: #1a2a3a; border: 1px solid #205070;"
                "  border-radius: 6px; min-height: 24px;"
                "}"
                "QProgressBar::chunk { background: #4a7ba8; }")));
            m_wisdomDialog->show();
        }

        if (m_wisdomDialog) {
            m_wisdomDialog->setValue(percent);
            if (!status.isEmpty() && percent < 100) {
                m_wisdomDialog->setLabelText(
                    QStringLiteral("Optimizing FFT plans for DSP engine...\n\n%1").arg(status));
            }
            if (percent >= 100) {
                m_wisdomDialog->setLabelText(QStringLiteral("FFTW planning complete!"));
                m_wisdomDialog->setValue(100);
                // Auto-close after brief delay
                QTimer::singleShot(800, this, [this]() {
                    if (m_wisdomDialog) {
                        m_wisdomDialog->close();
                        m_wisdomDialog->deleteLater();
                        m_wisdomDialog = nullptr;
                    }
                });
            }
        }
    });

    // Start discovery in background so radios are found before the user opens the panel
    m_radioModel->discovery()->startDiscovery();

    // Open the Connect-to-Radio panel — deferred so the event loop is running
    // before any signal/slot activity (e.g. discovery radioDiscovered).
    // No longer attempts an automatic connection first: that feature was
    // removed 2026-08-27 (operator decision, OE5SOS — it kept silently
    // re-arming itself and auto-connecting was actively unwanted, not just
    // unreliable over a flaky WLAN link).
    QTimer::singleShot(0, this, &MainWindow::openConnectionPanelOnLaunch);

    // Phase 3J-2 + 3R M3: restore each spot client's auto-connect /
    // auto-start state. Sibling to the panel-open call above; deferred via
    // singleShot(0, ...) so the QTcpSocket / QUdpSocket / QWebSocket
    // owned by each client see a fully-spun event loop before any
    // network I/O fires. Each client guards against double-start so
    // re-invocation is harmless (e.g. if a future code path also calls
    // this after a profile switch).
    //
    // 2026-05-18 bench fix: seed the SpectrumWidget per-source visibility
    // mask BEFORE restoring any auto-start state.  Without this, the
    // panadapter overlay mask (m_spotSourceVisible) is empty at startup
    // and the renderer's missing-key fallback (true == visible) leaks
    // every source's spots regardless of the user's "Show on panadapter"
    // checkbox state.  Bench operator hit it with FreeDV: FreeDvAutoStart
    // + Display checkbox unchecked = spots still painting at app start
    // until SpotHub was opened manually.  Six of seven sources share the
    // same symmetry break (PSK Reporter is send-only); fixing it here
    // closes the gap for all of them in one place.  The matching seed
    // inside openSpotHub() is now a defensive guard kept for the case
    // where the spectrum widget races construction; see lines below.
    QTimer::singleShot(0, this, [this] {
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->loadSpotDisplaySettings();
        }
        if (m_radioModel) {
            m_radioModel->restoreSpotClientAutoStartState();
        }
    });

    // Phase 3O Sub-Phase 11/12 — VAX first-run / startup rescan.
    // The Setup → Audio → VAX page (AudioVaxPage) is now live; users
    // who skip the first-run dialog can reach cable binding via
    // Setup → Audio → VAX at any time.
    // Deferred via singleShot(0, ...) so the UI is fully built first.
    QTimer::singleShot(0, this, &MainWindow::checkVaxFirstRun);

    // Task 23 — auto-open the Linux audio first-run dialog when no backend
    // is detected on this host. The check is deferred via singleShot(0, ...)
    // so the event loop is spinning before the modal dialog is posted.
    // The dialog's Dismiss button (Task 18) sets Audio/LinuxFirstRunSeen=True,
    // so this trigger fires at most once per user installation.
#if defined(Q_OS_LINUX)
    if (m_radioModel->audioEngine()->linuxBackend() == LinuxAudioBackend::None
        && AppSettings::instance().value(QStringLiteral("Audio/LinuxFirstRunSeen"),
                                          QStringLiteral("False")).toString()
               != QStringLiteral("True")) {
        QTimer::singleShot(0, this, &MainWindow::showAudioDiagnoseDialog);
    }
#endif

    // ── Phase 23: TCI Server instantiation ───────────────────────────────────
    // Phase 23 (bench fix 2026-05-10): TciServer instance is now created in
    // the constructor BEFORE buildUI (above) so populateDefaultMeter's applet
    // gate sees a non-null m_tciServer. Auto-start happens HERE (post-UI) so
    // serverStarted/serverStopped signals find the applets + indicator wired.
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer) {
        auto& s = AppSettings::instance();
        const bool enabled = s.value(QStringLiteral("TciServerEnabled"),
                                     QStringLiteral("False")).toString()
                             == QStringLiteral("True");
        const quint16 port = static_cast<quint16>(
            s.value(QStringLiteral("TciServerPort"),
                    QStringLiteral("50001")).toString().toUShort());
        // Phase 3J-1 closeout Item 1 (2026-05-12): bind-interface dropdown.
        // Default to loopback so a fresh install only exposes TCI to localhost;
        // operator opts into LAN exposure via Setup → CAT & Network → TCI Server.
        const QString bindStr = s.value(QStringLiteral("TciServerBindAddress"),
                                        QStringLiteral("127.0.0.1")).toString();
        QHostAddress bindAddr;
        if (!bindAddr.setAddress(bindStr)) {
            // Malformed AppSettings value — fall back to loopback rather than
            // refusing to start the server.  populateBindAddressCombo() ensures
            // only valid strings get persisted, but a hand-edited XML file
            // shouldn't crash startup.
            bindAddr = QHostAddress(QHostAddress::LocalHost);
        }
        if (enabled) {
            m_tciServer->start(bindAddr, port);
        }
    }
#endif

    // Defensive save on aboutToQuit. closeEvent is fine for ⌘Q but
    // does NOT run when the process is signaled (SIGTERM from pkill,
    // Activity Monitor force-quit, debugger detach). Without this
    // hook, any container/state change made mid-session is lost on
    // signal-based shutdown. saveState is idempotent so the ⌘Q path
    // (closeEvent → saveState; aboutToQuit → saveState again) is
    // harmless. (Preserved from main PR #13 alongside Phase 3I's
    // singleShot auto-reconnect above.)
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        // Betreiber 2026-09-02, "das letzte Profil ist nie automatisch
        // sichtbar" (zum wiederholten Mal gemeldet): dieser Handler kam
        // am 2026-08-31 als Rueckfallpfad fuer den roten-Punkt-Weg dazu,
        // OHNE die Sperre, die closeEvent() fuer den ANDEREN
        // aboutToQuit-Handler (wireProfileRail(), siehe dessen Kommentar
        // "prueft jetzt m_shuttingDown und tut auf diesem Weg nichts
        // mehr") extra deswegen eingefuehrt hat -- Absturzbericht
        // 2026-08-27 23:10, Cocoa loest aboutToQuit REENTRANT aus,
        // waehrend closeEvent() noch auf dem Stapel steht. Ohne dieselbe
        // Sperre HIER lief genau das weiter: closeEvent() erfasst den
        // richtigen Stand zuerst (SOFORT, ganz am Anfang von closeEvent),
        // dieser Handler erfasst DANACH nochmal -- unbedingt, auch wenn
        // laengst m_shuttingDown gilt -- und ueberschreibt die richtige
        // Erfassung mit dem halb abgebauten Zwischenstand. "Kein Fehler
        // im Log" (jeder Schritt lief durch) UND "nie der richtige
        // Letztzustand" (der zweite, unbeaufsichtigte Schreibvorgang
        // gewinnt) sind damit kein Widerspruch. Fuer den Weg, den dieser
        // Handler eigentlich abdeckt (SIGTERM/roter Punkt, closeEvent()
        // laeuft dort NIE), bleibt er unveraendert wirksam: dort steht
        // m_shuttingDown beim ersten und einzigen Aufruf noch auf false.
        if (m_shuttingDown) { return; }
        // Same flush as closeEvent — covers the signal-shutdown path
        // (SIGTERM, force-quit, debugger detach) where closeEvent
        // doesn't run. Idempotent when closeEvent already flushed.
        m_shuttingDown = true;
        // Betreiber 2026-08-31: dieselbe Sperre wie in closeEvent() --
        // schadet hier nichts (die normale QObject-Elternschaft loest
        // ohnehin kein QCloseEvent aus, siehe dockRotorPanel()'s
        // Kommentar), schuetzt aber jeden kuenftigen Weg, der einem
        // dieser Fenster doch ein echtes QCloseEvent zustellt.
        if (m_panStack) { m_panStack->setShuttingDown(true); }
        if (m_rotorWindow) { m_rotorWindow->setShuttingDown(true); }
        // 2026-05-22 bench-finding: graceful radio disconnect MUST happen
        // before the process tears down so the SendStop frame (run=0
        // CmdHighPriority) actually reaches the wire.  Without this,
        // pkill / SIGTERM / closing the window via the dock all skip
        // P2RadioConnection::disconnect, the radio gateware never sees
        // run=0, and lockups on the G2E require power-cycle.  Call
        // disconnectFromRadio FIRST so its 20 ms flush+sleep runs before
        // any other shutdown cleanup.
        if (m_radioModel) {
            m_radioModel->disconnectFromRadio();
            m_radioModel->flushPendingSettingsSave();
        }
        if (m_containerManager) {
            m_containerManager->saveState();
        }
        // Betreiber 2026-08-31: fehlte hier bislang, steht aber in
        // closeEvent() (Phase 3F Sub-Epic D Task 15) -- Panadapter-
        // Aufteilung/Splittergroessen (PanLayoutId + PanLayoutSplitter_*)
        // wurden ueber DIESEN Beenden-Weg also nie gesichert, nur ueber
        // den closeEvent()-Pfad, den dieser Nutzer nachweislich nie
        // durchlaeuft.
        if (m_panStack) { m_panStack->saveSplitterState(); }
        // Betreiber 2026-08-31, per Log bewiesen: sein Beenden-Weg
        // ("roter Punkt") liefert NIE ein QCloseEvent an MainWindow --
        // in fuenf aufeinanderfolgenden Log-Dateien taucht die
        // Diagnosezeile aus closeEvent() kein einziges Mal auf, obwohl
        // der Funkgeraete-Rueckbau (WDSP/TCI/P2, alles unten in diesem
        // Block) sichtbar sauber laeuft -- dieser aboutToQuit-Zweig ist
        // fuer ihn also nicht der Rueckfallpfad, sondern der EINZIGE.
        // captureIntoCurrent()+save() standen bisher nur in
        // closeEvent() (Annahme im Kommentar oben: "closeEvent is fine
        // for Cmd+Q" -- stimmte fuer diesen Beenden-Weg nicht). Ohne
        // diese zwei Zeilen hier wurde der Profilstand beim Beenden nie
        // aus der laufenden Oberflaeche neu eingesammelt, sondern blieb
        // auf dem Stand der letzten Stelle, die captureIntoCurrent()
        // sonst noch traf (Menue-Haken, Applet-Ablösen) -- fuer den
        // Betreiber sah das aus wie "der gespeicherte Zustand ist nie
        // der Letztzustand".
        if (m_layoutProfiles) {
            m_layoutProfiles->captureIntoCurrent();
            m_layoutProfiles->save();
            const QVariantMap snap =
                m_layoutProfiles->snapshot(m_layoutProfiles->current());
            qWarning() << "[ProfileSaveOnQuit:aboutToQuit]"
                       << m_layoutProfiles->current()
                       << "floatingApplets="
                       << snap.value(QStringLiteral("floatingApplets")).toMap().size()
                       << "floatingPans="
                       << snap.value(QStringLiteral("floatingPans")).toMap().size()
                       << "rotor=" << snap.value(QStringLiteral("rotor")).toMap();
        }
        // Issue #206 — also flush window geometry on signal-based
        // shutdown (SIGTERM / force-quit). Idempotent with the
        // closeEvent path above.
        saveMainWindowGeometry();
        AppSettings::instance().save();
        qWarning() << "[ProfileSaveOnQuit:aboutToQuit] AppSettings::save() done";
    });
}

MainWindow::~MainWindow()
{
    // Sicherheitsnetz, 2026-09-03: regulaer haelt closeEvent() den
    // SpectrumThread an, lange bevor es hierher kommt. Wird ein
    // MainWindow aber OHNE closeEvent() zerstoert (Ausnahmepfad, Test-
    // Harness), loescht ~QObject gleich darauf m_fftThread als Kind --
    // und Qt bricht mit qFatal ab, wenn der Faden da noch laeuft. Der
    // Destruktor-Rumpf laeuft VOR dem Abbau der Kinder; hier ist der
    // letzte Ort, an dem sich das noch abfangen laesst. Im Normalfall
    // ist isRunning() hier bereits false und die Zeile kostet nichts.
    if (m_fftThread && m_fftThread->isRunning()) {
        m_fftThread->quit();
        m_fftThread->wait();
    }
}

// Phase 3F Sub-Epic D Task 12: resolve the active pan's SpectrumWidget.
// Used as a backward-compat shim for call sites that still address "the"
// spectrum widget; long-term these should migrate to per-pan addressing.
// Returns nullptr if m_panStack isn't constructed yet (during early init)
// or if the active pan has no widget.
SpectrumWidget* MainWindow::activeSpectrumWidget() const
{
    if (!m_panStack) { return nullptr; }
    auto* applet = m_panStack->panadapter(m_panStack->activePanId());
    return applet ? applet->spectrumWidget() : nullptr;
}

// Phase 3F multi-pan: resolve the SpectrumWidget that owns this slice's
// panadapter from the slice's panKey(), falling back to the active pan when
// the key is empty (Slice A pre-seed) or the pan was removed.
// Ported from AetherSDR MainWindow::spectrumForSlice (MainWindow.cpp:14856
// [@6a142807]); AetherSDR uses s->panId() (a string), NereusSDR uses
// s->panKey().
SpectrumWidget* MainWindow::spectrumForSlice(SliceModel* s) const
{
    if (s && m_panStack) {
        if (auto* sw = m_panStack->spectrum(s->panKey())) {
            return sw;
        }
    }
    return activeSpectrumWidget();  // fallback to active pan
}

// Herausgezogen aus rebuildFftRouting() (2026-08-24), damit der SunSDR-Weg
// (MainWindow_SunSdr.cpp) dieselbe Suche verwenden kann statt sie zu
// verdoppeln -- reine Verschiebung, kein Verhaltensunterschied.
//
// Resolving the pan is not just slice->panKey(). Slice A never has
// one: RadioModel::addSlice stamps panKey only when it is given one
// and connectToRadio calls the no-argument overload
// (RadioModel.cpp:4137), so Slice A's panKey is permanently empty.
// Skipping empties (as the Task 9 spec had it) would leave pan 0
// with no subscription and no trace at all.
//
// Fall back to the pan that actually hosts the slice, recorded by
// the sliceAdded handler through PanadapterApplet::addSlice. That
// record is stable; activePanId() is not, and using it alone would
// migrate Slice A's subscription (and darken pan 0) the moment the
// operator made another pan active. activePanId() stays as the last
// resort, matching spectrumForSlice (MainWindow.cpp:880).
QString MainWindow::panIdForSlice(SliceModel* slice) const
{
    if (!slice || !m_panStack) { return QString(); }
    QString panId = slice->panKey();
    if (panId.isEmpty() || !m_panStack->panadapter(panId)) {
        panId.clear();
        for (auto* applet : m_panStack->allApplets()) {
            if (applet
                && applet->associatedSlices().contains(slice->sliceIndex())) {
                panId = applet->panId();
                break;
            }
        }
    }
    if (panId.isEmpty()) { panId = m_panStack->activePanId(); }
    return panId;
}

SliceModel* MainWindow::sliceForAddedIdForTest(RadioModel* model, int sliceId)
{
    return model ? model->sliceById(sliceId) : nullptr;
}

// ── Die Schiene und ihre Dialoge ─────────────────────────────────────
//
// ProfileRail kennt keinen Dialog. Sie meldet nur, was der Betreiber
// will; den Namen erfragt das Fenster. Damit bleibt die Schiene ohne
// laufende Oberfläche prüfbar, und die Dialoge liegen dort, wo die
// anderen Dialoge dieses Programms auch liegen.
QString MainWindow::screenKeyFor(const QWidget* w)
{
    const QScreen* s = (w && w->window()) ? w->window()->screen() : nullptr;
    if (!s) { return QString{}; }
    const QString serial = s->serialNumber();
    return serial.isEmpty() ? s->name() : serial;
}

// Die Aufloesung selbst steht in AppletKeys — als freie Funktion ueber
// einer uebergebenen Karte, weil MainWindow sich im Pruefstand nicht
// bauen laesst (ein blosses `MainWindow w;` startet echte UDP-Suche).
// Eine Aufloesung, die niemand pruefen kann, ist genau die Art Code, in
// der dieser Fehler entstanden ist.
QString MainWindow::panelIdFor(const AppletWidget* applet) const
{
    return AppletKeys::panelIdFor(m_appletsById, applet);
}

AppletWidget* MainWindow::appletForKey(const QString& key) const
{
    return AppletKeys::appletFor(m_appletsById, key);
}

QString MainWindow::canonicalAppletKey(const QString& key) const
{
    return AppletKeys::canonical(m_appletsById, key);
}

// ── Ein Applet auf die freie Flaeche legen ───────────────────────────
//
// OE5SOS, 2026-08-20, zum zehnten Mal: „man kann alles x beliebig
// verschieben! das geht bei uns nicht!"
//
// Er hat recht, und der Grund ist keine fehlende Taste: unsere
// Oberflaeche ist ein QSplitter-Geruest — Spektrum links, Applet-STAPEL
// rechts, Rotor unten. Ein Stapel hat keine Positionen, nur eine
// Reihenfolge. Da hilft kein Knopf.
//
// Zeus Link legt jedes Fenster als KACHEL mit eigener Position auf eine
// freie Flaeche (Bildschirmvideo 2026-08-20: neues Layout = leere
// Flaeche, Fenster einzeln hinzugefuegt, frei geschoben, an den Ecken
// gezogen, aneinander geschnappt).
//
// Genau das kann ContainerWidget im Zustand OverlayDocked seit Phase
// 3G-1: absolute Lage ueber dem Hauptbereich, Ziehen an der
// Titelleiste, Groesse an der Ecke, Schloss. Was fehlte, war der Weg
// von einem Applet DORTHIN — detachApplet macht ein eigenes
// Betriebssystem-Fenster daraus, was etwas anderes ist.
//
// Hier ist der Weg. Das Applet verlaesst den Stapel und bekommt eine
// Kachel; von da an gilt fuer es alles, was fuer Container gilt.
// ── Die Lagen der Kacheln merken ─────────────────────────────────────
//
// Format: „Rx:40,40,360,260;Tx:66,66,360,260" — Panelkennung und
// Rechteck, mit Strichpunkt getrennt.
//
// EIGENE Speicherung und nicht die des ContainerManagers: dessen
// saveState() schreibt Instrumentenbehaelter, deren INHALT mitgehoert.
// Bei einer Applet-Kachel gehoert der Inhalt woanders hin — er lebt im
// Applet-Stapel und wird ueber die Sichtbarkeit gefuehrt. Beides in
// denselben Topf zu werfen hiesse, dass beim Start zwei Stellen
// dasselbe Fenster herstellen wollen.
// Welche anderen Fenster koennen in diese Kachel? Das Menue am
// Zahnrad der Kachel.
void MainWindow::showTileTabMenu(const QString& tileId)
{
    if (!m_containerManager || !m_appletVis) { return; }
    ContainerWidget* tile = m_containerManager->container(tileId);
    if (!tile) { return; }

    QMenu menu(this);
    menu.setStyleSheet(QString::fromLatin1(kPopupMenu));
    menu.addSection(QStringLiteral("Fenster hierher holen"));

    QHash<QAction*, QString> byAction;
    for (auto it = m_canvasApplets.constBegin();
         it != m_canvasApplets.constEnd(); ++it) {
        if (it.value() == tileId) { continue; }   // sich selbst nicht
        QAction* a = menu.addAction(m_appletVis->displayName(it.key()));
        byAction.insert(a, it.key());
    }

    if (byAction.isEmpty()) {
        QAction* none = menu.addAction(
            QStringLiteral("keine anderen freien Fenster"));
        none->setEnabled(false);
    }

    QAction* chosen = menu.exec(QCursor::pos());
    if (!chosen || !byAction.contains(chosen)) { return; }

    const QString otherId = byAction.value(chosen);
    AppletWidget* a = m_appletsById.value(otherId, nullptr);
    if (!a) { return; }

    // Aus seiner eigenen Kachel heraus, in diese hinein.
    const QString otherTileId = m_canvasApplets.take(otherId);
    if (!otherTileId.isEmpty()
        && m_containerManager->container(otherTileId)) {
        // Erst herausnehmen, dann abraeumen — sonst nimmt
        // destroyContainer das Applet als Kind mit ins Grab.
        a->setParent(nullptr);
        m_containerManager->destroyContainer(otherTileId);
    }
    tile->addTab(a, m_appletVis->displayName(otherId));
    a->show();

    // Der Reiter gehoert jetzt zu DIESER Kachel. Die Zuordnung muss
    // mit, sonst findet returnAppletFromCanvas ihn nicht wieder.
    m_canvasApplets.insert(otherId, tileId);
    saveCanvasLayout();
}

// Ein Reiter wird wieder eigenstaendig.
void MainWindow::detachTabToOwnTile(const QString& containerId, int index)
{
    if (!m_containerManager) { return; }
    ContainerWidget* tile = m_containerManager->container(containerId);
    if (!tile) { return; }

    QWidget* w = tile->takeTab(index);
    auto* a = qobject_cast<AppletWidget*>(w);
    if (!a) { return; }

    const QString id = panelIdFor(a);
    if (!id.isEmpty()) { m_canvasApplets.remove(id); }

    // Ueber den gewoehnlichen Weg zurueck: erst in den Stapel, dann
    // auf die Flaeche. So laeuft es durch dieselbe Stelle wie jedes
    // andere Freistellen und kann nicht auseinanderlaufen.
    if (m_appletPanel && !m_appletPanel->applets().contains(a)) {
        m_appletPanel->addApplet(a);
    }
    moveAppletToCanvas(a);
}

void MainWindow::saveCanvasLayout()
{
    QStringList parts;
    for (auto it = m_canvasApplets.constBegin();
         it != m_canvasApplets.constEnd(); ++it) {
        ContainerWidget* tile = m_containerManager
            ? m_containerManager->container(it.value()) : nullptr;
        if (!tile) { continue; }
        const QRect r(tile->pos(), tile->size());
        parts << QStringLiteral("%1:%2,%3,%4,%5")
                     .arg(it.key())
                     .arg(r.x()).arg(r.y()).arg(r.width()).arg(r.height());
    }
    AppSettings::instance().setValue(QStringLiteral("CanvasApplets"),
                                     parts.join(QLatin1Char(';')));
}

// Beim Start wiederherstellen.
//
// NACH der Sichtbarkeit aufrufen: ein Applet, das gar nicht sichtbar
// ist, bekommt keine Kachel. Und nur wenn die freie Flaeche ansteht —
// sonst wuerde ein einmal freigestelltes Fenster fuer immer
// freigestellt bleiben, auch nachdem der Betreiber den Schalter wieder
// ausgemacht hat.
void MainWindow::restoreCanvasLayout()
{
    if (!m_freeCanvasMode || !m_appletPanel || !m_containerManager) { return; }

    const QString blob = AppSettings::instance()
        .value(QStringLiteral("CanvasApplets"), QString{}).toString();
    if (blob.isEmpty()) { return; }

    for (const QString& entry : blob.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
        const int colon = entry.indexOf(QLatin1Char(':'));
        if (colon <= 0) { continue; }
        const QString id = entry.left(colon);
        const QStringList n = entry.mid(colon + 1).split(QLatin1Char(','));
        if (n.size() != 4) { continue; }

        AppletWidget* a = m_appletsById.value(id, nullptr);
        if (!a) { continue; }
        if (m_appletVis && !m_appletVis->isEffectivelyVisible(id)) { continue; }

        moveAppletToCanvas(a);

        const QString tileId = m_canvasApplets.value(id);
        ContainerWidget* tile = m_containerManager->container(tileId);
        if (!tile) { continue; }

        bool ok = true;
        const QPoint pos(n[0].toInt(&ok), n[1].toInt(&ok));
        const QSize  sz (n[2].toInt(&ok), n[3].toInt(&ok));
        if (!ok || sz.width() < 80 || sz.height() < 60) { continue; }

        tile->setDockedLocation(pos);
        tile->setDockedSize(sz);
        tile->restoreLocation();
    }
}

void MainWindow::moveAppletToCanvas(AppletWidget* applet)
{
    if (!applet || !m_appletPanel || !m_containerManager) { return; }

    const QString id = panelIdFor(applet);
    if (id.isEmpty() || m_canvasApplets.contains(id)) { return; }

    // Erst aus dem Stapel nehmen, dann umhaengen. Andersherum haelt das
    // Panel einen Zeiger auf ein Widget, das schon woanders lebt.
    const int dockIndex = m_appletPanel->appletPosition(applet);
    m_appletPanel->removeApplet(applet);

    ContainerWidget* tile =
        m_containerManager->createContainer(1, DockMode::OverlayDocked);
    if (!tile) { return; }

    tile->setNotes(m_appletVis ? m_appletVis->displayName(id) : id);
    tile->setContent(applet);
    applet->show();

    // Versetzt legen, nicht gestapelt: zwei Kacheln genau uebereinander
    // sehen aus wie eine, und man sucht die zweite.
    const int n = static_cast<int>(m_canvasApplets.size());
    tile->setDockedLocation(QPoint(40 + 26 * n, 40 + 26 * n));
    tile->setDockedSize(QSize(360, 260));
    tile->restoreLocation();
    tile->show();
    tile->raise();

    // ── Fenster hierher holen ────────────────────────────────────
    //
    // Zeus fasst Fenster durch Ziehen-und-Fallenlassen zu Reitern
    // zusammen. Das ist die schoenere Geste und der riskantere Weg:
    // ein Wurf ueber Fenstergrenzen mit QRhiWidgets darin ist genau
    // die Stelle, an der AetherSDR abstuerzte (#2495, #4319) — und
    // der Grund, warum unser Applet-Ziehen bis heute auf das
    // Umsortieren in der Spalte beschraenkt ist.
    //
    // Deshalb erst ueber das Menue. Erreichbar ist wichtiger als
    // elegant; ein Wurf, den man nicht findet, hilft niemandem, und
    // einer, der abstuerzt, hilft noch weniger.
    connect(tile, &ContainerWidget::settingsRequested, this,
            [this, tileId = tile->id()]() { showTileTabMenu(tileId); });

    connect(tile, &ContainerWidget::tabDetachRequested, this,
            &MainWindow::detachTabToOwnTile);

    // Diese Kachel speichert ihre Lage SELBST — der ContainerManager
    // soll sie nicht mitschreiben, sonst kaeme beim naechsten Start ein
    // leerer Rahmen zurueck, waehrend das Applet im Stapel liegt.
    m_containerManager->setPersisted(tile->id(), false);

    m_canvasApplets.insert(id, tile->id());

    // Beim Ziehen und beim Groessenaendern mitschreiben. Ohne das
    // ueberlebt eine Anordnung den Neustart nicht — und eine Anordnung,
    // die man jeden Morgen neu herstellen muss, ist keine.
    connect(tile, &ContainerWidget::dockedMoved, this,
            [this]() { saveCanvasLayout(); });

    saveCanvasLayout();

    Q_UNUSED(dockIndex)

    if (m_layoutProfiles) {
        m_layoutProfiles->captureIntoCurrent();
        m_layoutProfiles->save();
    }
}

// Alles auf einmal. „es muss alles auf den mm verschoben werden
// koennen. jedes window! jeder panel, ueberall" (OE5SOS, 2026-08-20).
//
// Ueber eine Kopie der Liste: moveAppletToCanvas nimmt das Applet aus
// m_applets heraus, und wer waehrend des Herausnehmens ueber dieselbe
// Liste laeuft, ueberspringt jedes zweite.
// Der Rueckweg: die Kachel aufloesen, das Applet in den Stapel.
//
// Ohne ihn bliebe beim Ausblenden eine leere Kachel stehen und das
// Applet waere nirgends — es taucht dann auch ueber das Plus nicht
// wieder auf, weil applyAppletVisibility es im Stapel sucht.
// Die freie Flaeche an- oder abschalten. Gemerkt, damit sie einen
// Neustart ueberlebt — eine Anordnung, die man jeden Morgen neu
// herstellen muss, ist keine.
void MainWindow::setFreeCanvasMode(bool on)
{
    if (m_freeCanvasMode == on) { return; }
    m_freeCanvasMode = on;
    AppSettings::instance().setValue(QStringLiteral("FreeCanvasMode"),
                                     on ? QStringLiteral("True")
                                        : QStringLiteral("False"));
    if (m_freeCanvasAction) {
        QSignalBlocker b(m_freeCanvasAction);
        m_freeCanvasAction->setChecked(on);
    }
}

void MainWindow::returnAppletFromCanvas(const QString& id)
{
    const QString tileId = m_canvasApplets.take(id);
    if (tileId.isEmpty() || !m_containerManager) { return; }

    AppletWidget* a = m_appletsById.value(id, nullptr);
    if (a) {
        // Erst herausnehmen, dann die Kachel abraeumen — andersherum
        // nimmt destroyContainer das Applet als Kind mit ins Grab.
        a->setParent(nullptr);
    }
    m_containerManager->destroyContainer(tileId);
    saveCanvasLayout();

    if (a && m_appletPanel && !m_appletPanel->applets().contains(a)) {
        m_appletPanel->addApplet(a);
        m_appletPanel->setAppletVisible(a, false);
    }
}

// ── WARUM HIER ECHTE FENSTER STEHEN UND KEINE KACHELN ────────────────
//
// Am 2026-08-20 habe ich einen halben Tag lang Kacheln gebaut:
// ContainerWidget im Zustand OverlayDocked, absolute Lage ueber dem
// Hauptbereich, Ziehen an der Titelleiste. Die Tests waren gruen, auf
// dem Schirm aenderte sich NICHTS. Der Betreiber hat es viermal
// gemeldet, bevor ich den Grund gesucht habe statt weiterzubauen.
//
// Der Grund steht in unserem eigenen Quelltext, SpectrumWidget.cpp:551:
//
//     „QRhiWidget with WA_NativeWindow on macOS does not support
//      child widget overlays"
//
// Der Panadapter ist ein NATIVES Fenster (setAttribute(WA_NativeWindow),
// SpectrumWidget.cpp:519). Auf macOS zeichnet ein natives NSView immer
// UEBER allen nicht-nativen Geschwistern — unabhaengig von raise() und
// unabhaengig von der Reihenfolge im Baum. Die Kacheln hingen als
// Kinder derselben Flaeche und lagen damit hinter dem Panadapter.
//
// Das ist keine Frage von mehr Muehe. Es ist strukturell. Zeus Link
// kann in-Fenster-Kacheln, weil Zeus im BROWSER laeuft und seine
// „Fenster" DOM-Elemente sind; dort gibt es keine nativen Geschwister.
//
// AetherSDR ist gegen dieselbe Wand gelaufen und hat dieselbe Antwort
// gefunden: FloatingContainerWindow — ECHTE Fenster. Die liegen immer
// oben, lassen sich ueberall hinschieben, in der Groesse aendern, auf
// einen zweiten Schirm ziehen, und ihre Geometrie steht im Profil.
//
// Genau das tut detachApplet, und genau das gab es schon, bevor ich
// angefangen habe. Mein Pfeil im Fensterkopf zeigte eine Zeit lang auf
// den Kachelweg — das war eine Verschlechterung, und sie ist
// zurueckgenommen.
void MainWindow::moveAllAppletsToCanvas()
{
    if (!m_appletPanel) { return; }

    // Kopie der Liste: detachApplet nimmt das Applet aus m_applets
    // heraus, und wer waehrend des Herausnehmens ueber dieselbe Liste
    // laeuft, ueberspringt jedes zweite.
    const QList<AppletWidget*> all = m_appletPanel->applets();
    for (AppletWidget* a : all) {
        detachApplet(a, m_appletPanel->appletPosition(a));
    }

    if (m_layoutProfiles) {
        m_layoutProfiles->captureIntoCurrent();
        m_layoutProfiles->save();
    }
}

void MainWindow::detachApplet(AppletWidget* applet, int dockIndex,
                              const QRect& rect, const QString& screenKey)
{
    if (!applet || !m_appletPanel) { return; }
    // Die PANELKENNUNG, nicht die Eigenkennung. Unter ihr steht die
    // Sichtbarkeit, unter ihr wird das Profil geschrieben — siehe den
    // Abschnitt „EINE Kennung, nicht zwei" in MainWindow.h. Hier stand
    // applet->appletId(), und weil das „rx" statt „Rx" ist, gab
    // isEffectivelyVisible() unten falsch zurueck: das Fenster wurde
    // gebaut und nie gezeigt, das Applet war aus der Spalte heraus und
    // nirgends zu sehen.
    const QString id = panelIdFor(applet);
    if (id.isEmpty() || m_floatingApplets.contains(id)) { return; }

    // ── Der Grund, warum es nur EINEN Weg hierher gibt ───────────────
    //
    // Dieses setParent() geht über eine Top-Level-Grenze, und genau da
    // stürzt Qt ab, wenn ein QRhiWidget im Baum hängt: der Aufräum-
    // Rückruf des alten QRhi feuert später gegen freigegebenen Zustand
    // (AetherSDR #2495; #4319 dieselbe Familie auf D3D11). AetherSDR
    // räumt deshalb vor JEDEM Reparent alle QRhiWidget-Kinder ab
    // (prepareRhiChildrenForReparent), und NereusSDR macht in
    // ContainerManager mit extractMeterItems/installFreshMeter das
    // Gleiche für die Meter-Container.
    //
    // Die zwölf Applets sind heute reine QWidgets — kein Meter, kein
    // Spektrum, keine GPU-Fläche. Deshalb reicht hier ein schlichtes
    // Umhängen. Sobald das nicht mehr stimmt, ist es keine Warnung
    // mehr wert, sondern ein Absturz beim nächsten Beenden; die Prüfung
    // unten macht daraus eine Zeile im Protokoll, die die Ursache
    // benennt, statt eines Sturzes in fremdem Code.
    //
    // inherits() statt findChild<QRhiWidget*>(): der Kopf steht hinter
    // NEREUS_GPU_SPECTRUM, und eine Sicherung, die im falschen Aufbau
    // wegfällt, ist keine.
    for (const QWidget* child : applet->findChildren<QWidget*>()) {
        if (child && child->inherits("QRhiWidget")) {
            qCWarning(lcContainer)
                << "Applet" << id << "enthält ein QRhiWidget ("
                << child->metaObject()->className()
                << ") und wird über eine Top-Level-Grenze umgehängt."
                << "Ohne einen Abräumschritt wie ContainerManager::"
                   "extractMeterItems ist das AetherSDR #2495 — Absturz"
                   " beim Beenden oder verdorbene Darstellung.";
            break;
        }
    }

    // Erst aus der Spalte, dann ins Fenster. removeApplet hängt aus,
    // OHNE zu löschen — das ist die Eigenschaft, auf der das hier
    // beruht. Zwischen den beiden Zeilen hat das Applet keinen
    // Besitzer; deshalb stehen sie direkt beieinander und nichts
    // dazwischen, was scheitern könnte.
    // ── Wo stand die Flaeche? ────────────────────────────────────────
    //
    // Der Betreiber, 2026-08-20: „man muss zuerst den pfeil klicken,
    // dann oeffnet sich das fenster wo anders. man sollte gleich das
    // bestehende fenster verschieben koennen."
    //
    // Ein Fenster, das an einer fremden Stelle aufgeht, sieht aus wie
    // ein NEUES Fenster. Geht es genau dort auf, wo das Applet eben
    // stand, sieht dieselbe Geste aus wie „aufheben" — und danach
    // schiebt man es, wohin man will. Das ist der Unterschied zwischen
    // zwei Schritten und einem.
    //
    // Vor removeApplet greifen: danach ist das Applet aus der Spalte
    // heraus und hat keine Lage auf dem Schirm mehr.
    QRect pickedUpAt;
    if (applet->isVisible()) {
        // Vom Rasterfeld, nicht vom Applet: das Feld schliesst die
        // Kopfleiste ein, und die gehoert zu dem, was der Nutzer als
        // „diese Flaeche" sieht.
        QWidget* box = applet;
        for (QWidget* w = applet->parentWidget(); w; w = w->parentWidget()) {
            if (QString::fromLatin1(w->metaObject()->className())
                    .contains(QStringLiteral("GridCellWidget"))) {
                box = w; break;
            }
        }
        pickedUpAt = QRect(box->mapToGlobal(QPoint(0, 0)), box->size());
    }

    m_appletPanel->removeApplet(applet);
    auto* win = new AppletFloatingWindow(applet, id, dockIndex, this);
    m_floatingApplets.insert(id, win);

    // Kommt eine Geometrie aus dem Profil, gilt sie — und der
    // Bildschirm, auf dem sie stand, entscheidet mit. Steht das Fenster
    // laut Profil auf einem Schirm, der nicht mehr da ist, wird das
    // Rechteck gar nicht erst gesetzt: ensureOnVisibleScreen holt es
    // dann auf den Schirm des Hauptfensters, statt es erst hinaus- und
    // dann wieder hereinzuschieben.
    bool trustedRectApplied = false;
    if (rect.isValid()) {
        bool screenStillHere = screenKey.isEmpty();
        QScreen* matchedScreen = nullptr;
        if (!screenStillHere) {
            for (QScreen* s : QGuiApplication::screens()) {
                if (!s) { continue; }
                const QString key = s->serialNumber().isEmpty()
                                        ? s->name() : s->serialNumber();
                if (key == screenKey) {
                    screenStillHere = true;
                    matchedScreen = s;
                    break;
                }
            }
        }
        if (screenStillHere) {
            // Betreiber 2026-09-02: TX-Fenster liess sich nicht mehr
            // verschieben/vergroessern -- Ursache war nicht das Ziehen
            // selbst, sondern dass es 78 Punkte rechts ueber den
            // Bildschirmrand hinausstand: die Titelleisten-Symbole
            // (Schloss/Einklappen/Schliessen) und der Groessengriff
            // unten rechts (FramelessResizer) lagen physisch ausserhalb
            // des sichtbaren Bereichs, mit der Maus nicht erreichbar.
            //
            // Dieser Zweig prueft nur "gibt es den Bildschirm noch",
            // nicht "passt das Rechteck noch drauf" -- eine auf einem
            // BREITEREN Bildschirm (oder vor einer Aufloesungsaenderung)
            // gespeicherte Position blieb dadurch unangetastet
            // bestehen, obwohl sie den heutigen Bildschirm ueberragt.
            // ensureOnVisibleScreen() waere hier die naheliegende
            // Antwort, liest aber win->geometry() NACH setGeometry() --
            // fuer ein frisch erzeugtes, noch nie gezeigtes natives
            // Fenster ist das auf macOS unzuverlaessig (Kommentar
            // unten). Deshalb hier stattdessen gegen die SCHIRM-eigene
            // (immer verlaessliche) verfuegbare Flaeche klemmen, bevor
            // ueberhaupt gesetzt wird -- kein zweiter, unsicherer
            // Lesevorgang noetig.
            QRect clamped = rect;
            if (matchedScreen) {
                const QRect avail = matchedScreen->availableGeometry();
                const int x = qBound(avail.x(), rect.x(), avail.right() - rect.width());
                const int y = qBound(avail.y(), rect.y(), avail.bottom() - rect.height());
                clamped.moveTopLeft(QPoint(x, y));
            }
            win->setGeometry(clamped);
            trustedRectApplied = true;
        }
    }
    // Betreiber 2026-08-30: "panadapter und filter wieder verrückt" --
    // Bandwidth Filter, Frequenz und S-Meter landeten nach dem Neustart
    // trotz gueltiger, bildschirmgeprueften Profil-Geometrie mittig
    // ueber dem Panadapter statt an ihrer gemerkten Stelle.
    // ensureOnVisibleScreen() liest win->geometry() und vertraut ihr
    // nicht (siehe dort, "atOrigin"/Mindestgroesse) -- fuer ein frisch
    // erzeugtes, noch nie gezeigtes Top-Level-Fenster ist genau dieser
    // Wert auf macOS unmittelbar nach setGeometry() nicht verlaesslich
    // dieselbe Zahl, die eben gesetzt wurde (das native Fenster
    // existiert vor dem ersten show() schlicht noch nicht). Ein Rechteck,
    // das schon durch die eigene Bildschirm-Pruefung oben kam, braucht
    // keine zweite, weniger verlaessliche Pruefung mehr -- die galt
    // ohnehin nur fuer den Fall, dass gar keine brauchbare Geometrie
    // vorlag.
    if (!trustedRectApplied) {
        ensureOnVisibleScreen(win, this,
                              QSize(Style::kAppletPanelW, 120));
    }

    connect(win, &AppletFloatingWindow::dockRequested,
            this, &MainWindow::dockAppletBack);
    connect(win, &AppletFloatingWindow::geometrySettled, this,
            [this](const QString&) {
        // Ende der Geste in den Speicher, Beenden auf die Platte —
        // dasselbe Muster wie bei StripEqPanel. captureIntoCurrent()
        // ist reine Speicherarbeit, save() legt den JSON-Satz in
        // AppSettings ab; auf die Platte geht das erst beim Beenden.
        if (m_layoutProfiles) {
            m_layoutProfiles->captureIntoCurrent();
            m_layoutProfiles->save();
        }
    });

    // Ein Applet, das im Auswähler ausgeschaltet ist, bekommt kein
    // sichtbares Fenster — sonst hätte das Ablösen das Ausblenden
    // stillschweigend rückgängig gemacht.
    if (!m_appletVis || m_appletVis->isEffectivelyVisible(id)) {
        win->show();
        // Erst JETZT die Groesse: vor dem Anzeigen verlangt die
        // Anordnung fast nichts, danach ihre echte Untergrenze — und
        // die zieht das Fenster bildschirmfuellend auf. Nur wenn das
        // Profil nichts vorgibt; eine gespeicherte Lage gehoert dem
        // Betreiber.
        if (!rect.isValid()) {
            win->applyDefaultSize();
            // Und dann dorthin, wo es hergekommen ist. Die Groesse
            // bleibt die eben berechnete — die Lage des Feldes waere
            // meist zu schmal fuer ein Fenster mit eigener Leiste.
            if (pickedUpAt.isValid()) {
                win->move(pickedUpAt.topLeft());
                ensureOnVisibleScreen(win, this,
                                      QSize(Style::kAppletPanelW, 120));
            }
        }
        win->raise();
    }
}

void MainWindow::dockAppletBack(const QString& appletId)
{
    // ── Beim Beenden: NICHTS mehr andocken ───────────────────────────
    //
    // DER Grund fuer "profil nicht automatisch gespeichert" (Betreiber,
    // 2026-08-30, zum wiederholten Mal): Cmd+Q schickt auch jedem
    // Schwebefenster ein Schliessereignis, dessen closeEvent "Schliessen
    // heisst andocken" ausloest -- und der Andock-Weg hier unten rief
    // danach captureIntoCurrent()+save() und UEBERSCHRIEB damit die
    // korrekte Profilaufnahme vom Anfang von MainWindow::closeEvent()
    // mit "alles angedockt". Je nach Fensterreihenfolge verlor das
    // Profil so bei jedem Beenden ein anderes abgeloestes Fenster
    // (nachweisbar in Longpath.settings: floatingApplets schrumpfte
    // von Sitzung zu Sitzung). Beim Herunterfahren gibt es nichts mehr
    // anzudocken -- die Fenster sterben ohnehin mit dem Programm, und
    // die Aufnahme ist laengst im Kasten.
    if (m_shuttingDown) { return; }

    AppletFloatingWindow* win = m_floatingApplets.take(appletId);
    if (!win) { return; }

    // Betreiber 2026-09-01 (Untersuchung nach einem Haenger/OOM-Verdacht
    // beim Profilwechsel): dasselbe "erst verstecken, dann zerlegen"-
    // Muster wie in dockRotorPanel() -- siehe dessen Kommentar fuer die
    // volle Begruendung (PanadapterStack.cpp-Vorbild). win ist seit
    // heute regelmaessig noch sichtbar, wenn releaseApplet()+deleteLater()
    // darauf laufen.
    win->hide();
    const int idx = win->dockIndex();
    AppletWidget* applet = win->releaseApplet();
    win->deleteLater();

    if (!applet || !m_appletPanel) { return; }
    m_appletPanel->addApplet(applet);
    if (idx >= 0) {
        // addApplet hängt hinten an; erst das setzt es wieder dorthin,
        // wo es herkam. Ohne diese Zeile wandert jedes Applet nach
        // jedem Ausflug ans Ende der Spalte.
        m_appletPanel->moveApplet(applet, idx);
    }
    if (m_appletVis) {
        m_appletPanel->setAppletVisible(
            applet, m_appletVis->isEffectivelyVisible(appletId));
    }
    if (m_layoutProfiles) {
        m_layoutProfiles->captureIntoCurrent();
        m_layoutProfiles->save();
    }
}

void MainWindow::applyAppletVisibility(const QString& id, bool effective)
{
    // ── Die eigenen Fenster ──────────────────────────────────────────
    //
    // Sie stehen seit 2026-08-20 mit in der Auswahl (siehe kWindows in
    // buildAppletVisibility). Fuer den Auswaehler sind sie Eintraege
    // wie jeder andere; DASS sie ein Fenster oeffnen statt eine Huelle
    // in der Spalte einzuhaengen, entscheidet sich hier — und nur hier.
    if (id.startsWith(QLatin1String("Win"))) {
        applyWindowVisibility(id, effective);
        return;
    }

    if (auto* win = m_floatingApplets.value(id, nullptr)) {
        win->setVisible(effective);
        return;
    }
    if (auto* a = m_appletsById.value(id, nullptr)) {
        if (m_appletPanel) {
            // ── Einschalten heisst notfalls EINBAUEN ─────────────────
            //
            // setAppletVisible sucht die Huelle des Applets in dieser
            // Spalte und tut nichts, wenn es keine gibt. Fuer ein
            // Applet, das nur ausgeblendet ist, stimmt das. Fuer eines,
            // das gar nicht mehr in der Spalte steht, ist es eine
            // stille Sackgasse — und genau die hat der Betreiber am
            // 2026-08-18 gefunden: „der Eintrag steht in der Liste,
            // aber Klick und Doppelklick bewirken nichts."
            //
            // Der Auswaehler setzt eine Absicht („zeig das"), keinen
            // Widget-Zustand. Wenn das Applet nirgends steht, ist die
            // Absicht nur zu erfuellen, indem es zurueck in die Spalte
            // kommt. Ohne das braeuchte der Auswaehler Wissen darueber,
            // WO ein Applet gerade ist — und das ist genau die Frage,
            // die er nicht stellen koennen soll.
            // ── Freie Flaeche: neue Fenster kommen als KACHEL ────────
            //
            // OE5SOS, 2026-08-20: „es muss alles auf den mm verschoben
            // werden koennen. jedes window! jeder panel, ueberall."
            //
            // Steht die freie Flaeche an, landet ein eingeschaltetes
            // Fenster nicht im Stapel, sondern bekommt sofort eine
            // eigene Lage. Sonst muesste man jedes neue Fenster erst
            // wieder von Hand freistellen — und dann ist das Plus nur
            // ein halber Weg.
            if (effective && m_freeCanvasMode
                && !m_floatingApplets.contains(id)) {
                if (!m_appletPanel->applets().contains(a)) {
                    m_appletPanel->addApplet(a);
                }
                detachApplet(a, m_appletPanel->appletPosition(a));
                return;
            }

            // Ausschalten, waehrend es als Kachel liegt: die Kachel
            // aufloesen und das Applet zurueck in den Stapel, damit es
            // ueber das Plus wieder auftauchen kann. Ohne das bliebe
            // eine leere Kachel stehen und das Applet waere nirgends.
            if (!effective && m_canvasApplets.contains(id)) {
                returnAppletFromCanvas(id);
            }

            if (effective && !m_appletPanel->applets().contains(a)) {
                m_appletPanel->addApplet(a);
            }
            m_appletPanel->setAppletVisible(a, effective);
        }
        return;
    }
    // Kein Applet dahinter — die Knopfleiste und die Statuszeile gehen
    // ihren eigenen Weg.
    applyChromeVisibility(id, effective);
}

// Betreiber 2026-09-01: siehe m_borderlessFullSize in MainWindow.h fuer
// die volle Begruendung. Kurzfassung: showFullScreen() legt das Fenster
// in einen eigenen macOS-Space, und schwebende Werkzeugfenster
// (Panadapter, S-Meter, Bandwidth Filter, Rotor/Log, TX, Frequenz ...)
// folgten dabei nicht zuverlaessig -- sie blieben live beobachtet auf
// dem normalen Schreibtisch-Space zurueck, unsichtbar hinter dem
// Vollbild-Hauptfenster. Ein randloses Fenster auf voller
// Bildschirmflaeche erreicht optisch dasselbe (kein Desktop sichtbar),
// ohne je einen eigenen Space zu bekommen.
void MainWindow::enterBorderlessFullSize()
{
    // availableGeometry(), NICHT geometry() (Betreiber 2026-09-01:
    // "fullsize ist so groß, dass ich die untere leiste gar nicht
    // öffnen kann"): die volle Bildschirmflaeche schliesst den Streifen
    // unter der macOS-Menueleiste und hinter dem Dock mit ein -- macOS
    // schiebt das Fenster unter der Menueleiste ein, die volle Hoehe
    // ragt dann unten aus dem Schirm, und Longpaths eigene Statuszeile
    // liegt unerreichbar hinter/unter dem Dock. availableGeometry() ist
    // die Flaeche, die ein Fenster wirklich einnehmen kann; der
    // Schreibtisch bleibt damit trotzdem vollstaendig bedeckt.
    if (m_borderlessFullSize) {
        // Schon randlos -- nur die Flaeche nachziehen, falls sich der
        // Bildschirm (Aufloesung, externer Monitor) seither geaendert hat.
        if (QScreen* scr = this->screen()) {
            setGeometry(scr->availableGeometry());
        }
        return;
    }
    m_borderlessFullSize = true;
    // hide()/show(), nicht bloss setWindowFlag(): Qt dokumentiert, dass
    // ein Rahmen-Flag am schon realisierten nativen Fenster (winId()
    // existiert laengst -- die Applet-/Meter-Fenster brauchen es schon
    // vorher) erst nach einem erneuten show() ankommt, auf macOS
    // zuverlaessig nur mit einem hide() davor.
    const bool wasVisible = isVisible();
    if (wasVisible) { hide(); }
    setWindowFlag(Qt::FramelessWindowHint, true);
    if (QScreen* scr = this->screen()) {
        setGeometry(scr->availableGeometry());
    }
    show();
    raise();
    activateWindow();
}

void MainWindow::exitBorderlessFullSize()
{
    if (!m_borderlessFullSize) { return; }
    m_borderlessFullSize = false;
    if (isVisible()) { hide(); }
    setWindowFlag(Qt::FramelessWindowHint, false);
    show();
    raise();
    activateWindow();
}

QVariantMap MainWindow::blankLayoutState() const
{
    // ── Was „leer“ heißt ─────────────────────────────────────────────
    //
    // Alles aus, was im Auswähler steht — auch die Knopfleiste und die
    // Statuszeile. Sie stehen dort als Widgets, also verhalten sie sich
    // hier wie Widgets; eine Ausnahme „diese zwei bleiben an, weil ein
    // leeres Fenster erschrickt“ wäre eine Regel, die man sich merken
    // muss, und niemand merkt sie sich.
    //
    // Panadapter und Wasserfall bleiben. Sie stehen nicht im Auswähler,
    // weil sie kein Beiwerk sind: ohne sie wäre es kein Empfänger,
    // sondern ein leeres Fenster mit einem Plus.
    //
    // Die Splitterstellung wird MITGENOMMEN, nicht zurückgesetzt. Wer
    // ein neues Profil anlegt, will eine leere Fläche — nicht ein
    // Fenster, dessen Aufteilung ohne Vorwarnung springt.
    //
    // Kein "floatingApplets": ein leeres Profil hat keine abgelösten
    // Fenster. Der fehlende Schlüssel IST die Aussage — beim Anwenden
    // kehrt jedes abgelöste Applet in die Spalte zurück, statt über
    // einem Profil stehen zu bleiben, das es nicht kennt.
    //
    // "containerGeometry" dagegen MUSS als leere Karte dastehen, nicht
    // fehlen — anders als bei floatingApplets prüft die Anwendung hier
    // nur s.contains("containerGeometry"), bevor sie
    // ContainerManager::applyFloatingGeometries() ueberhaupt aufruft
    // (MainWindow's Profil-Anwenden-Hook). Ohne den Schluessel bliebe
    // ein Container, der beim Sichern des vorigen Profils zufaellig
    // freistand ("RX1 Main Panel", der Panel-Container), auch in diesem
    // absichtlich leeren Profil weiter freistehend -- die leere Karte
    // ist das Signal, das applyFloatingGeometries() braucht, um jeden
    // floatenden Container zurueck in die Spalte zu holen.
    QVariantMap s;

    QVariantMap vis;
    if (m_appletVis) {
        for (const QString& id : m_appletVis->registeredIds()) {
            vis.insert(id, false);
        }
    }
    s.insert(QStringLiteral("visible"), vis);
    s.insert(QStringLiteral("order"), QStringList{});
    s.insert(QStringLiteral("containerGeometry"), QVariantMap{});
    // Rotor / Log ist ein QDockWidget, kein Applet -- siehe die eigene
    // Begruendung an der Erfassungsstelle (Profil-Hook, "rotorDockVisible").
    s.insert(QStringLiteral("rotorDockVisible"), false);

    if (m_mainSplitter) {
        QVariantList sizes;
        for (int v : m_mainSplitter->sizes()) { sizes << v; }
        s.insert(QStringLiteral("splitter"), sizes);
    }
    return s;
}

void MainWindow::applyChromeVisibility(const QString& id, bool visible)
{
    if (id == QLatin1String(kChromeOverlayId)) {
        // Alle Pans, nicht nur der aktive. Eine Knopfleiste, die auf
        // Pan 1 verschwindet und auf Pan 2 stehen bleibt, sieht aus
        // wie ein Fehler und ist einer.
        for (const QPointer<SpectrumOverlayPanel>& p :
             std::as_const(m_overlayPanels)) {
            if (p) { p->setVisible(visible); }
        }
        return;
    }
    if (id == QLatin1String(kChromeStatusId)) {
        if (QStatusBar* sb = statusBar()) { sb->setVisible(visible); }
        return;
    }
}

// Das Plus sitzt schwebend in der unteren rechten Ecke der
// Arbeitsfläche — wie in der Vorlage. Es hat keinen Platz in einer
// Anordnung, deshalb wird es von Hand gesetzt und muss bei jeder
// Größenänderung mit.
void MainWindow::positionAddWidgetButton()
{
    if (!m_addWidgetBtn || !centralWidget()) { return; }
    const QSize s = m_addWidgetBtn->sizeHint();
    m_addWidgetBtn->resize(s);
    m_addWidgetBtn->move(centralWidget()->width()  - s.width()  - 18,
                         centralWidget()->height() - s.height() - 18);
    m_addWidgetBtn->raise();
}

void MainWindow::wireProfileRail()
{
    if (!m_profileRail || !m_layoutProfiles) { return; }

    auto askName = [this](const QString& title, const QString& preset)
                   -> QString {
        bool ok = false;
        const QString name = QInputDialog::getText(
            this, title, QStringLiteral("Name:"), QLineEdit::Normal,
            preset, &ok).trimmed();
        return ok ? name : QString();
    };

    auto complain = [this](const QString& name) {
        QMessageBox::warning(
            this, QStringLiteral("Profil"),
            name.isEmpty()
                ? QStringLiteral("Ein Profil braucht einen Namen.")
                : QStringLiteral("„%1“ gibt es schon.").arg(name));
    };

    connect(m_profileRail, &ProfileRail::newProfileRequested, this,
            [this, askName, complain]() {
        const QString name = askName(QStringLiteral("Neues Profil"),
                                     QStringLiteral("Neu"));
        if (name.isNull()) { return; }
        // ── Leer, nicht als Kopie ────────────────────────────────────
        //
        // OE5SOS, 2026-08-15: „Wenn ich links ein neues Profil öffne,
        // sollte dieses leer sein."
        //
        // Das Plus legt eine leere Arbeitsfläche an, die man mit dem
        // anderen Plus füllt. Legte es eine Kopie an, wäre es ein
        // Duplizieren-Knopf mit falscher Beschriftung — und Duplizieren
        // gibt es schon, im Rechtsklick auf das Abzeichen.
        if (!m_layoutProfiles->createWith(name, blankLayoutState())) {
            complain(name);
            return;
        }
        m_layoutProfiles->save();
    });

    // Betreiber 2026-08-28: "vielleicht sollte es ein profil speichern
    // auch geben" -- dieselben zwei Aufrufe wie beim Beenden
    // (closeEvent(), siehe dessen Kommentar), nur von Hand ausloesbar,
    // fuer wer ohne Verbindung gestaltet und sofort wissen will, dass
    // es sitzt, statt der Beenden-Sequenz zu vertrauen.
    connect(m_profileRail, &ProfileRail::saveRequested, this,
            [this](const QString&) {
        if (!m_layoutProfiles) { return; }
        m_layoutProfiles->captureIntoCurrent();
        m_layoutProfiles->save();
    });

    // Betreiber 2026-08-30: "Das Layout Profil ... sollten auf dem
    // Desktop zur Sicherheit abspeicherbar zu sein" -- eine Kopie neben
    // AppSettings' XML, unabhaengig lesbar und fuer ein Backup geeignet.
    // Beim aktiven Profil erst captureIntoCurrent(), sonst wuerde eine
    // gerade eben umgebaute, aber noch nicht per "Jetzt sichern" oder
    // Umschalten gesicherte Ansicht als der VORHERIGE Stand exportiert --
    // bei einem anderen Profil ist dessen zuletzt gesicherter Zustand
    // schon das Richtige, da nie aktiv seit dem letzten Umbau.
    connect(m_profileRail, &ProfileRail::exportRequested, this,
            [this](const QString& name) {
        if (!m_layoutProfiles) { return; }
        if (m_layoutProfiles->current() == name) {
            m_layoutProfiles->captureIntoCurrent();
        }
        const QByteArray json = m_layoutProfiles->exportToJson(name);
        if (json.isEmpty()) { return; }

        const QString suggested =
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)
            + QStringLiteral("/Longpath-Layout-%1-%2.json")
                .arg(name,
                     QDateTime::currentDateTime()
                         .toString(QStringLiteral("yyyy-MM-dd_HHmm")));
        const QString path = QFileDialog::getSaveFileName(
            this, QStringLiteral("Layout-Profil auf Schreibtisch sichern"),
            suggested, QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) { return; }

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(
                this, QStringLiteral("Profil"),
                QStringLiteral("„%1“ konnte nicht geschrieben werden.")
                    .arg(path));
            return;
        }
        f.write(json);
    });

    // Betreiber 2026-08-30: "DIE gespeicherten profile sollte man auch
    // mit rechter moustaste importiren können" -- das Gegenstueck zum
    // Export oben. Ersetzt DIESES Profil (Betreiber, selber Tag: "wenn
    // ich importiere will ich es nicht als neues profil importiren" --
    // ein frueherer Anlauf legte hier "Buero (2)" an statt zu ersetzen).
    // Die Rueckfrage steht HIER, nicht in LayoutProfiles::
    // importFromJson(): die Klasse selbst kennt keine Dialoge, siehe
    // ihre eigene Begruendung ("kein Selbstzweck").
    connect(m_profileRail, &ProfileRail::importRequested, this,
            [this](const QString& name) {
        if (!m_layoutProfiles) { return; }

        const QString suggested =
            QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        const QString path = QFileDialog::getOpenFileName(
            this, QStringLiteral("Layout-Profil vom Schreibtisch laden"),
            suggested, QStringLiteral("JSON (*.json)"));
        if (path.isEmpty()) { return; }

        const auto answer = QMessageBox::question(
            this, QStringLiteral("Profil"),
            QStringLiteral("„%1“ mit dem Inhalt der Datei ersetzen?")
                .arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) { return; }

        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            QMessageBox::warning(
                this, QStringLiteral("Profil"),
                QStringLiteral("„%1“ konnte nicht gelesen werden.").arg(path));
            return;
        }
        const QByteArray json = f.readAll();

        QString error;
        if (!m_layoutProfiles->importFromJson(name, json, &error)) {
            QMessageBox::warning(
                this, QStringLiteral("Profil"),
                error.isEmpty()
                    ? QStringLiteral("„%1“ liess sich nicht als Profil lesen.")
                          .arg(path)
                    : error);
        }
    });

    connect(m_profileRail, &ProfileRail::renameRequested, this,
            [this, askName, complain](const QString& old) {
        const QString name = askName(QStringLiteral("Profil umbenennen"), old);
        if (name.isNull() || name == old) { return; }
        if (!m_layoutProfiles->rename(old, name)) { complain(name); return; }
        m_layoutProfiles->save();
    });

    connect(m_profileRail, &ProfileRail::duplicateRequested, this,
            [this, askName, complain](const QString& from) {
        const QString name = askName(QStringLiteral("Profil duplizieren"),
                                     from + QStringLiteral(" Kopie"));
        if (name.isNull()) { return; }
        if (!m_layoutProfiles->duplicate(from, name)) {
            complain(name);
            return;
        }
        m_layoutProfiles->save();
    });

    connect(m_profileRail, &ProfileRail::removeRequested, this,
            [this](const QString& name) {
        // Nachfragen, weil es kein Rückgängig gibt. In so einem Profil
        // steckt eine halbe Stunde Einrichten.
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Profil löschen"),
            QStringLiteral("„%1“ mit seinem ganzen Aufbau löschen?")
                .arg(name),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (answer != QMessageBox::Yes) { return; }
        m_layoutProfiles->remove(name);
        m_layoutProfiles->save();
    });

    // Beim Beenden den jetzigen Stand ins aktive Profil. Ohne das
    // verlöre man alles, was seit dem letzten Umschalten gebaut wurde —
    // activate() sichert, ein Programmende bisher nicht.
    //
    // 2026-08-27: der eigentliche Aufruf ist jetzt an den Anfang von
    // MainWindow::closeEvent() gewandert (siehe dessen eigener
    // Kommentar) -- dort laeuft er GARANTIERT vor jedem Abbau, statt
    // reentrant mittendrin, wie es dieser Handler tat, als
    // aboutToQuit ueber die Cocoa-Terminate-Kaskade noch INNERHALB von
    // closeEvent feuerte (Absturzbericht 2026-08-27 23:10). Auf dem
    // ⌘Q-Pfad hat closeEvent m_shuttingDown also schon gesetzt, wenn
    // dieser Handler drankommt, und tut hier nichts mehr. Er bleibt nur
    // noch fuer den Signal-Pfad (SIGTERM/Kill/Debugger-Trennen)
    // zustaendig, auf dem closeEvent nie laeuft -- dort ist er weiterhin
    // der einzige Ort, der den Stand sichert.
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
        if (m_shuttingDown) { return; }
        m_layoutProfiles->captureIntoCurrent();
        m_layoutProfiles->save();
    });
}

void MainWindow::applyAntennaChangeForTest(RadioModel* model, int sliceId,
                                            const QString& antennaName)
{
    if (SliceModel* slice = sliceForAddedIdForTest(model, sliceId)) {
        slice->setRxAntenna(antennaName);
    }
}

// wireRadeFlagForTest ist mit der Flagge weggefallen. Es war ein
// Pruefzugang auf deren RADE-Zeile; die Groesse steht jetzt als
// Kennung MeterBinding::RadeSnr in jedem Instrument.

void MainWindow::configureSpectrumForPanForTest(SpectrumWidget* spectrum,
                                                 const QString& panId)
{
    if (!spectrum) { return; }
    bool ok = false;
    const int parsed = panId.startsWith(QStringLiteral("pan-"))
        ? panId.mid(4).toInt(&ok) : 0;
    spectrum->setPanIndex(ok && parsed >= 0 ? parsed : 0);
    spectrum->loadSettings();
}

void MainWindow::wireWidebandExtensionForTest(SpectrumWidget* spectrum,
                                              RadioModel* model,
                                              PanadapterStack* stack,
                                              const QString& panId)
{
    if (!spectrum || !model || !stack) { return; }
    const QPointer<RadioModel> modelRef(model);
    const QPointer<PanadapterStack> stackRef(stack);
    const auto resolve = [modelRef, stackRef, panId]() -> SliceModel* {
        if (!modelRef || !stackRef) { return nullptr; }
        PanadapterApplet* applet = stackRef->panadapter(panId);
        if (!applet) { return nullptr; }
        if (SliceModel* active =
                modelRef->sliceById(applet->activeSliceIndex())) {
            return active;
        }
        for (SliceModel* slice : modelRef->slices()) {
            if (slice
                && applet->associatedSlices().contains(slice->sliceIndex())) {
                return slice;
            }
        }
        return nullptr;
    };
    connect(spectrum, &SpectrumWidget::widebandExtensionStateChanged,
            spectrum, [resolve](bool on) {
        if (SliceModel* slice = resolve()) {
            slice->setWidebandExtensionRequested(on);
        }
    });
    connect(spectrum, &SpectrumWidget::ddcRetuneRequested,
            spectrum, [resolve](double freqHz) {
        if (SliceModel* slice = resolve()) {
            slice->setFrequency(freqHz);
        }
    });
    // Settings restore and rate seeding may have derived the actual state
    // before this bridge existed, so seed the resolved slice immediately.
    if (SliceModel* slice = resolve()) {
        slice->setWidebandExtensionRequested(spectrum->extendedMode());
    }
}

void MainWindow::fanWidebandBinsForTest(PanadapterStack* stack, int adcIndex,
                                        const QVector<float>& bins)
{
    if (!stack) { return; }
    for (PanadapterApplet* applet : stack->allApplets()) {
        if (applet && applet->spectrumWidget()) {
            applet->spectrumWidget()->setWidebandBins(adcIndex, bins);
        }
    }
}

// ── Die VFO-Flagge ist geloescht (2026-08-18) ───────────────────────
//
// Hier stand createSliceFlag() mit 421 Zeilen Verdrahtung: die
// schwebende Flagge ueber dem Panadapter, mit Frequenz, Betriebsart,
// Filter, AGC, Antennen, S-Meter-Balken und vier ausklappbaren
// Reitern (Audio, DSP, Mode, X/RIT).
//
// Sie faellt ERSATZLOS weg — Zielbild Punkt 1, Entscheidung des
// Betreibers am 2026-08-18: „Sie kommt ersatzlos weg, nicht
// ausblendbar." Alles, was nur dort lebte, ist vorher umgezogen und
// hat eigene Tests bekommen:
//
//   Frequenz mit Rad und Klick   FrequencyApplet / FrequencyInstrument
//   Lautstaerke, Stumm, Binaural RxApplet (waren NUR hier — die
//                                „TitleBar master volume", auf die der
//                                alte Kommentar verwies, gab es nie)
//   die sieben Rauschminderungen RxApplet, gegenseitig ausschliessend
//   NB/NB2, SNB, ANF, APF+Hz     RxApplet
//   der Schnellregler-Rechtsklick DspQuickPopups, von beiden Flaechen
//                                rufbar (das war der Fund beim
//                                zeilenweisen Abgleich)
//   S-Meter-Balken               das Zeigerinstrument als Applet
//   RADE-SNR-Zeile               Kennung im Instrument, Bereich aus
//                                dem RADE-Quelltext gerechnet
//
// Was NICHT mitgezogen ist, benannt statt verschwiegen:
//   * die Schnellwahl USB/CW/DIG — die RxApplet hat den vollen
//     Betriebsartenwaehler, es geht eine Abkuerzung verloren
//   * recordToggled/playToggled — waren an nichts verdrahtet; laut
//     Zielbild bekommen Aufnahme und Wiedergabe ein eigenes Feld
//   * sechs Signale des Mehrfach-Panadapters (Scheibe schliessen,
//     TX-Uebergabe, Abtastrate, Filterpolitik, Scheibe entfernen,
//     Antennenwahl je Scheibe) — sie gehoeren zu Phase 3F und
//     brauchen dort ohnehin eine neue Flaeche
//
// Der Port bleibt auffindbar: letzter Stand in Commit 300e8d48,
// `git show 300e8d48:src/gui/widgets/VfoWidget.cpp`.

// ── Phase 3F Sub-Epic I Task 8: one FFTEngine per DDC stream ────────────────
//
// The panadapter belongs to the DDC, not to the sub-receiver: ChannelMaster
// holds a single `volatile long run_pan` per `_rcvr` alongside
// `audio[cmMAXSubRcvr]` (cmaster.h:75-82 [v2.10.3.15]).  So slices that share
// a DDC share one spectrum and appear as separate flags on it, and the engine
// pool is sized by stream, not by slice.
FFTEngine* MainWindow::createFftEngineForStream(int streamIndex)
{
    if (streamIndex < 0) { return nullptr; }
    if (FFTEngine* existing = m_fftEngines.value(streamIndex, nullptr)) {
        return existing;
    }
    // m_fftThread must exist before an engine can be parked on it, and
    // m_radioModel before the I/Q feed can be wired; buildUI has both in
    // place ahead of the first createFftEngineForStream call.
    if (!m_fftThread || !m_radioModel) { return nullptr; }

    // No QObject parent: ownership is the deleteLater below, fired when the
    // FFT thread finishes.  Matches the pre-pool single-engine lifecycle.
    auto* engine = new FFTEngine(streamIndex);
    engine->setSampleRate(768000.0);
    engine->setFftSize(4096);

    auto& s = AppSettings::instance();
    const int persistedFps = qBound(1,
        s.value(QStringLiteral("DisplaySpectrumFps"),
                QStringLiteral("30")).toString().toInt(),
        60);
    engine->setOutputFps(persistedFps);

    const int persistedFftSize = s.value(
        QStringLiteral("DisplayFftSize"),
        QString::number(engine->fftSize())).toString().toInt();
    engine->setFftSizeBaseline(persistedFftSize);
    engine->setFftSize(persistedFftSize);

    const int defaultWin = static_cast<int>(engine->windowFunction());
    const int persistedWin = qBound(0,
        s.value(QStringLiteral("DisplayFftWindow"),
                QString::number(defaultWin)).toString().toInt(),
        static_cast<int>(WindowFunction::Count) - 1);
    engine->setWindowFunction(static_cast<WindowFunction>(persistedWin));

    engine->setHzPerBinTarget(
        s.value(QStringLiteral("DisplayHzPerBinTarget"),
                QStringLiteral("0")).toString().toDouble());

    engine->moveToThread(m_fftThread);
    connect(m_fftThread, &QThread::finished, engine, &QObject::deleteLater);

    // Absturz gegengepruefte Korrektur (2026-08-24, "kein SunSDR-Ton"-
    // Nebenbefund): m_fftEngines wurde nirgends geleert, wenn ein Engine
    // tatsaechlich zerstoert wird. Ein spaeterer Aufruf mit demselben
    // streamIndex traf dann auf `if (FFTEngine* existing = m_fftEngines.
    // value(streamIndex, nullptr))` -- die Abfrage sah einen nicht-Null-
    // Zeiger auf laengst freigegebenen Speicher und gab ihn zurueck, statt
    // einen frischen Engine anzulegen. Der naechste QMetaObject::
    // invokeMethod(engine, ...) las darauf engine->thread() auf
    // freigegebenem Speicher -- SIGSEGV, absturzbericht Longpath-2026-08-
    // 24-093116.ips, ausgeloest durch einen Panadapter-Klick nach einer
    // per addSlice() ohne echtes Funkgeraet angelegten Scheibe.
    //
    // destroyed() feuert unabhaengig davon, WARUM der Engine stirbt
    // (deleteLater oben, oder jeder andere Weg) -- die Abbildung raeumt
    // sich damit immer korrekt selbst auf, bevor ein zweiter Aufruf mit
    // demselben Index eine Chance hat, den alten Zeiger noch zu sehen.
    connect(engine, &QObject::destroyed, this, [this, streamIndex]() {
        m_fftEngines.remove(streamIndex);
    });

    // Raw I/Q for this stream -> this engine.  The context object is the
    // ENGINE, not MainWindow, deliberately: RadioModel emits
    // rawIqDataForStream from the Connection thread (Lever 2, 2026-05-24,
    // RadioModel.cpp Step 2a), and an engine-scoped connection resolves to a
    // queued delivery straight onto the FFT thread.  Routing through a
    // MainWindow-scoped lambda instead would put every I/Q packet through the
    // main thread's event loop (~3200/s per stream at 768 kHz), silently
    // undoing that fix.  The index filter costs one compare on the FFT
    // thread; the alternative -- reading m_fftEngines from the Connection
    // thread -- would need synchronisation AND lose Qt's automatic
    // disconnect-on-destroy, which is what makes this safe at shutdown.
    connect(m_radioModel, &RadioModel::rawIqDataForStream, engine,
            [engine, streamIndex](int idx, const QVector<float>& samples) {
        if (idx != streamIndex) { return; }
        engine->feedIQ(samples);
    });

    // Linear-power frame -> every pan subscribed to this stream.
    connect(engine, &FFTEngine::fftReadyLinear,
            this, &MainWindow::dispatchFftFrameToPans);

    // One NoiseFloorTracker per stream, fed by that stream's own FFT.
    //
    // Auto AGC-T derives its threshold from the noise floor, so a slice must
    // measure the band it is actually on. There used to be a single tracker
    // fed only by primaryFftEngine(), i.e. stream 0 -- fine while one slice
    // existed, but it would set a 20m slice's threshold from 40m's noise
    // floor once auto-AGC ran for every slice.
    auto* nf = new NoiseFloorTracker;
    m_streamNoiseFloors.insert(streamIndex, nf);
    if (m_radioModel) { m_radioModel->setStreamNoiseFloorTracker(streamIndex, nf); }
    connect(engine, &FFTEngine::fftReady, this,
            [nf](int, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nf->feed(binsDbm, kFrameIntervalMs);
    });

    m_fftEngines.insert(streamIndex, engine);
    return engine;
}

void MainWindow::applyStreamWindowToPan(const QString& panId, int streamIndex)
{
    if (!m_panStack) { return; }
    const auto it = m_streamWindows.constFind(streamIndex);
    if (it == m_streamWindows.constEnd()) { return; }
    SpectrumWidget* sw = m_panStack->spectrum(panId);
    if (!sw) { return; }
    sw->setDdcCenterFrequency(it->centreHz);
    if (it->sampleRateHz > 0) {
        sw->setSampleRate(static_cast<double>(it->sampleRateHz));
    }

    // Move the DISPLAY window onto the stream too, not just the DDC centre.
    //
    // setDdcCenterFrequency only tells the widget where the DDC sits for
    // bin-to-frequency mapping; the visible span is separate state, and a pan
    // created after startup keeps SliceModel's 14.225 MHz default. Bench-caught
    // 2026-07-26 on a 2v layout: pan-1 was correctly subscribed to a 7.265 MHz
    // stream while still displaying 14.2258 MHz, which
    //   - made visibleBinRange() select bins entirely outside the stream, so
    //     the waterfall rendered saturated (solid red), and
    //   - put the slice's flag at an x position far off the left edge, so the
    //     pan looked like it had no flag at all.
    //
    // Both symptoms are the same missing line. Recentre, preserving the pan's
    // current span so an operator's zoom is not thrown away -- only a pan that
    // has never been placed is actually moved, because pan-0 already sits on
    // its stream and this is a no-op there.
    // Span is clamped to the stream's own width. Preserving a wider one would
    // reintroduce the same failure at the edges: a pan left at the 192 kHz
    // default over a 48 kHz DDC has three quarters of its window outside the
    // data, which is exactly the out-of-range saturation this is fixing.
    const double streamWidthHz = static_cast<double>(it->sampleRateHz);
    double spanHz = sw->bandwidth();
    if (spanHz <= 0.0 || (streamWidthHz > 0.0 && spanHz > streamWidthHz)) {
        spanHz = streamWidthHz;
    }
    if (spanHz > 0.0) {
        sw->setFrequencyRange(it->centreHz, spanHz);
    }
}

// Phase 3F: WIDE badge fan-out. See RadioModel::panBypassState for the
// routing (pan -> slices -> stream -> ADC -> BpfEffective) and for the
// per-cause reason strings.
//
// Every pan is refreshed on every pass, not just the ones that changed. A
// bypass is a property of the CHAIN, so one slice crossing a band edge can
// flip the badge on pans that do not host it and never saw an event of
// their own. Rechecking all of them is one query per pan against
// single-digit slice and pan counts.
void MainWindow::refreshPanWideBadges()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const RadioModel::PanBypassState st =
            m_radioModel->panBypassState(applet->associatedSlices());
        applet->setWideBpf(st.bypassed, st.reason);
    }
}

// Phase 3F: status-overlay fan-out. Sibling of refreshPanWideBadges above,
// and deliberately the same shape: ask the model per pan, push the answer at
// that pan, never reach into the widget tree.
//
// Which slice a pan shows is its OWN activeSliceIndex, not the globally
// active slice -- per Sub-Epic E plan Task 2 Step 4. A pan hosting several
// slices has one of them active (PanadapterApplet::addSlice seeds it,
// removeSlice re-picks), and a global read would make every pan on a
// multi-pan layout paint identical text, which is the one thing a per-pan
// overlay exists not to do.
//
// updateStatusOverlay had zero callers before this, so every pan painted the
// widget's construction-time placeholders -- slice "A", "0.000", "USB",
// "CH 0" -- on a live radio.
void MainWindow::refreshPanStatusOverlays()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const int sliceId = applet->activeSliceIndex();
        SliceModel* slice = m_radioModel->sliceById(sliceId);
        // A pan with no slices keeps whatever it last painted rather than
        // being blanked: the operator is mid-drag between pans and a flash
        // to placeholder text reads as a fault.
        if (!slice) { continue; }
        applet->updateStatusOverlay(slice, m_radioModel->sliceChainIndex(sliceId));
    }
}

void MainWindow::wirePanStatusOverlayTriggers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        connect(applet, &PanadapterApplet::activeSliceChanged,
                this, &MainWindow::refreshPanStatusOverlays,
                Qt::UniqueConnection);
    }
}

// TNF: notch-marker fan-out. Fourth sibling of refreshPanWideBadges,
// refreshPanStatusOverlays and wirePanBadgeHandlers, on the same hook and
// the same shape: ask the model once, push the answer at every pan.
//
// Every pan is refreshed on every pass because the notch list is GLOBAL
// (design D1): a notch added from one pan is a notch on all of them. This is
// deliberately not the spot overlay's activeSpectrumWidget() push, which
// binds one widget forever and would leave every secondary pan blank.
//
// This is the ONLY Hz-to-MHz conversion site in the TNF stack. NotchModel
// stores absolute RF Hz; NotchMarker::freqMhz is MHz; the five interaction
// signals coming back the other way are Hz again.
void MainWindow::refreshPanNotchMarkers()
{
    if (!m_panStack || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }

    QVector<SpectrumWidget::NotchMarker> markers;
    markers.reserve(notches->notches().size());
    // `auto` here: the element type is obvious from notches(), and this
    // stays correct whichever scope the Notch value type is declared in.
    for (const auto& n : notches->notches()) {
        SpectrumWidget::NotchMarker m;
        m.id      = n.id;
        m.freqMhz = n.centerHz / 1.0e6;
        m.widthHz = n.widthHz;
        m.active  = n.active;
        markers.append(m);
    }

    const bool globalOn = notches->globalEnabled();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        sw->setNotchMarkers(markers);
        sw->setNotchGlobalEnabled(globalOn);
    }
}

// TNF: visual-notch fan-out (design section 8.3). See the declaration for why
// every pan is refreshed on every pass.
void MainWindow::refreshPanVisualNotch()
{
    if (!m_panStack || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }
    const bool on = notches->visualEnabled();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        if (SpectrumWidget* sw = applet->spectrumWidget()) {
            sw->setVisualNotchEnabled(on);
        }
    }
}

// TNF: minimum-notch-width fan-out (design sections 7.2 and 8.3).
//
// WDSP recomputes the minimum on every read as
// 1600.0 / (nc / 256) * (rate / 48000): the wintype-0 arm of
// min_notch_width (third_party/wdsp/src/nbp.c:88), which is the arm that
// governs because nbp0 is created with wintype 0 (RXA.c:103). So it moves
// whenever the filter size or the channel rate does. Thetis has the same
// problem and
// solves it the same way, re-reading through UpdateMinimumNotchWidthRX and
// firing MinimumRXNotchWidthChangedHandlers (console.cs:48787-48818
// [v2.10.3.15]) from the DSP-options apply path at console.cs:39052-39053.
//
// A pan with no resolvable channel keeps whatever it last had rather than
// being reset: the alternative is a visible dent-width flicker every time the
// operator drags a slice between pans.
void MainWindow::refreshPanNotchMinWidth()
{
    if (!m_panStack || !m_radioModel) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        // Through RadioModel, not WdspEngine: scripts/verify-no-gui-dsp-
        // access.py fails the build on a bare rxChannel() from src/gui/.
        RxChannel* ch = m_radioModel->rxChannelForSlice(applet->activeSliceIndex());
        if (!ch) { continue; }
        // Re-armed every pass because the channel a pan resolves to changes
        // with the slice set. UniqueConnection makes the repeat a no-op, and
        // a destroyed channel drops its own connections.
        connect(ch, &RxChannel::minNotchWidthChanged,
                this, &MainWindow::refreshPanNotchMinWidth,
                Qt::UniqueConnection);
        sw->setNotchMinWidthHz(ch->minNotchWidthHz());
    }
}

void MainWindow::wirePanNotchHandlers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        SpectrumWidget* sw = applet->spectrumWidget();
        if (!sw) { continue; }
        // The pan's own slice selection decides which channel's minimum
        // notch width it draws with, so a slice switch has to re-resolve it.
        connect(applet, &PanadapterApplet::activeSliceChanged,
                this, &MainWindow::refreshPanNotchMinWidth,
                Qt::UniqueConnection);
        // The pan identity still has to be recovered — SpectrumWidget does
        // not know its own pan id, and without it the handler would fall
        // back to activeSlice(), which is not necessarily the slice on the
        // pan that was clicked (Codex review of PR #313). It used to be
        // bound into a lambda here. It can't be: Qt6 REJECTS a connect that
        // pairs Qt::UniqueConnection with a functor target — qWarning
        // "unique connections require a pointer to member function of a
        // QObject subclass", and connectImpl returns an invalid Connection.
        // Not "silently ignored" as the comments on the sibling handlers
        // say; the connection was never made at all, so notch-create from a
        // panadapter click did nothing. onNotchCreateFromPan resolves the
        // pan from sender() instead.
        connect(sw, &SpectrumWidget::notchCreateRequested,
                this, &MainWindow::onNotchCreateFromPan,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchMoveRequested,
                this, &MainWindow::onNotchMoveRequested,
                Qt::UniqueConnection);
        // Flush the coalesced notch push the moment a drag ends, so the
        // final position is exact rather than up to one coalescing window
        // stale. RadioModel::scheduleNotchEditPush explains the window.
        connect(sw, &SpectrumWidget::notchDragFinished,
                m_radioModel, &RadioModel::commitPendingNotchEdits,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchWidthRequested,
                this, &MainWindow::onNotchWidthRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchActiveRequested,
                this, &MainWindow::onNotchActiveRequested,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchRemoveAllRequested,
                this, &MainWindow::onNotchRemoveAll,
                Qt::UniqueConnection);
        connect(sw, &SpectrumWidget::notchRemoveRequested,
                this, &MainWindow::onNotchRemoveRequested,
                Qt::UniqueConnection);
    }
}

void MainWindow::onNotchCreateFromPan(double freqHz, bool narrow)
{
    if (!m_panStack) { return; }
    // Resolve the emitting pan the same way wirePanNotchHandlers() found it,
    // so the two cannot disagree: whichever applet owns this SpectrumWidget.
    const QObject* src = sender();
    for (auto* applet : m_panStack->allApplets()) {
        if (applet && applet->spectrumWidget() == src) {
            onNotchCreateRequested(applet->panId(), freqHz, narrow);
            return;
        }
    }
    // A widget that is wired but no longer in the stack: fall back to the
    // active pan rather than dropping the operator's click. Reachable only
    // between a layout change and the re-wire that follows it.
    onNotchCreateRequested(m_panStack->activePanId(), freqHz, narrow);
}

void MainWindow::onNotchCreateRequested(const QString& panId, double freqHz,
                                       bool narrow)
{
    if (!m_radioModel) { return; }
    // Narrow is the Shift-held add. Both widths live on NotchModel because
    // they are Thetis constants (console.cs:40268-40269 [v2.10.3.15]).
    //
    // The slice comes from the pan that emitted the signal, not from
    // activeSlice(): clicking a pan activates it in PanadapterStack without
    // necessarily changing the active slice, so on two pans running different
    // filter sizes the clamp would resolve against the wrong channel. Standing
    // rule: a control drawn on a pan targets that pan. Codex review of PR #313.
    m_radioModel->addNotchForSlice(
        sliceForPan(panId), freqHz,
        narrow ? NotchModel::kNarrowNotchWidthHz
               : NotchModel::kDefaultNotchWidthHz);
}

void MainWindow::onNotchMoveRequested(int id, double newFreqHz)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setCenter(id, newFreqHz);
}

void MainWindow::onNotchWidthRequested(int id, double widthHz)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setWidth(id, widthHz);
}

void MainWindow::onNotchActiveRequested(int id, bool active)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->setActive(id, active);
}

void MainWindow::onNotchRemoveRequested(int id)
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    m_radioModel->notchModel()->removeNotch(id);
}

// ── Alle Notch-Filter auf einmal ─────────────────────────────────────
//
// Mit Rueckfrage, und das ist kein Hoeflichkeitsschritt: es gibt kein
// Zurueck. Ein Notch, der eine Stoerstelle abdeckt, ist Arbeit — sechs
// davon sind sechsmal Arbeit, und ein Fehlgriff im Menue wuerfe sie
// weg, ohne dass jemand sie wiederherstellen koennte.
void MainWindow::onNotchRemoveAll()
{
    if (!m_radioModel || !m_radioModel->notchModel()) { return; }
    const int n = m_radioModel->notchModel()->notches().size();
    if (n <= 0) { return; }

    if (QMessageBox::question(
            this, tr("Notch-Filter"),
            tr("Alle %1 Notch-Filter entfernen?\n\n"
               "Das laesst sich nicht rueckgaengig machen.").arg(n),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No)
        != QMessageBox::Yes) {
        return;
    }
    m_radioModel->notchModel()->clear();
}

// The +TNF button on one pan's control strip. Distinct from
// onNotchCreateRequested above because a panadapter click already knows its
// frequency and this does not: the centre is composed from the pan's own
// slice, so a strip drawn on pan-2 notches pan-2's signal.
//
// From Thetis console.cs:40313-40331 [v2.10.3.15], TNFAdd(rx): VFO, plus RIT,
// shifted into the sideband, then AddNotch. The arithmetic lives in
// NotchModel::tnfAddCenterHz (design sections 7.5 and 10.2, which keeps the
// Thetis-derived maths out of the AetherSDR-registered overlay panel), and the
// admin-busy guard upstream repeats at console.cs:40315 is already enforced
// inside NotchModel::addNotch (console.cs:40224), so the reject path is the
// model's.
void MainWindow::onAddTnfClicked(const QString& panId)
{
    if (!m_radioModel) { return; }
    SliceModel* slice = sliceForPan(panId);
    if (!m_radioModel->notchModel() || !slice) { return; }
    // demodulatedRxFrequency(), not effectiveRxFrequency(): composedShiftHz
    // feeds WDSP the notch origin including the DIG click-tune offset, so a
    // centre computed without it lands displaced by exactly that offset in
    // DIGU/DIGL. Codex review of PR #313.
    m_radioModel->addNotchForSlice(
        slice,
        NotchModel::tnfAddCenterHz(slice->demodulatedRxFrequency(),
                                   slice->filterLow(), slice->filterHigh()),
        NotchModel::kDefaultNotchWidthHz);
}

// A rejected add is not a failure worth an error badge, but it must not be
// silent either: pressing +TNF twice on the same signal lands inside the 10 Hz
// dedupe window (console.cs:40259-40260 [v2.10.3.15]) and would otherwise do
// nothing at all, which reads as a dead button.
void MainWindow::onNotchAddRejected(const QString& reason)
{
    showToast(tnfAddRejectedNotice(reason), ToastSeverity::Warning, 3000);
}

// Repaint the status-bar TNF light. Both halves of what it shows can move
// independently: the master enable from this label, the DSP menu or TCI, and
// the count from any pan's panadapter or the MNF settings page.
void MainWindow::refreshTnfIndicator()
{
    if (!m_tnfLabel || !m_radioModel) { return; }
    NotchModel* notches = m_radioModel->notchModel();
    if (!notches) { return; }
    const bool on    = notches->globalEnabled();
    const int  count = static_cast<int>(notches->notches().size());
    m_tnfLabel->setStyleSheet(tnfIndicatorStyleSheet(on, count));
    m_tnfLabel->setToolTip(tnfIndicatorTooltip(count, on));
}

// Phase 3F: badge-click fan-out. Third sibling of refreshPanWideBadges and
// wirePanStatusOverlayTriggers above, on the same hook and for the same
// reason: a pan that comes into existence after startup has to be wired
// without anyone remembering to wire it.
//
// The connects name member slots rather than lambdas on purpose. This runs
// again on every countChanged, so it has to be idempotent, and
// Qt::UniqueConnection only works on a pointer-to-member target. Qt6 does
// not quietly drop the flag on a lambda -- it warns ("unique connections
// require a pointer to member function of a QObject subclass") and refuses
// the connect, so a lambda here would not stack duplicates but simply
// never fire. A named slot is the only shape that gets the dedup asked for.
// ---------------------------------------------------------------------------
// ensureOverlayPanels — one control strip per panadapter.
//
// The strip used to be a single instance parented to pan-0's SpectrumWidget,
// so every other pan had no BAND / ANT / Display / +RX at all, and the one
// button that did exist had to guess which pan the operator meant. A control
// drawn on a pan acts on that pan: each strip now owns its panId and emits it,
// the same shape as AetherSDR's SpectrumOverlayMenu (SpectrumOverlayMenu.cpp:
// 292-315 [@c6481cbf], where +RX emits addRxClicked(m_panId)).
//
// Idempotent and re-armed from PanadapterStack::countChanged, so pans created
// by a layout switch or an Add Panadapter get their strip by construction
// rather than by remembering. Panels for pans that went away are dropped --
// the widget itself dies with its parent SpectrumWidget.
// ---------------------------------------------------------------------------
// ---------------------------------------------------------------------------
// wireSpectrumForPan — mouse interaction for one panadapter.
//
// Every SpectrumWidget signal used to be connected exactly once, to whatever
// activeSpectrumWidget() returned during startup -- pan-0's widget. The connect
// binds the OBJECT, not the expression, so it never re-resolved: on any other
// pan, click-to-tune, filter-edge drag, pan drag, zoom-replan and the dBm strip
// were all inert. The flag stayed live because it is wired per slice, which is
// why tuning appeared to work only with the pointer over the flag's digits.
//
// Resolves the slice per call through sliceForPan, so each pan drives its own
// slice, and uses that slice's WDSP channel rather than the hardcoded
// rxChannel(0) the single-pan path used.
// ---------------------------------------------------------------------------
// Push the live connection state into every pan's spectrum widget. Safe to
// call at any time; a pan created later is seeded by wireSpectrumForPan.
void MainWindow::pushConnectionStateToPans()
{
    if (!m_radioModel || !m_panStack) { return; }
    const ConnectionState st = m_radioModel->connectionState();
    const bool live = m_radioModel->isConnected();
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        if (SpectrumWidget* sw = m_panStack->spectrum(applet->panId())) {
            sw->setConnectionState(st);
            // Clearing stale history on disconnect was pan-0 only, so every
            // other pan kept painting the waterfall it had when the radio went
            // away -- indistinguishable from live data.
            if (!live) { sw->clearWaterfallHistory(); }
        }
    }
}

void MainWindow::wireSpectrumForPan(SpectrumWidget* sw, const QString& panId)
{
    if (!sw || !m_radioModel) { return; }

    configureSpectrumForPanForTest(sw, panId);

    // Seed the new pan with the CURRENT connection state. Without this a pan
    // created after connect sits at the Disconnected default until the next
    // state change, and its disconnected guard swallows every mouse press.
    sw->setConnectionState(m_radioModel->connectionState());

    // Band plan too: setBandPlanManager was another activeSpectrumWidget()
    // one-shot, so the band-segment bar ("PHONE General" and friends) drew on
    // pan-0 only and every other pan showed a bare spectrum.
    sw->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

    // Right-click a spot on THIS pan to remove it. Was pan-0 only, so spots
    // could be dismissed from one pan and were inert on every other.
    if (SpotModel* spots = m_radioModel->spotModel()) {
        connect(sw, &SpectrumWidget::spotRemoveRequested,
                spots, &SpotModel::removeSpot);
    }

    // "Turn rotor to <call>" from THIS pan's spot menu (2026-08-11) —
    // same handler as the Spot List's right-click: raise the rotor dock
    // so the operator sees the needle they just commanded, then let the
    // panel do the bearing maths and the turn.
    connect(sw, &SpectrumWidget::spotRotorRequested,
            this, [this](const QString& dxCall) {
        if (RotorLogbookPanel* panel = ensureRotorPanel()) {
            raiseRotorPanel();
            panel->workSpot(dxCall);
        }
    });

    // 2026-08-19: Doppelklick auf ein Spot-Etikett — Log auf, Rotor-Lage
    // sichtbar, Zeiger auf die Zielposition des Rufzeichens. takeSpot
    // dreht NICHT: der Sinn ist der Vergleich zwischen Ist und Ziel
    // („so sehe ich sofort, ob der Rotor in diese Richtung steht").
    // Gedreht wird ueber Rotate daneben oder den Rechtsklick-Weg oben.
    //
    // Frequenz, Band und Betriebsart kommen nicht mit: der erste Klick
    // des Doppelklicks hat schon abgestimmt, und buildEntry liest sie
    // aus der aktiven Scheibe.
    connect(sw, &SpectrumWidget::spotLogRequested,
            this, [this](const QString& dxCall) {
        if (RotorLogbookPanel* panel = ensureRotorPanel()) {
            raiseRotorPanel();
            panel->takeSpot(dxCall);
        }
    });

    // Hovering a spot on any pan drives the Spot Hub highlight.
    if (m_spotHubDialog) {
        connect(sw, &SpectrumWidget::spotHoverIndexChanged,
                m_spotHubDialog.data(),
                &SpotHubDialog::setHoveredPanadapterSpot);
    }

    // Clicking a disconnected pan opens the connection panel, from any pan.
    connect(sw, &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::showConnectionPanel);

    // CTUN max-bin offset follows THIS pan's slice rather than the globally
    // active one, so a max-bin readout on a background pan is not measured
    // against a slice sitting on some other pan's stream.
    connect(sw, &SpectrumWidget::ddcCenterFrequencyChanged, this,
            [this, panId](double ddcCenter) {
        if (!m_radioModel || !m_radioModel->wdspEngine()) { return; }
        if (SliceModel* s = sliceForPan(panId)) {
            m_radioModel->wdspEngine()->setMaxBinSliceOffsetHz(
                /*disp=*/0, s->frequency() - ddcCenter);
        }
    });

    // NOT wired here: bandwidthChangeRequested. Zoom itself already works on
    // every pan (SpectrumWidget narrows its own visible bin range), but the
    // auto-replan that keeps bins-per-pixel constant across zoom levels runs
    // against primaryFftEngine() -- one engine, pan-0's. Routing it per pan
    // needs the FFT engine looked up by the pan's stream, which is a bigger
    // change than this function. Until then a deep zoom on another pan stays
    // visually correct but does not gain FFT resolution.
}

// ---------------------------------------------------------------------------
// wireSpectrumSliceControls: the four spectrum controls that act on a slice.
//
// Bench 2026-07-28: "when I click to tune it always tunes flag A, not the last
// selected." Proven with per-hop instrumentation rather than by reading: a
// synthetic frequencyClicked on pan-0 fired exactly one handler, the copy in
// wireSliceToSpectrum, and sliceForPan was never called in the whole session.
//
// These four used to exist twice. wireSpectrumForPan held the per-pan version,
// and ensureOverlayPanels skips that function for pan-0 (its spot, connection
// and MaxBin hooks are wired elsewhere in this file and would double), so pan-0
// ran an older copy in wireSliceToSpectrum whose lambdas closed over
//
//     SliceModel* slice = m_radioModel->activeSlice();
//
// by value. connect() binds the object, and the lambda binds the pointer, so
// that copy was Slice A for the life of the session. On the single pan almost
// every operator uses, click-to-tune, the filter-edge drag, the pan drag and
// the CTUN toggle all drove Slice A and never consulted the pan at all. The
// per-pan active slice this epic added was correct and simply unread.
//
// One home now, called for every pan including pan-0, so the two cannot drift
// apart again. verify-no-captured-slice-spectrum-wiring.py fails the build if
// any of the four is connected to a sender other than `sw`; it is listed in
// the pre-commit hook and in CI's compliance job beside
// verify-no-gui-dsp-access.py, which guards the same class of mistake one
// layer down.
//
// Every handler resolves its slice through sliceForPan(panId) on each signal.
// That is the whole fix: resolving late is what lets the answer change when
// the operator selects a different flag.
// ---------------------------------------------------------------------------
void MainWindow::wireSpectrumSliceControls(SpectrumWidget* sw,
                                           const QString& panId)
{
    if (!sw || !m_radioModel) { return; }

    // Click on the spectrum tunes this pan's slice.
    connect(sw, &SpectrumWidget::frequencyClicked, this,
            [this, panId](double hz) {
        SliceModel* s = sliceForPan(panId);
        if (!s) { return; }

        // ── Eine Sperre muss sich melden ────────────────────────────
        //
        // SliceModel::setFrequency() beginnt mit "if (m_locked)
        // return;" — wortlos. Der Betreiber am 2026-08-22: "balken
        // spring nicht dort hin." Im Quelltext und im echten Fenster
        // stimmt der Klick nachweislich ab (gemessen: 7,100 ->
        // 7,128 MHz), live tut er es nicht — und eine gesperrte
        // Scheibe ist der einzige Weg, auf dem das schweigend
        // passieren kann.
        //
        // Ob es diesmal die Ursache war, weiss ich nicht. Aber ein
        // Bedienelement, das ohne ein Wort nichts tut, hat uns heute
        // schon Stunden gekostet — und wenn es nicht die Ursache ist,
        // erscheint der Hinweis nie und wir haben sie ausgeschlossen.
        if (s->locked()) {
            showToast(tr("Die Frequenz dieser Scheibe ist gesperrt — "
                         "das Schloss in der VFO-Leiste gibt sie frei."),
                      ToastSeverity::Info, 5000);
            return;
        }
        s->setFrequency(hz);
    });

    // Drag a filter edge on this pan.
    connect(sw, &SpectrumWidget::filterEdgeDragged, this,
            [this, panId](int low, int high) {
        if (SliceModel* s = sliceForPan(panId)) { s->setFilterByHand(low, high); }
    });

    // Drag the pan. Non-CTUN moves the VFO; CTUN pins the DDC to the pan centre
    // and offsets WDSP so audio stays on the VFO (Thetis radio.cs:1417 --
    // SetRXAShiftFreq receives +(freq - center)).
    connect(sw, &SpectrumWidget::centerChanged, this,
            [this, panId, sw](double centerHz) {
        if (m_handlingBandJump) { return; }
        SliceModel* s = sliceForPan(panId);
        if (!s) { return; }
        if (!sw->ctunEnabled()) {
            s->setFrequency(centerHz);
            return;
        }
        const int stream = s->streamIndex();
        if (stream >= 0 && m_radioModel->receiverManager()) {
            m_radioModel->receiverManager()->forceHardwareFrequency(
                stream, static_cast<quint64>(centerHz));
        }
        sw->setDdcCenterFrequency(centerHz);
        // Re-shift the WHOLE stream, not just this pan's slice. Addressing the
        // dragged slice's own channel fixed the hardcoded rxChannel(0), but a
        // shared DDC window still has one centre and N slices at their own
        // offsets inside it, so a co-host on the same stream kept the offset it
        // had before the drag and demodulated the wrong signal with its flag
        // still reading right.
        //
        // From Thetis radio.cs:1417 [v2.10.3.15]: SetRXAShiftFreq receives
        // +(freq - center). reshiftSlicesOnStream applies that per member.
        m_radioModel->reshiftSlicesOnStream(stream, centerHz);
    });

    // CTUN toggle restores this pan's whole stream rather than channel 0.
    //
    // Unpinning does not retune the DDC, so the members are still offset from
    // an unmoved centre; zeroing the shift would drop the demodulator onto the
    // DDC centre. Restore against where the DDC actually sits (the drag above
    // bypasses the allocator via forceHardwareFrequency and writes the centre
    // into this widget), and let the next VFO move settle the offsets to zero
    // through the now-unpinned allocator.
    connect(sw, &SpectrumWidget::ctunEnabledChanged, this,
            [this, panId, sw](bool enabled) {
        if (m_radioModel->receiverManager()) {
            m_radioModel->receiverManager()->setDdcFrequencyLocked(enabled);
        }
        if (enabled) { return; }
        if (SliceModel* s = sliceForPan(panId)) {
            m_radioModel->reshiftSlicesOnStream(s->streamIndex(),
                                                sw->ddcCenterFrequency());
        }
    });
}

// Keep the S-meter poller's channel list in step with the live slices.
//
// Slice id == WDSP RX channel id, so this is also the set of channels it reads
// for the per-slice pass that drives each flag's level bar.
//
// Hung off sliceAdded / sliceRemoved, NOT off the pan-count hook. It first
// lived inside ensureOverlayPanels, which only runs on
// PanadapterStack::countChanged -- so adding a slice to an EXISTING pan never
// refreshed the list and that slice's flag bar stayed dead. Bench-caught
// 2026-07-26.
void MainWindow::refreshMeterPollerSlices()
{
    if (!m_meterPoller || !m_radioModel) { return; }
    QList<int> ids;
    for (SliceModel* s : m_radioModel->slices()) {
        if (s) { ids << s->sliceIndex(); }
    }
    m_meterPoller->setSliceChannels(ids);
}

void MainWindow::ensureOverlayPanels()
{
    if (!m_panStack || !m_radioModel) { return; }

    refreshMeterPollerSlices();

    // Drop entries whose pan (and therefore whose parent widget) is gone.
    for (auto it = m_overlayPanels.begin(); it != m_overlayPanels.end(); ) {
        if (it.value().isNull() || !m_panStack->panadapter(it.key())) {
            it = m_overlayPanels.erase(it);
        } else {
            ++it;
        }
    }

    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        const QString panId = applet->panId();
        if (m_overlayPanels.contains(panId)) { continue; }

        SpectrumWidget* sw = m_panStack->spectrum(panId);
        if (!sw) { continue; }

        // Same one-shot-per-pan guard as the strip below: this loop only runs
        // for pans that have no entry yet, so the interaction connects cannot
        // stack on a layout switch.
        //
        // The slice controls go to EVERY pan, pan-0 included. pan-0 used to be
        // excluded from the whole of wireSpectrumForPan and left running an
        // older copy of these four in wireSliceToSpectrum, whose lambdas held
        // a captured Slice A pointer -- which is the 2026-07-28 bench defect
        // where click-to-tune always retuned flag A. Those copies are gone, so
        // this is the only wiring for them and nothing doubles.
        wireSpectrumSliceControls(sw, panId);
        wireWidebandExtensionForTest(sw, m_radioModel, m_panStack, panId);

        if (panId != QStringLiteral("pan-0")) {
            // Still pan-0-excluded: its spot, connection and MaxBin hooks are
            // wired one-shot elsewhere in this file (the activeSpectrumWidget()
            // connects near the spot / ConnectionPanel / MaxBin sections), so
            // running them again here would double every one. Unifying those
            // the way the slice controls just were is a separate change with
            // its own regression surface.
            wireSpectrumForPan(sw, panId);
        }

        auto* panel = new SpectrumOverlayPanel(sw);
        panel->setPanId(panId);
        panel->setSliceResolver([this, panId]() {
            return sliceForPan(panId);
        });
        panel->move(4, 4);
        panel->show();
        // Ein später angelegter Pan bekommt seine Knopfleiste in dem
        // Zustand, den der Betreiber gewählt hat. Ohne das erschiene sie
        // auf jedem neuen Pan wieder, obwohl sie überall sonst aus ist.
        if (m_appletVis) {
            panel->setVisible(m_appletVis->isEffectivelyVisible(
                QString::fromLatin1(kChromeOverlayId)));
        }
        m_overlayPanels.insert(panId, panel);

        // Phase 3O Sub-Phase 9 Task 9.2c — bind the VAX Ch combo to the model.
        panel->setRadioModel(m_radioModel);
        connect(applet, &PanadapterApplet::activeSliceChanged, panel,
                [panel](const QString&, int) {
            panel->bindToPanSlice();
        });

        // Phase 3P-I-a T18 — board caps drive the antenna combos, and are
        // re-pushed on every radio swap.
        panel->setBoardCapabilities(m_radioModel->boardCapabilities());
        connect(m_radioModel, &RadioModel::currentRadioChanged, panel,
                [this, panel]() {
            panel->setBoardCapabilities(m_radioModel->boardCapabilities());
        });

        // +RX adds a slice on the pan this strip belongs to. The id comes from
        // the signal, so this never consults an "active" pan.
        connect(panel, &SpectrumOverlayPanel::addRxClicked, this,
                [this](const QString& id) {
            if (!m_radioModel || id.isEmpty()) { return; }
            m_radioModel->addSliceOnPan(id);
        });

        // +TNF adds a notch on the slice THIS pan is showing, at the frequency
        // the operator is actually listening to. The composition lives in
        // onAddTnfClicked; a named slot, so re-arming this loop on a layout
        // switch cannot stack duplicate connections.
        connect(panel, &SpectrumOverlayPanel::addTnfClicked, this,
                &MainWindow::onAddTnfClicked, Qt::UniqueConnection);

        // Band clicks act on this pan's active slice rather than the globally
        // active one, for the same reason (#118 fixed the mode-vs-frequency
        // half of this; the pan-targeting half arrives with per-pan strips).
        //
        // sliceIndex() is a stable slice ID, so it goes through
        // setActiveSliceById rather than the positional setActiveSlice --
        // otherwise a band click on a pan whose slice had a higher id than the
        // list is long (any mid-list removal) left the active slice where it
        // was and onBandButtonClicked retuned the wrong slice.
        connect(panel, &SpectrumOverlayPanel::bandSelected, this,
                [this, panId](const QString& name, double, const QString&) {
            if (!m_radioModel) { return; }
            if (SliceModel* s = sliceForPan(panId)) {
                m_radioModel->setActiveSliceById(s->sliceIndex());
            }
            m_radioModel->onBandButtonClicked(bandFromName(name));
        });

        // Pan-0's strip stays the one the display-settings wiring targets.
        if (m_overlayPanel == nullptr || panId == QStringLiteral("pan-0")) {
            m_overlayPanel = panel;
        }
    }
}

// The slice this pan hosts: its own active slice if it has one, else the first
// slice associated with it. Returns nullptr for a pan with no slices.
SliceModel* MainWindow::sliceForPan(const QString& panId) const
{
    if (!m_panStack || !m_radioModel) { return nullptr; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return nullptr; }
    const int active = applet->activeSliceIndex();
    if (active >= 0) {
        if (SliceModel* s = m_radioModel->sliceById(active)) { return s; }
    }
    for (SliceModel* s : m_radioModel->slices()) {
        if (s && applet->associatedSlices().contains(s->sliceIndex())) {
            return s;
        }
    }
    return nullptr;
}

void MainWindow::wirePanBadgeHandlers()
{
    if (!m_panStack) { return; }
    for (auto* applet : m_panStack->allApplets()) {
        if (!applet) { continue; }
        connect(applet, &PanadapterApplet::wideBadgeClicked,
                this, &MainWindow::onPanWideBadgeClicked,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::chainTagClicked,
                this, &MainWindow::onPanChainTagClicked,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::txBadgeClicked,
                this, &MainWindow::onPanTxBadgeClicked,
                Qt::UniqueConnection);
        // Task B5: add-slice / float, both carrying the applet's own panId().
        // Member-pointer targets, same reasoning as the three connects above:
        // this function re-runs on every countChanged.
        connect(applet, &PanadapterApplet::addSliceRequested,
                this, &MainWindow::onPanAddSliceRequested,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::floatRequested,
                this, &MainWindow::onPanFloatRequested,
                Qt::UniqueConnection);
        // Der Rueckweg. Ohne ihn ist der Schalter im Kopf der Kachel
        // eine Einbahnstrasse — genau der Zustand, ueber den sich der
        // Betreiber am 2026-08-19 beschwert hat.
        connect(applet, &PanadapterApplet::dockRequested,
                this, &MainWindow::onPanDockRequested,
                Qt::UniqueConnection);

        // ── Der Zahnrad-Knopf in der Panadapter-Kopfleiste ───────────
        //
        // Das Applet meldet nur den Wunsch; geaendert wird hier, an der
        // Stelle, die den Renderer kennt. Member-Zeiger als Ziel, aus
        // demselben Grund wie oben: diese Funktion laeuft bei jedem
        // countChanged erneut, und Qt::UniqueConnection wirkt nur mit
        // einem Zeiger auf eine Methode — mit einem Lambda stellt Qt
        // die Verbindung GAR NICHT her (am 2026-08-20 schon einmal
        // teuer gelernt, siehe AppletPanelWidget::addApplet).
        connect(applet, &PanadapterApplet::backgroundImageRequested,
                this, &MainWindow::onPanBackgroundImage,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::backgroundOpacityRequested,
                this, &MainWindow::onPanBackgroundOpacity,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::backgroundColourResetRequested,
                this, &MainWindow::onPanBackgroundColourReset,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::backgroundColourRequested,
                this, &MainWindow::onPanBackgroundColour,
                Qt::UniqueConnection);
        connect(applet, &PanadapterApplet::displaySetupRequested,
                this, &MainWindow::onPanDisplaySetup,
                Qt::UniqueConnection);
        // Phase 3F: clicking a pan makes it the active pan. Straight to the
        // stack's setter, exactly as AetherSDR MainWindow.cpp:12964 [@6a142807]
        // does it:
        //   connect(applet, &PanadapterApplet::activated,
        //           m_panStack, &PanadapterStack::setActivePan);
        // Member-pointer target on both ends, so Qt::UniqueConnection is
        // actually honoured here (it is silently dropped for lambdas -- see
        // RadioModel.cpp:5666), which matters because this re-runs on every
        // countChanged.
        connect(applet, &PanadapterApplet::activated,
                m_panStack, &PanadapterStack::setActivePan,
                Qt::UniqueConnection);
    }
}

int MainWindow::panChainIndex(const QString& panId) const
{
    if (!m_panStack || !m_radioModel) { return -1; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return -1; }

    const int chain = m_radioModel->sliceChainIndex(applet->activeSliceIndex());
    if (chain >= 0) { return chain; }

    // The slice is bound to no stream, so the model has no chain to give.
    // updateStatusOverlay holds the CH tag at its last known-good value in
    // exactly this case rather than claiming chain 0, so following it keeps
    // the dialog on the chain the operator can see. statusChainIndex() is the
    // read-back 0896b4f3 added for the overlay.
    return applet->statusChainIndex();
}

void MainWindow::onPanWideBadgeClicked(const QString& panId)
{
    onPanChainTagClicked(panId, -1);
}

// chainIdx is what the CH tag was painting when it was clicked; it is passed
// so the two entry points stay one function, but the live resolution wins.
// The tag is refreshed from the same call panChainIndex makes, so they agree
// in every case except a stale repaint, and the live answer is the honest one
// to open a dialog on.
void MainWindow::onPanChainTagClicked(const QString& panId, int chainIdx)
{
    if (!m_radioModel) { return; }

    int chain = panChainIndex(panId);
    if (chain < 0) { chain = chainIdx; }
    if (chain < 0) { return; }

    auto* alex = &m_radioModel->alexControllerMutable();
    FilterPolicyDialog dlg(chain, alex, this);
    dlg.exec();
}

// The TX pill hands the transmitter to the slice THIS pan is showing, which
// is the pan's own activeSliceIndex -- a slice ID, so it goes through
// RadioModel::requestTxHandoffToSlice, which converts to the list position
// TxSliceArbiter indexes by and drops MOX before flipping. The MOX drop stays
// the arbiter's; nothing here reproduces or bypasses it.
//
// Reachable only while this pan's slice already holds TX, because
// SpectrumStatusOverlay paints and hit-tests the pill on m_txBound. The
// handoff is therefore a no-op today, and is wired to the correct target so
// that it stays correct if the pill is ever given an unlit clickable state --
// a visual decision, not one to make here.
void MainWindow::onPanTxBadgeClicked(const QString& panId)
{
    if (!m_panStack || !m_radioModel) { return; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return; }
    m_radioModel->requestTxHandoffToSlice(applet->activeSliceIndex());
}

// Task B5: straight forwarders. panId is the emitting applet's own id (see
// PanadapterApplet::buildContextMenu), never resolved through
// m_panStack->activePanId() -- see MainWindow.h for why that distinction
// matters here.
void MainWindow::onPanAddSliceRequested(const QString& panId)
{
    if (m_radioModel) { m_radioModel->addSliceOnPan(panId); }
}

void MainWindow::onPanFloatRequested(const QString& panId)
{
    if (m_panStack) { m_panStack->floatPanadapter(panId); }
}

// ── Anzeige-Wuensche aus der Panadapter-Kopfleiste ───────────────────
//
// Der Betreiber, 2026-08-20: „weiters fehlt auch die option button um
// beim pandapter gleich etwas zu aendern wie zb bild, groesse usw."
//
// Bild, Deckkraft und Grundfarbe lagen nur unter Setup -> Display,
// also drei Klicks und einen Dialog entfernt von der Flaeche, die man
// gerade ansieht.
void MainWindow::onPanBackgroundImage(const QString& path)
{
    if (SpectrumWidget* w = m_radioModel ? m_radioModel->spectrumWidget()
                                         : nullptr) {
        w->setBackgroundImage(path);
    }
}

void MainWindow::onPanBackgroundOpacity(int percent)
{
    if (SpectrumWidget* w = m_radioModel ? m_radioModel->spectrumWidget()
                                         : nullptr) {
        w->setBackgroundOpacity(percent);
    }
}

void MainWindow::onPanBackgroundColourReset()
{
    SpectrumWidget* w = m_radioModel ? m_radioModel->spectrumWidget() : nullptr;
    if (!w) { return; }
    w->resetBackgroundFillColor();
}

void MainWindow::onPanBackgroundColour()
{
    SpectrumWidget* w = m_radioModel ? m_radioModel->spectrumWidget() : nullptr;
    if (!w) { return; }
    // Mit Alphakanal: die Grundflaeche darf durchscheinen, wenn ein
    // Hintergrundbild darunter liegt.
    const QColor c = QColorDialog::getColor(
        w->backgroundFillColor(), this, tr("Grundfarbe des Panadapters"),
        QColorDialog::ShowAlphaChannel);
    if (c.isValid()) { w->setBackgroundFillColor(c); }
}

void MainWindow::onPanDisplaySetup()
{
    // Der Weg zum Rest. Ein Menue, das ALLES kann, waere wieder der
    // Dialog, nur schlechter.
    // Dasselbe Muster wie beim Overlay-Panel weiter oben: frisch
    // gebaut, WA_DeleteOnClose, verdrahtet, auf die Seite gestellt.
    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);
    dialog->selectPage(QStringLiteral("Colors & Theme"));
    dialog->show();
}

void MainWindow::onPanDockRequested(const QString& panId)
{
    if (m_panStack) { m_panStack->dockPanadapter(panId); }
}

void MainWindow::wireSliceStatusOverlayTriggers(SliceModel* slice)
{
    if (!slice) { return; }

    // Connect the NOTIFY signal of every property the overlay reads, resolved
    // through the metaobject from the list PanadapterApplet publishes. Naming
    // the signals here instead would put the trigger set in a second place
    // that has to be remembered -- which is how updateStatusOverlay came to
    // have no callers at all.
    const QMetaObject* sliceMeta = slice->metaObject();
    const int refreshIdx =
        MainWindow::staticMetaObject.indexOfSlot("refreshPanStatusOverlays()");
    if (refreshIdx < 0) {
        // Renaming the slot without updating this string would otherwise
        // strand every overlay silently -- the exact failure this change
        // exists to fix. tst_pan_status_overlay pins the lookup so it cannot
        // reach a release, and this keeps a debug build loud if it ever does.
        qWarning("MainWindow: refreshPanStatusOverlays() slot not found; "
                 "per-pan status overlays will not follow slice state");
        return;
    }
    const QMetaMethod refresh = MainWindow::staticMetaObject.method(refreshIdx);

    for (const QByteArray& name : PanadapterApplet::statusOverlaySliceProperties()) {
        const int propIdx = sliceMeta->indexOfProperty(name.constData());
        if (propIdx < 0) { continue; }
        const QMetaProperty prop = sliceMeta->property(propIdx);
        if (!prop.hasNotifySignal()) { continue; }
        connect(slice, prop.notifySignal(), this, refresh, Qt::UniqueConnection);
    }
}

void MainWindow::rebuildFftRouting()
{
    if (!m_radioModel) { return; }

    // The WIDE badges ride this pass rather than getting their own connects
    // at each of the four call sites. The trigger set is identical -- any
    // change to which slices feed which pan through which stream moves both
    // answers -- so hanging the refresh here means every present and future
    // topology trigger reaches the badge by construction. Same reasoning the
    // Alex republish uses for hanging off bpfStateChanged (RadioModel.cpp:
    // 596-601). Ahead of the router guard on purpose: the badge is a model
    // question and stays correct with no FFTRouter present.
    refreshPanWideBadges();

    // The status overlay rides the same pass, for the same reason: every
    // topology trigger (slice add / remove, pan migration, stream rebind,
    // chain reassignment) moves which slice a pan shows and which chain feeds
    // it, so hanging the refresh here means present and future triggers reach
    // the overlay by construction rather than by remembering. The per-slice
    // frequency / mode triggers and each pan's activeSliceChanged are wired
    // separately, since those move the overlay without moving the topology.
    refreshPanStatusOverlays();

    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // Wholesale rebuild, not an incremental edit. A pan can host several
    // slices and FFTRouter::removePan drops a pan from EVERY receiver, so a
    // remove-then-add on one slice's migration would silently unsubscribe
    // its co-hosted neighbours. Binding signals also fire before sliceAdded
    // (plan discovery item 7), so only a rebuild-from-model consumer is
    // safe.
    //
    // Snapshot the pre-rebuild topology first so a brand-new subscription
    // can be told apart from one that merely survived. Only new ones get
    // the stream window pushed: streamBindingsChanged fires on every bind,
    // so on every VFO tick, and re-pushing the allocator's centre each time
    // would yank a CTUN pan back after an operator pan-drag (that path
    // retunes the DDC through forceHardwareFrequency without going through
    // the allocator).
    QHash<QString, QList<int>> before;
    if (m_panStack) {
        // PanadapterStack::allApplets (PanadapterStack.h:70) and
        // PanadapterApplet::panId (PanadapterApplet.h:65) both already exist.
        for (auto* applet : m_panStack->allApplets()) {
            if (!applet) { continue; }
            const QString panId = applet->panId();
            before.insert(panId, router->receiversForPan(panId));
            router->removePan(panId);
        }
    }

    for (SliceModel* slice : m_radioModel->slices()) {
        if (!slice) { continue; }
        const int stream = slice->streamIndex();
        if (stream < 0) { continue; }          // unbound slice feeds nothing

        if (!m_panStack) { continue; }
        const QString panId = panIdForSlice(slice);
        if (panId.isEmpty()) { continue; }

        const bool isNewSubscription = !before.value(panId).contains(stream);
        // mapPanToReceiver de-duplicates (FFTRouter.cpp:17), so two slices
        // sharing a stream and a pan produce one subscription, not two.
        router->mapPanToReceiver(panId, stream);
        if (isNewSubscription) {
            applyStreamWindowToPan(panId, stream);
        }
    }
}

void MainWindow::dispatchFftFrameToPans(int streamIndex,
                                        const QVector<float>& binsLinear,
                                        double windowEnb,
                                        double dbmOffset)
{
    if (!m_panStack || !m_radioModel) { return; }
    auto* router = m_radioModel->fftRouter();
    if (!router) { return; }

    // FFTRouter is the topology oracle rather than a signal hop:
    // pansForReceiver is public and unit-tested, and routing through its
    // own signal would add a queued hop on the render path for no gain.
    // One stream can feed N pans (different zoom levels of the same I/Q),
    // which is the AetherSDR overlay model the router was designed for.
    for (const QString& panId : router->pansForReceiver(streamIndex)) {
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            sw->updateSpectrumLinear(streamIndex, binsLinear,
                                     windowEnb, dbmOffset);
        }
    }
}

// Phase 3F Sub-Epic D Task 16: disconnect-before-removal for safe pan teardown.
// AetherSDR issue #242: deleting a widget with active connections to lambdas
// can race with queued signal delivery and crash. Disconnect first, then
// remove.
void MainWindow::disconnectPanadapter(const QString& panId)
{
    if (!m_panStack) { return; }
    auto* applet = m_panStack->panadapter(panId);
    if (!applet) { return; }

    if (auto* sw = applet->spectrumWidget()) {
        sw->disconnect(this);
    }
    applet->disconnect(this);

    if (m_radioModel) {
        if (auto* router = m_radioModel->fftRouter()) {
            router->removePan(panId);
        }
    }
}

// Issue #206 — main-window geometry persistence. Qt's saveGeometry()
// returns a versioned QByteArray that already encodes position, size,
// AND window state (Normal/Maximized/FullScreen) plus screen identity
// for multi-monitor setups. We base64-encode it so AppSettings (which
// stores QString values) can round-trip the blob unmodified.
void MainWindow::saveMainWindowGeometry()
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("MainWindowGeometry"),
               QString::fromLatin1(saveGeometry().toBase64()));
}

bool MainWindow::restoreMainWindowGeometry()
{
    auto& s = AppSettings::instance();
    const QString blob = s.value(QStringLiteral("MainWindowGeometry")).toString();
    if (blob.isEmpty()) {
        return false;
    }
    const QByteArray bytes = QByteArray::fromBase64(blob.toLatin1());
    if (bytes.isEmpty() || !restoreGeometry(bytes)) {
        return false;
    }

    // Multi-screen safety: if the restored frame's center sits outside
    // every connected screen (monitor disconnected since last save),
    // fall back to the centered 1280×800 default rather than parking
    // the window offscreen where the user can't reach it.
    const QPoint center = frameGeometry().center();
    if (!QGuiApplication::screenAt(center)) {
        resize(1280, 800);
        if (auto* primary = QGuiApplication::primaryScreen()) {
            const QRect avail = primary->availableGeometry();
            move(avail.center() - QPoint(width() / 2, height() / 2));
        }
        // Drop any saved Maximized/FullScreen bit so we don't immediately
        // re-maximize onto a screen layout that no longer matches.
        setWindowState(Qt::WindowNoState);
        return false;
    }

    return true;
}

void MainWindow::buildUI()
{
    // Title: name, version, then whatever else the operator needs to tell
    // this window apart from another one.
    //
    // Issue #100 added the active profile, so two instances against
    // different radios are distinguishable. The build tag is the same idea
    // for the same reason: a smoke build under test has to be
    // distinguishable from a release and from another worktree's build.
    // Standing rule (JJ, KG4VCF, 2026-07-30), earned when a session spent
    // real time proving by pgrep which binary was on screen.
    //
    // The tag is empty on release artifacts, which are built from a tag ref,
    // so their title stays exactly as it was. main() fills BuildIdentity in
    // from a header regenerated on every build, so the sha here is the sha
    // that was compiled, not the one that was current at the last configure.
    // Der Name der Anwendung, nicht der des Vorgaengers. Der
    // Betreiber hat am 2026-08-20 zu Recht reklamiert, dass hier
    // noch „NereusSDR" stand, obwohl das Programm laengst Longpath
    // heisst — alle Urhebervermerke der Vorlagen bleiben davon
    // unberuehrt und stehen weiterhin im Ueber-Dialog.
    QString title = QStringLiteral("Longpath %1").arg(NEREUSSDR_VERSION);

    const QString buildTag = BuildIdentity::buildTag();
    if (!buildTag.isEmpty()) {
        title += QStringLiteral(" · %1").arg(buildTag);
    }

    // Profile stays last: it is per-run operator state, whereas the build
    // tag is a property of the binary, and the binary identity reads better
    // next to the version it belongs to.
    const QString profile = AppSettings::profileOverride();
    if (!profile.isEmpty()) {
        title += QStringLiteral(" [%1]").arg(profile);
    }

    setWindowTitle(title);
    setMinimumSize(800, 600);
    resize(1280, 800);

    // Issue #206 — restore last session's window position, size, and
    // maximized/fullscreen state. The 1280×800 above stays as the
    // first-launch fallback; restoreMainWindowGeometry() returns false
    // when no saved blob exists or the blob is corrupted, in which
    // case Qt centers the default size on the primary screen as before.
    restoreMainWindowGeometry();

    // --- Main QSplitter: spectrum (left) + container panel (right) ---
    // AetherSDR pattern: right panel is a proper layout element, not an overlay.
    m_mainSplitter = new QSplitter(Qt::Horizontal, this);
    m_mainSplitter->setChildrenCollapsible(false);
    m_mainSplitter->setHandleWidth(Style::kSplitterHandlePx);
    m_mainSplitter->setStyleSheet(Style::splitterStyle());

    // Left side: spectrum + zoom bar
    auto* spectrumPane = new QWidget(m_mainSplitter);
    spectrumPane->setMinimumWidth(400);
    auto* layout = new QVBoxLayout(spectrumPane);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Phase 3F Sub-Epic D Task 12: replace the single SpectrumWidget with
    // a PanadapterStack. PanadapterStack's constructor pre-creates
    // "pan-0" containing a PanadapterApplet whose embedded SpectrumWidget
    // becomes the new single-pan default. activeSpectrumWidget() resolves
    // to that widget for backward-compat call sites.
    m_panStack = new PanadapterStack(spectrumPane);
    m_panStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(m_panStack, 1);

    // Betreiber 2026-08-31: "Panadapter ändert sich immer!" -- ein
    // abgeloester Panadapter kehrte bei JEDEM Neustart wieder angedockt
    // zurueck, ganz gleich, wie er verlassen wurde. Ursache: PanadapterStack
    // sendet panFloatStateChanged() seit Sub-Epic D Task 8 treu bei jedem
    // Abloesen/Andocken -- nur hoerte hier nie jemand zu, und
    // applyLayout() (der Start-Wiederherstellungs-Pfad ein paar Zeilen
    // weiter unten) ruft als allerersten Schritt dockAllFloatingPans() auf,
    // unbedingt, ohne jede Ausnahme. Ohne einen gemerkten Zustand blieb
    // "schwebend" also niemals ueber einen Neustart hinweg erhalten --
    // dasselbe Bild wie beim Rotor vor dem heutigen RotorFloating-Fix, nur
    // dass hier noch nie ueberhaupt ein Schluessel dafuer existierte.
    connect(m_panStack, &PanadapterStack::panFloatStateChanged,
            this, [](const QString& panId, bool floating) {
        AppSettings::instance().setValue(
            QStringLiteral("PanFloating_%1").arg(panId),
            floating ? QStringLiteral("True") : QStringLiteral("False"));
    });

    SpectrumWidget* const initialSpectrum = activeSpectrumWidget();
    if (initialSpectrum) {
        configureSpectrumForPanForTest(initialSpectrum,
                                       QStringLiteral("pan-0"));
    }

    // Task 13 wires per-pan rebinding when the active pan changes; for
    // now we just log/no-op.  Future polish: re-attach overlay panel,
    // peak-detector, spot bridges, etc. to the new active pan's widget.
    // Point the Setup dialog's Display pages at the pan the operator is on.
    //
    // Bench report 2026-07-30 (JJ, KG4VCF): "the second-N panadapters seem
    // not to honour the settings for display, pan 1 does, seems no way to
    // adjust the others."
    //
    // RadioModel::m_spectrumWidget is the single hook every Display setup
    // page pushes through (82 call sites across DisplaySetupPages,
    // AppearanceSetupPages and SpectrumPeaksPage). It was assigned once
    // during wiring from activeSpectrumWidget() and never again, so it kept
    // whichever widget happened to be active at startup for the whole
    // session. Every display control in Setup therefore acted on one pan and
    // silently did nothing for the others.
    //
    // Following the active pan is the smallest thing that makes the controls
    // reachable at all, and it suits the per-pan model: SpectrumWidget
    // already persists every display preference under
    // settingsKey(key, m_panIndex), so the pans genuinely have their own
    // settings and Setup just needs to say which one it means. Selecting a
    // pan and opening Setup now adjusts that pan.
    //
    // Not an activePanId indirection of the kind that rule forbids: this is
    // a global dialog choosing a target, not a control drawn on one pan
    // reaching sideways into another. A per-pan selector inside Setup, the
    // way Thetis splits RX1 and RX2 display settings, is the fuller answer.
    connect(m_panStack, &PanadapterStack::activePanChanged, this,
            [this](const QString& panId) {
        if (!m_radioModel || !m_panStack) { return; }
        if (SpectrumWidget* sw = m_panStack->spectrum(panId)) {
            m_radioModel->setSpectrumWidget(sw);
        }
    });

    // Phase 3F: the status overlay's non-slice trigger. countChanged fires
    // from addPanadapter / removePanadapter, including the ones applyLayout
    // makes when the operator switches template, so this catches every pan
    // that comes into existence after startup -- which is every pan except
    // pan-0. Re-arming is safe: the connects inside are UniqueConnection.
    connect(m_panStack, &PanadapterStack::countChanged, this, [this](int) {
        wirePanStatusOverlayTriggers();
        wirePanBadgeHandlers();
        wirePanNotchHandlers();
        ensureOverlayPanels();
        refreshPanStatusOverlays();
        refreshPanNotchMarkers();
        refreshPanVisualNotch();
        refreshPanNotchMinWidth();
    });

    // TNF: the notch list is global, so one connect per NotchModel signal
    // repaints every pan. refreshPanNotchMarkers takes no arguments; Qt
    // drops the extra ones from notchAdded / notchChanged / notchRemoved /
    // globalEnabledChanged.
    if (NotchModel* notches = m_radioModel->notchModel()) {
        connect(notches, &NotchModel::notchAdded,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchChanged,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchRemoved,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::notchesReset,
                this, &MainWindow::refreshPanNotchMarkers);
        connect(notches, &NotchModel::globalEnabledChanged,
                this, &MainWindow::refreshPanNotchMarkers);
        // Design section 8.3: the visual-notch toggle is model state, so it
        // gets a model trigger as well as the pan-count one above.
        connect(notches, &NotchModel::visualEnabledChanged,
                this, &MainWindow::refreshPanVisualNotch);
    }

    // TNF: the minimum notch width comes off an RxChannel, and channels open
    // when the pool does. Both of these run long after the pans exist, so
    // they are what gets the real value onto a pan that started on the 100 Hz
    // construction default. refreshPanNotchMinWidth arms the per-channel
    // follow itself, so this only has to cover channels coming into being.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int) { refreshPanNotchMinWidth(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int) { refreshPanNotchMinWidth(); });
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](Longpath::ConnectionState) { refreshPanNotchMinWidth(); });

    // The S-meter poller's slice list keys off SLICE lifetime, not pan count.
    // Adding a slice to an existing pan moves no pan count, so hanging this on
    // countChanged alone left the new slice's flag bar dead.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int) { refreshMeterPollerSlices(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int) { refreshMeterPollerSlices(); });
    wirePanStatusOverlayTriggers();
    wirePanBadgeHandlers();
    wirePanNotchHandlers();
    // Seed pan-0 with whatever NotchModel::restoreFromSettings() already
    // loaded in the RadioModel constructor. The layout restore below fires
    // countChanged and re-runs both of these for the pans it creates.
    refreshPanNotchMarkers();
    refreshPanVisualNotch();
    refreshPanNotchMinWidth();

    // ── Bench 2026-07-28: click-to-tune always tuned flag A ────────────────
    //
    // The pan a slice lives on has to follow the operator's selection.
    // RadioModel::activeSlice() (global) and
    // PanadapterApplet::activeSliceIndex() (per pan) are independent, and
    // only the global one had a writer once addSlice had seeded the pan --
    // so a pan latched onto the first slice added to it and stayed there.
    // MainWindow::sliceForPan reads the per-pan value, which is what
    // click-to-tune, the filter-edge drag, the CH tag and the pan TX pill
    // all act on, so selecting flag B moved every global surface and left
    // all four still driving A. This is the missing writer.
    //
    // Direction is one way, pan follows global, and deliberately so:
    //
    //   * Every route the operator has for selecting a slice (a VfoWidget
    //     flag press, an RxApplet slice tab, a band button, TCI) already
    //     lands on RadioModel::setActiveSlice, so following it here covers
    //     all of them in one wire instead of one per entry point.
    //   * The reverse -- a pan's own re-pick promoting itself to the global
    //     active slice -- is NOT wired. PanadapterApplet::removeSlice
    //     re-picks silently when a pan loses its active slice, and letting a
    //     background pan's bookkeeping steal the RX applet, the container
    //     S-meter and the DSP menu out from under the operator is a change
    //     to what they see, not to what tunes. RadioModel::removeSlice
    //     already re-seats the global active slice on its own when the
    //     removed one held it.
    //
    // Only the pan hosting the slice moves; setActiveSliceOnHostingPan is
    // where that rule lives. activeSliceChanged carries a LIST POSITION, so
    // the id is resolved through activeSlice()->sliceIndex() rather than
    // used as it arrives.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int) {
        if (!m_panStack || !m_radioModel) { return; }
        if (SliceModel* active = m_radioModel->activeSlice()) {
            m_panStack->setActiveSliceOnHostingPan(active->sliceIndex());
        }
    });

    // Phase 3F Sub-Epic D Task 15: restore persisted pan layout + splitter
    // sizes. Reads PanLayoutId from AppSettings (default "1") and asks the
    // stack to materialise that many pans before restoring per-splitter
    // QByteArray state. Operators get their last layout back on launch.
    {
        auto& s = AppSettings::instance();
        const QString restoredLayout = s.value(QStringLiteral("PanLayoutId"),
                                                QStringLiteral("1")).toString();
        // Shares the template table with applyPanLayout, but deliberately not
        // the rest of it: this runs at startup, before a radio is connected
        // and before any slice exists, so there is nothing to rehome and the
        // slice add-loop would manufacture a slice pre-connect.
        // (Codex review round 3, PR #293.)
        m_panStack->applyLayout(restoredLayout, panIdsForLayout(restoredLayout));
        m_panStack->restoreSplitterState();

        // Betreiber 2026-08-31: "Panadapter ändert sich immer!" -- siehe
        // die ausfuehrliche Begruendung beim panFloatStateChanged-Anschluss
        // oben. applyLayout() hat gerade eben jeden Panadapter angedockt
        // (sein allererster Schritt, dockAllFloatingPans(), unbedingt);
        // hier wird direkt danach nachgeholt, was zuletzt tatsaechlich
        // schwebte. PanFloatingWindow::restoreGeometryState() (in dessen
        // eigenem Konstruktor) liest die dazugehoerige Lage gleich mit.
        for (const QString& panId : panIdsForLayout(restoredLayout)) {
            if (s.value(QStringLiteral("PanFloating_%1").arg(panId),
                        QStringLiteral("False")).toString()
                    == QStringLiteral("True")) {
                m_panStack->floatPanadapter(panId);
            }
        }

        // ...and finish the job once there IS a radio. Skipping the slice
        // add-loop above is right, but nothing used to pick it up afterwards,
        // so a persisted 2v layout came back with pan-1 permanently dead: no
        // trace, no waterfall, a 0.0000 flag, and no way forward except
        // noticing you have to add Slice B by hand (bench, 2026-08-01,
        // J.J. Boyd KG4VCF).
        //
        // Queued so it lands after the connect handlers that size the stream
        // pool and bind Slice A have run; addSliceOnPan needs a sized pool to
        // bind what it creates.
        connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                [this](ConnectionState s) {
            if (s != ConnectionState::Connected) { return; }
            QMetaObject::invokeMethod(this, [this]() { populateEmptyPans(); },
                                      Qt::QueuedConnection);
        });
    }

    // Phase 3F Sub-Epic E Task 3: the badge clicks are armed for every pan
    // from the countChanged hook above, alongside the status-overlay
    // triggers. See wirePanBadgeHandlers.

    // Left overlay panel (SpectrumOverlayPanel) — child of the active
    // pan's SpectrumWidget. Construction is deferred when the active
    // pan has no widget (shouldn't happen because the stack ctor
    // creates pan-0, but be defensive).
    // One strip per pan, created here for the pans that exist at startup and
    // re-armed from the countChanged hook for every pan created later.
    // m_overlayPanel stays pointing at pan-0's so the display-settings and
    // band wiring further down keeps a stable target.
    ensureOverlayPanels();

    // ── 2026-05-12 bench fix: SpotModel → SpectrumWidget bridge ───────────
    //
    // The missing bridge — every spot ingestion path (Cluster / RBN /
    // WSJT-X / SpotCollector / POTA / FreeDV / PSK Reporter) lands in
    // RadioModel's per-source `on<Source>SpotReceived` slot which calls
    // `m_spotModel->applySpotStatus(...)`.  SpotModel then emits
    // spotAdded / spotUpdated / spotRemoved / spotsCleared.
    //
    // But upstream AetherSDR's `refreshSpots()` lambda on MainWindow —
    // the thing that translates SpotData into SpectrumWidget::SpotMarker
    // and calls `setSpotMarkers(...)` to repaint the panadapter overlay —
    // never carried over in the port.  Result: SpotTableModel (the Spot
    // List tab) fills correctly because it has its own per-source
    // wireClient lambda; the panadapter, however, has been blind to
    // every spot since Phase 3J-2 shipped.
    //
    // This block rebuilds the marker vector on every SpotModel change
    // (full rebuild, not delta — the typical spot map is <500 entries
    // with a 30-min lifetime).  DxCC-aware coloring uses the same
    // DxccColorProvider the SpotTableModel queries.
    if (auto* spotModel = m_radioModel->spotModel()) {
        auto refreshSpots = [this]() {
            if (!activeSpectrumWidget() || !m_radioModel) { return; }
            auto* spotModel = m_radioModel->spotModel();
            if (!spotModel) { return; }
            auto* dxccColor = m_radioModel->dxccColorProvider();

            const auto& spots = spotModel->spots();
            QVector<SpectrumWidget::SpotMarker> markers;
            markers.reserve(spots.size());
            for (auto it = spots.constBegin(); it != spots.constEnd(); ++it) {
                const SpotData& s = it.value();
                SpectrumWidget::SpotMarker m;
                m.index            = s.index;
                m.callsign         = s.callsign;
                // Prefer the RX (heard-on) frequency so cluster spots
                // land on the actual TX freq of the DX station.  Fall
                // back to txFreqMhz when only one is populated (e.g.
                // POTA / RBN sometimes ship rxFreqMhz only).
                m.freqMhz          = (s.rxFreqMhz > 0.0)
                                         ? s.rxFreqMhz
                                         : s.txFreqMhz;
                m.color            = s.color;
                m.mode             = s.mode;
                m.source           = s.source;
                m.spotterCallsign  = s.spotterCallsign;
                m.comment          = s.comment;
                m.timestampMs      = s.timestamp.isValid()
                                         ? s.timestamp.toMSecsSinceEpoch()
                                         : QDateTime::currentMSecsSinceEpoch();
                if (dxccColor && dxccColor->isEnabled()
                    && !m.callsign.isEmpty() && m.freqMhz > 0.0) {
                    m.dxccColor = dxccColor->colorForSpot(
                        m.callsign, m.freqMhz, m.mode);
                }
                markers.append(m);
            }
            activeSpectrumWidget()->setSpotMarkers(markers);
        };

        connect(spotModel, &SpotModel::spotAdded,
                this, [refreshSpots](const SpotData&) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotUpdated,
                this, [refreshSpots](const SpotData&) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotRemoved,
                this, [refreshSpots](int) { refreshSpots(); });
        connect(spotModel, &SpotModel::spotsCleared,
                this, refreshSpots);
        connect(spotModel, &SpotModel::spotsRefreshed,
                this, refreshSpots);

        // 2026-05-12 bench fix (Gap #3 follow-on).  Right-click → Remove
        // Spot on the panadapter emits spotRemoveRequested(idx) → purge
        // the spot from SpotModel → spotRemoved fires → refreshSpots
        // above repaints the overlay without it.
        connect(activeSpectrumWidget(), &SpectrumWidget::spotRemoveRequested,
                spotModel, &SpotModel::removeSpot);

        // 2026-08-11 bench fix, found ON the bench the same afternoon it
        // shipped: "Turn rotor to <call>" was wired only in
        // wireSpectrumForPan, and pan-0 — the pan everybody actually
        // right-clicks — is excluded from that function (its spot hooks
        // live HERE, as the comment at the exclusion says). So the menu
        // entry existed and did nothing. Same handler as the per-pan
        // wiring and the Spot List path.
        connect(activeSpectrumWidget(), &SpectrumWidget::spotRotorRequested,
                this, [this](const QString& dxCall) {
            if (RotorLogbookPanel* panel = ensureRotorPanel()) {
                raiseRotorPanel();
                panel->workSpot(dxCall);
            }
        });

        // Derselbe Weg fuer den Doppelklick, siehe Begruendung bei der
        // per-pan-Verdrahtung.
        connect(activeSpectrumWidget(), &SpectrumWidget::spotLogRequested,
                this, [this](const QString& dxCall) {
            if (RotorLogbookPanel* panel = ensureRotorPanel()) {
                raiseRotorPanel();
                panel->takeSpot(dxCall);
            }
        });
    }

    // Zoom slider bar below spectrum
    auto* zoomBar = new QSlider(Qt::Horizontal, spectrumPane);
    zoomBar->setRange(1, 768);
    zoomBar->setValue(768);
    zoomBar->setFixedHeight(20);
    zoomBar->setToolTip(QStringLiteral("Zoom: drag to adjust spectrum bandwidth"));
    zoomBar->setStyleSheet(Style::themed(QStringLiteral(
        "QSlider { background: #0a0a14; }"
        "QSlider::groove:horizontal { background: #1a2a3a; height: 6px; border-radius: 3px; }"
        "QSlider::handle:horizontal { background: #4a7ba8; width: 14px; margin: -4px 0; border-radius: 7px; }")));
    layout->addWidget(zoomBar);
    connect(zoomBar, &QSlider::valueChanged, this, [this](int val) {
        double bwHz = val * 1000.0;
        activeSpectrumWidget()->setFrequencyRange(activeSpectrumWidget()->centerFrequency(), bwHz);
        emit activeSpectrumWidget()->bandwidthChangeRequested(bwHz);
    });
    // Bench-gefunden 2026-09-03: zoomBar->setValue(768) oben ist nur der
    // Startwert -- nichts hielt den Regler danach synchron, wenn sich die
    // Bandbreite auf einem ANDEREN Weg aendert (Scroll-Zoom, Frequenzskalen-
    // Ziehen am Panadapter, oder ein wiederhergestelltes Profil mit
    // gespeicherter Zoom-Stufe, Phase 3G-12 persistiert genau das). Der
    // Regler zeigte danach dauerhaft die Stellung vom Start, auch wenn der
    // Panadapter laengst enger oder weiter gezoomt war — funktional
    // folgenlos (Ziehen am Regler setzt weiterhin korrekt), aber
    // irrefuehrend fuer den Bedienenden, der dem Regler nicht mehr
    // ansehen kann, wo der Panadapter tatsaechlich steht.
    // frequencyRangeChanged() feuert am Ende JEDER setFrequencyRange()
    // unabhaengig von der Ursache (SpectrumWidget.cpp), ein einziger
    // Anschluss deckt also alle Wege ab. QSignalBlocker verhindert die
    // Rueckkopplung in den Connect-Handler oben (der seinerseits
    // setFrequencyRange() aufriefe).
    // Codereview 2026-09-03 (gefunden, nicht gemeldet): wirePane-artige
    // Neuverdrahtung, siehe m_zoomBarSyncConn in MainWindow.h. Ein Lambda
    // statt einer einzelnen connect()-Zeile, weil dieselbe Verdrahtung
    // jetzt an zwei Stellen laufen muss: hier beim Bau und unten bei
    // jedem PanadapterStack::activePanChanged.
    auto wireZoomBarSync = [this, zoomBar](SpectrumWidget* sw) {
        disconnect(m_zoomBarSyncConn);
        if (!sw) { return; }
        m_zoomBarSyncConn = connect(sw, &SpectrumWidget::frequencyRangeChanged,
                this, [zoomBar](double /*centerHz*/, double bandwidthHz) {
            const int kHz = qBound(zoomBar->minimum(),
                                    qRound(bandwidthHz / 1000.0),
                                    zoomBar->maximum());
            if (kHz != zoomBar->value()) {
                const QSignalBlocker blocker(zoomBar);
                zoomBar->setValue(kHz);
            }
        });
    };
    wireZoomBarSync(activeSpectrumWidget());
    connect(m_panStack, &PanadapterStack::activePanChanged, this,
            [this, wireZoomBarSync](const QString& panId) {
        if (m_panStack) { wireZoomBarSync(m_panStack->spectrum(panId)); }
    });

    m_mainSplitter->addWidget(spectrumPane);

    // Right side: Container #0 will be added by ContainerManager
    //
    // ── Kommandoleiste über allem ────────────────────────────────────
    //
    // Zeus setzt MODE und STEP als Pillenreihe an den oberen Rand, über
    // die ganze Breite. Deshalb ist das zentrale Widget jetzt eine
    // Säule: Leiste oben, Splitter darunter. Der Splitter bleibt sonst
    // unangetastet — alle 900 Zeilen darunter kennen ihn unverändert.
    m_commandBar = new CommandBar(this);

    // ── Bandwechsel aus der Leiste ──────────────────────────────────
    //
    // Der Betreiber am 2026-08-22: "bandwechsel sollte auch mit
    // buttons möglich sein, am besten in der leiste oben."
    //
    // Die Leiste kennt nur die Scheibe; ein Bandwechsel geht ueber
    // RadioModel::onBandButtonClicked, weil daran Bandplan,
    // Antennenwahl und die je Band gespeicherte Frequenz haengen.
    // Deshalb ein Signal statt eines zweiten Modellzeigers in der
    // Leiste.
    connect(m_commandBar, &CommandBar::bandRequested, this,
            [this](Longpath::Band band) {
        if (m_radioModel) { m_radioModel->onBandButtonClicked(band); }
    });
    auto* centre = new QWidget(this);

    // Profilschiene ganz links über die volle Höhe, wie bei Zeus.
    // Daneben die Säule aus Kommandoleiste und Splitter.
    m_layoutProfiles = new LayoutProfiles(this);
    m_profileRail = new ProfileRail(m_layoutProfiles, centre);

    auto* centreRow = new QHBoxLayout(centre);
    centreRow->setContentsMargins(0, 0, 0, 0);
    centreRow->setSpacing(0);
    centreRow->addWidget(m_profileRail, 0);

    auto* centreCol = new QVBoxLayout;
    centreCol->setContentsMargins(6, 6, 6, 0);
    centreCol->setSpacing(6);
    centreCol->addWidget(m_commandBar, 0);

    // ── Panadapter in BEIDE Richtungen veraenderbar (2026-08-19) ──────
    //
    // Auf Ansage des Betreibers: „der Panadapter soll auf der x- und
    // y-Achse verschoben werden koennen, sprich in alle Groessen und
    // Richtungen veraenderbar."
    //
    // Ein einzelner Splitter hat einen Griff, also EINE Achse. Also wird
    // der vorhandene GESCHACHTELT: aussen senkrecht, innen der bisherige
    // waagerechte.
    //
    //   aussen (senkrecht)
    //     +-- innen (waagerecht):  [ Panadapter | Applet-Leiste ]
    //     +-- untere Flaeche       (fuer Rotor/Log und spaeter Kacheln)
    //
    // Der innere Splitter behaelt seine IDENTITAET: ContainerManager
    // haelt einen Zeiger darauf und dockt die Applet-Leiste dort ein.
    // Haette ich ihn ersetzt statt umschlossen, waere die ganze
    // Container-Verwaltung mitzuziehen gewesen.
    m_outerSplitter = new QSplitter(Qt::Vertical, this);
    m_outerSplitter->setObjectName(QStringLiteral("outerSplitter"));
    m_outerSplitter->setChildrenCollapsible(false);
    m_outerSplitter->setHandleWidth(Style::kSplitterHandlePx);
    m_outerSplitter->setStyleSheet(Style::splitterStyle());
    m_outerSplitter->addWidget(m_mainSplitter);

    // Die untere Flaeche bleibt leer und verborgen, bis jemand etwas
    // hineinlegt. Ein sichtbarer leerer Streifen waere ein Versprechen,
    // das niemand eingeloest hat.
    m_belowPane = new QWidget(m_outerSplitter);
    m_belowPane->setObjectName(QStringLiteral("belowPane"));
    auto* belowCol = new QVBoxLayout(m_belowPane);
    belowCol->setContentsMargins(0, 0, 0, 0);
    belowCol->setSpacing(0);
    m_belowPane->setMinimumHeight(120);
    m_belowPane->hide();
    m_outerSplitter->addWidget(m_belowPane);
    syncOuterSplitterHandle();

    centreCol->addWidget(m_outerSplitter, 1);
    centreRow->addLayout(centreCol, 1);

    setCentralWidget(centre);

    // --- Container Infrastructure (Phase 3G-1) ---
    m_containerManager = new ContainerManager(spectrumPane, m_mainSplitter, this);

    // Phase 3P-I-a T17 — push board caps into every container so
    // AntennaButtonItems gate their click handler on hasAlex. Re-runs
    // when the active radio changes (currentRadioChanged) and also fires
    // when a new container is added (containerAdded). Without this,
    // freshly-created or restored containers keep the default
    // m_hasAlex=true and would allow clicks on HL2/Atlas.
    auto pushCapsToAllContainers = [this]() {
        const auto caps = m_radioModel->boardCapabilities();
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            c->setBoardCapabilities(caps);
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            pushCapsToAllContainers);

    // Per-SKU power-meter rescale.  Bench-reported #167 follow-up: the
    // top MeterPanel BarItem stack ships with a 0-120 W default that
    // makes HL2 (5 W) a sliver and ANAN-G2-1K (1000 W) saturate.  When
    // the active radio changes, ask every MeterWidget to rescale its
    // PowerBar / PowerScale pair to the new SKU's PA ceiling.  Same
    // paMaxWattsFor() helper TxApplet uses for its RF Pwr HGauge so
    // both meter surfaces share a single source of truth.
    auto rescaleAllPowerMeters = [this]() {
        const HPSDRModel m = m_radioModel->hardwareProfile().model;
        const int maxW     = paMaxWattsFor(m);
        if (m_meterWidget) {
            m_meterWidget->rescalePowerMeters(maxW);
        }
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            for (MeterWidget* mw : c->findChildren<MeterWidget*>()) {
                mw->rescalePowerMeters(maxW);
            }
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            rescaleAllPowerMeters);

    // Phase 3F Sub-Epic D Task 11: gate the bottom-bar CH 1 stacked
    // indicator widget on whether the connected radio drives a second RX
    // filter chain. buildStatusBar() ships chain1Widget hidden by default;
    // this slot toggles it on/off as the user switches radios.
    //
    // Defect D4: this used to read adcCount, which names the wrong thing. The
    // indicator reports a CHAIN's band-pass state, and ANAN-100D / ANAN-200D
    // have two ADCs behind one filter bank: the setAlex2HPF model list at
    // Thetis console.cs:15435-15443 [v2.10.3.15] never hands them a second
    // filter word. Showing CH 1 there offered the operator a second chain to
    // reason about, and a Filter Policy override to set on it, that the radio
    // does not have.
    // Upstream inline attribution preserved verbatim (console.cs:15441):
    //   HardwareSpecific.Model == HPSDRModel.REDPITAYA) //DH1KLM
    // Registered with m_chromeBar at rung 4 (design §6); the >=2 fact is
    // reported via setItemAvailable, not a direct setVisible call, per
    // ChromeBarController::setItemAvailable's own doc comment.
    auto updateChain1Visibility = [this]() {
        if (!m_chain1IndicatorWidget) { return; }
        const auto caps = m_radioModel->boardCapabilities();
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setItemAvailable(m_chain1IndicatorWidget,
                                          caps.rxFilterChainCount >= 2);
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    };
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            updateChain1Visibility);

    // Phase 3F Sub-Epic D Task 11: drive the CH 0 / CH 1 bottom-bar
    // indicator text + colour from AlexController's per-ADC BPF state.
    // Filtered = green; WidebandLocked / Bypass = amber. reasonText
    // (set by AlexController::recomputeBpf) drives the body label.
    connect(&m_radioModel->alexController(),
            &AlexController::bpfStateChanged, this,
            [this](int adc, const AlexController::AlexAdcState& state) {
                auto* lbl = findChild<QLabel*>(
                    QStringLiteral("chainIndicator%1").arg(adc));
                if (!lbl) { return; }
                lbl->setText(state.reasonText);
                const QString color =
                    (state.effective == AlexController::BpfEffective::Filtered)
                        ? Style::kGreenText
                        : Style::kAmberWarn;
                lbl->setStyleSheet(
                    QStringLiteral("color: %1; font-size: 9px; font-weight: bold;")
                        .arg(color));

                // reasonText ranges from "idle" to
                // "BYPASS (multi-band: 160m + 80m + 40m + 20m + 10m)", so the
                // owning chain widget's sizeHint (registered with m_chromeBar
                // at rung 4) just went stale. Report it so folding stays a
                // pure function of width rather than overflowing on the next
                // band change (final-fix-wave finding 1).
                if (m_chromeBar && m_chromeBarWidget) {
                    QWidget* chainWidget = (adc == 0) ? m_chain0IndicatorWidget
                                                       : m_chain1IndicatorWidget;
                    if (chainWidget) {
                        m_chromeBar->setNaturalWidth(
                            chainWidget, chainWidget->sizeHint().width());
                        m_chromeBar->relayout(m_chromeBarWidget->width());
                    }
                }
            });

    // Phase 3F: and drive the per-pan WIDE pill from the same signal.
    //
    // A separate connect rather than a line inside the lambda above: the two
    // surfaces answer different questions. The bottom-bar label reports one
    // chain by number, and this reports every pan that chain happens to feed,
    // which on a multi-pan layout is what tells the operator WHICH of their
    // receivers is exposed.
    //
    // The signal's own adc argument is deliberately unused. A recompute on
    // chain 1 can leave a pan straddling both chains bypassed for a reason
    // that now belongs to chain 0, so the refresh re-asks per pan rather than
    // trying to patch only the pans on the chain that moved.
    //
    // This covers the state-change half of the trigger set (band crossings,
    // wideband toggles, Filter Policy edits); rebuildFftRouting covers the
    // topology half (slice add / remove / pan migration / stream rebind).
    connect(&m_radioModel->alexController(),
            &AlexController::bpfStateChanged, this,
            [this](int, const AlexController::AlexAdcState&) {
                refreshPanWideBadges();
            });

    // Issue #118 — helper: wire a container's bandClicked signal through
    // the RadioModel handler. Invoked from the containerAdded callback,
    // which fires for every container materialized by ContainerManager
    // (runtime-added, restoreState(), and createDefaultContainers()).
    auto wireContainerBandClick = [this](ContainerWidget* c) {
        if (!c) { return; }
        connect(c, &ContainerWidget::bandClicked, this, [this](int idx) {
            m_radioModel->onBandButtonClicked(bandFromUiIndex(idx));
        });
    };

    connect(m_containerManager, &ContainerManager::containerAdded, this,
            [this, wireContainerBandClick](const QString& id) {
        if (auto* c = m_containerManager->container(id)) {
            c->setBoardCapabilities(m_radioModel->boardCapabilities());
            wireContainerBandClick(c);
        }
    });

    // Create the MeterPoller BEFORE restoreState / populateDefaultMeter
    // so the meterReadyForPolling signal fires into a live poller as
    // each container's MeterWidget is materialized. Previously the
    // poller was created later and only the panel container's
    // m_meterWidget was registered manually — every user-created
    // container's meter sat orphaned and bars never received setValue()
    // calls, the root of the "BarMeter not drawing" symptom.
    m_meterPoller = new MeterPoller(this);

    // ── Die Instrumente an denselben Umlauf ──────────────────────────
    //
    // readingUpdated kommt aus MeterPoller::dispatch, also aus
    // derselben Verteilung, die auch die Meter-Items speist. Kein
    // zweiter Timer, keine zweite Abfrage — die Instrumente sehen
    // dieselben Zahlen zum selben Zeitpunkt.
    //
    // Die Applets entstehen erst weiter unten; QPointer wäre hier
    // falsch, weil die Verbindung an EINEN Zeiger gebunden würde, den
    // es noch nicht gibt. Also über `this` und ein Lambda, das die
    // Mitglieder zur Laufzeit liest.
    connect(m_meterPoller, &MeterPoller::readingUpdated, this,
            [this](int bindingId, double value) {
        if (m_swrInstrument)    { m_swrInstrument->onReading(bindingId, value); }
        if (m_signalInstrument) { m_signalInstrument->onReading(bindingId, value); }
        // Das Frequenz-Widget kann seit dem 2026-08-23 zwei Zusatzzeilen
        // fuehren (Stehwelle, SWR). Sie haengen an derselben Quelle wie
        // die eigenstaendigen Anzeigen, damit beide dieselbe Zahl zum
        // selben Zeitpunkt zeigen — der Grund, aus dem diese Verteilung
        // ueberhaupt an EINER Stelle steht. Das eigene SWR/Leistung-
        // Applet (TxMeterApplet) ist am 2026-08-30 entfernt worden --
        // Betreiber: "nur zusaetzlich im Bereich des Frequenzfenster,
        // nicht alle" -- das Frequenz-Widget ist seither die EINZIGE
        // Stelle dafuer.
        if (m_frequencyApplet)  { m_frequencyApplet->onReading(bindingId, value); }
    });

    // Task 3.1: expose MeterPoller via RadioModel so MultimeterPage can
    // apply live interval + averaging-window changes without a MainWindow
    // round-trip.  Non-owning; RadioModel stores the pointer only.
    m_radioModel->setMeterPoller(m_meterPoller);
    // Task 3.2: expose ContainerManager via RadioModel so MultimeterPage
    // can broadcast unit-mode changes to all live MeterItems.
    m_radioModel->setContainerManager(m_containerManager);
    connect(m_containerManager, &ContainerManager::meterReadyForPolling,
            this, [this](MeterWidget* meter) {
        if (!meter || !m_meterPoller) { return; }
        m_meterPoller->addTarget(meter);
        // Auto-unregister when the meter is destroyed (e.g.
        // ContainerWidget::setContent deleteLater()s a previous
        // content during a swap). Without this the poller would
        // dereference a dangling pointer on its next tick.
        connect(meter, &QObject::destroyed, m_meterPoller,
                [this](QObject* obj) {
            if (m_meterPoller) {
                m_meterPoller->removeTarget(static_cast<MeterWidget*>(obj));
            }
        });
    });

    m_containerManager->restoreState();
    if (m_containerManager->containerCount() == 0) {
        createDefaultContainers();
    }

    // Betreiber 2026-08-28: dieselbe "vor der ersten Verbindung bedeutungslos"
    // -Regel wie beim Rotor/Log gilt fuer jedes Meter/Applet-Fenster -- S-
    // Meter, Stehwelle, Mitschrift und Co. zeigen ohne Radio nichts
    // Sinnvolles (kein Signal, keine SWR, nichts zu transkribieren).
    // restoreState() hat sie gerade eben unconditional wieder gezeigt.
    //
    // ZWEI Faelle, nicht nur einer: ein Container ist entweder isFloating()
    // (eigenes FloatingContainer-Top-Level-Fenster -- c->window() ist das
    // Richtige zum Verstecken) ODER isOverlayDocked() (direkt ins
    // Hauptfenster eingeblendet, ueber m_dockParent, KEIN eigenes Fenster --
    // window() waere hier `this`, das Hauptfenster selbst, und das Verstecken
    // muss am Container ansetzen). Der erste Anlauf traf nur isFloating();
    // Stehwelle/Frequenz/S-Meter liefen als OverlayDocked durch und blieben
    // sichtbar -- am Bildschirm nachgesehen, nicht geraten. Nur die, die
    // dadurch tatsaechlich zugingen, landen in der Liste, die mit der
    // ersten Verbindung wieder aufgeht (siehe die connectionStateChanged-
    // Bindung bei der Profil-Wiederherstellung weiter unten).
    if (!m_radioModel
        || m_radioModel->connectionState() != ConnectionState::Connected) {
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            if (!c) { continue; }
            if (c->isFloating()) {
                QWidget* win = c->window();
                if (win && win != this && win->isVisible()) {
                    win->hide();
                    m_floatingContainersHiddenPreConnect.append(win);
                }
            } else if (c->isOverlayDocked()) {
                if (c->isVisible()) {
                    c->hide();
                    m_floatingContainersHiddenPreConnect.append(c);
                }
            }
        }
        // Betreiber 2026-09-01: "wieder panadapter auf der anmeldeleiste!"
        // -- ein VIERTER, unabhaengiger Schwebe-Mechanismus (PanadapterStack/
        // PanFloatingWindow, ueber den globalen PanFloating_<id>-Schluessel,
        // nicht ueber ContainerManager oder m_floatingApplets), den die
        // beiden Schleifen oben nie trafen. Ohne Funkgeraet zeigt der
        // Panadapter dieselbe bedeutungslose Leerflaeche wie S-Meter & Co.
        // -- UND sein eigener Schliessen-Knopf ueberlappte den Verbinden-
        // Dialog: ein Klick auf "Connect" konnte den Panadapter-Knopf
        // darunter treffen und ihn andocken (siehe [PanFloatClose]-Fund,
        // spontaneous=false -- ein echter Klick auf den eigenen X-Knopf,
        // keine Nebenwirkung von aussen).
        if (m_panStack) {
            for (const QString& panId : m_panStack->panIdsForTesting()) {
                if (m_panStack->isPanFloating(panId)) {
                    if (auto* pf = m_panStack->floatingWindowForTest(panId)) {
                        if (pf->isVisible()) {
                            pf->hide();
                            m_floatingContainersHiddenPreConnect.append(pf);
                        }
                    }
                }
            }
        }
    }
    // Always populate the panel container's content (meters + applets).
    // On first run, createDefaultContainers() creates the shell; on restore,
    // restoreState() recreates the shell but content is lost. This ensures
    // the applet panel is always populated regardless of restore path.
    populateDefaultMeter();
    m_containerManager->restoreSplitterState();

    // Phase 3P-I-a T17 — initial push. `containerAdded` fires during
    // restoreState() but the content (and any AntennaButtonItems) are
    // installed after the signal by populateDefaultMeter() or the saved
    // content factory. Do a one-shot sweep here so the final items
    // pick up the startup board capabilities.
    pushCapsToAllContainers();

    // Default splitter sizes on first run: ~80% spectrum, ~20% panel
    if (!AppSettings::instance().contains(QStringLiteral("MainSplitterSizes"))) {
        m_mainSplitter->setSizes({1024, 256});
    }

    // Die zuletzt gewaehlte Richtung wiederherstellen. Vorgabe ist
    // daneben — das ist der Istzustand, und eine Vorgabe stellt niemandem
    // das Fenster um.
    if (AppSettings::instance().value(QStringLiteral("AppletPanelBelow"),
                                      QStringLiteral("False")).toString()
            == QStringLiteral("True")) {
        setAppletPanelBelow(true);
    }

    // Rotor/Log unter dem Panadapter, falls zuletzt so gewaehlt. Nach
    // dem Fensteraufbau, weil ensureRotorPanel() das Dock erst anlegt.
    if (AppSettings::instance().value(QStringLiteral("RotorPanelBelow"),
                                      QStringLiteral("False")).toString()
            == QStringLiteral("True")) {
        setRotorPanelBelow(true);
    } else if (AppSettings::instance().value(QStringLiteral("RotorFloating"),
                                             QStringLiteral("True")).toString()
                   == QStringLiteral("True")) {
        // Betreiber, 2026-08-28: Rotor/Log soll von sich aus ein eigenes
        // Fenster sein (Titelleiste, Schloss, Anfasser unten rechts) statt
        // rechts im schmalen, native-chromen QDockWidget zu haengen. Vorgabe
        // "True", nicht nur bei explizitem Wunsch: das trifft auch jede
        // Installation, die die Einstellung noch nie gesetzt hat.
        detachRotorPanel();
    }

    // Betreiber, 2026-08-28: "das ist bevor ich mich einlogge - das rotor
    // fenster gehört weg" -- der Kompass ist ohne Verbindung ohnehin
    // bedeutungslos (000°, kein Ziel). NUR NOCH m_rotorDock (die angedockte
    // Form) folgt dieser Regel, siehe die dedizierte
    // m_rotorDockWantedVisible-Logik im Profil-Anwenden weiter unten.
    // m_rotorWindow (die SCHWEBENDE Form) NICHT mehr: Betreiber 2026-09-01,
    // "kein Funkgerät hier, alles nicht verbunden" -- schwebende Fenster
    // (Panadapter/Applets/Rotor-Log) sollen sich auch ohne Verbindung
    // anordnen und ansehen lassen, sonst ist Layout-Arbeit ohne Radio am
    // Tisch unmöglich. "000° kein Ziel" bleibt sichtbar stehen, statt das
    // Fenster selbst zu verstecken.
    if (!m_radioModel
        || m_radioModel->connectionState() != ConnectionState::Connected) {
        if (m_rotorDock) { m_rotorDock->hide(); }
    }

    // Wire spectrum display to SliceModel (values come from persisted state,
    // no longer hardcoded). Connection is deferred to wireSliceToSpectrum()
    // which runs after RadioModel creates slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index == 0) {
            wireSliceToSpectrum();
        }
    });

    // Push restored slice state into spectrum + VFO views once
    // RadioModel::loadSliceState() completes.
    //
    // wireSliceToSpectrum() above runs at sliceAdded() time — BEFORE
    // loadSliceState() runs inside connectToRadio.  At that earlier moment
    // the slice still holds its pre-restore default values, so the
    // spectrum widget's m_ddcCenterHz, center freq, and VFO freq were
    // seeded with the default (typically 14.225 MHz / 20m).  After
    // loadSliceState() restores the persisted band/freq/mode/filter,
    // SliceModel::frequencyChanged is gated by qFuzzyCompare and by the
    // CTUN-shift branch in wireSliceToSpectrum's frequencyChanged lambda
    // (the offScreen path that calls setDdcCenterFrequency only fires
    // when persisted freq is OUTSIDE default ±halfBw).  Without an
    // explicit re-push, m_ddcCenterHz stays at the default and spectrum
    // bin labels point to the wrong band of RF until the first dial-
    // tune crosses the offScreen threshold.
    //
    // Mirrors Thetis txtVFOAFreq_LostFocus's unconditional Display.VFOA
    // / Display.CentreFreqRX1 push at console.cs:31272 + 15378
    // [v2.10.3.13], invoked from chkPower_CheckedChanged at
    // console.cs:27204 [v2.10.3.13] as the explicit "push state to
    // display" step on power-on.
    connect(m_radioModel, &RadioModel::sliceStateRestored, this,
            [this](int index) {
        if (index != 0 || !activeSpectrumWidget()) {
            return;
        }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) {
            return;
        }
        const double freq = slice->frequency();
        activeSpectrumWidget()->setCenterFrequency(freq);
        activeSpectrumWidget()->setDdcCenterFrequency(freq);
        activeSpectrumWidget()->setVfoFrequency(freq);
        activeSpectrumWidget()->setFilterOffset(slice->filterLow(), slice->filterHigh());
    });

    // The dashboard follows the ACTIVE slice, not slice 0. It was pinned to
    // id 0 and never rebound, so after multi-pan landed (#312) an operator
    // working Slice B was shown Slice A's mode, filter, AGC and NR as
    // current. See design §4.2.
    auto rebindDashboard = [this]() {
        if (!m_rxDashboard || !m_radioModel) { return; }
        SliceModel* s = m_radioModel->activeSlice();
        if (!s) { return; }
        m_rxDashboard->bindSlice(s);
        // Dieselbe Scheibe an die Frequenzanzeige. Sie haengt an
        // derselben Stelle, damit die untere Leiste und das Widget
        // niemals verschiedene Scheiben beschreiben.
        if (m_frequencyApplet) { m_frequencyApplet->bindSlice(s); }
        // Use SliceModel::sliceLetter(), do NOT derive the letter here.
        // It is already derived from sliceIndex() upstream. It previously
        // returned a stored member defaulting to 'A', so every slice
        // reported 'A' and three call sites mislabelled their slices; see
        // the comment at SliceModel.h:503. Deriving locally would
        // reintroduce a second source of truth for the same fact.
        m_rxDashboard->setSliceLetter(s->sliceLetter());
    };
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [rebindDashboard](int) { rebindDashboard(); });
    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [rebindDashboard]() { rebindDashboard(); });
    rebindDashboard();

    // Phase 3F: the status overlay's per-slice triggers. Topology changes
    // reach the overlay through rebuildFftRouting, but a retune or a mode
    // change moves no topology at all, so those need their own wire -- and
    // they are the ones an operator exercises on every VFO detent.
    //
    // Resolved by id, not list position: sliceAdded carries the stable slice
    // id (RadioModel.cpp addSlice hands out the lowest free one), which is a
    // position only until the operator removes a slice from the middle.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int sliceId) {
        wireSliceStatusOverlayTriggers(m_radioModel->sliceById(sliceId));
        refreshPanStatusOverlays();
    });
    // Slices that already exist. RadioModel creates Slice A at connect, so on
    // a reconnect this loop is what re-arms it; sliceAdded has already fired.
    for (SliceModel* existing : m_radioModel->slices()) {
        wireSliceStatusOverlayTriggers(existing);
    }

    // Phase 3F Sub-Epic D Task 13: bind newly-created slices to a pan
    // and register pan-to-receiver routing in the FFTRouter.
    //
    // RadioModel::addSlice stamps the requested pan id on the SliceModel via
    // setPanKey() (and the transitional initialPanId property) so this handler
    // knows where to dock the slice. If the slice was added by some other path
    // (no pan key), we fall back to the active pan. If the requested pan does
    // not exist yet (e.g. Add Panadapter ran between addSliceOnPan emit and
    // this slot), we create it on the fly via PanadapterStack::addPanadapter.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [this](int sliceIndex) {
        if (!m_panStack) { return; }
        SliceModel* slice = sliceForAddedIdForTest(m_radioModel, sliceIndex);
        if (!slice) { return; }

        const QString panKey = slice->panKey();
        const QString targetPan = panKey.isEmpty()
                                      ? m_panStack->activePanId()
                                      : panKey;

        auto* applet = m_panStack->panadapter(targetPan);
        if (!applet) {
            applet = m_panStack->addPanadapter(targetPan);
        }
        if (applet) {
            applet->addSlice(sliceIndex);
        }

        // Phase 3F Sub-Epic I Task 9: re-derive the whole topology instead
        // of mapping this one pan. The old call keyed the router on
        // slice->ddcIndex(), which is the hardware DDC number (2..6 on
        // Saturn-class), while the FFTEngine pool is keyed on stream index
        // (0..userDdcCount-1) -- plan invariant 3. It also ran too early for
        // Slice A: the pool is not sized until connect, so Slice A's
        // addSlice-time bind is a no-op and its streamIndex is still -1 here.
        // The streamBindingsChanged rebuild picks it up once it binds.
        rebuildFftRouting();

        // Phase 3F hotfix 2026-05-27 legte hier eine VFO-Flagge je
        // Scheibe an, damit eine zweite Scheibe ueberhaupt zu sehen
        // war. Die Flaggen sind am 2026-08-18 geloescht; die offene
        // Frage dazu steht weiter unten in diesem Block.
        //
        // What's intentionally NOT done here: per-slice rxChannel/CTUN/
        // MaxBin/NR/ANF DSP wiring (those hardcode rxChannel(0) today
        // and are the actual Phase 3F multi-slice DSP epic).  This hotfix
        // is "make the flag appear + let the operator interact"; the
        // DSP-side per-slice fanout is the next sub-epic.
        if (sliceIndex == 0) {
            // Slice A is handled by wireSliceToSpectrum() which already
            // ran via the index==0 handler at the top of this file.
            return;
        }
        // Bis 2026-08-18 wurde hier eine VFO-Flagge auf dem Panadapter
        // der Scheibe angelegt. Sie ist geloescht; was folgt, ist nur
        // noch die Zuordnung Scheibe → Panadapter.
        //
        // Fuer Phase 3F bleibt eine offene Frage, hier vermerkt statt
        // still gelassen: die Flagge WAR die Bedienflaeche einer
        // zweiten Scheibe. Ohne sie bedient die RxApplet immer die
        // AKTIVE Scheibe, und Scheibe B braucht eine eigene Flaeche —
        // im Zielbild ein eigenes Feld im Raster.

        // Phase 3F: die Zuordnung wandert mit, wenn die Scheibe den
        // Panadapter wechselt. Ported from AetherSDR's panIdChanged
        // migration (MainWindow.cpp:11560 [@6a142807]).
        connect(slice, &SliceModel::panKeyChanged, this,
                [this, slice](const QString& newPanKey) {
            const int idx = slice->sliceIndex();
            // Die Zuordnung Panadapter → Scheibe muss mitwandern. Ohne
            // das fuehrte der verlassene Panadapter die Scheibe weiter
            // in associatedSlices(), stimmte womoeglich weiter auf sie
            // ab und malte ihr CH-Schild — fuer eine Scheibe, die
            // woanders zu sehen ist.
            if (m_panStack) {
                m_panStack->moveSliceToPan(idx, newPanKey);
            }
            if (idx == 0) { return; }
            // Phase 3F Sub-Epic I Task 9: die Scheibe speist jetzt einen
            // anderen Panadapter, also muss die FFT-Verteilung folgen.
            rebuildFftRouting();
        });
    });

    // Phase 3F Sub-Epic D Task 13: detach a removed slice index from
    // every pan. The associatedSlices set is small (single-digit) so a
    // linear scan across all applets is fine.
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [this](int sliceIndex) {
        if (m_panStack) {
            for (auto* applet : m_panStack->allApplets()) {
                if (applet) { applet->removeSlice(sliceIndex); }
            }
        }
        // Bis 2026-08-18 wurde hier die VFO-Flagge der Scheibe
        // abgeraeumt, samt der Sorgfalt um ihre schwebenden Knoepfe
        // (die gehoerten dem SpectrumWidget und ueberlebten sonst ein
        // blosses delete). Sie ist geloescht; was bleibt, ist das
        // Loesen der Scheibe von allen Panadaptern (oben) und die
        // FFT-Verteilung (unten).
        // Phase 3F Sub-Epic I Task 9: the removed slice may have been the
        // last one feeding its pan, or the last one on its stream. Rebuild
        // from the surviving slice set rather than unsubscribing this pan,
        // which would also drop any co-hosted slices still showing on it.
        rebuildFftRouting();
    });

    // Phase 3F Sub-Epic F Task 6: route wideband bins from RadioModel into
    // the active pan's SpectrumWidget.  RadioModel emits one
    // widebandSpectrumReady per ADC per assembled frame; we forward the
    // bins to the currently-active SpectrumWidget which silently stores
    // them per ADC.  Per-pan ADC routing (so extended-pan views on the
    // correct ADC's bins) lands in Sub-Epic F polish (T7-T10).
    connect(m_radioModel, &RadioModel::widebandSpectrumReady, this,
            [this](int adcIdx, const QVector<float>& dbmBins) {
        fanWidebandBinsForTest(m_panStack, adcIdx, dbmBins);
    });

    // Create the FFTEngine pool on a worker thread (spectrum thread from
    // architecture).  Sample rate starts at P2 default (768k);
    // RadioModel::wireSampleRateChanged updates it to the actual wire rate on
    // each connect (P1=192k, P2=768k).
    //
    // Phase 3F Sub-Epic I Task 8: the thread is created BEFORE the engines
    // now, because createFftEngineForStream parks each engine on it.  Only
    // stream 0's engine is built here: the SKU's stream count is not known
    // until connect, and the rest are built on demand as the allocator
    // claims their DDCs (see the streamCentreChanged handler below).
    m_fftThread = new QThread(this);
    m_fftThread->setObjectName(QStringLiteral("SpectrumThread"));
    createFftEngineForStream(0);

    connect(m_radioModel, &RadioModel::wireSampleRateChanged,
            this, [this](double rateHz) {
        // Every stream on a P1 radio shares the wire rate, and on P2 the
        // per-stream rate published by streamCentreChanged is derived from
        // the same value, so the whole pool follows this signal.
        for (FFTEngine* engine : std::as_const(m_fftEngines)) {
            if (!engine) { continue; }
            QMetaObject::invokeMethod(engine, [engine, rateHz]() {
                engine->setSampleRate(rateHz);
            });
        }
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setSampleRate(rateHz);
            // Phase 3G-12: preserve the user's current zoom level across
            // sample rate changes. Only reset the visible span if the
            // current bandwidth would now exceed the new DDC sample rate
            // (in which case we clamp to full-span).
            const double freq = activeSpectrumWidget()->centerFrequency();
            const double currentBw = activeSpectrumWidget()->bandwidth();
            const double clampedBw = (currentBw > rateHz) ? rateHz : currentBw;
            activeSpectrumWidget()->setFrequencyRange(freq, clampedBw);
        }
    });
    // Spectrum/waterfall FPS — load persisted value (default 30), apply to
    // BOTH the FFT engine (row production cadence) and the SpectrumWidget
    // display timer (paint cadence) so the two stay locked.  Without this
    // load, FFTEngine defaulted to 30 and SpectrumWidget defaulted to its
    // own 30 fps constructor value, but Setup -> Display -> Spectrum
    // changes did not survive restart.  Persistence write side is in
    // SpectrumDefaultsPage::pushFps.
    //
    // Phase 3F Sub-Epic I Task 8: the engine half of this (setOutputFps),
    // plus the persisted FFT size / window / Hz-per-bin target that used to
    // follow it, now live in createFftEngineForStream so every pooled engine
    // comes up on the same display knobs.  Only the SpectrumWidget half is
    // left here.
    {
        const int persistedFps = qBound(1,
            AppSettings::instance().value(
                QStringLiteral("DisplaySpectrumFps"),
                QStringLiteral("30")).toString().toInt(),
            60);
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->setDisplayFps(persistedFps);
        }
    }

    // 2026-05-25 KG4VCF bench fix: elevate the spectrum FFT thread.
    // 2026-05-26 KG4VCF revisit: bumped from USER_INITIATED to
    // USER_INTERACTIVE -- the INITIATED tier still let compile workers
    // preempt the FFT pass during build kickoff, producing visible
    // waterfall stutter even though the WaterfallTicker had its own
    // worker thread.  USER_INTERACTIVE puts the FFT thread in the
    // same scheduling class as audio + GUI so compile workers (DEFAULT)
    // consistently lose the time-slice race.  See
    // src/core/audio/RealtimeAudioPriority.h.
    // The lambda elevates the thread it runs on, so it is thread-scoped, not
    // engine-scoped: one connection using stream 0's engine as context is
    // enough for the whole pool.  Engines created later (at connect, when the
    // SKU's stream count is known) land on an already-elevated thread.
    connect(m_fftThread, &QThread::started, primaryFftEngine(),
            []() { Longpath::elevateLatencyCriticalThreadPriority(); });

    // Per-engine cleanup (QThread::finished -> deleteLater) and the raw I/Q
    // feed are wired inside createFftEngineForStream, so engines added after
    // this point get both.
    //
    // The linear-power frame no longer goes straight to activeSpectrumWidget()
    // -- that resolved to pan 0 permanently and was the reason a secondary pan
    // never animated.  Each engine's fftReadyLinear now lands in
    // MainWindow::dispatchFftFrameToPans, which consults the FFTRouter and
    // pushes the frame to every pan subscribed to that stream.  fftReadyLinear
    // carries the raw |X[k]|² bins plus windowEnb + dbmOffset metadata so the
    // detector + avenger pipeline reproduces the legacy fftReady dBm output
    // (FFTEngine.cpp:348 [v2.10.3.13]) at display-pixel resolution.  fftReady
    // (full-bin dBm) is kept as a separate signal for chrome / AGC consumers
    // (ClarityController, NoiseFloorTracker) wired below; those stay on the
    // primary engine because each is a single global consumer.

    // Phase 3G-8: expose view hooks on RadioModel so Display setup pages can
    // reach the renderer / FFT engine without depending on MainWindow.
    m_radioModel->setSpectrumWidget(activeSpectrumWidget());
    m_radioModel->setFftEngine(primaryFftEngine());

    // Phase 3F Sub-Epic I Task 8: follow each stream's DDC centre + rate.
    //
    // RadioModel::bindSliceToStream emits this whenever the allocator claims
    // or moves a DDC.  The pans showing the stream must recentre with it, or
    // SpectrumWidget::visibleBinRange maps the incoming bins against a stale
    // window.  Also cached, because at emit time the router may not yet know
    // which pan shows the stream (see m_streamWindows in the header).
    connect(m_radioModel, &RadioModel::streamCentreChanged, this,
            [this](int streamIndex, double centreHz, int sampleRateHz) {
        m_streamWindows.insert(streamIndex, StreamWindow{centreHz, sampleRateHz});
        // This signal is emitted exactly when the allocator claims or moves
        // a DDC, which is the only way a stream starts producing I/Q, so it
        // is also the right moment to build that stream's engine. No-op
        // after the first time.
        if (FFTEngine* engine = createFftEngineForStream(streamIndex)) {
            QMetaObject::invokeMethod(engine, [engine, sampleRateHz]() {
                engine->setSampleRate(static_cast<double>(sampleRateHz));
            }, Qt::QueuedConnection);
        }
        if (m_radioModel) {
            if (auto* router = m_radioModel->fftRouter()) {
                for (const QString& panId : router->pansForReceiver(streamIndex)) {
                    applyStreamWindowToPan(panId, streamIndex);
                }
            }
        }
    });

    // Phase 3F Sub-Epic I Task 9: any change to which slices sit on which
    // stream re-derives the FFT topology. This is the authoritative trigger:
    // it fires from bindSliceToStream AFTER SliceModel::streamIndex is set,
    // including for Slice A's deferred bind at connect (the pool is not
    // sized when Slice A is created, so its addSlice-time bind is a no-op
    // and the sliceAdded rebuild sees streamIndex == -1).
    connect(m_radioModel, &RadioModel::streamBindingsChanged, this,
            [this](int, const QVector<int>&) { rebuildFftRouting(); });

    // Sub-epic E: flush the rewind ring buffer when the radio disconnects so
    // a new session starts with a clean history. AetherSDR's clearDisplay()
    // did this implicitly; NereusSDR has no equivalent single-call reset, so
    // we plumb the connection-state signal through here. See
    // docs/architecture/phase3g-rx-epic-e-waterfall-scrollback-plan.md task 4.
    connect(m_radioModel, &RadioModel::connectionStateChanged, activeSpectrumWidget(),
            [this]() {
        if (!m_radioModel->isConnected() && activeSpectrumWidget()) {
            activeSpectrumWidget()->clearWaterfallHistory();
        }
    });

    // Phase 3Q-8: clicking the spectrum while disconnected opens ConnectionPanel.
    connect(activeSpectrumWidget(), &SpectrumWidget::disconnectedClickRequest,
            this, &MainWindow::showConnectionPanel);

    // Wire BandPlanManager → SpectrumWidget so the bandplan strip renders on launch.
    activeSpectrumWidget()->setBandPlanManager(&m_radioModel->bandPlanManagerMutable());

    // Phase 3G-9b: no first-launch auto-apply of smooth defaults. Per
    // user decision 2026-04-15, the out-of-box waterfall stays on
    // WfColorScheme::Default; ClarityBlue is reachable only via the
    // "Reset to Smooth Defaults" button on SpectrumDefaultsPage or by
    // manually selecting "Clarity Blue" from the Waterfall Defaults combo.

    // --- Phase 3G-13: Step attenuator + ADC overload ---
    m_stepAttController = new StepAttenuatorController(this);
    m_radioModel->setStepAttController(m_stepAttController);

    // 3M-1a G.1 / F.2: MoxController::hardwareFlipped → StepAttenuatorController.
    // Both objects are now live; RadioModel owns MoxController, MainWindow owns
    // StepAttenuatorController. Wire here where both sides are accessible.
    // Qt::QueuedConnection documents cross-component intent (both main-thread)
    // and ensures the slot body runs after the emit call stack unwinds.
    // F.2 connect note: this is the connect deferred from StepAttenuatorController.h
    // line 257 ("The connect() call wiring this slot to MoxController::hardwareFlipped
    // is deferred to Task G.1").
    // From Thetis console.cs:29546-29576 [v2.10.3.13] — ATT-on-TX in HdwMOXChanged.
    // Inline attribution tags preserved verbatim from the cited range:
    //MW0LGE [2.9.0.7] added option to always apply 31 att from setup form when not in ps  [console.cs:29561]
    //[2.10.3.6]MW0LGE att_fixes  [original inline comment from console.cs:29567]
    //[2.10.3.6]MW0LGE att_fixes NOTE: this will eventually call Display.TXAttenuatorOffset with the value  [console.cs:29568]
    // Display.TXAttenuatorOffset = 0; //[2.10.3.6]MW0LGE att_fixes  [console.cs:29576]
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::hardwareFlipped,
                m_stepAttController, &StepAttenuatorController::onMoxHardwareFlipped,
                Qt::QueuedConnection);

        // H.1 (Phase 3M-1a): SpectrumWidget MOX overlay.
        // Wire MoxController::moxStateChanged → SpectrumWidget::setMoxOverlay.
        // From Thetis display.cs:1569-1593 [v2.10.3.13] Display.MOX setter:
        // the flag drives grid pen selection (tx_vgrid_pen red vs rx grey).
        // In 3M-1a we render a 3 px red border tint; full grid recolouring
        // is deferred to 3M-3.
        // Qt::QueuedConnection: MoxController and SpectrumWidget both live on
        // the main thread but a queued connection is used to match the deferred
        // pattern established for the hardwareFlipped connect above.
        if (activeSpectrumWidget()) {
            connect(mox, &MoxController::moxStateChanged,
                    activeSpectrumWidget(), &SpectrumWidget::setMoxOverlay,
                    Qt::QueuedConnection);
        }

        // ── Phase 3M-4 Task 12: SpectrumWidget IMD overlay state wiring ──────
        // From Thetis display.cs:5008 [v2.10.3.13] show condition:
        //   show_imd_measurements = local_mox && _testing_imd
        //                           && _show_imd_measurements && displayduplex;
        // local_mox is already wired above. Wire the other two flags from
        // their authoritative coordinators:
        //   testing_imd            <- TwoToneController::twoToneActiveChanged
        //                              (mirrors Thetis Display.TestingIMD,
        //                              display.cs:296-302 [v2.10.3.13])
        //   show_imd_measurements  <- PureSignal::show2ToneMeasurementsChanged
        //                              (mirrors Thetis Display.ShowIMDMeasurments,
        //                              display.cs:304-311 [v2.10.3.13])
        // displayduplex stays at SpectrumWidget's default (true) — see header.
        if (activeSpectrumWidget()) {
            if (auto* tt = m_radioModel->twoToneController()) {
                connect(tt, &TwoToneController::twoToneActiveChanged,
                        activeSpectrumWidget(), &SpectrumWidget::setTestingIMD);
            }
            // Phase 3M-4 bench-fix: PureSignal coordinator is late-bound
            // during WDSP-init — at MainWindow build time it's typically
            // nullptr, so the connection below never gets made and the
            // IMD overlay show condition stays at m_showIMDMeasurements=
            // false even after the user checks chkShow2ToneMeasurements.
            // Subscribe to RadioModel::pureSignalCoordinatorReady so we
            // re-attempt the connect when the coordinator becomes available.
            // Mirrors the pattern in PureSignalApplet.cpp:118-123 +
            // PsaIndicatorWidget bench-fix.
            auto wireSpectrumToPs = [this](PureSignal* ps) {
                if (!ps || !activeSpectrumWidget()) { return; }
                connect(ps, &PureSignal::show2ToneMeasurementsChanged,
                        activeSpectrumWidget(),
                        &SpectrumWidget::setShowIMDMeasurements,
                        Qt::UniqueConnection);
            };
            wireSpectrumToPs(m_radioModel->pureSignal());
            connect(m_radioModel, &RadioModel::pureSignalCoordinatorReady,
                    this, [wireSpectrumToPs](PureSignal* ps) {
                        wireSpectrumToPs(ps);
                    });
        }

        // ── 3M-1c Phase L.3: VFO TX badge routing ─────────────────────────────
        //
        // MoxController::moxChanged(rx, oldMox, newMox) → VfoDisplayItem
        // setTransmitting on every VfoDisplayItem hosted by the app.  The rx
        // semantic (Thetis console.cs:29677 [v2.10.3.13]) is:
        //   rx==1  → VFO-A (TX comes off VFO-A in 3M-1; default in NereusSDR)
        //   rx==2  → VFO-B (only when RX2 enabled AND VFOBTX — neither
        //                    plumbed in NereusSDR today)
        //
        // Lookup strategy: walk every container's MeterWidget and update
        // every VfoDisplayItem found.  This is coarse but correct for 3M-1
        // (one VFO instance) — when RX2 lands (3F multi-pan), upgrade to
        // per-VfoDisplayItem item-name routing so VFO-B gets rx==2 only.
        //
        // The G.2 routing test (tst_vfo_display_item_tx_badge.cpp) demonstrates
        // the canonical lambda shape that this code mirrors at production scale.
        // TODO [3F]: split routing per-item so RX2's VFO-B instance only
        // updates on rx==2.
        connect(mox, &MoxController::moxChanged, this,
                [this](int rx, bool /*oldMox*/, bool newMox) {
            if (!m_containerManager) { return; }
            // 3M-1: only rx==1 is ever emitted (default RX2/VFOBTX both
            // false in MoxController), so the broadcast fires the same set
            // of items.  Filter on rx==1 to leave the door open for the
            // 3F upgrade without changing the connect site.
            if (rx != 1) { return; }
            for (ContainerWidget* c : m_containerManager->allContainers()) {
                if (!c) { continue; }
                auto* mw = qobject_cast<MeterWidget*>(c->content());
                if (!mw) { continue; }
                for (MeterItem* item : mw->items()) {
                    if (auto* vfo = qobject_cast<VfoDisplayItem*>(item)) {
                        vfo->setTransmitting(newMox);
                    }
                }
            }
        }, Qt::QueuedConnection);
    }

    // --- Phase 3G-9c: Clarity adaptive display tuning ---
    m_clarityController = new ClarityController(this);
    m_radioModel->setClarityController(m_clarityController);

    // Restore enabled state from AppSettings + sync the clarityActive
    // flag on SpectrumWidget so legacy AGC knows to stand down.
    {
        auto& s = AppSettings::instance();
        // Ship default 2026-04-30: Clarity ON for fresh installs. Auto-tuning
        // the noise floor is the better first-launch experience than asking
        // the user to find and toggle the setting themselves.
        bool clarityOn = s.value(QStringLiteral("ClarityEnabled"), QStringLiteral("True"))
                            .toString() == QStringLiteral("True");
        m_clarityController->setEnabled(clarityOn);
        activeSpectrumWidget()->setClarityActive(clarityOn);
    }

    // Feed FFT bins to Clarity (auto-queued: spectrum thread → main).
    // Primary engine only: ClarityController holds one adaptive-display
    // state, so it tracks stream 0 rather than whichever stream last
    // produced a frame. Per-stream Clarity is Phase 3F follow-up work.
    connect(primaryFftEngine(), &FFTEngine::fftReady,
            m_clarityController, [this](int /*rxId*/, const QVector<float>& binsDbm) {
        m_clarityController->feedBins(binsDbm);
    });

    // ── NoiseFloorTracker for Auto AGC-T ────────────────────────────────
    auto* nfTracker = new NoiseFloorTracker;
    m_radioModel->setNoiseFloorTracker(nfTracker);

    connect(primaryFftEngine(), &FFTEngine::fftReady,
            this, [nfTracker](int /*rxId*/, const QVector<float>& binsDbm) {
        static constexpr float kFrameIntervalMs = 33.0f;
        nfTracker->feed(binsDbm, kFrameIntervalMs);
    });

    // Max Bin detector: feed FFTEngine dBm bins into WdspEngine's NereusSDR-native
    // Max Bin pipeline.  See WdspEngine::setupMaxBinDetector for the algorithm
    // cite and the divergence rationale (WDSP analyzer not wired; FFTEngine
    // uses raw FFTW3 directly; NereusSDR runs the same Thetis algorithm against
    // the dBm bins emitted here).
    //
    // Algorithm from Thetis wdsp/analyzer.c:800-822 [@501e3f5].
    //
    // Primary engine only: setupMaxBinDetector is called with disp=0 (single
    // display channel), so the detector reads stream 0's bins.
    if (auto* eng = (m_radioModel ? m_radioModel->wdspEngine() : nullptr)) {
        connect(primaryFftEngine(), &FFTEngine::fftReady,
                eng,               &WdspEngine::onSpectrumBinsForMaxBin);
    }

    // 2026-05-22 bench fix: MaxBin meter accuracy. The raw FFT bin path
    // above reads ~12-17 dB below what the spectrum visually displays
    // because the spectrum runs the bins through a detector + invEnb
    // window-normalization + avenger time-smoothing pipeline that
    // reconstructs window-spread integrated power. After every spectrum
    // render frame, push the slice's passband peak (from m_renderedPixels,
    // post detector + avenger) into the MaxBin detector so the analog
    // S-meter reads what the operator actually sees on the trace.
    if (activeSpectrumWidget() && m_radioModel) {
        connect(activeSpectrumWidget(), &SpectrumWidget::spectrumFrameRendered,
                this, [this]() {
            auto* eng = m_radioModel ? m_radioModel->wdspEngine() : nullptr;
            if (!eng || !activeSpectrumWidget()) { return; }
            const double dbm = activeSpectrumWidget()->peakDbmInSlicePassband();
            if (dbm > -400.0) {
                eng->setMaxBinDbmFromSpectrum(/*disp=*/0, dbm);
            }
        });
    }

    // Fast-attack triggers — deferred until slice exists
    // From Thetis v2.10.3.13 display.cs:905 — freq change triggers fast attack
    // From Thetis v2.10.3.13 display.cs:880 — mode change triggers fast attack
    // Connected in wireSliceToSpectrum() where activeSlice() is guaranteed non-null
    // From Thetis v2.10.3.13 display.cs:890 — OnAttenuatorDataChanged
    if (m_stepAttController) {
        connect(m_stepAttController, &StepAttenuatorController::attenuationChanged,
                this, [nfTracker](int /*dB*/) {
            nfTracker->triggerFastAttack();
        });
    }

    // Periodic visual update: auto-AGC timer → refresh NF visuals on both widgets
    if (m_radioModel->autoAgcTimer()) {
        connect(m_radioModel->autoAgcTimer(), &QTimer::timeout, this, [this]() {
            SliceModel* s = m_radioModel->activeSlice();
            auto* nft = m_radioModel->noiseFloorTracker();
            if (s && s->autoAgcEnabled() && nft) {
                float nf = nft->noiseFloor();
                double offset = s->autoAgcOffset();
                if (m_rxApplet) {
                    m_rxApplet->updateAgcAutoVisuals(true, nf, offset);
                }
            }
        });
    }

    // TX pause: MOX signal → ClarityController
    connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
            m_clarityController, &ClarityController::setTransmitting);

    // Plan 4 D9 (Cluster E): TX filter audio range → spectrum overlay.
    // TransmitModel::filterChanged carries (low, high) audio Hz; SpectrumWidget
    // converts to IQ-space at draw time using m_txMode (set below via slice).
    if (activeSpectrumWidget()) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::filterChanged,
                activeSpectrumWidget(), &SpectrumWidget::setTxFilterRange);

        // Initial sync from current TransmitModel state.
        const auto& txModel = m_radioModel->transmitModel();
        activeSpectrumWidget()->setTxFilterRange(txModel.filterLow(), txModel.filterHigh());
    }

    // Clarity → SpectrumWidget threshold update + clarityActive flag.
    // Issue #230 fix: write the render-active mirror, not the
    // persistent user fields — Clarity is runtime state per Thetis's
    // AGC pattern (display.cs:6584 [v2.10.3.13] uses
    // _RX1waterfallPreviousMinValue, a runtime field separate from
    // waterfall_low_threshold).  The previous setWfLow/HighThreshold
    // calls were silently overwriting the user's saved thresholds via
    // scheduleSettingsSave() on every Clarity tick.
    connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
            activeSpectrumWidget(), [this](float low, float high) {
        activeSpectrumWidget()->setClarityActive(true);
        activeSpectrumWidget()->setClarityWaterfallThresholds(low, high);
    });

    // Clarity → SpectrumWidget NF-aware grid (Task 2.9).
    // NereusSDR-original — no Thetis equivalent.
    // noiseFloorChanged fires after EWMA smoothing but before the deadband
    // gate so the grid tracks the floor at every cadence tick.
    connect(m_clarityController, &ClarityController::noiseFloorChanged,
            activeSpectrumWidget(), &SpectrumWidget::onNoiseFloorChanged);

    // ── Der zweite Platz im S-Meter-Panel ───────────────────────────
    //
    // Derselbe Rauschflur, denselben Weg. Ein FESTER Platz, der immer
    // eine Messung traegt -- in RADE traegt er stattdessen das SNR, und
    // nur die Beschriftung wechselt. Eine Zeile, die je nach
    // Betriebsart erscheint und verschwindet, liesse den festen Kopf in
    // der Hoehe springen.
    connect(m_clarityController, &ClarityController::noiseFloorChanged,
            this, [this](float nfDbm) {
        // ── Als MESSGROESSE (2026-08-18) ─────────────────────────────
        //
        // OE5SOS, 2026-08-18: „die Rauschflur-Beschriftung gehört zum
        // Empfang, also ins Instrument als Quelle."
        //
        // Ueber den Poller, nicht direkt ans Instrument: sonst gaebe es
        // zwei Wege, an einen Messwert zu kommen, und das Instrument
        // muesste wissen, welcher fuer welche Groesse gilt. Genau diese
        // Doppelung war der Grund, warum „Max Bin" nur ueber ein
        // zweites Menue erreichbar war.
        if (m_meterPoller) {
            m_meterPoller->feedReading(MeterBinding::NoiseFloor,
                                       static_cast<double>(nfDbm));
        }
    });

    // RADE verdraengt den Rauschflur, solange es laeuft: beide
    // beantworten dieselbe Frage -- wie gut komme ich hier durch.
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::radeSnrChanged,
                this, [this](int, float snrDb) {
            // Auch das RADE-SNR ist ein Messwert mit Skala und gehoert
            // als Quelle ins Instrument — der Bereich steht in
            // ReadingSource, aus dem RADE-Quelltext gerechnet.
            // PB SNR (MeterBinding::PbSnr) bleibt davon getrennt: das
            // ist Spitze-zu-Grundlinie aus dem Spektrum, nicht die
            // Schaetzung des RADE-Decoders.
            if (m_meterPoller) {
                m_meterPoller->feedReading(MeterBinding::RadeSnr,
                                           static_cast<double>(snrDb));
            }
        });
    }

    // Task 2.10: per-band NF priming — settle detector.
    // NereusSDR-original — no Thetis equivalent.
    //
    // On each noiseFloorChanged tick, keep a 2-second sliding window of NF
    // samples. When variance drops below 1 dB for a sustained window of ≥30
    // samples (≈ 15 s / cadence-0.5s = 30 ticks), save the current floor to
    // the panadapter's per-band NF slot so the next band-switch can snap
    // instantly instead of cold-starting from zero.
    {
        struct NFHistoryEntry { qint64 t; float value; };
        struct SettleState {
            QList<NFHistoryEntry> history;
        };
        auto settle = QSharedPointer<SettleState>::create();

        PanadapterModel* pan0 = m_radioModel->panadapters().isEmpty()
                                ? nullptr
                                : m_radioModel->panadapters().first();
        if (pan0) {
            connect(m_clarityController, &ClarityController::noiseFloorChanged,
                    this, [pan0, settle](float nf) {
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                settle->history.append({now, nf});

                // Trim to 2-second window.
                const qint64 cutoff = now - 2000;
                while (!settle->history.isEmpty() && settle->history.first().t < cutoff) {
                    settle->history.removeFirst();
                }

                // Compute variance when we have ≥30 samples (~30 cadence ticks).
                if (settle->history.size() >= 30) {
                    float sum = 0.0f;
                    for (const auto& e : std::as_const(settle->history)) { sum += e.value; }
                    const float mean = sum / static_cast<float>(settle->history.size());
                    float sqSum = 0.0f;
                    for (const auto& e : std::as_const(settle->history)) {
                        const float d = e.value - mean;
                        sqSum += d * d;
                    }
                    const float variance = sqSum / static_cast<float>(settle->history.size());

                    if (variance < 1.0f) {
                        // NereusSDR-original — no Thetis equivalent.
                        // NF settled within 1 dB variance over 2s; save for this band.
                        pan0->setBandNFEstimate(pan0->band(), nf);
                    }
                }
            });

            // Task 2.10: band-change → prime ClarityController EWMA with stored NF.
            // NereusSDR-original — no Thetis equivalent.
            //
            // PanadapterModel::bandChanged fires when the pan center crosses a band
            // boundary. snapToFloor() seeds the EWMA (m_smoothedFloor) and emits
            // waterfallThresholdsChanged immediately so the waterfall snaps to the
            // remembered state rather than cold-starting from an uninitialized floor.
            // NaN is ignored by snapToFloor (band with no stored data is a no-op).
            connect(pan0, &PanadapterModel::bandChanged,
                    this, [this, pan0](Longpath::Band newBand) {
                // NereusSDR-original — no Thetis equivalent.
                // Prime estimator with last-seen NF for this band to eliminate
                // cold-start visual jump after band change.
                const float storedNF = pan0->bandNFEstimate(newBand);
                m_clarityController->snapToFloor(storedNF);
            });

            // NF fast-attack triggers — From Thetis display.cs:879-905
            // [v2.10.3.13]:
            //   if (rx == 1) FastAttackNoiseFloorRX1 = true;  // band change
            //   if (Math.Abs(oldFreq - newFreq) > 0.5)         // freq jump
            //       FastAttackNoiseFloorRX1 = true;
            // While in fast-attack state SpectrumWidget renders the NF
            // line/box/text in gray to signal the smoothed estimate is
            // still settling.  Auto-clear is internal to the setter (see
            // SpectrumWidget::setNoiseFloorFastAttack — 1000ms timer
            // matching Thetis display.cs:5906 minimum delay).
            if (activeSpectrumWidget()) {
                connect(pan0, &PanadapterModel::bandChanged,
                        this, [this](Longpath::Band) {
                    activeSpectrumWidget()->setNoiseFloorFastAttack(true);
                });
            }
        }
    }

    // Slice freq-jump > 0.5 MHz fast-attack trigger — Thetis display.cs:905
    // [v2.10.3.13]: if (Math.Abs(oldFreq - newFreq) > 0.5) FastAttack = true.
    // Smaller jumps (in-band tuning) don't shift the noise floor enough to
    // warrant resetting the smoothed estimate.
    //
    // Stores the last-trigger frequency as a QObject dynamic property on
    // the slice itself — Qt cleans it up when the slice is destroyed, and
    // the same wiring works for slices added later via RadioModel::sliceAdded.
    if (activeSpectrumWidget()) {
        // 500 kHz threshold matches Thetis display.cs:905: > 0.5 MHz.
        constexpr double kFastAttackFreqJumpHz = 500000.0;
        constexpr const char* kLastFreqProp = "nfLastFastAttackFreq";

        auto subscribeSlice = [this](SliceModel* slice) {
            if (!slice) { return; }
            slice->setProperty(kLastFreqProp, slice->frequency());
            connect(slice, &SliceModel::frequencyChanged, this,
                    [this, slice](double freq) {
                const double last =
                    slice->property(kLastFreqProp).toDouble();
                if (std::abs(last - freq) > kFastAttackFreqJumpHz) {
                    activeSpectrumWidget()->setNoiseFloorFastAttack(true);
                }
                slice->setProperty(kLastFreqProp, freq);
            });
        };
        for (SliceModel* slice : m_radioModel->slices()) {
            subscribeSlice(slice);
        }
        connect(m_radioModel, &RadioModel::sliceAdded, this,
                [this, subscribeSlice](int index) {
            subscribeSlice(sliceForAddedIdForTest(m_radioModel, index));
        });
    }

    // Phase 3F Sub-Epic C Task 8: toast on slice-add rejection.
    // RadioModel::addSliceOnPan() emits sliceAddRejected(reason) when the
    // SKU cap blocks a +RX click (e.g. "Hermes Lite 2 supports a maximum
    // of 1 slices"). Surface that for 4 seconds so the operator sees why
    // the click did nothing.
    connect(m_radioModel, &RadioModel::sliceAddRejected, this,
            [this](const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 4000);
    });

    // Phase 3F Sub-Epic I closeout, defect F4.
    //
    // The operator turned the knob to somewhere no DDC can reach. The VFO has
    // already snapped back to the last frequency that bound, so the message
    // has to explain the snap rather than talk about adding a slice, which is
    // what the rejection used to say. 6 s: it names a frequency the operator
    // needs time to read.
    connect(m_radioModel, &RadioModel::sliceRetuneRejected, this,
            [this](int, const QString& reason) {
        showToast(reason, ToastSeverity::Warning, 6000);
    });

    // Phase 3F Sub-Epic I closeout, defect F3.
    //
    // The 1-ADC HERMES class drops every extra receiver the moment PureSignal
    // transmits or diversity engages. That is what Thetis does and it stays,
    // but Thetis says nothing about it either, so on the bench a slice simply
    // stopped producing audio with no explanation. Same surface as the
    // rejection message above; 6 s because it names slice letters the
    // operator has to map back to their flags.
    //
    // This is the notice that produced the 2026-07-30 bench report: it fires
    // on MOX with PureSignal running, which is exactly when the operator most
    // needs the PureSignal indicator and the TX badge it used to cover. It is
    // a toast for that reason and must stay one.
    connect(m_radioModel, &RadioModel::streamsSuspended, this,
            [this](const QVector<int>& streams, const QString& reason) {
        if (streams.isEmpty()) {
            // Everything is back. Take the notice down rather than leaving a
            // stale one on screen for its full timeout: on unkey the streams
            // return in well under six seconds.
            if (m_suspendToast) { m_suspendToast->close(); }
            return;
        }
        m_suspendToast = showToast(reason, ToastSeverity::Warning, 6000);
    });

    // Phase 3F closeout — Sub-Epic E Task 6 consumer wire-up.
    // antennaAutoSwitched(sliceIdx, oldAnt, newAnt) is emitted when an
    // AlexController conflict-policy re-route moves a slice off its old
    // antenna onto a new one. MainWindow constructs an AntennaSwitchToast
    // anchored to the MainWindow bottom-right corner, 8 s auto-dismiss,
    // UNDO button logs (real undo wires when the conflict-detection state
    // machine lands).
    connect(m_radioModel, &RadioModel::antennaAutoSwitched, this,
            [this](int sliceIdx, const QString& oldAnt, const QString& newAnt) {
        const QString msg = QStringLiteral("Slice %1 moved from %2 to %3.")
                                .arg(QChar(QLatin1Char('A' + sliceIdx)),
                                     oldAnt, newAnt);
        auto* toast = new AntennaSwitchToast(msg, this);
        toast->setAttribute(Qt::WA_DeleteOnClose);
        const QRect mwGeom = frameGeometry();
        toast->move(mwGeom.right() - toast->width() - 20,
                     mwGeom.bottom() - toast->height() - 50);
        toast->show();
        connect(toast, &AntennaSwitchToast::undoRequested, this,
                [sliceIdx, oldAnt]() {
            qCInfo(lcContainer) << "AntennaSwitchToast: undo requested for slice"
                                 << sliceIdx << "(would revert to" << oldAnt
                                 << ")  -  real undo wires when conflict-detection lands";
        });
    });

    // Phase 3F closeout — Sub-Epic E Task 7 consumer wire-up.
    // txBoundReRouteRequested(proposedAntenna, existingAntenna) opens a
    // modal TxBoundConfirmDialog with three outcomes (Cancelled /
    // UseExistingAntenna / ConfirmReroute). Today we log the outcome;
    // outcome routing to AlexController lands when the conflict-detection
    // state machine in addSliceOnPan ships in a follow-up.
    connect(m_radioModel, &RadioModel::txBoundReRouteRequested, this,
            [this](const QString& proposed, const QString& existing) {
        if (!m_radioModel) { return; }
        TxBoundConfirmDialog dlg(proposed, existing,
                                  m_radioModel->slices(), this);
        dlg.exec();
        qCInfo(lcContainer) << "TxBoundConfirmDialog: outcome="
                             << int(dlg.outcome())
                             << "(0=Cancelled, 1=UseExistingAntenna, 2=ConfirmReroute)";
    });

    // Phase 3F Sub-Epic C Task 10: TxSliceArbiter state → UI updates.
    // Wechselt die sendende Scheibe, wird die Anzeige nachgefuehrt und ein
    // Hinweis „TX > Slice X" fuer zwei Sekunden gezeigt.
    //
    // Bis 2026-08-18 fuehrte diese Schleife das TX-Abzeichen jeder
    // VFO-Flagge nach. Die Flaggen sind geloescht; welche Scheibe
    // sendet, sagt jetzt die TX-Pille in der unteren Leiste
    // (RxDashboard, Rung 5 — sie faltet nie).
    if (TxSliceArbiter* arb = m_radioModel->txSliceArbiter()) {
        connect(arb, &TxSliceArbiter::txBoundSliceChanged, this,
                [this](int oldId, int newId) {
            Q_UNUSED(newId);
            // oldId < 0 is the arbiter's initial bind (TxSliceArbiter::
            // syncToSliceList), not an operator handoff: the transmitter
            // did not move, it acquired its first home when the first slice
            // appeared. Announcing "TX > Slice A" on every connect would be
            // noise about something that did not happen.
            if (oldId < 0) { return; }
            showToast(QStringLiteral("TX > Slice %1")
                          .arg(QChar(QLatin1Char('A' + newId))),
                      ToastSeverity::Info, 2000);
        });
    }

    // MOX transition fast-attack trigger — Thetis display.cs:889-892:
    //   if (rx == 1) FastAttackNoiseFloorRX1 = true;
    // Fires on both RX→TX and TX→RX transitions; the buffer-clear pulse on
    // either edge resets the noise-floor settling window.
    if (activeSpectrumWidget()) {
        connect(&m_radioModel->transmitModel(), &TransmitModel::moxChanged,
                this, [this](bool) {
            activeSpectrumWidget()->setNoiseFloorFastAttack(true);
        });
    }

    // When Clarity pauses (TX, or an explicit pause), let legacy AGC
    // hold the display in the meantime -- and when it resumes, hand
    // control back. Before this else branch existed, m_clarityActive
    // only ever went one direction: any MOX pulse (including a bare
    // TUNE/SWR sweep) parked the waterfall on legacy AGC for the rest
    // of the session, since nothing else re-enabled it except the
    // cadence+deadband-gated waterfallThresholdsChanged path below --
    // which only fires once the noise floor has actually drifted, so
    // on a quiet band it could stay stuck black indefinitely. Live on
    // the bench 2026-08-25 (OE5SOS): the waterfall would recover
    // color only when something else happened to recreate the
    // SpectrumWidget (a responsive-layout window resize), which reset
    // m_clarityActive to its construction default -- coincidence, not
    // a fix.
    connect(m_clarityController, &ClarityController::pausedChanged,
            activeSpectrumWidget(), [this](bool paused) {
        if (paused) {
            activeSpectrumWidget()->setClarityActive(false);
        } else if (m_clarityController->isEnabled()) {
            activeSpectrumWidget()->setClarityActive(true);
        }
    });

    // Clarity ↔ overlay panel: badge + Re-tune button (wired after
    // m_overlayPanel creation in buildUI, which runs before this point).
    if (m_overlayPanel) {
        connect(m_clarityController, &ClarityController::waterfallThresholdsChanged,
                m_overlayPanel, [this](float, float) {
            m_overlayPanel->setClarityStatus(/*active=*/true, /*paused=*/false);
        });
        connect(m_clarityController, &ClarityController::pausedChanged,
                m_overlayPanel, [this](bool paused) {
            bool enabled = m_clarityController->isEnabled();
            m_overlayPanel->setClarityStatus(enabled, paused);
        });
        connect(m_overlayPanel, &SpectrumOverlayPanel::clarityRetuneRequested,
                m_clarityController, &ClarityController::retuneNow);

        // B8 Task 20: wire Display-flyout orphaned signals to SpectrumWidget.
        // These three signals were emitted but never connected — moving the
        // WF Gain / WF Black Level sliders and the Scheme combo did nothing.
        connect(m_overlayPanel, &SpectrumOverlayPanel::wfColorGainChanged,
                activeSpectrumWidget(), &SpectrumWidget::setWfColorGain);
        connect(m_overlayPanel, &SpectrumOverlayPanel::wfBlackLevelChanged,
                activeSpectrumWidget(), &SpectrumWidget::setWfBlackLevel);
        connect(m_overlayPanel, &SpectrumOverlayPanel::colorSchemeChanged,
                activeSpectrumWidget(), [this](int idx) {
            // colorSchemeChanged carries a raw combo index (int); setWfColorScheme
            // takes the WfColorScheme enum — adapt with a bounds-checked cast.
            const int schemeCount = static_cast<int>(WfColorScheme::Count);
            activeSpectrumWidget()->setWfColorScheme(
                static_cast<WfColorScheme>(qBound(0, idx, schemeCount - 1)));
        });
        connect(m_overlayPanel, &SpectrumOverlayPanel::spectrumRenderModeChanged,
                activeSpectrumWidget(), [this](int idx) {
            activeSpectrumWidget()->setSpectrumRenderMode(
                idx == 1 ? SpectrumRenderMode::Mode3D : SpectrumRenderMode::Mode2D);
        });

        // ── Die vier Zoomknoepfe (S B − +) ───────────────────────────
        //
        // Sie waren seit jeher TOT. Der Betreiber am 2026-08-22: "test
        // auch mal, ob das plus und minus funktioniert" — gemessen an
        // der Frequenzskala vor und nach dem Druck: Punkt fuer Punkt
        // identisch. Grund: zoomIn/zoomOut/zoomBand/zoomSegment sind
        // Signale OHNE EMPFAENGER. Genau der Fehlertyp, der hier schon
        // einmal auffiel ("B8 Task 20: wire Display-flyout orphaned
        // signals") — die vier hat man dabei uebersehen.
        //
        // Faktor 1.5 je Druck — AetherSDRs Wert
        // (SpectrumWidget.cpp:2205-2206 [@0cd4559]), nicht mein
        // geratener. Zwei fuehlt sich am Geraet grob an; anderthalb
        // laesst einen die Bandkante noch finden. Untergrenze
        // 2 kHz (darunter sieht man nur noch die eigene Bandbreite),
        // Obergrenze die Abtastrate (mehr liefert der Empfaenger nicht).
        auto zoomBy = [this](double factor) {
            SpectrumWidget* sw = activeSpectrumWidget();
            if (!sw) { return; }
            const double maxBw = sw->sampleRate() > 0.0 ? sw->sampleRate() : 384000.0;
            const double bw = qBound(2000.0, sw->bandwidth() * factor, maxBw);
            sw->setFrequencyRange(sw->centerFrequency(), bw);
        };
        connect(m_overlayPanel, &SpectrumOverlayPanel::zoomIn,  this,
                [zoomBy]() { zoomBy(1.0 / 1.5); });
        connect(m_overlayPanel, &SpectrumOverlayPanel::zoomOut, this,
                [zoomBy]() { zoomBy(1.5); });

        // B und S nehmen ihre Kanten aus dem BandPlanManager — derselben
        // Quelle, aus der der farbige Balken unter der Kurve gezeichnet
        // wird. Keine zweite Tabelle, die auseinanderlaufen kann.
        auto fitSegments = [this](bool wholeBand) {
            SpectrumWidget* sw = activeSpectrumWidget();
            if (!sw || !m_radioModel) { return; }
            const double fMhz = sw->centerFrequency() / 1e6;
            const auto& segs =
                m_radioModel->bandPlanManagerMutable().segments();

            double lo = 0.0, hi = 0.0;
            for (const BandSegment& seg : segs) {
                if (fMhz < seg.lowMhz || fMhz > seg.highMhz) { continue; }
                if (!wholeBand) { lo = seg.lowMhz; hi = seg.highMhz; break; }
                // Ganzes Band: alle Segmente einsammeln, die mit diesem
                // zusammenhaengen (Kante an Kante, 1 kHz Toleranz).
                lo = seg.lowMhz; hi = seg.highMhz;
                bool grew = true;
                while (grew) {
                    grew = false;
                    for (const BandSegment& o : segs) {
                        if (o.highMhz < lo - 0.001 || o.lowMhz > hi + 0.001) {
                            continue;
                        }
                        if (o.lowMhz  < lo) { lo = o.lowMhz;  grew = true; }
                        if (o.highMhz > hi) { hi = o.highMhz; grew = true; }
                    }
                }
                break;
            }
            if (hi <= lo) { return; }   // Frequenz liegt in keinem Segment

            const double maxBw = sw->sampleRate() > 0.0 ? sw->sampleRate() : 384000.0;
            // 10 % Luft an den Raendern, damit die Bandkante SICHTBAR
            // bleibt statt genau auf dem Bildrand zu liegen.
            const double span = qBound(2000.0, (hi - lo) * 1e6 * 1.1, maxBw);
            sw->setFrequencyRange((lo + hi) * 0.5 * 1e6, span);
        };
        connect(m_overlayPanel, &SpectrumOverlayPanel::zoomBand, this,
                [fitSegments]() { fitSegments(true); });
        connect(m_overlayPanel, &SpectrumOverlayPanel::zoomSegment, this,
                [fitSegments]() { fitSegments(false); });

        // B8 Task 21: wire Cursor Freq toggle to SpectrumWidget visibility guard.
        connect(m_overlayPanel, &SpectrumOverlayPanel::cursorFreqVisibleChanged,
                activeSpectrumWidget(), &SpectrumWidget::setCursorFreqVisible);

        // B8 Task 22: wire Fill Color button to SpectrumWidget::setFillColor.
        connect(m_overlayPanel, &SpectrumOverlayPanel::fillColorChanged,
                activeSpectrumWidget(), &SpectrumWidget::setFillColor);

        // B8 fix-up: wire Fill Alpha slider to SpectrumWidget::setFillAlpha.
        // The slider emitted fillAlphaChanged but had no connect — opacity
        // never reached the renderer.
        connect(m_overlayPanel, &SpectrumOverlayPanel::fillAlphaChanged,
                activeSpectrumWidget(), &SpectrumWidget::setFillAlpha);

        // B8 Task 24: wire "More Display Options →" link to Setup → Display.
        // Rechtsklick auf den Panadapter -> Setup-Seite. Derselbe Weg,
        // den die Overlay-Leiste schon nimmt (2026-08-18).
        if (auto* sw = activeSpectrumWidget()) {
            connect(sw, &SpectrumWidget::openSetupPageRequested,
                    this, [this](const QString& page) {
                auto* dialog = new SetupDialog(m_radioModel, this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                wireSetupDialog(dialog);
                dialog->selectPage(page);
                dialog->show();
            });
        }

        connect(m_overlayPanel, &SpectrumOverlayPanel::openSetupRequested,
                this, [this](const QString& page) {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(page);
            dialog->show();
        });
    }

    // Wire: zoom changes -> auto-replan FFT size to maintain constant
    // bins-per-pixel across zoom levels.  NereusSDR-original (Thetis
    // does not auto-replan on zoom; the user manually picks FFT size).
    //
    // Math: the slider's "FFT size at full DDC bandwidth" baseline
    // implies a target K = baseline / displayWidth bins per pixel.
    // To maintain K as bwHz narrows (zoom in), the FFT size must scale
    // inversely with bwHz:
    //   targetSize = baseline * sampleRate / bwHz
    //
    // Cap at kAutoZoomMaxFftSize = 65536.  Set well below kMaxFftSize
    // (262144) to bound the buffer-fill pause on every replan: at
    // 768 kHz DDC, 65536 fills in 85 ms (barely perceptible).  262144
    // would take 340 ms (jarring) and create a multi-frame avenger
    // ghost in the waterfall as the smoothed state crosses fftSize
    // resolutions.  Users who want larger FFTs explicitly opt in via
    // the slider (one-time pause they chose); auto-zoom won't push
    // above the cap automatically.
    //
    // Floor at the slider baseline (we never replan BELOW the user's
    // chosen value).
    //
    // Hysteresis: only replan when computed/current is outside
    // [0.66, 1.5].  Avoids replan thrash on smooth zoom drag.
    //
    // Phase 3F Sub-Epic I Task 8: the primary engine, because this lambda is
    // wired to pan 0's SpectrumWidget (the signal is per-pan). Per-pan
    // auto-zoom on secondary pans is Phase 3F follow-up work.
    constexpr int kAutoZoomMaxFftSize = 65536;
    connect(activeSpectrumWidget(), &SpectrumWidget::bandwidthChangeRequested,
            this, [this, kAutoZoomMaxFftSize](double bwHz) {
        FFTEngine* engine = primaryFftEngine();
        if (!engine || !activeSpectrumWidget()) { return; }
        const double sampleRate = activeSpectrumWidget()->sampleRate();
        if (sampleRate <= 0.0 || bwHz <= 0.0) { return; }

        const int baseline = engine->fftSizeBaseline();

        // Hz/bin override (Option 3 from the 2026-05-08 design).  When the
        // user has set a non-zero target Hz/bin in
        // Setup → Display → Spectrum Defaults, the auto-zoom formula
        // becomes zoom-INDEPENDENT:
        //   targetSize = sampleRate / hzPerBinTarget
        // The FFT delivers the requested resolution at any zoom — useful
        // for hunting narrow features (CW, digital).  Floor at baseline
        // still applies, so the FFT slider remains a minimum-FFT-size
        // knob.  When hzPerBinTarget == 0 we use the original
        // bins-in-window default (constant K = baseline).
        const double hzPerBinTarget = engine->hzPerBinTarget();
        double desired;
        if (hzPerBinTarget > 0.0) {
            desired = sampleRate / hzPerBinTarget;
        } else {
            const double scale = sampleRate / bwHz;        // 1.0 at full bw
            desired = static_cast<double>(baseline) * scale;
        }

        // Round up to next power of 2.
        int targetSize = 1024;
        while (targetSize < desired && targetSize < kAutoZoomMaxFftSize) {
            targetSize *= 2;
        }
        // Floor at baseline (slider's choice always honoured), then cap at
        // auto-zoom max.  When baseline > cap (user explicitly picked a
        // larger size via the slider), baseline wins and auto-zoom is a
        // no-op for that range.
        targetSize = (std::max)(targetSize, baseline);
        targetSize = (std::min)(targetSize, (std::max)(baseline, kAutoZoomMaxFftSize));

        // Hysteresis: only replan if outside [current * 2/3, current * 3/2].
        const int currentSize = engine->fftSize();
        if (currentSize > 0) {
            const double ratio = static_cast<double>(targetSize)
                                 / static_cast<double>(currentSize);
            if (ratio > 0.66 && ratio < 1.5) {
                return;  // small enough change to ignore
            }
        }

        engine->setFftSize(targetSize);
    });

    m_fftThread->start();

    // --- Meter Poller (Phase 3G-2) ---
    // Poller was created earlier so the meterReadyForPolling signal
    // could catch container restore + populateDefaultMeter emits.
    // m_meterWidget is registered automatically via that signal —
    // the call here is a defensive belt only and dedupes inside
    // MeterPoller::addTarget.
    if (m_meterWidget) {
        m_meterPoller->addTarget(m_meterWidget);
    }

    // Wire RxChannel to poller when WDSP finishes initializing.
    // RadioModel's initializedChanged handler creates the RxChannel, but
    // it was registered AFTER this connection (RadioModel registers during
    // onConnectionStateChanged, not buildUI). Qt fires in registration order,
    // so we defer by one event loop pass to ensure RxChannel exists.
    connect(m_radioModel->wdspEngine(), &WdspEngine::initializedChanged,
            this, [this](bool ok) {
        if (!ok) { return; }
        QTimer::singleShot(0, this, [this]() {
            // Phase 3F Sub-Epic J Task 11: RadioModel::rxChannelForSlice()
            // replaces the direct wdspEngine()->rxChannel() reach. Still
            // channel 0 here on purpose -- this is the boot-time seed, before
            // any slice but A exists, and the activeSliceChanged handler
            // below immediately supersedes it once other slices are added.
            RxChannel* rxCh = m_radioModel->rxChannelForSlice(0);
            if (rxCh) {
                m_meterPoller->setRxChannel(rxCh);
                m_meterPoller->start();
                qCDebug(lcMeter) << "MeterPoller started on RxChannel 0";
            } else {
                qCWarning(lcMeter) << "MeterPoller: RxChannel 0 still null after WDSP init";
            }

            // H.2 (Phase 3M-1a): wire TxChannel to MeterPoller for TX meters.
            // TxChannel is valid after WDSP initialization via createTxChannel().
            // Guard: txChannel() is null before initialization; null guard in
            // pollTxMeters() handles the case where it isn't set yet.
            if (TxChannel* txCh = m_radioModel->txChannel()) {
                m_meterPoller->setTxChannel(txCh);
                qCDebug(lcMeter) << "MeterPoller: TxChannel wired for TX meters";
            }
        });
    });

    // Phase 3F Sub-Epic J Task 4: the container S-meter is attached to no
    // flag, so it must show the active slice, not always slice A. The
    // WDSP-init binding above is only the seed for the first slice --
    // MeterPoller::pollSMeter() (bis 2026-08-18 die analoge Anzeige im
    // festen Kopf der AppletPanelWidget, heute der Sammelanschluss
    // smeterUpdated) reads m_rxChannel exclusively and that pointer never
    // moved after the seed, so the container meter showed RxChannel 0
    // whatever the operator was working. The per-flag mini S-meters do not
    // have this bug: they already resolve their own channel per slice via
    // MeterPoller::pollSliceSMeters() (wdspEngine()->rxChannel(sliceId)), so
    // they are untouched here.
    connect(m_radioModel, &RadioModel::activeSliceChanged, this, [this](int) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }
        // Phase 3F Sub-Epic J Task 11: RadioModel::rxChannelForSlice()
        // replaces the direct wdspEngine()->rxChannel() reach.
        RxChannel* rxCh = m_radioModel->rxChannelForSlice(slice->sliceIndex());
        if (rxCh) { m_meterPoller->setRxChannel(rxCh); }
        // Die Kopfleiste zeigt den Modus der Kette, auf der man gerade
        // ist. attach() löst die vorige — sonst meldete die Leiste nach
        // dem Umschalten weiter den Modus des alten Pans.
        if (m_commandBar) { m_commandBar->attach(slice); }
    });

    // Und einmal jetzt, für den Zustand beim Start: das Signal oben
    // feuert erst beim ersten Wechsel, und bis dahin stünde die Leiste
    // auf ihrem Vorgabewert statt auf dem, was das Gerät tut.
    if (m_commandBar) { m_commandBar->attach(m_radioModel->activeSlice()); }

    // H.2 (Phase 3M-1a): wire MoxController::moxStateChanged → MeterPoller::setInTx.
    // Switches the poll set between RX meters (TX off) and TX meters (TX on).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter dispatch.
    // Qt::QueuedConnection: ensures the flip happens at the start of the next
    // event loop tick rather than mid-poll, matching Thetis's timer dispatch.
    if (MoxController* mox = m_radioModel->moxController()) {
        connect(mox, &MoxController::moxStateChanged,
                m_meterPoller, &MeterPoller::setInTx,
                Qt::QueuedConnection);

        // ── Phase 3M-1b K.2: MOX rejection → toast ──────────────────────────
        // moxRejected fires when BandPlanGuard::checkMoxAllowed() rejects a
        // setMox(true) request (wrong mode, out-of-band freq, cross-band TX).
        // Presented for 3 seconds, matching the bandClickIgnored pattern.
        // The toast is transient: it clears automatically and does not affect
        // bottom-bar layout or persistence.
        connect(mox, &MoxController::moxRejected,
                this, [this](const QString& reason) {
            showToast(reason, ToastSeverity::Warning, 3000);
        });
    }

    // ── Phase 3M-0 Task 17: safety controller → status-bar wiring ────────────
    //
    // Wire PA Fwd/Rev/SWR telemetry into MeterPoller's cache so PA power
    // meter items (MeterPoller::setRadioStatus) receive live data as
    // RadioStatus::powerChanged fires. Called here (after poller creation)
    // so the connection outlives the poller's lifetime.
    m_meterPoller->setRadioStatus(&m_radioModel->radioStatus());

    // Ganymede PA-trip badge: RadioModel::paTrippedChanged → setPaTripped.
    // setPaTripped() was added in Task 14 and updates m_paStatusBadge text
    // and colour atomically.
    connect(m_radioModel, &RadioModel::paTrippedChanged,
            this, &MainWindow::setPaTripped);

    // TX Inhibit pill: TxInhibitMonitor::txInhibitedChanged → setTxInhibited.
    // setTxInhibited() was added in Task 14 and toggles m_txInhibitLabel
    // visibility. The Source parameter is ignored by the UI slot (the pill
    // is binary: visible or hidden).
    connect(&m_radioModel->txInhibit(),
            &safety::TxInhibitMonitor::txInhibitedChanged,
            this, [this](bool inhibited, safety::TxInhibitMonitor::Source /*source*/) {
        setTxInhibited(inhibited);
    });
}

void MainWindow::rebuildEditContainerSubmenu()
{
    if (!m_editContainerMenu) { return; }
    m_editContainerMenu->clear();

    if (!m_containerManager) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    if (all.isEmpty()) {
        auto* none = m_editContainerMenu->addAction(
            QStringLiteral("(no containers)"));
        none->setEnabled(false);
        return;
    }

    // Alphabetical by title so the menu order is predictable even
    // when containers are created in different orders.
    QList<ContainerWidget*> sorted = all;
    std::sort(sorted.begin(), sorted.end(),
              [](ContainerWidget* a, ContainerWidget* b) {
        const QString na = a->notes().isEmpty()
            ? a->id().left(8) : a->notes();
        const QString nb = b->notes().isEmpty()
            ? b->id().left(8) : b->notes();
        return na.localeAwareCompare(nb) < 0;
    });

    for (ContainerWidget* c : sorted) {
        const QString label = c->notes().isEmpty()
            ? (QStringLiteral("(unnamed) ") + c->id().left(8))
            : c->notes();
        const QString id = c->id();

        // Ein Untermenue je Container statt einer einzelnen Aktion
        // (2026-08-19). Grund: der Rechtsklick AUF dem Container kommt
        // nicht ueberall an — im Applet-Container deckt die Applet-Saeule
        // die Flaeche vollstaendig ab und fuehrt ihr eigenes Menue. Beim
        // Selbsttest im laufenden Programm war mein neues Container-Menue
        // deshalb NICHT erreichbar. Ueber die Menueleiste ist es das
        // immer.
        QMenu* sub = m_editContainerMenu->addMenu(label);

        QAction* settings = sub->addAction(QStringLiteral("Settings…"));
        connect(settings, &QAction::triggered, this, [this, id]() {
            if (!m_containerManager) { return; }
            ContainerWidget* target = m_containerManager->container(id);
            if (!target) { return; }
            ContainerSettingsDialog dialog(target, this, m_containerManager);
            dialog.exec();
        });

        sub->addSeparator();

        // Die drei Andock-Arten in den Worten des Betreibers: „jeder
        // Container muss sich ueberall hinbewegen koennen … anschliessend
        // mit einem Schloss um es zu fixieren."
        QAction* move = sub->addAction(QStringLiteral("Move freely"));
        move->setCheckable(true);
        move->setChecked(c->isOverlayDocked());
        move->setToolTip(QStringLiteral(
            "Place this container anywhere over the main area — drag its "
            "title bar to move, the corner to resize."));
        connect(move, &QAction::triggered, this, [this, id]() {
            if (m_containerManager) { m_containerManager->overlayDockContainer(id); }
        });

        QAction* window = sub->addAction(QStringLiteral("Own window"));
        window->setCheckable(true);
        window->setChecked(c->isFloating());
        connect(window, &QAction::triggered, this, [this, id]() {
            if (m_containerManager) { m_containerManager->floatContainer(id); }
        });

        QAction* edge = sub->addAction(QStringLiteral("Dock at the edge"));
        edge->setCheckable(true);
        edge->setChecked(c->isPanelDocked());
        edge->setToolTip(QStringLiteral(
            "Back into the side panel, where it shares one axis with the "
            "panadapter."));
        connect(edge, &QAction::triggered, this, [this, id]() {
            if (m_containerManager) { m_containerManager->panelDockContainer(id); }
        });

        sub->addSeparator();

        QAction* lock = sub->addAction(QStringLiteral("Lock in place"));
        lock->setCheckable(true);
        lock->setChecked(c->isLocked());
        lock->setToolTip(QStringLiteral(
            "Freeze position and size until the lock comes off."));
        connect(lock, &QAction::triggered, this, [this, id](bool on) {
            if (!m_containerManager) { return; }
            if (ContainerWidget* target = m_containerManager->container(id)) {
                target->setLocked(on);
            }
        });
    }
}

void MainWindow::resetDefaultLayout()
{
    if (!m_containerManager) { return; }

    // Destroy every non-panel container. Collect IDs first because
    // destroyContainer mutates the underlying map.
    const QList<ContainerWidget*> all = m_containerManager->allContainers();
    ContainerWidget* panel = m_containerManager->panelContainer();
    QStringList toRemove;
    for (ContainerWidget* c : all) {
        if (c == panel) { continue; }
        toRemove.append(c->id());
    }
    for (const QString& id : toRemove) {
        m_containerManager->destroyContainer(id);
    }

    // Also wipe the panel container's MeterWidget items and rebuild
    // from factories. Before this, a persisted item payload (e.g. a
    // bar-style S-Meter saved by an earlier build) would survive
    // Reset because only the non-panel containers were destroyed —
    // Container #0's MeterWidget was left untouched, so the stale
    // items reloaded every launch and Reset felt like a no-op for
    // the main meter column.
    if (m_meterWidget) {
        m_meterWidget->clearItems();

        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    rebuildEditContainerSubmenu();
    qCInfo(lcContainer) << "Reset default layout: removed"
                         << toRemove.size() << "non-panel containers"
                         << "and rebuilt panel meter defaults";
}

void MainWindow::createDefaultContainers()
{
    // Container #0: panel-docked right side (AetherSDR pattern).
    // Placeholder content in 3G-1, replaced by AppletPanel in 3G-AP.
    ContainerWidget* c0 = m_containerManager->createContainer(1, DockMode::PanelDocked);
    c0->setNotes(QStringLiteral("Main Panel"));
    c0->setNoControls(false);

    qCDebug(lcContainer) << "Created default Container #0 (panel-docked):" << c0->id();
}

void MainWindow::populateDefaultMeter()
{
    ContainerWidget* c0 = m_containerManager->panelContainer();
    if (!c0) {
        qCWarning(lcContainer) << "No panel container for meter widget";
        return;
    }

    // Guard: don't repopulate if real content (AppletPanelWidget) already exists.
    // The container constructor creates a placeholder QLabel("Container") which
    // we need to replace. Check if content is already an AppletPanelWidget.
    if (qobject_cast<AppletPanelWidget*>(c0->content()) != nullptr) {
        return;
    }

    // ContainerManager::restoreState may have set a MeterWidget with
    // user-saved items as the panel container's content. Capture that
    // payload before we overwrite c0's content. We can't reuse the
    // pointer directly because ContainerWidget::setContent() calls
    // deleteLater on the previous m_content; round-tripping through
    // serialize/deserialize transfers the items into a fresh
    // m_meterWidget that becomes the AppletPanelWidget header.
    QString restoredItems;
    if (auto* existingMeter = qobject_cast<MeterWidget*>(c0->content())) {
        if (!existingMeter->items().isEmpty()) {
            restoredItems = existingMeter->serializeItems();
        }
    }

    m_meterWidget = new MeterWidget();

    if (!restoredItems.isEmpty()) {
        m_meterWidget->deserializeItems(restoredItems);
        // Rebuild the runtime stack metadata from geometry so bar
        // rows restored from a saved container participate in the
        // reflow-on-resize path again. No-op for panels that don't
        // contain a stack.
        m_meterWidget->inferStackFromGeometry();
        qCDebug(lcContainer) << "Restored" << m_meterWidget->items().size()
                             << "panel meter items from saved state";
    } else {
        // S-Meter: top 45% — arc needle bound to SignalAvg
        // From Thetis MeterManager.cs: ANAN needle uses AVG_SIGNAL_STRENGTH
        ItemGroup* smeter = ItemGroup::createSMeterPreset(
            MeterBinding::SignalAvg, QStringLiteral("S-Meter"), m_meterWidget);
        smeter->installInto(m_meterWidget, 0.0f, 0.0f, 1.0f, 0.45f);
        delete smeter;

        // Power/SWR: middle 40% — stacked bars (stub TX bindings)
        ItemGroup* pwrSwr = ItemGroup::createPowerSwrPreset(
            QStringLiteral("Power/SWR"), m_meterWidget);
        pwrSwr->installInto(m_meterWidget, 0.0f, 0.45f, 1.0f, 0.40f);
        delete pwrSwr;

        // ALC: bottom 15% — compact single-line bar (stub TX binding)
        ItemGroup* alc = ItemGroup::createAlcPreset(m_meterWidget);
        alc->installInto(m_meterWidget, 0.0f, 0.85f, 1.0f, 0.15f);
        delete alc;
    }

    // Build an AppletPanelWidget: scrollable applets.
    // Task 40 (Phase 3P-II) installierte hier die analoge Anzeige als
    // festen Kopf; sie ist am 2026-08-18 weggefallen, der Kopf mit ihr.
    // Der noch aeltere setHeaderWidget(m_meterWidget, ...) fiel schon
    // damals weg; the composite
    // MeterWidget (m_meterWidget) remains the Container #0 content for the
    // traditional GroupBox-based meters and is not affected.
    m_appletPanel = new AppletPanelWidget();
    auto* panel = m_appletPanel;

    // WdspEngine an den Poller: getMaxBinDbm() braucht sie, und Max Bin
    // ist seit 2026-08-18 eine Kennung wie jede andere. Der setSMeter-
    // Aufruf, der hier stand, ist mit dem festen S-Meter-Kopf
    // weggefallen.
    if (m_meterPoller) {
        m_meterPoller->setWdspEngine(m_radioModel->wdspEngine());

        // RX meter cal offset source (Thetis-faithful port).
        // RadioModel::rxMeterOffsetDb() returns RXPreampOffset(1) +
        // RXCalibrationOffset(1) per Thetis console.cs:21040 [v2.10.3.13].
        // The callable captures m_radioModel by raw pointer (lives for
        // the lifetime of MainWindow); pollSMeter / poll invoke it
        // once per tick.  Without this wire-up the WDSP S-meter readings
        // sit in raw ADC dBFS instead of at-antenna dBm.
        m_meterPoller->setRxOffsetSource([rm = m_radioModel]() -> double {
            return rm ? rm->rxMeterOffsetDb() : 0.0;
        });
    }

    // 2026-05-22 spectrum-calibration fix (Fix 1 from the S-meter / spectrum
    // alignment research). FFTEngine bins ship in raw dBFS (window-coherent
    // gain compensated against the digital I/Q full-scale reference). The
    // S-meter path adds the same RXPreampOffset + RXCalibrationOffset chain
    // Thetis applies at console.cs:21040 to land at antenna-referenced dBm,
    // but the spectrum path never got that calibration step. Result: the
    // S-meter and the spectrum trace lived in different reference frames
    // (38 dB gap on the bench at preamp Off; 18 dB at preamp On). Forward
    // the SAME rxMeterOffsetDb value to SpectrumWidget::setDbmCalOffset so
    // both views share the antenna reference. Per the calibration research,
    // carriers will then agree to ~1 dB between meter and spectrum. Noise
    // floor will still differ by 10*log10(NBP_BW/bin_BW) which is physics
    // (S-meter is passband-integrated; spectrum is per-bin) and matches
    // Thetis behavior.
    if (activeSpectrumWidget() && m_radioModel) {
        auto pushSpectrumCal = [this](double db) {
            if (activeSpectrumWidget()) {
                activeSpectrumWidget()->setDbmCalOffset(static_cast<float>(db));
            }
        };
        connect(m_radioModel, &RadioModel::rxMeterOffsetChanged,
                this, pushSpectrumCal);
        // Initial push so the cal lands at startup before any controller
        // change. setStepAttController already calls the recompute lambda
        // once on attach, but that may have run before this connect was
        // wired -- push the current value here defensively.
        pushSpectrumCal(m_radioModel->rxMeterOffsetDb());
    }

    // Refresh MaxBin's CTUN slice offset whenever the DDC center moves.
    // The frequencyChanged lambda in wireSliceToSpectrum pushes the
    // offset on slice retune, but a spectrum pan moves the DDC NCO
    // without moving the slice -- without this hook, MaxBin's scan
    // window stays at the OLD DDC-relative bin range until the next
    // slice retune (or CTUN toggle).  Observable as "MaxBin meter
    // drifts off the carrier when I pan the panadapter."
    if (activeSpectrumWidget()) {
        connect(activeSpectrumWidget(), &SpectrumWidget::ddcCenterFrequencyChanged,
                this, [this](double ddcCenter) {
            if (!m_radioModel) { return; }
            auto* eng = m_radioModel->wdspEngine();
            if (!eng) { return; }
            SliceModel* slice = m_radioModel->activeSlice();
            if (!slice) { return; }
            eng->setMaxBinSliceOffsetHz(/*disp=*/0,
                                        slice->frequency() - ddcCenter);
        });
    }

    // Task 43 (Phase 3P-II): PGXL-aware power scale + TX meter feed.
    //
    // Four permanent connects (RadioModel persists across radio connects):
    //
    // 1. ampMetersChanged: when PGXL is OPERATE, forward the amp's
    //    forward-power/SWR readings to the TxApplet power display.
    //    RadioModel::ampMetersChanged is fired by PgxlConnection on each
    //    statusUpdated containing "peakfwd" and "swr" keys.
    //
    // 2. RadioStatus::powerChanged: when PGXL is absent or STANDBY, use the
    //    radio's own PA telemetry (barefoot or Aurora) for the TX display.
    //    fwd is forward power in watts; the third argument (swr) is used.
    //
    // 3. amplifierChanged: snap the power scale to 2 kW on PGXL connect.
    //
    // 4. ampStateChanged: re-evaluate scale whenever OPERATE/STANDBY toggles.
    //    When returning to STANDBY (amp present but not OPERATE), the scale
    //    reverts to barefoot by passing hasAmplifier()=false to setPowerScale.
    {
        // Connect 1: PGXL amp meters (OPERATE path).
        // Connect 2: radio barefoot/Aurora TX meters (STANDBY or no amp).
        // RadioStatus::powerChanged carries (fwdWatts, revWatts, swr); we
        // take fwdWatts (arg 1) and swr (arg 3) to match setTxMeters() signature.
        //
        // 2026-05-25 KG4VCF bench fix: gate on isAnyExternalAmpInOperate
        // (cross-vendor) instead of just PGXL state.  RadioStatus::powerChanged
        // fires much faster than RF-Kit's 1 Hz REST poll, so when RF-Kit
        // (without PGXL) is in OPERATE, the radio's barefoot reading was
        // overwriting the RF-Kit amp reading at ~20 Hz.  Now barefoot only
        // feeds when no external amp is amplifying.
        // Connect 1 und 2 speisten die analoge Nadel: PGXL ueber
        // ampMetersChanged, das Geraet selbst ueber powerChanged, mit
        // einer Torschaltung dazwischen (KG4VCF-Befund 2026-05-25:
        // 20 Hz Geraetetelemetrie ueberschrieb sonst den 1-Hz-Wert des
        // Verstaerkers). Beide Wege samt Torschaltung stehen jetzt in
        // TxApplet::wireControls — dort, wo die Anzeige ist.

        // Connect 3: PGXL connect event snaps scale to 2 kW.
        //
        // 2026-08-18: die TxApplet bekommt dieselbe Skala. Bis dahin
        // sprang nur die analoge Anzeige im Panelkopf auf 2 kW,
        // waehrend die Leistungsanzeige in der TxApplet weiter auf der
        // Barfussdecke des Geraets stand und bei jedem Sendevorgang am
        // Anschlag klebte. „Die 2-kW-Skala geht mit ihnen — sie gehoert
        // zur Leistungsanzeige, nicht zum Empfangszeiger." (OE5SOS)
        connect(m_radioModel, &RadioModel::amplifierChanged, this,
                [this](bool present) {
            if (m_txApplet) { m_txApplet->setPowerScale(0, present); }
        });

        // Connect 4: OPERATE/STANDBY state change re-evaluates scale.
        // When STANDBY: hasAmplifier()=true but we want barefoot scale,
        // so pass false (treat as absent) until OPERATE resumes.
        //
        // 2026-05-25 KG4VCF bench fix: use the cross-vendor predicate so
        // PGXL going STANDBY does not flip the power scale back to
        // barefoot if RF-Kit is still amplifying (and vice versa).
        connect(m_radioModel, &RadioModel::ampStateChanged, this,
                [this]() {
            const bool amplifying = m_radioModel->isAnyExternalAmpInOperate();
            if (m_txApplet) { m_txApplet->setPowerScale(0, amplifying); }
        });

        // Eine fuenfte Verbindung (2026-05-20) liess die analoge Nadel
        // beim Senden auf die Leistungsskala wechseln. Sie faellt mit
        // dem festen Kopf weg; das Zeigerinstrument braucht den Wechsel
        // nicht, weil es die Groesse selbst waehlt statt sie aus dem
        // Sendezustand abzuleiten.
    }

    // Phase 3P-III review fix C1 verband die cross-vendor RF-Kit-Signale
    // mit der analogen Anzeige, die sie bis dahin als einzige nicht
    // abonniert hatte: die 2-kW-Umschaltung blieb bei RF-Kit OPERATE
    // stumm und die Sendenadel stand.
    //
    // Die Anzeige ist weg; die zwei Verbindungen bleiben und speisen
    // dieselbe Skala wie die PGXL-Wege (Connect 1..5 oben) in der
    // TxApplet. Sie stehen weiter hier, damit alle Verstaerkerwege an
    // einer Stelle liegen.
    {
        // RF-Kit Connect A: snap 2 kW scale on RF-Kit OPERATE.
        // externalAmpOperateChanged is now transition-gated (fix I2) so this
        // fires only when the amp enters or leaves OPERATE, not every poll.
        //
        // 2026-05-25 KG4VCF bench fix: use the cross-vendor predicate so
        // RF-Kit going STANDBY does not flip back to barefoot scale if
        // PGXL is still amplifying.
        connect(m_radioModel, &RadioModel::externalAmpOperateChanged, this,
                [this](bool /*inOp*/) {
            const bool amplifying = m_radioModel->isAnyExternalAmpInOperate();
            // Dieselbe Skala fuer die TxApplet — siehe Connect 3 oben.
            if (m_txApplet) { m_txApplet->setPowerScale(0, amplifying); }
        });

        // RF-Kit Connect B: feed RF-Kit forward power + SWR to the TX needle.
        // externalAmpFwdSwrUpdated carries watts (int) and SWR (float);
        // setTxMeters takes (float fwdPower, float swr).
        //
        // 2026-05-25 KG4VCF bench fix: gate so a STANDBY amp doesn't push
        // 0 W into the needle while the radio's barefoot reading is also
        // wanting the meter.  Only fire when the amp is actually amplifying.
        //
        // The gate is RF-Kit's own OPERATE state, not the cross-vendor
        // isAnyExternalAmpInOperate(): this signal carries RF-Kit telemetry
        // exclusively, so the cross-vendor form let a PGXL in OPERATE pass
        // RF-Kit /power polls through from an RF2K-S sitting in STANDBY,
        // clobbering the live PGXL reading with RF-Kit's 0 W once per poll.
        // Codex review, PR #291.
        // Der RF-Kit-Zufluss ebenso — er steht mit derselben
        // Torschaltung in der TxApplet.
    }

    // Connect 5: Phase 3P-II Phase 4 Task 97 -- PGXL power cap soft-alert.
    // Fires a 5-second status-bar toast when peak forward power exceeds the
    // cap configured in Setup -> Peripherals -> PGXL Advanced -> Hardware.
    // De-bounced: one toast per exceedance event (re-arms below cap).
    connect(m_radioModel, &RadioModel::ampMetersChanged,
            this, &MainWindow::onAmpMetersForPowerCap);

    // Phase 3P-II review fix C2: surface TX interlock decisions to the
    // operator via 5-second status-bar toasts.  Without these connections
    // Block mode silently gates TX with no operator feedback.
    if (TxInterlockPolicy* policy = m_radioModel->txInterlockPolicy()) {
        connect(policy, &TxInterlockPolicy::warned,
                this, &MainWindow::onTxInterlockWarning);
        connect(policy, &TxInterlockPolicy::denied,
                this, &MainWindow::onTxInterlockDenial);
    }

    // RxApplet — Tier 1 wired to SliceModel (slice attached in wireSliceToSpectrum)
    m_rxApplet = new RxApplet(nullptr, m_radioModel, nullptr);
    panel->addApplet(m_rxApplet);

    // Phase 3P-I-a T16 — push board caps into RxApplet so ANT buttons
    // hide on HL2/Atlas. Matches the VFO Flag wiring below (T15).
    // B3: also push SKU profile so AntennaPopupBuilder knows rxOnlyLabels.
    m_rxApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    m_rxApplet->setHpsdrSku(m_radioModel->hardwareProfile().model);
    connect(m_radioModel, &RadioModel::currentRadioChanged, m_rxApplet,
            [this]() {
        m_rxApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
        m_rxApplet->setHpsdrSku(m_radioModel->hardwareProfile().model);
    });

    // ── Phase 3F (Bug 3): per-slice RX applet + active-slice switching ──────
    //
    // Clicking a slice tab in the RX applet, or clicking a VfoWidget flag,
    // routes to RadioModel::setActiveSlice. RadioModel::activeSliceChanged
    // then rebinds the RX applet to the new active slice, refreshes the tab
    // row highlight, and updates the static badge. The slice tab row is also
    // refreshed whenever the slice list changes (add/remove). Workflow ported
    // from AetherSDR (RxApplet::sliceActivationRequested ->
    // MainWindow::setActiveSlice at MainWindow.cpp:3277, and the
    // setActiveSliceInternal rebind at MainWindow.cpp:12132 [@6a142807]).
    //
    // setActiveSliceById for the same reason the flag path uses it:
    // updateSliceButtons keys each tab's button-group id to the slice's own
    // sliceIndex() rather than its list position, so the id has to be
    // converted rather than indexed with.
    connect(m_rxApplet, &RxApplet::sliceActivationRequested, this,
            [this](int sliceId) {
        if (m_radioModel) { m_radioModel->setActiveSliceById(sliceId); }
    });

    auto refreshSliceTabs = [this]() {
        if (m_rxApplet && m_radioModel) {
            m_rxApplet->updateSliceButtons(
                m_radioModel->slices(),
                m_radioModel->activeSlice()
                    ? m_radioModel->activeSlice()->sliceIndex()
                    : -1);
        }
    };

    connect(m_radioModel, &RadioModel::activeSliceChanged, this,
            [this](int sliceIndex) {
        if (!m_radioModel) { return; }
        SliceModel* active = m_radioModel->activeSlice();
        // Rebind the RX applet to the new active slice + refresh its badge.
        if (m_rxApplet) {
            m_rxApplet->setSlice(active);
            if (active) { m_rxApplet->setSliceIndex(active->sliceIndex()); }
        }
        // Refresh the tab row highlight (uses sliceIndex param for the
        // checked state even if activeSlice() is briefly stale).
        if (m_rxApplet) {
            m_rxApplet->updateSliceButtons(m_radioModel->slices(), sliceIndex);
        }
    });

    // Keep the tab row in sync with the slice population.
    connect(m_radioModel, &RadioModel::sliceAdded, this,
            [refreshSliceTabs](int) { refreshSliceTabs(); });
    connect(m_radioModel, &RadioModel::sliceRemoved, this,
            [refreshSliceTabs](int) { refreshSliceTabs(); });

    // TxApplet — NYI shell (Phase 3I-1)
    // 3M-3a-ii Batch 6: cache pointer in m_txApplet so SetupDialog
    // instances can wire CfcSetupPage's [Configure CFC bands…] button
    // through to TxApplet::requestOpenCfcDialog.
    auto* txApplet = new TxApplet(m_radioModel, nullptr);
    m_txApplet = txApplet;
    panel->addApplet(txApplet);

    // ── 3M-1c Phase L: hand TxApplet the controllers it needs ──────────────
    //
    // L.1 — MicProfileManager (J.1 setter): drives the TX Profile combo
    // population + active-profile mirror + "Default" seed surfacing.  The
    // pointer is obtained from RadioModel (constructed in the ctor; per-MAC
    // scope is set inside connectToRadio).  Pre-connect, the manager is
    // unscoped and the combo simply stays at the placeholder "Default" item
    // (rebuildProfileCombo() no-ops when no manager is set).
    //
    // L.2 — TwoToneController (J.2 setter): drives the 2-TONE button toggle
    // round-trip.  The controller's setActive(true) refuses with a
    // qCWarning when m_powerOn is false, so pre-connect button presses are
    // safely rejected.
    //
    // L (J.4) — txProfileMenuRequested signal: a right-click on the profile
    // combo opens SetupDialog at "TX Profile".  Lambda-construct a fresh
    // SetupDialog each time (matches the 7 other "open setup" sites in
    // MainWindow.cpp at lines 1283 / 2824 / 2834 / 2846 / 3029 / 3428).
    if (m_radioModel) {
        txApplet->setMicProfileManager(m_radioModel->micProfileManager());
        txApplet->setTwoToneController(m_radioModel->twoToneController());
    }
    connect(txApplet, &TxApplet::txProfileMenuRequested, this, [this]() {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(QStringLiteral("TX Profile"));
        dialog->show();
    });

    // ── Phase 3M-4 Task 13: PS-A right-click → open PsForm ─────────────────
    // Mirrors Thetis chkFWCATUBypass_MouseDown (console.cs:46149-46152
    // [v2.10.3.13]).  PsForm is the same singleton dialog opened from the
    // Tools / DSP menu and from PureSignalApplet right-click handlers.
    connect(txApplet, &TxApplet::openPureSignalDialogRequested,
            this, &MainWindow::openPureSignalDialog);

    // Phase 3M-4 Task 13 — capability-gated PS-A visibility.  Push initial
    // caps + keep them in sync on RadioModel::currentRadioChanged.  TxApplet
    // hides m_psaBtn when caps.hasPureSignal == false (HL2 / Atlas).
    txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    connect(m_radioModel, &RadioModel::currentRadioChanged, txApplet,
            [this, txApplet]() {
        txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
    });

    // Phase 3M-4 Task 13 — late-bound coordinator handoff.  TxApplet
    // already self-subscribes inside its ctor (see TxApplet.cpp wireControls
    // PS-A block), so no explicit connect needed here.  Still: push the
    // current coordinator at startup in case it's already live (test path).
    if (PureSignal* ps = m_radioModel->pureSignal()) {
        txApplet->setPureSignal(ps);
    }

    // 3M-1a H.1-H.4 fixup: wire panadapter band changes to TxApplet so
    // the per-band Tune Power slider tracks the active band.
    // Without this, m_currentBand stays at Band::Band20m permanently.
    if (!m_radioModel->panadapters().isEmpty()) {
        PanadapterModel* pan0 = m_radioModel->panadapters().first();
        connect(pan0, &PanadapterModel::bandChanged,
                txApplet, &TxApplet::setCurrentBand);
        // Push the initial band immediately so the slider shows the right value.
        txApplet->setCurrentBand(pan0->band());
    }

    // 3M-1a (2026-04-27): also track every SLICE'S frequency.  The
    // panadapter band only changes when its center crosses a band
    // boundary, but the user's slice can sit on a different band entirely
    // (e.g. the slice loaded at 7.241 MHz while the panadapter center is
    // still on 14 MHz from a prior session). Without this wire,
    // TxApplet's m_currentBand lags the slice → the TUN-power slider
    // writes to the wrong band's stored value (or no-ops on the
    // m_currentBand-default band — bench-confirmed 2026-04-27 with the
    // slider only working on 20m even after retuning to 40m).
    //
    // Pattern matches AntennaAlexAlex2Tab.cpp:408-425 — subscribe to
    // every current slice AND every future-added slice (slices are
    // created by addSlice() AFTER MainWindow construction, so a single-
    // shot activeSlice() check at construction returns null and never
    // wires up).
    {
        auto subscribeToSlice = [txApplet](SliceModel* slice) {
            if (!slice) { return; }
            connect(slice, &SliceModel::frequencyChanged,
                    txApplet, [txApplet](double freq) {
                        txApplet->setCurrentBand(bandFromFrequency(freq));
                    });
            // Push the slice's current band immediately — overrides the
            // panadapter initial when the slice is on a different band.
            txApplet->setCurrentBand(bandFromFrequency(slice->frequency()));
        };
        for (SliceModel* slice : m_radioModel->slices()) {
            subscribeToSlice(slice);
        }
        connect(m_radioModel, &RadioModel::sliceAdded, txApplet,
                [this, subscribeToSlice](int index) {
                    subscribeToSlice(
                        sliceForAddedIdForTest(m_radioModel, index));
                });
    }

    // PhoneCwApplet — Phone + CW pages, NYI
    m_phoneCwApplet = new PhoneCwApplet(m_radioModel, nullptr);
    panel->addApplet(m_phoneCwApplet);

    // RadeApplet — Phase 3R L2.  Sits alongside PhoneCwApplet but is
    // visible only when the active slice's mode is DSPMode::RADE_U or
    // DSPMode::RADE_L.  The initial mode is set in the dspModeChanged
    // lambda below; for the default startup mode (USB) the applet
    // starts hidden.
    m_radeApplet = new RadeApplet(m_radioModel, nullptr);
    panel->addApplet(m_radeApplet);
    m_radeApplet->setVisible(false);

    // Ghost applets — hidden per docs/superpowers/plans/2026-05-01-ui-polish-right-panel.md §Task 6.
    // These applets are entirely placeholder-only today (no wired controls).
    // Showing them is misleading — users click e.g. "Equalizer" and nothing happens.
    // Uncomment each when its feature phase ships (one-line re-enable).
    //
    // m_eqApplet = new EqApplet(m_radioModel, nullptr);           // TODO 3I-3: TX/RX EQ wiring
    // panel->addApplet(m_eqApplet);

    // VaxApplet — per-VAX-channel gain + mute + level meters
    // (Phase 3O Sub-Phase 9 Task 9.2b).
    m_vaxApplet = new VaxApplet(m_radioModel,
                                m_radioModel->audioEngine(), nullptr);
    panel->addApplet(m_vaxApplet);

    // DvkApplet — der Sprachspeicher (2026-08-19).
    //
    // Er stand bis heute in der Geister-Liste weiter unten: gebaut, aber
    // nie erzeugt. Ich habe ihn mit zehn Plaetzen gefuellt, OHNE vorher
    // nachzusehen, ob ihn jemand oeffnen kann — genau der Fehler, den
    // ich heute vier Mal woanders gefunden habe. Jetzt haengt er.
    //
    // Er ist auch ohne Funkgeraet brauchbar: Ansagen aus WAV-Dateien
    // laden, benennen, Tasten zuordnen. Aufnahme und Wiedergabe brauchen
    // den Sendeweg und sagen das an ihren Knoepfen.
    m_dvkApplet = new DvkApplet(m_radioModel, nullptr);
    panel->addApplet(m_dvkApplet);

    // QsoRecorderApplet — die QSO-Aufnahme (2026-08-19, Entwurf 3).
    //
    // Direkt neben dem Sprachspeicher, weil beide dasselbe Mikrofon
    // abgreifen und man sonst zweimal sucht.
    //
    // Auch ohne Funkgeraet sichtbar: die Liste der Aufnahmen und der
    // Ordner sind Dateiarbeit. Der Knopf braucht die Verbindung; ohne
    // sie bleiben beide Spuren still und der Kopf sagt „no radio".
    m_qsoRecorderApplet = new QsoRecorderApplet(m_radioModel, nullptr);
    panel->addApplet(m_qsoRecorderApplet);

    // ── KiwiSDR (Stufe 4, 2026-08-23) ───────────────────────────────
    //
    // Der Betreiber am 2026-08-22: "kiwi sdr wäre auch eine tolle
    // sache. das bitte für die nachtschicht."
    //
    // Was hier ankommt, ist die ANZEIGE: welche Empfaenger eingerichtet
    // sind, wie es um ihre Verbindung steht und welche Scheibe an
    // welchem haengt. Ton und Wasserfall folgen; die Grenze ist am Kopf
    // von MainWindow_KiwiSdr.cpp ausdruecklich aufgeschrieben, damit
    // sie niemand fuer erledigt haelt.
    m_kiwiSdrManager = new KiwiSdrManager(this);
    m_kiwiSdrApplet  = new KiwiSdrApplet(m_radioModel, nullptr);
    m_kiwiSdrApplet->setKiwiSdrManager(m_kiwiSdrManager);
    panel->addApplet(m_kiwiSdrApplet);
    m_kiwiWaterfallPanel = new KiwiWaterfallPanel(m_radioModel, nullptr);
    panel->addApplet(m_kiwiWaterfallPanel);
    wireKiwiSdr();

    // BandwidthFilterApplet — die Durchlassflaeche (2026-08-20).
    //
    // Eine Flaeche je Empfaenger auf ECHTER Frequenzachse, nach der
    // Vorlage des Betreibers (Zeus Link „BANDWIDTH FILTER"). Die
    // Rechenregeln dahinter kommen aus Thetis und sitzen im Modell
    // (SliceModel::widthToEdges), damit sie auch fuer CAT und Tastatur
    // gelten.
    //
    // Die Doppelung, die hier bis 2026-08-20 vermerkt war, ist
    // aufgeloest: das alte FilterPassbandWidget im RxApplet ist
    // entfernt, dort steht jetzt dieselbe BandwidthFilterPane, nur
    // kleiner. Eine Umsetzung, zwei Plaetze.
    m_bwFilterApplet = new BandwidthFilterApplet(m_radioModel, nullptr);

    // ── Das Signal in den Bandfilter ────────────────────────────────
    //
    // Der Betreiber am 2026-08-22, nach einer Vorfuehrung von Zeus
    // Link: "er sollte mir ja auch das signal zeigen" und "genau wo
    // ich im panadapter bin soll auch der bandwith filter sein."
    //
    // Geholt wird der Ausschnitt beim PANADAPTER derselben Scheibe:
    // dort sind Bins, DDC-Mitte, Abtastrate und dBm-Versatz beisammen
    // und geprueft. Eine eigene Abbildung im Bandfilter waere ein
    // zweiter Ort, an dem sie falsch sein kann — und genau davon
    // hatten wir heute genug.
    //
    // AetherSDR hat das nicht (sein FilterPassbandWidget ist ein
    // reiner Kanteneditor); nachgesehen, bevor gebaut wurde.
    m_bwFilterApplet->setSpectrumSource(
        [this](int sliceIndex, double loHz, double hiHz, int points)
            -> QVector<float> {
        if (!m_radioModel || !m_panStack) { return {}; }
        const QList<SliceModel*> slices = m_radioModel->slices();
        if (sliceIndex < 0 || sliceIndex >= slices.size()) { return {}; }
        SliceModel* s = slices.at(sliceIndex);
        if (!s) { return {}; }

        // Der Panadapter, der diese Scheibe zeigt — sonst der aktive.
        SpectrumWidget* sw = nullptr;
        const QString panId = s->panKey();
        if (!panId.isEmpty()) { sw = m_panStack->spectrum(panId); }
        if (!sw) { sw = activeSpectrumWidget(); }
        if (!sw) { return {}; }
        return sw->dbmOverRange(loHz, hiHz, points);
    });
    panel->addApplet(m_bwFilterApplet);

    // Betreiber 2026-08-30: der Bandwidth Filter zeigt die Filterkanten
    // (ein Einstellungswert), keine Live-Messung wie S-Meter/Stehwelle/SWR
    // -- er bleibt auch ohne Verbindung sinnvoll und darf nicht in
    // derselben Vorverbindungs-Versteckung landen wie die Meter (siehe der
    // isFloating()/isOverlayDocked()-Block weiter oben). Der Block laeuft
    // aber VOR dieser Zeile, als m_bwFilterApplet noch null war -- ein
    // direkter Ausschluss dort war deshalb unmoeglich. Stattdessen hier,
    // sobald das Applet existiert: falls sein eigener (OverlayDocked-)
    // Container gerade eben mit in die Liste geraten und versteckt wurde,
    // sofort wieder herausnehmen und zeigen, statt auf die erste Verbindung
    // zu warten.
    if (!m_floatingContainersHiddenPreConnect.isEmpty()) {
        for (ContainerWidget* c : m_containerManager->allContainers()) {
            if (c && c->content() == m_bwFilterApplet
                && m_floatingContainersHiddenPreConnect.removeOne(c)) {
                c->show();
                break;
            }
        }
    }

    // Phase 3M-4 Task 13 — PureSignalApplet quick-access surface.
    //
    // Constructed unconditionally and added to the right panel, but
    // visibility is gated on caps.hasPureSignal in onConnectionStateChanged
    // (HL2 / Atlas hide the applet entirely; G2-class boards show it).
    // Right-click on every PureSignalApplet control opens PsForm via the
    // openPureSignalDialogRequested signal, which MainWindow forwards to
    // openPureSignalDialog (same singleton dialog as Tools / DSP menu).
    m_pureSignalApplet = new PureSignalApplet(m_radioModel, nullptr);
    panel->addApplet(m_pureSignalApplet);
    connect(m_pureSignalApplet,
            &PureSignalApplet::openPureSignalDialogRequested,
            this, &MainWindow::openPureSignalDialog);
    // Initial visibility from current board caps; tracked thereafter via
    // onConnectionStateChanged (where hasPureSignal is also gated on the
    // PSA bottom-banner indicator).
    m_pureSignalApplet->setVisible(
        m_radioModel->boardCapabilities().hasPureSignal);

    // ── Die beiden Instrumente (2026-08-17) ──────────────────────────
    //
    // Zwei Stück, weil der Betreiber „eine oder zwei Anzeigen" je
    // Instrument wählen können soll und dafür mehr als eines braucht.
    // Die Vorbelegung ist die naheliegende Paarung beim Senden
    // (Stehwelle) und beim Hören (Signal); geändert wird sie später
    // über den Kopf des Panels.
    //
    // Sie hängen an MeterPoller::readingUpdated, also am SELBEN Umlauf
    // wie die Meter-Items — keine zweite Abfrage, keine zweite Liste.
    // Die Verdrahtung steht weiter unten bei m_meterPoller.
    // Die Frequenz im Zeigerstil (2026-08-17). Sie steht VOR den
    // Messanzeigen in der Spalte: sie ist der Zustand, nach dem man am
    // haeufigsten sieht, und sie muss dastehen, bevor die VFO-Flagge
    // ausgeblendet werden darf.
    m_frequencyApplet = new FrequencyApplet(m_radioModel, nullptr);
    m_frequencyApplet->restoreState();
    panel->addApplet(m_frequencyApplet);

    // ── Die KIWI-Kachel an den richtigen Panadapter ──────────────────
    //
    // Die Kachel meldet nur, DASS sie gedrueckt wurde, und nennt ihre
    // Scheibe. Welcher Panadapter dazugehoert, weiss allein das
    // MainWindow — und spectrumForSlice ist der eine Ort, an dem diese
    // Zuordnung steht. Haette das Applet sie selbst nachgebildet,
    // gaebe es zwei Antworten auf dieselbe Frage.
    connect(m_frequencyApplet, &FrequencyApplet::kiwiDisplayToggleRequested,
            this, [this](SliceModel* slice) {
        if (!slice) { return; }
        SpectrumWidget* sw = spectrumForSlice(slice);
        if (!sw) { return; }
        sw->setKiwiDisplaySource(!sw->kiwiDisplaySource());
        if (m_frequencyApplet) {
            m_frequencyApplet->setKiwiState(sw->kiwiDisplaySource());
        }
    });

    m_swrInstrument = new InstrumentApplet(QStringLiteral("SwrInstrument"),
                                           QStringLiteral("Stehwelle"),
                                           m_radioModel, nullptr);
    m_swrInstrument->setPrimary(MeterBinding::TxSwr);
    m_swrInstrument->restoreState();
    panel->addApplet(m_swrInstrument);

    m_signalInstrument = new InstrumentApplet(QStringLiteral("SignalInstrument"),
                                              QStringLiteral("S-Meter"),
                                              m_radioModel, nullptr);
    m_signalInstrument->setPrimary(MeterBinding::SignalAvg);
    // NACH setPrimary: restoreState stellt Form und Balkenart wieder
    // her, nicht die Quelle — die gehoert dem Aufrufer. Siehe die
    // Notiz an InstrumentApplet::restoreState.
    m_signalInstrument->restoreState();
    panel->addApplet(m_signalInstrument);

    // ── Frequenz zusaetzlich in Stehwelle/S-Meter (2026-09-02) ───────
    //
    // Betreiber: "die Idee ist, dass man sich bei kleinen Bildschirmen
    // vielleicht ein Fenster erspart" -- aus dem Rechtsklickmenue der
    // beiden Instrumente an-/abschaltbar (InstrumentApplet::
    // setShowFrequency); hier nur die Werteversorgung. Gleiches Muster
    // wie FreeDVReporterDialog::setActiveFrequency weiter unten: Wert
    // sofort setzen + auf jedes frequencyChanged der aktiven Scheibe
    // hoeren.
    //
    // Bench-gefunden 2026-09-03 ("die Frequenzanzeige... da steht nur
    // 0"): ein frisches MainWindow hat noch KEINE Scheibe -- die
    // entsteht erst, wenn ein Funkgeraet verbindet (derselbe Fund wie
    // in MainWindow_SunSdr.cpp's eigenem Kopf-Kommentar). Die einmalige
    // Verdrahtung hier lief also beim Start ins Leere und wurde nie
    // wiederholt. wireInstrumentFrequency() macht dasselbe wie vorher,
    // laeuft aber jetzt bei jedem RadioModel::activeSliceChanged erneut
    // -- und loest zuerst die alte Verbindung, falls eine vorherige
    // Scheibe (z.B. nach einem Radiowechsel) noch dranhing.
    auto wireInstrumentFrequency = [this]() {
        disconnect(m_instrumentFreqConn);
        SliceModel* slice = m_radioModel ? m_radioModel->activeSlice() : nullptr;
        if (!slice) { return; }
        if (m_swrInstrument)    { m_swrInstrument->setFrequencyHz(slice->frequency()); }
        if (m_signalInstrument) { m_signalInstrument->setFrequencyHz(slice->frequency()); }
        m_instrumentFreqConn = connect(slice, &SliceModel::frequencyChanged, this,
                                       [this](double hz) {
            if (m_swrInstrument)    { m_swrInstrument->setFrequencyHz(hz); }
            if (m_signalInstrument) { m_signalInstrument->setFrequencyHz(hz); }
        });
    };
    wireInstrumentFrequency();
    connect(m_radioModel, &RadioModel::activeSliceChanged, this, wireInstrumentFrequency);

    // ── SWR / Leistung in EINER Flaeche — entfernt ───────────────────
    //
    // Stand hier von 2026-08-23 bis 2026-08-30 als eigenes Applet
    // (TxMeterApplet, "ein widget, wo SWR und Stehwelle in einem
    // Diagramm sind"). Der Betreiber am 2026-08-30, vor demselben
    // Fenster wieder: "SWR / Leistungs Fenster soll es nur zusaetzlich
    // im Bereich des Frequenzfenster geben, nicht alle." Die beiden
    // Zusatzzeilen im Frequenz-Widget (FrequencyApplet::setShowPower/
    // setShowSwr, seit 2026-08-23) decken dieselbe Anzeige schon ab —
    // ein eigenes drittes Fenster dafuer war die Dopplung, die der
    // Betreiber loswerden wollte.
    //
    // ── Spracherkennung (2026-08-23) ────────────────────────────────
    //
    // Der Betreiber hat den OERTLICHEN Weg gewaehlt: ein
    // Whisper-Dienst auf seinem eigenen Rechner. Nichts davon laeuft
    // von selbst an — Adresse und Sprache stehen in den Einstellungen,
    // und ohne laufenden Dienst bleibt der Haken schlicht wirkungslos.
    // Genau darum ist die Rueckmeldung im Applet so ausfuehrlich.
    m_asrApplet = new AsrApplet(m_radioModel, nullptr);
    panel->addApplet(m_asrApplet);
    connect(m_asrApplet, &AsrApplet::enableRequested,
            this, &MainWindow::setAsrEnabled);

    // Phase 23: TCI applets — live in Container #0 below the existing applets.
    // Visibility is now managed by AppletVisibilityController below
    // (registered as ids "Tci" + "ClientChain", keys AppletTciVisible +
    // AppletClientChainVisible). Legacy keys TciApplet_Visible /
    // ClientChainApplet_Visible from earlier versions become orphans on
    // upgrade; existing users get TCI applets back to default-visible.
#ifdef HAVE_WEBSOCKETS
    if (m_tciServer) {
        m_tciApplet = new TciApplet(m_tciServer, nullptr);
        panel->addApplet(m_tciApplet);
        connect(m_tciApplet, &TciApplet::setupRequested,
                this, &MainWindow::openTciSetupPage);
        // showClientsRequested: scroll/show the ClientChainApplet.
        // ClientChainApplet is constructed immediately below, so capture
        // by pointer — the lambda runs only after full construction.
        connect(m_tciApplet, &TciApplet::showClientsRequested,
                this, [this]() {
                    if (m_clientChainApplet && m_appletVis) {
                        m_appletVis->setVisible(
                            QStringLiteral("ClientChain"), true);
                        m_clientChainApplet->raise();
                    }
                });

        m_clientChainApplet = new ClientChainApplet(m_tciServer, nullptr);
        panel->addApplet(m_clientChainApplet);
    }
#endif

    // Phase 3P-II Task 20: AmpApplet (PGXL telemetry + OPERATE toggle).
    // Added to the panel alongside the other applets. Signal routing to
    // PgxlConnection is wired in onConnectionStateChanged() so every
    // radio-connect gets a fresh binding without double-connects.
    m_ampApplet = new AmpApplet(m_radioModel, nullptr);
    panel->addApplet(m_ampApplet);

    // Phase 3P-II Task 20: TunerApplet (TGXL controls + relay bars).
    // Was previously commented out ("TODO ATU phase"). Now constructed
    // with the TunerModel* owned by RadioModel so it tracks TGXL state
    // from construction time.
    // Phase 3P-II Phase 4 Task 89: pass RadioModel's shared TuneMemoryStore
    // so saves from the context menu are visible in TgxlAdvancedPage and vice versa.
    m_tunerApplet = new TunerApplet(m_radioModel,
                                    m_radioModel->tunerModel(),
                                    nullptr,
                                    m_radioModel->tuneMemoryStore());
    panel->addApplet(m_tunerApplet);

    // 2026-05-20 bench fix: rescale TunerApplet's fwd-power bar when
    // PGXL comes into the chain. TunerApplet defaults to 0-200 W
    // (barefoot) which pegs out the moment PGXL pushes its amplified
    // ~450-2000 W through the tuner. Mirror the same amplifierChanged
    // + ampStateChanged wires we use for the TxApplet above.
    if (m_tunerApplet) {
        // Initial scale: match current PGXL state at construction time.
        const bool amplifyingNow =
            m_radioModel->hasAmplifier() && m_radioModel->ampOperate();
        m_tunerApplet->setPowerScale(/*maxWatts=*/0, amplifyingNow);

        // PGXL connect/disconnect snaps the scale to 2 kW or back to
        // barefoot. maxWatts=0 means "use the standard barefoot/PGXL
        // range from TunerApplet::setPowerScale defaults".
        connect(m_radioModel, &RadioModel::amplifierChanged, this,
                [this](bool present) {
            if (m_tunerApplet) {
                m_tunerApplet->setPowerScale(/*maxWatts=*/0, present);
            }
        });

        // OPERATE/STANDBY edges re-evaluate scale. STANDBY -> barefoot
        // until OPERATE resumes (pass amplifying=false to drop the
        // 2 kW scale back to 200 W).
        connect(m_radioModel, &RadioModel::ampStateChanged, this,
                [this]() {
            if (m_tunerApplet) {
                const bool amplifying = m_radioModel->hasAmplifier()
                                        && m_radioModel->ampOperate();
                m_tunerApplet->setPowerScale(/*maxWatts=*/0, amplifying);
            }
        });
    }

    // Phase 3P-III Task 14: RF-Kit RF2K-S applet.
    // Constructed unconditionally alongside the other peripheral applets.
    // Visibility is gated on rfKitEnabled() via the AppletVisibilityController
    // availability axis (set below, live-updated via rfKitEnabledChanged).
    //
    // Data-flow and context-menu signals are wired once here in buildUI()
    // because rfKitConnection() returns the same permanent object for the
    // app lifetime (RadioModel creates it in its ctor, never destroys it).
    // This differs from the AmpApplet/TunerApplet pattern (wired in
    // onConnectionStateChanged) because PGXL/TGXL use PGX-specific
    // auto-connect logic gated on fourO3AEnabled; the RF-Kit connects
    // independently at startup when rfKitEnabled is true.
    m_rfKitApplet = new Rf2ksApplet(m_radioModel, nullptr);
    panel->addApplet(m_rfKitApplet);

    {
        // Connection -> applet data flow.
        Rf2ksConnection* rfKitConn = m_radioModel->rfKitConnection();
        if (rfKitConn) {
            connect(rfKitConn, &Rf2ksConnection::powerUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setPower);
            connect(rfKitConn, &Rf2ksConnection::tunerUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setTuner);
            connect(rfKitConn, &Rf2ksConnection::antennasUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setAntennas);
            connect(rfKitConn, &Rf2ksConnection::activeAntennaUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setActiveAntenna);
            connect(rfKitConn, &Rf2ksConnection::operateModeUpdated,
                    m_rfKitApplet, &Rf2ksApplet::setOperateMode);
            connect(rfKitConn, &Rf2ksConnection::connected,
                    this, [this]() {
                if (m_rfKitApplet) {
                    m_rfKitApplet->setConnectedState(true);
                }
            });
            connect(rfKitConn, &Rf2ksConnection::disconnected,
                    this, [this]() {
                if (m_rfKitApplet) {
                    m_rfKitApplet->setConnectedState(false);
                }
            });
            connect(rfKitConn, &Rf2ksConnection::infoUpdated,
                    this, [this](const QString& /*deviceName*/,
                                 const QString& softwareVersion,
                                 const QString& nicknameFromAmp) {
                if (m_rfKitApplet) {
                    m_rfKitApplet->setNicknameAndVersion(nicknameFromAmp, softwareVersion);
                }
            });

            // Applet -> connection (antenna click, operate toggle).
            connect(m_rfKitApplet, &Rf2ksApplet::antennaRequested,
                    rfKitConn, &Rf2ksConnection::setActiveAntenna);
            connect(m_rfKitApplet, &Rf2ksApplet::operateToggled,
                    this, [this](bool wantOperate) {
                Rf2ksConnection* conn = m_radioModel->rfKitConnection();
                if (!conn) { return; }
                conn->setOperateMode(wantOperate
                    ? QStringLiteral("OPERATE")
                    : QStringLiteral("STANDBY"));
            });
        }

        // Context menu signals (always wired regardless of connection state).
        connect(m_rfKitApplet, &Rf2ksApplet::navigationRequested,
                this, &MainWindow::openSetup);

        connect(m_rfKitApplet, &Rf2ksApplet::connectionToggleRequested,
                this, [this]() {
            Rf2ksConnection* conn = m_radioModel->rfKitConnection();
            if (!conn) { return; }
            if (conn->isConnected()) {
                conn->disconnect();
            } else {
                // Per-radio peripherals refactor (2026-05-26): host/port
                // live under hardware/<mac>/peripherals/.  When no radio
                // is connected, peripheralValue() returns the default and
                // the empty-host gate below makes this a safe no-op.
                const QString host = m_radioModel->peripheralValue(
                    QStringLiteral("RfKit_ManualIp"));
                const quint16 port = static_cast<quint16>(
                    m_radioModel->peripheralValue(
                        QStringLiteral("RfKit_ManualPort"),
                        QStringLiteral("8080")).toUInt());
                if (!host.isEmpty()) {
                    conn->connectToAmp(host, port);
                }
            }
        });

        connect(m_rfKitApplet, &Rf2ksApplet::diagnosticsCopyRequested,
                this, [this]() {
            Rf2ksConnection* conn = m_radioModel->rfKitConnection();
            QString diag;
            diag += QStringLiteral("RF-Kit RF2K-S diagnostics\n");
            if (conn) {
                diag += QStringLiteral("Host: %1:%2\n")
                            .arg(conn->peerAddress()).arg(conn->peerPort());
                diag += QStringLiteral("Version: %1\n")
                            .arg(conn->softwareVersion());
                diag += QStringLiteral("Polls OK/failed: %1/%2\n")
                            .arg(conn->pollsSucceeded())
                            .arg(conn->pollsFailed());
                diag += QStringLiteral("RTT avg: %1 ms\n")
                            .arg(conn->rttAvgLast10Ms());
            } else {
                diag += QStringLiteral("(connection unavailable)\n");
            }
            QGuiApplication::clipboard()->setText(diag);
        });
    }

    // Antenna labels: load operator-set labels from AppSettings at startup.
    // Keys: RfKit_Ant1_Label .. RfKit_Ant4_Label (stored by RfKitPage.cpp).
    // If a key is absent or empty the applet already shows "ANT N" by default.
    for (int i = 1; i <= 4; ++i) {
        const QString label = AppSettings::instance()
            .value(QStringLiteral("RfKit_Ant%1_Label").arg(i)).toString();
        if (!label.isEmpty()) {
            m_rfKitApplet->setAntennaLabel(i, label);
        }
    }

    // ── Applet visibility controller (Containers > Applets + ☰ menus) ──
    // NereusSDR-original. Backs the show/hide menu surfaces.
    //
    // Registered applets get a checkable menu entry in Containers > Applets
    // AND in the right-side panel's ☰ banner menu. Add new entries here as
    // additional applets ship.
    //
    // RadeApplet caveat: its visibility is mode-driven (auto-shown when
    // the active slice is in DSPMode::RADE_U/_L; hidden otherwise) via
    // the dspModeChanged lambda further down. The menu toggle here is a
    // user override that lasts until the next mode change repopulates
    // visibility. Acceptable for v1; tighter integration is a follow-up.
    // ── „LEER BEIM START" GAB ES SCHON, NUR WOANDERS ─────────────────
    //
    // Am 2026-08-20 habe ich hier alle 17 Fenster auf „unsichtbar"
    // vorgegeben, weil der Betreiber sagte, beim Start solle alles leer
    // sein. FALSCH, und am selben Tag zurueckgenommen.
    //
    // Das Bildschirmvideo zeigt, was er meint: in Zeus Link legt das
    // Plus in der linken Leiste ein NEUES LAYOUT an, und DAS ist leer —
    // das vorhandene behaelt seine Fenster. Genau das gibt es bei uns
    // seit dem 2026-08-15, auf seine eigene Ansage hin, im
    // newProfileRequested-Empfaenger (MainWindow.cpp, „Leer, nicht als
    // Kopie").
    //
    // Eine globale Vorgabe waere etwas anderes gewesen: sie haette
    // jede frische Installation mit einer leeren Flaeche begruesst,
    // auch die von jemandem, der nie ein Layout anlegt. Zeus tut das
    // nicht — dort steht beim ersten Start das Layout „D" mit Fenstern
    // darin.
    //
    // Was bleibt: „Alle Fenster ausblenden" im Containers-Menue, fuer
    // den, der EINMAL aufraeumen will.
    // Die freie Flaeche ueberlebt den Neustart. Vor dem Menue gelesen,
    // damit der Haken gleich richtig steht.
    m_freeCanvasMode = AppSettings::instance()
        .value(QStringLiteral("FreeCanvasMode"), QStringLiteral("False"))
        .toString() == QStringLiteral("True");

    m_appletVis = new AppletVisibilityController(this);

    m_appletsById[QStringLiteral("Rx")]         = m_rxApplet;
    m_appletsById[QStringLiteral("Tx")]         = m_txApplet;
    m_appletsById[QStringLiteral("PhoneCw")]    = m_phoneCwApplet;
    m_appletsById[QStringLiteral("Rade")]       = m_radeApplet;
    m_appletsById[QStringLiteral("Vax")]        = m_vaxApplet;
    m_appletsById[QStringLiteral("Dvk")]        = m_dvkApplet;
    m_appletsById[QStringLiteral("QsoRec")]     = m_qsoRecorderApplet;
    m_appletsById[QStringLiteral("BwFilter")]   = m_bwFilterApplet;
    m_appletsById[QStringLiteral("PureSignal")] = m_pureSignalApplet;
    m_appletsById[QStringLiteral("Amp")]        = m_ampApplet;
    m_appletsById[QStringLiteral("Tuner")]      = m_tunerApplet;
    m_appletsById[QStringLiteral("RfKit")]      = m_rfKitApplet;
    m_appletsById[QStringLiteral("Frequency")]        = m_frequencyApplet;
    m_appletsById[QStringLiteral("SwrInstrument")]    = m_swrInstrument;
    m_appletsById[QStringLiteral("SignalInstrument")] = m_signalInstrument;

    // ── Die vier vom 2026-08-23 ─────────────────────────────────────
    //
    // Sie waren gebaut, ins Panel gehaengt und trotzdem UNSICHTBAR:
    // ohne Eintrag hier kennt sie der Auswaehler nicht, und ohne
    // registerApplet weiter unten steht kein Punkt im Menue. Der
    // Betreiber fragte am selben Tag: "wo ist die applet mitschrift".
    //
    // Dieselbe Art Fehler wie beim KiwiSDR, der vollstaendig gebaut
    // und vollstaendig unerreichbar war: gebaut und nicht erreichbar
    // ist so gut wie nicht gebaut, und keine Einzelpruefung sieht es,
    // weil jeder Baustein fuer sich in Ordnung ist.
    m_appletsById[QStringLiteral("KiwiSdr")] = m_kiwiSdrApplet;
    m_appletsById[QStringLiteral("KiwiWaterfalls")] = m_kiwiWaterfallPanel;
    m_appletsById[QStringLiteral("Asr")]     = m_asrApplet;
#ifdef HAVE_WEBSOCKETS
    if (m_tciApplet) {
        m_appletsById[QStringLiteral("Tci")]        = m_tciApplet;
    }
    if (m_clientChainApplet) {
        m_appletsById[QStringLiteral("ClientChain")] = m_clientChainApplet;
    }
#endif

    // Display names match each applet's appletTitle() — keep in sync if
    // an applet's title changes.
    //
    // PureSignal defaults to visible. The existing onConnectionStateChanged
    // handler still calls m_pureSignalApplet->setVisible(caps.hasPureSignal)
    // on radio connect, which can hide the inner widget on HL2/Atlas; the
    // menu entry stays available for users who want to force-show or
    // permanently hide it. Defaulting to true here means new G2 users see
    // PS immediately without having to discover the menu toggle.
    m_appletVis->registerApplet(QStringLiteral("Rx"),
                                QStringLiteral("RX"),           true);
    m_appletVis->registerApplet(QStringLiteral("Tx"),
                                QStringLiteral("TX"),           true);
    m_appletVis->registerApplet(QStringLiteral("PhoneCw"),
                                QStringLiteral("Phone / CW"),   true);
    // RADE: defaultVisible=true (user pref). Actual visibility is gated
    // on the active slice's mode via the availability axis — the
    // dspModeChanged lambda below calls setAvailable(true) only when
    // mode is DSPMode::RADE_U/_L. Initial availability set to false here
    // since the default startup mode is USB; the mode lambda fires
    // shortly after to correct it if needed.
    m_appletVis->registerApplet(QStringLiteral("Rade"),
                                QStringLiteral("RADE"),         true);
    m_appletVis->registerApplet(QStringLiteral("Vax"),
                                QStringLiteral("VAX"),          true);
    // Sprachspeicher (2026-08-19). Sichtbar ab Werk: er ist auch ohne
    // Funkgeraet brauchbar (Ansagen laden, benennen, Tasten zuordnen),
    // und ein Merkmal, das man erst in einem Menue suchen muss, findet
    // niemand.
    m_appletVis->registerApplet(QStringLiteral("Dvk"),
                                QStringLiteral("Voice Keyer"),  true);
    m_appletVis->registerApplet(QStringLiteral("QsoRec"),
                                QStringLiteral("QSO Recorder"), true);
    m_appletVis->registerApplet(QStringLiteral("BwFilter"),
                                QStringLiteral("Bandwidth Filter"), true);
    m_appletVis->registerApplet(QStringLiteral("PureSignal"),
                                QStringLiteral("PureSignal"),   true);
    m_appletVis->registerApplet(QStringLiteral("Amp"),
                                QStringLiteral("Power Genius"), true);
    m_appletVis->registerApplet(QStringLiteral("Tuner"),
                                QStringLiteral("Tuner Genius"), true);
    m_appletVis->registerApplet(QStringLiteral("RfKit"),
                                QStringLiteral("RF-Kit RF2K-S"), true);
    // Die beiden Instrumente. defaultVisible=true, damit sie beim
    // ersten Start dastehen und angesehen werden können — das ist der
    // Zweck dieses Schritts. Wer sie nicht will, blendet sie über das
    // Plus aus wie jedes andere Widget.
    m_appletVis->registerApplet(QStringLiteral("Frequency"),
                                QStringLiteral("Frequenz"),     true);
    m_appletVis->registerApplet(QStringLiteral("SwrInstrument"),
                                QStringLiteral("Stehwelle"),    true);
    m_appletVis->registerApplet(QStringLiteral("SignalInstrument"),
                                QStringLiteral("S-Meter"),      true);
    m_appletVis->registerApplet(QStringLiteral("KiwiSdr"),
                                QStringLiteral("KiwiSDR"),      true);
    // defaultVisible=false, bewusst anders als KiwiSdr: ein leeres Panel
    // ("kein Wasserfall eingeschaltet") fuer alle, die keine Vorschau
    // nutzen, waere Ballast. Ueber das "+" wie jedes andere Widget
    // einblendbar, sobald man will.
    m_appletVis->registerApplet(QStringLiteral("KiwiWaterfalls"),
                                QStringLiteral("KIWI-WASSERFÄLLE"), false);
    m_appletVis->registerApplet(QStringLiteral("Asr"),
                                QStringLiteral("Mitschrift"),   true);

    // ── Kategorie und Schlagwoerter ──────────────────────────────────
    //
    // Fuer die Spalte links und das Suchfeld im „Widget hinzufuegen"-
    // Dialog. Die Schlagwoerter sind das, was die Suche brauchbar macht:
    // wer „swr" tippt, soll die Messanzeigen finden, ohne ihren Namen zu
    // kennen. Also drin steht, wonach jemand SUCHEN wuerde, nicht was
    // der Titel ohnehin schon sagt.
    //
    // ── Feinere Aufteilung (2026-08-20) ──────────────────────────────
    //
    // Der Betreiber: „bei der auswahl der widget sollte es mehr
    // aufteilung geben, um mehr auswaehlbar sein um individueller
    // auswaehlen zu koennen."
    //
    // Vorher lagen 17 Applets in 8 Toepfen, und die grossen waren die
    // unbrauchbaren: unter „Empfang" standen Frequenz, RX, S-Meter und
    // Bandfilter nebeneinander — vier Dinge, die man zu verschiedenen
    // Zeiten sucht. Unter „Audio" lagen VAX, Sprachspeicher und
    // Aufnahme, die ausser dem Wort Audio nichts teilen.
    //
    // Jetzt 12 Toepfe, geschnitten nach dem, WOFUER man etwas sucht,
    // nicht nach dem Baustein:
    //
    //   Abstimmen        Frequenz
    //   Empfang          RX
    //   Senden           TX
    //   Betriebsart      Phone/CW
    //   Messanzeigen     S-Meter, Stehwelle
    //   Filter           Bandfilter
    //   Sprachspeicher   DVK
    //   Aufnahme         QSO-Recorder
    //   Audio            VAX
    //   Digital          RADE
    //   Linearisierung   PureSignal
    //   Endstufen        Verstaerker, RF-Kit
    //   Tuner            Tuner
    //   Netzwerk         TCI, Client-Kette
    //
    // Die Schlagwoerter bleiben unangetastet: wer „swr" tippt, findet
    // die Stehwelle weiterhin, egal in welchem Topf sie steht.
    //
    // Die Reihenfolge der Kategorien ergibt sich aus der Anmeldung
    // oben, nicht aus dem Alphabet — siehe categories().
    // Die beiden Instrumente. Ohne describeApplet landen sie im
    // Auswähler unter „Sonstiges" — sichtbar, aber nicht dort, wo
    // jemand sie sucht. Die Schlagwörter sind das, wonach man tippt:
    // wer „swr" oder „zeiger" eingibt, soll sie finden, ohne den
    // Applet-Namen zu kennen.
    m_appletVis->describeApplet(QStringLiteral("Frequency"),
        QStringLiteral("Abstimmen"),
        {QStringLiteral("frequenz"), QStringLiteral("vfo"),
         QStringLiteral("qrg"), QStringLiteral("band"),
         QStringLiteral("abstimmen"), QStringLiteral("split"),
         QStringLiteral("mhz")});
    m_appletVis->describeApplet(QStringLiteral("SwrInstrument"),
        QStringLiteral("Messanzeigen"),
        {QStringLiteral("swr"), QStringLiteral("stehwelle"),
         QStringLiteral("anpassung"), QStringLiteral("zeiger"),
         QStringLiteral("instrument"), QStringLiteral("balken")});
    m_appletVis->describeApplet(QStringLiteral("SignalInstrument"),
        QStringLiteral("Messanzeigen"),
        {QStringLiteral("s-meter"), QStringLiteral("smeter"),
         QStringLiteral("signal"), QStringLiteral("pegel"),
         QStringLiteral("zeiger"), QStringLiteral("instrument"),
         QStringLiteral("balken")});

    m_appletVis->describeApplet(QStringLiteral("Rx"),
        QStringLiteral("Empfang"),
        {QStringLiteral("rx"), QStringLiteral("empfang"),
         QStringLiteral("filter"), QStringLiteral("agc"),
         QStringLiteral("squelch"), QStringLiteral("daempfung")});
    m_appletVis->describeApplet(QStringLiteral("Tx"),
        QStringLiteral("Senden"),
        {QStringLiteral("tx"), QStringLiteral("senden"),
         QStringLiteral("leistung"), QStringLiteral("mox"),
         QStringLiteral("vox"), QStringLiteral("tune"),
         QStringLiteral("swr"), QStringLiteral("mikrofon")});
    m_appletVis->describeApplet(QStringLiteral("PhoneCw"),
        QStringLiteral("Betriebsart"),
        {QStringLiteral("cw"), QStringLiteral("morse"),
         QStringLiteral("keyer"), QStringLiteral("phone"),
         QStringLiteral("wpm"), QStringLiteral("paddle")});
    m_appletVis->describeApplet(QStringLiteral("Rade"),
        QStringLiteral("Digital"),
        {QStringLiteral("rade"), QStringLiteral("freedv"),
         QStringLiteral("digital"), QStringLiteral("codec"),
         QStringLiteral("sprache")});
    m_appletVis->describeApplet(QStringLiteral("Vax"),
        QStringLiteral("Audio"),
        {QStringLiteral("vax"), QStringLiteral("audio"),
         QStringLiteral("routing"), QStringLiteral("kanal")});
    // Sprachspeicher und QSO-Aufnahme. Der Sprachspeicher hatte bisher
    // keine Beschreibung und landete in „Sonstiges" — meine eigene
    // Auslassung von heute frueh.
    m_appletVis->describeApplet(QStringLiteral("Dvk"),
        QStringLiteral("Sprachspeicher"),
        {QStringLiteral("dvk"), QStringLiteral("sprachspeicher"),
         QStringLiteral("ansage"), QStringLiteral("cq"),
         QStringLiteral("voice keyer"), QStringLiteral("wav")});
    m_appletVis->describeApplet(QStringLiteral("QsoRec"),
        QStringLiteral("Aufnahme"),
        {QStringLiteral("aufnahme"), QStringLiteral("recorder"),
         QStringLiteral("qso"), QStringLiteral("mitschnitt"),
         QStringLiteral("wav"), QStringLiteral("stereo")});
    m_appletVis->describeApplet(QStringLiteral("BwFilter"),
        QStringLiteral("Filter"),
        {QStringLiteral("filter"), QStringLiteral("bandbreite"),
         QStringLiteral("durchlass"), QStringLiteral("passband"),
         QStringLiteral("low cut"), QStringLiteral("high cut"),
         QStringLiteral("shift")});
    m_appletVis->describeApplet(QStringLiteral("PureSignal"),
        QStringLiteral("Linearisierung"),
        {QStringLiteral("puresignal"), QStringLiteral("ps"),
         QStringLiteral("linearisierung"), QStringLiteral("vorverzerrung"),
         QStringLiteral("zweiton")});
    m_appletVis->describeApplet(QStringLiteral("Amp"),
        QStringLiteral("Endstufen"),
        {QStringLiteral("verstaerker"), QStringLiteral("endstufe"),
         QStringLiteral("pgxl"), QStringLiteral("power genius"),
         QStringLiteral("4o3a")});
    m_appletVis->describeApplet(QStringLiteral("Tuner"),
        QStringLiteral("Tuner"),
        {QStringLiteral("tuner"), QStringLiteral("antenne"),
         QStringLiteral("anpassung"), QStringLiteral("swr"),
         QStringLiteral("tgxl"), QStringLiteral("4o3a")});
    m_appletVis->describeApplet(QStringLiteral("RfKit"),
        QStringLiteral("Endstufen"),
        {QStringLiteral("rf-kit"), QStringLiteral("rf2k"),
         QStringLiteral("verstaerker"), QStringLiteral("endstufe")});
#ifdef HAVE_WEBSOCKETS
    if (m_tciApplet) {
        m_appletVis->registerApplet(QStringLiteral("Tci"),
                                    QStringLiteral("TCI Server"),  true);
        // Ohne describeApplet landen sie in „Sonstiges" und sind nur
        // ueber ihren Anzeigenamen zu finden — die Suche im Auswaehler
        // greift dann nicht auf „netzwerk" oder „fernsteuerung".
        // 15 der 17 Applets waren beschrieben, diese zwei nicht.
        m_appletVis->describeApplet(QStringLiteral("Tci"),
            QStringLiteral("Netzwerk"),
            {QStringLiteral("tci"), QStringLiteral("server"),
             QStringLiteral("netzwerk"), QStringLiteral("fernsteuerung"),
             QStringLiteral("websocket"), QStringLiteral("expert")});
    }
    if (m_clientChainApplet) {
        m_appletVis->registerApplet(QStringLiteral("ClientChain"),
                                    QStringLiteral("TCI Clients"), true);
        m_appletVis->describeApplet(QStringLiteral("ClientChain"),
            QStringLiteral("Netzwerk"),
            {QStringLiteral("tci"), QStringLiteral("clients"),
             QStringLiteral("netzwerk"), QStringLiteral("verbindungen"),
             QStringLiteral("kette")});
    }
#endif

    // ── Was kein Applet ist und trotzdem ins Plus gehört ─────────────
    //
    // OE5SOS: „…die einzelnen Widget, sodass man jedes auswählen,
    // aktivieren und verschieben kann."
    //
    // Der Auswähler zeigte bisher nur die neun Applets der rechten
    // Spalte. Zwei große Teile des Fensters standen nicht darin: die
    // Knopfleiste über dem Spektrum und die Statuszeile unten. Ein
    // Auswähler, der „alle Widgets" verspricht und zwei davon
    // verschweigt, ist schlimmer als eine ehrliche Teilliste.
    //
    // VERSCHIEBEN geht bei diesen beiden nicht, und das ist kein
    // Versäumnis: die Knopfleiste liegt über dem Panadapter und gehört
    // dorthin, die Statuszeile ist die Statuszeile. Bei Zeus ist es
    // genauso. Ein- und Ausblenden ist hier die ganze Bedienung.
    m_appletVis->registerApplet(QString::fromLatin1(kChromeOverlayId),
                                QStringLiteral("Knopfleiste am Spektrum"),
                                true);
    m_appletVis->describeApplet(QString::fromLatin1(kChromeOverlayId),
        QStringLiteral("Empfang"),
        {QStringLiteral("knoepfe"), QStringLiteral("band"),
         QStringLiteral("antenne"), QStringLiteral("tnf"),
         QStringLiteral("rx"), QStringLiteral("display"),
         QStringLiteral("vax"), QStringLiteral("overlay")});

    m_appletVis->registerApplet(QString::fromLatin1(kChromeStatusId),
                                QStringLiteral("Statuszeile"), true);
    m_appletVis->describeApplet(QString::fromLatin1(kChromeStatusId),
        QStringLiteral("Fenster"),
        {QStringLiteral("status"), QStringLiteral("cat"),
         QStringLiteral("tci"), QStringLiteral("pa"),
         QStringLiteral("cpu"), QStringLiteral("spannung"),
         QStringLiteral("unten")});

    // ── Die eigenen Fenster gehoeren auch in die Auswahl ─────────────
    //
    // Der Betreiber, 2026-08-20: „logbook, rotor, channel strip,
    // antenne usw, sind nicht bei den widgets."
    //
    // Sie standen nicht drin, weil sie keine Applets sind: sie leben in
    // eigenen Fenstern und haengen an Menueeintraegen. Fuer den
    // Auswaehler ist das aber kein Unterschied — er verwaltet ABSICHTEN
    // („zeig mir das"), nicht Widgets. Wo etwas dann auftaucht, ist die
    // Sache von applyAppletVisibility.
    //
    // Vorgabe aus: ein Fenster, das beim ersten Start von selbst
    // aufgeht, ist eine Zumutung. Sie stehen in der Liste und warten.
    struct WindowEntry {
        const char* id;
        const char* label;
        const char* category;
        std::initializer_list<const char*> keywords;
    };
    static const WindowEntry kWindows[] = {
        {"WinLogbook", "Logbuch", "Aufnahme",
         {"logbuch", "log", "qso", "adif", "kontakte", "eintraege"}},
        {"WinRotorLog", "Rotor / Log", "Abstimmen",
         {"rotor", "antenne", "richtung", "peilung", "drehen", "azimut",
          "grad", "kompass"}},
        {"WinChannelStrip", "Kanalzug", "Senden",
         {"kanalzug", "strip", "channel", "eq", "kompressor", "leveler",
          "mikrofon", "sprache"}},
        {"WinAntenna", "Antenne", "Tuner",
         {"antenne", "swr", "stehwelle", "sweep", "wobbeln", "analyzer",
          "resonanz", "laenge"}},
        {"WinSpotHub", "Spot-Zentrale", "Netzwerk",
         {"spots", "cluster", "dx", "rbn", "wsjt", "pota", "meldungen"}},
    };
    for (const WindowEntry& w : kWindows) {
        const QString id = QString::fromLatin1(w.id);
        m_appletVis->registerApplet(id, QString::fromLatin1(w.label),
                                    /*defaultVisible=*/false);
        QStringList kw;
        for (const char* k : w.keywords) { kw << QString::fromLatin1(k); }
        m_appletVis->describeApplet(id, QString::fromLatin1(w.category), kw);
    }

    // ── DAS PLUS ─────────────────────────────────────────────────────
    //
    // Auf Ansage des Betreibers (2026-08-20): „danach soll ich mit plus
    // jedes windows adden können und verschieben" — wie bei Zeus Link.
    //
    // AddWidgetButton und der Auswähler dahinter (WidgetPicker, 433
    // Zeilen mit Kategorien, Suchfeld und Karten, gebaut am 2026-08-15
    // nach genau dieser Vorlage) waren FERTIG und wurden nirgends
    // erzeugt. MainWindow.cpp hatte den include und sonst nichts. Der
    // sechste Fall von „gebaut, an keiner Fläche" innerhalb von zwei
    // Tagen — und der Grund, warum der Betreiber dreimal sagen musste,
    // dass etwas nicht geht, das es längst gab.
    //
    // Schwebend unten rechts über der Arbeitsfläche, wie in der
    // Vorlage. Nicht in eine Leiste gesteckt: dort wäre es eines von
    // zwanzig Zeichen, und genau so war es bisher unsichtbar.
    m_addWidgetBtn = new AddWidgetButton(m_appletVis, centralWidget());
    m_addWidgetBtn->setToolTip(QStringLiteral(
        "Fenster hinzufügen oder entfernen"));
    m_addWidgetBtn->raise();
    m_addWidgetBtn->show();
    positionAddWidgetButton();

    // Capability gates: applets that depend on an external feature flag
    // get their availability set here. When availability is false, the
    // applet is hidden AND its menu entries are greyed out. The user's
    // persisted visibility preference is preserved across availability
    // changes (so re-enabling 4O3A pops the applet back if the user
    // wanted it visible).
    const bool fourO3AOn = m_radioModel && m_radioModel->fourO3AEnabled();
    m_appletVis->setAvailable(QStringLiteral("Amp"),   fourO3AOn);
    m_appletVis->setAvailable(QStringLiteral("Tuner"), fourO3AOn);
    // RADE: available only in RADE_U / RADE_L modes. Startup mode is
    // USB, so initial availability=false. The dspModeChanged lambda
    // below updates this on every mode change.
    m_appletVis->setAvailable(QStringLiteral("Rade"),  false);

    // RF-Kit RF2K-S: available only when the master toggle is enabled.
    // Default OFF; live-updated via rfKitEnabledChanged below.
    const bool rfKitOn = m_radioModel && m_radioModel->rfKitEnabled();
    m_appletVis->setAvailable(QStringLiteral("RfKit"), rfKitOn);

    // ── Die Reihenfolge im Stapel ────────────────────────────────────
    //
    // OE5SOS: „…sodass man jedes auswählen, aktivieren und verschieben
    // kann." Das Verschieben steckt in AppletPanelWidget; was hier
    // dazukommt, ist das Merken. Ein Fenster, das man eine Stunde lang
    // eingerichtet hat und das beim nächsten Start wieder in
    // Anmeldereihenfolge dasteht, ist kein einrichtbares Fenster.
    //
    // Gespeichert werden Kennungen, nicht Positionen: ein Update, das
    // ein Widget hinzufügt oder eines wegnimmt, verschiebt sonst alles
    // dahinter um eins.
    //
    // ── Zwei Speicher für dasselbe, mit Absicht ──────────────────────
    //
    // Die Reihenfolge steht ab jetzt AUCH in jedem Layout-Profil, und
    // das Profil gewinnt: es wird weiter unten geladen und angewandt,
    // nachdem dieser Block gelaufen ist.
    //
    // Dieser Schlüssel hier bleibt trotzdem, und er ist kein Rest zum
    // Wegräumen. Er ist der Startwert: beim ersten Start nach diesem
    // Update gibt es noch kein Profil, das Profil „Standard“ wird aus
    // dem gebaut, was gerade zu sehen ist — und was gerade zu sehen
    // ist, kommt von hier. Ohne ihn stünde jeder, der seine Anordnung
    // vor dem Update gebaut hat, wieder in Anmeldereihenfolge da.
    // Danach führt er nur noch nach, was das zuletzt aktive Profil sagt.
    if (m_appletPanel) {
        static const auto kOrderKey = QStringLiteral("AppletStackOrder");

        const QStringList saved = AppSettings::instance()
                                      .value(kOrderKey, QString{})
                                      .toString()
                                      .split(QLatin1Char(','), Qt::SkipEmptyParts);
        if (!saved.isEmpty()) {
            QList<AppletWidget*> order;
            for (const QString& id : saved) {
                // appletForKey statt m_appletsById: eine Aufnahme von
                // vor dem 2026-08-18 nennt hier Eigenkennungen („rx"),
                // und die fanden in der Panelkarte nichts. Von
                // vierzehn Eintraegen loesten sich vier auf, der Rest
                // fiel still weg — die Spalte stand danach in
                // Anmeldereihenfolge da.
                if (auto* a = appletForKey(id)) { order.append(a); }
            }
            m_appletPanel->setAppletOrder(order);
        }

        connect(m_appletPanel, &AppletPanelWidget::appletsReordered,
                this, [this]() {
            QStringList ids;
            for (AppletWidget* a : m_appletPanel->applets()) {
                if (a) { ids << panelIdFor(a); }
            }
            AppSettings::instance().setValue(
                QStringLiteral("AppletStackOrder"), ids.join(QLatin1Char(',')));
        });

        // Beide Wege hinaus enden hier: der Zug über die seitliche
        // Schwelle und der Menüpunkt „Als Fenster ablösen".
        // ── Der Pfeil legt auf die FLAECHE, nicht in ein eigenes
        //    Betriebssystem-Fenster ─────────────────────────────────
        //
        // detachApplet() macht ein Fenster daraus. Das ist zwar auch
        // beweglich, aber es ist nicht, was der Betreiber meint: „es
        // muss alles auf den mm verschoben werden koennen. jedes
        // window! jeder panel, ueberall" — gemeint sind Kacheln INNEN,
        // wie bei Zeus Link, nicht ein Schwarm einzelner Fenster.
        //
        // detachApplet bleibt: es haengt am Rechtsklick-Menue und an
        // der Wiederherstellung gespeicherter Fenster.
        connect(m_appletPanel, &AppletPanelWidget::appletDetachRequested,
                this, [this](AppletWidget* a, int dockIndex) {
            detachApplet(a, dockIndex);
            if (m_layoutProfiles) {
                m_layoutProfiles->captureIntoCurrent();
                m_layoutProfiles->save();
            }
        });

        // Das ✕ im Fensterkopf (2026-08-20). Es blendet aus, es
        // loescht nichts — das Plus unten rechts holt jedes Fenster
        // zurueck, und der Tooltip am Knopf sagt das auch.
        connect(m_appletPanel, &AppletPanelWidget::appletHideRequested,
                this, [this](AppletWidget* a) {
            if (!a || !m_appletVis) { return; }
            const QString id = panelIdFor(a);
            if (id.isEmpty()) { return; }
            m_appletVis->setVisible(id, false);
            if (m_layoutProfiles) {
                m_layoutProfiles->captureIntoCurrent();
                m_layoutProfiles->save();
            }
        });
    }

    // ── Was in einem Profil steht ────────────────────────────────────
    //
    // Drei Dinge: welche Widgets sichtbar sind, in welcher Reihenfolge
    // sie stehen, und wie der Splitter geteilt ist.
    //
    // NICHT dabei: die Fenstergröße (OE5SOS, 2026-08-15 — „baue für
    // große Schirme"; ein Profilwechsel soll Panels tauschen und nicht
    // das Fenster umspringen lassen) und die frei schwebenden
    // Meter-Container. Letztere schreibt ContainerManager direkt in die
    // Einstellungen und legt beim Herstellen NEUE an, statt die
    // vorhandenen zu ersetzen — ein Profilwechsel würde sie
    // verdoppeln. Das braucht captureState()/applyState() im Manager
    // und ist ein eigener Schritt.
    if (m_layoutProfiles && m_appletVis) {
        m_layoutProfiles->setHooks(
            // ── einsammeln ───────────────────────────────────────────
            [this]() -> QVariantMap {
                QVariantMap s;
                QVariantMap vis;
                for (const QString& id : m_appletVis->registeredIds()) {
                    vis.insert(id, m_appletVis->isVisible(id));
                }
                s.insert(QStringLiteral("visible"), vis);

                QStringList order;
                if (m_appletPanel) {
                    for (AppletWidget* a : m_appletPanel->applets()) {
                        if (a) { order << panelIdFor(a); }
                    }
                }
                s.insert(QStringLiteral("order"), order);

                // ── Abgelöste Applets ───────────────────────────────
                //
                // Das PROFIL besitzt die Geometrie, das Fenster meldet
                // sie nur (Entscheidung des Betreibers, 2026-08-16).
                // Eine Kennung, die hier steht, ist abgelöst; eine, die
                // fehlt, steht in der Spalte. Damit braucht es kein
                // eigenes „freistehend ja/nein" — das Vorhandensein IST
                // die Antwort, und zwei Felder, die dasselbe sagen,
                // können auseinanderlaufen.
                //
                // Der Bildschirm kommt mit, sonst stimmt „dorthin
                // zurück" bei zwei baugleichen Monitoren nicht.
                QVariantMap floating;
                for (auto it = m_floatingApplets.constBegin();
                     it != m_floatingApplets.constEnd(); ++it) {
                    AppletFloatingWindow* w = it.value();
                    if (!w) { continue; }
                    const QRect g = w->geometry();
                    QVariantMap one;
                    one.insert(QStringLiteral("x"), g.x());
                    one.insert(QStringLiteral("y"), g.y());
                    one.insert(QStringLiteral("w"), g.width());
                    one.insert(QStringLiteral("h"), g.height());
                    one.insert(QStringLiteral("screen"), screenKeyFor(w));
                    one.insert(QStringLiteral("dockIndex"), w->dockIndex());
                    floating.insert(it.key(), one);
                }
                s.insert(QStringLiteral("floatingApplets"), floating);

                // Die freistehenden Meter-Container, nach derselben
                // Regel. MeterDisplay_<id>_Geometry bleibt bestehen,
                // wird aber nur noch GELESEN — als Rückfall für den
                // ersten Start nach diesem Update, wenn noch kein
                // Profil etwas zu diesem Container sagt. Es abzuschaffen
                // verlöre bestehende Anordnungen; dieselbe Rücksicht wie
                // bei Migration v9.
                if (m_containerManager) {
                    s.insert(QStringLiteral("containerGeometry"),
                             m_containerManager->floatingGeometries());
                }

                // ── Rotor / Log ──────────────────────────────────────
                //
                // Ein QDockWidget, nicht Teil von m_appletVis und nicht
                // Teil von ContainerManager -- vierter Mechanismus, den
                // kein Profil bislang kannte. "WinRotorLog" schaltet nur
                // zwischen angedockt und abgeloest um, nie zwischen
                // sichtbar und unsichtbar; ohne diesen Schluessel blieb
                // Rotor/Log in JEDEM Profil stehen, auch einem
                // absichtlich leeren (Betreiber, 2026-08-27: "hier muss
                // alles raus").
                if (m_rotorDock) {
                    s.insert(QStringLiteral("rotorDockVisible"),
                             m_rotorDock->isVisible());
                }

                // ── Panadapter-Schwebefenster ───────────────────────
                //
                // Betreiber 2026-09-01: "Besonders Panadapter und Rotor
                // machen hier Probleme. [...] jedes Fenster muss
                // individuell sein, speicherbar sein, in seiner Größe
                // veränderbar sein und frei schwebend sein." Beide
                // liefen bis heute ausschliesslich ueber eigene,
                // GLOBALE Schluessel (PanFloating_<id> bzw.
                // RotorFloating/RotorPanelBelow) am Profil VORBEI --
                // ein Desktop-Export (01.json) enthielt sie deshalb
                // nie, und ein Import konnte ausgerechnet die zwei
                // auffaelligsten Fenster prinzipiell nicht herstellen.
                // Dieselbe Vorhandensein-ist-die-Antwort-Regel wie bei
                // floatingApplets oben: ein Eintrag heisst "schwebt",
                // ein fehlender heisst "angedockt". Die globalen
                // Schluessel bleiben bestehen (der Programmstart ohne
                // Profilwechsel liest weiterhin sie) -- das Profil
                // traegt ab jetzt eine eigene, vollstaendige Kopie.
                if (m_panStack) {
                    QVariantMap fpans;
                    for (const QString& panId
                         : m_panStack->panIdsForTesting()) {
                        if (!m_panStack->isPanFloating(panId)) {
                            continue;
                        }
                        auto* pf =
                            m_panStack->floatingWindowForTest(panId);
                        if (!pf) { continue; }
                        const QRect g = pf->geometry();
                        QVariantMap one;
                        one.insert(QStringLiteral("x"), g.x());
                        one.insert(QStringLiteral("y"), g.y());
                        one.insert(QStringLiteral("w"), g.width());
                        one.insert(QStringLiteral("h"), g.height());
                        one.insert(QStringLiteral("screen"),
                                   screenKeyFor(pf));
                        fpans.insert(panId, one);
                    }
                    s.insert(QStringLiteral("floatingPans"), fpans);
                }

                // ── Rotor/Log: FORM und Lage ────────────────────────
                //
                // rotorDockVisible (oben) kennt nur das Dock. Die
                // eigentliche Frage -- schwebt es, liegt es unter dem
                // Panadapter, oder haengt es im Dock -- lebte bislang
                // nur in den globalen RotorFloating/RotorPanelBelow-
                // Schluesseln. Ab jetzt traegt das Profil sie selbst,
                // samt Fenstergeometrie im Schwebe-Fall.
                {
                    QVariantMap rotor;
                    if (m_rotorWindow) {
                        rotor.insert(QStringLiteral("form"),
                                     QStringLiteral("floating"));
                        const QRect g = m_rotorWindow->geometry();
                        rotor.insert(QStringLiteral("x"), g.x());
                        rotor.insert(QStringLiteral("y"), g.y());
                        rotor.insert(QStringLiteral("w"), g.width());
                        rotor.insert(QStringLiteral("h"), g.height());
                    } else if (AppSettings::instance()
                                   .value(QStringLiteral("RotorPanelBelow"),
                                          QStringLiteral("False"))
                                   .toString()
                               == QStringLiteral("True")) {
                        rotor.insert(QStringLiteral("form"),
                                     QStringLiteral("below"));
                        // Review-Fund 2026-09-01: die HOEHE der
                        // Unten-Flaeche ist der aeussere Splitter --
                        // ohne sie kaeme die Form zurueck, die Groesse
                        // aber stets als 2/3-1/3-Vorgabe.
                        if (m_outerSplitter) {
                            QVariantList split;
                            for (int v : m_outerSplitter->sizes()) {
                                split << v;
                            }
                            rotor.insert(QStringLiteral("belowSplit"),
                                         split);
                        }
                    } else {
                        rotor.insert(QStringLiteral("form"),
                                     QStringLiteral("docked"));
                    }
                    s.insert(QStringLiteral("rotor"), rotor);
                }

                if (m_mainSplitter) {
                    QVariantList sizes;
                    for (int v : m_mainSplitter->sizes()) { sizes << v; }
                    s.insert(QStringLiteral("splitter"), sizes);
                }

                // ── Das Weltbild gehoert ins Profil ─────────────────
                //
                // Entscheidung des Betreibers: das PROFIL fuehrt, der
                // AppSettings-Schluessel GlobeWorldImagePath ist nur der
                // Rueckfall fuer "kein Profil". Zwei Besitzer fuer eine
                // Zahl waren an diesem Tag schon zweimal die Ursache,
                // deshalb steht hier ausdruecklich, welcher gilt.
                //
                // Gemeinsam fuer Karte und Globus -- ein Weltbild ist
                // ein Weltbild.
                s.insert(QStringLiteral("worldImage"),
                         WorldTexture::currentPath());

                // ── Das Hauptfenster selbst ─────────────────────────
                //
                // Betreiber 2026-08-30: "wenn ich vollbild habe und nur
                // das profil ändere muss auch vollbild bleiben" und
                // "nach dem import ist wieder nicht full screen". Das
                // widerruft die Ausnahme vom 2026-08-15 ("NICHT dabei:
                // die Fenstergroesse") fuer den Zustand: Vollbild/
                // Maximiert gehoert zum Profil. Die Lage in Pixeln
                // bleibt weiterhin beim globalen MainWindowGeometry-
                // Schluessel -- hier zaehlt nur der MODUS.
                QVariantMap mw;
                mw.insert(QStringLiteral("fullScreen"), m_borderlessFullSize);
                mw.insert(QStringLiteral("maximized"), isMaximized());
                s.insert(QStringLiteral("mainWindow"), mw);
                return s;
            },
            // ── herstellen ───────────────────────────────────────────
            [this](const QVariantMap& s) {
                // Betreiber 2026-09-01, nach einem Haenger/OOM-Verdacht
                // beim Profilwechsel: unbedingte Schritt-Marken, damit
                // ein kuenftiger Haenger im Log exakt zeigt, VOR welchem
                // Schritt die letzte Zeile steht -- ohne das war der
                // gesamte ~450-zeilige Block ein einziger, blinder
                // synchroner Aufruf.
                qWarning() << "[ProfileApply:Step] 1/6 Vollbild";
                // Vollbild MERKEN, bevor irgendein Schritt unten es
                // kippen kann -- am Ende wird es wieder durchgesetzt
                // (siehe der Block am Schluss dieses Lambdas).
                const bool wasFullScreen = m_borderlessFullSize;

                // ── Vollbild bleibt Vollbild ────────────────────────
                //
                // Betreiber 2026-08-30: "wenn ich vollbild habe und nur
                // das profil ändere muss auch vollbild bleiben." Zwei
                // Faelle: das Profil SAGT Vollbild (mainWindow-Karte,
                // seit heute erfasst) -- dann herstellen. Oder das
                // Profil kennt den Schluessel noch nicht (aeltere
                // Aufnahme, Import einer alten Datei) -- dann gilt der
                // Zustand von VOR dem Anwenden, festgehalten oben als
                // wasFullScreen, damit kein Schritt dazwischen ihn
                // stillschweigend kippen kann.
                //
                // Betreiber 2026-09-01: "panadapter zwar verschoben,
                // aber [...] nicht auf vollgröße" -- dieser ganze Block
                // stand bis eben GANZ AM ENDE der Lambda, NACH der
                // Wiederherstellung der schwebenden Fenster weiter
                // unten. ensureOnVisibleScreen() dort prueft, ob die
                // gespeicherte Position das HEUTIGE Hauptfenster
                // ueberlappt (WindowPlacement.cpp) -- und liest dafuer
                // anchor->window()->geometry() in genau dem Moment.
                // Stand der Block spaeter, war das Hauptfenster zu
                // diesem fruehen Zeitpunkt (Import mitten in der
                // Sitzung, Hauptfenster laengst sichtbar, aber noch mit
                // der Geometrie VOR diesem Import) eine STALE Flaeche --
                // enterBorderlessFullSize() setzte die richtige,
                // bildschirmfuellende Geometrie ja erst gleich
                // DANACH. Die Ueberlapp-Pruefung verglich also gegen
                // die falsche, alte Flaeche und verschob das Fenster
                // unnoetig. Hierher vorgezogen: die Hauptfenster-
                // Geometrie steht fest, BEVOR irgendein schwebendes
                // Fenster seine Position gegen sie prueft.
                const QVariantMap mw =
                    s.value(QStringLiteral("mainWindow")).toMap();
                const bool wantFullScreen =
                    mw.contains(QStringLiteral("fullScreen"))
                        ? mw.value(QStringLiteral("fullScreen")).toBool()
                        : wasFullScreen;
                // Diagnose 2026-09-01 (leeres-Fenster-Untersuchung): NUR
                // bei einer Abweichung loest enterBorderlessFullSize()/
                // exitBorderlessFullSize() ihren hide()/setWindowFlag()/
                // show()-Zyklus auf dem GESAMTEN MainWindow aus -- das
                // Log macht sichtbar, ob und wann das bei einem
                // Profilwechsel passiert.
                if (wantFullScreen != wasFullScreen) {
                    qWarning() << "[ProfileFullscreenRestore] wasFullScreen="
                               << wasFullScreen << "wantFullScreen="
                               << wantFullScreen;
                }
                if (wantFullScreen) {
                    enterBorderlessFullSize();
                } else {
                    exitBorderlessFullSize();
                    if (mw.value(QStringLiteral("maximized")).toBool()
                        && !isMaximized()) {
                        showMaximized();
                    }
                }

                // Erst das Weltbild: es loest den Geber aus, und die
                // Ansichten sollen einmal neu zeichnen und nicht
                // zweimal.
                if (s.contains(QStringLiteral("worldImage"))) {
                    const QString img =
                        s.value(QStringLiteral("worldImage")).toString();
                    if (img.isEmpty()) {
                        AppSettings::instance().setValue(
                            WorldTexture::settingsKey(), QString{});
                        WorldTexture::reload();
                    } else if (img != WorldTexture::currentPath()) {
                        // Schlaegt es fehl -- Datei geloescht, Platte
                        // nicht da -- bleibt das bisherige Bild stehen,
                        // statt die Karte zu leeren.
                        WorldTexture::setPath(img);
                    }
                }

                qWarning() << "[ProfileApply:Step] 2/6 Sichtbarkeits-Map";
                const QVariantMap vis =
                    s.value(QStringLiteral("visible")).toMap();
                for (auto it = vis.constBegin(); it != vis.constEnd(); ++it) {
                    // Nur bekannte Kennungen. Eine Aufnahme von vor
                    // einem Update kann Widgets nennen, die es nicht
                    // mehr gibt — der Controller soll sie nicht anlegen.
                    if (m_appletVis->registeredIds().contains(it.key())) {
                        m_appletVis->setVisible(it.key(), it.value().toBool());
                    }
                }

                // ── Abgelöste Applets ───────────────────────────────
                //
                // VOR der Reihenfolge: was abgelöst gehört, muss aus
                // der Spalte heraus sein, bevor die Spalte sortiert
                // wird — sonst sortiert setAppletOrder Widgets, die
                // gleich darauf verschwinden, und die Stellen dahinter
                // rutschen ein zweites Mal.
                //
                // Erst alles andocken, was das neue Profil nicht als
                // abgelöst führt. Das ist die Antwort auf die dritte
                // offene Frage der Übergabe („was passiert beim
                // Profilwechsel mit einem Fenster, das im neuen Profil
                // nicht vorkommt?"): es kehrt in die Spalte zurück,
                // statt herrenlos stehen zu bleiben. Ein Fenster ohne
                // Profil, das es kennt, kann niemand mehr wiederfinden.
                // Auf die Panelkennung bringen, bevor irgendetwas damit
                // verglichen wird: bis zum 2026-08-18 standen hier
                // Eigenkennungen („rx"), und die stimmten mit keinem
                // Schluessel in m_floatingApplets oder m_appletsById
                // ueberein.
                qWarning() << "[ProfileApply:Step] 3/6 Applet-Andocken/Ablösen";
                QVariantMap floating;
                {
                    const QVariantMap raw =
                        s.value(QStringLiteral("floatingApplets")).toMap();
                    for (auto it = raw.constBegin(); it != raw.constEnd();
                         ++it) {
                        floating.insert(canonicalAppletKey(it.key()),
                                        it.value());
                    }
                }
                for (const QString& id : m_floatingApplets.keys()) {
                    if (!floating.contains(id)) { dockAppletBack(id); }
                }
                for (auto it = floating.constBegin();
                     it != floating.constEnd(); ++it) {
                    const QVariantMap one = it.value().toMap();
                    const QRect rect(one.value(QStringLiteral("x")).toInt(),
                                     one.value(QStringLiteral("y")).toInt(),
                                     one.value(QStringLiteral("w")).toInt(),
                                     one.value(QStringLiteral("h")).toInt());
                    const QString screen =
                        one.value(QStringLiteral("screen")).toString();
                    const int dockIndex =
                        one.value(QStringLiteral("dockIndex"), -1).toInt();
                    if (auto* w = m_floatingApplets.value(it.key(), nullptr)) {
                        // Schon abgelöst — nur nachführen.
                        //
                        // Betreiber 2026-09-01: "so wurde es nicht
                        // abgespeichert" -- ein Profil-Import bei
                        // getrennter Verbindung importierte den
                        // gespeicherten Inhalt korrekt (settings-Datei
                        // stimmte), zeigte am Bildschirm aber NICHTS:
                        // Bandwidth Filter/Frequenz/S-Meter blieben
                        // unsichtbar. Ursache war der eigene "ALLE
                        // fliegenden Fenster..."-Fix von vorhin -- der
                        // hatte diese Fenster bei "keine Verbindung"
                        // per w->hide() versteckt, OHNE sie aus
                        // m_floatingApplets zu nehmen. Dieser Zweig hier
                        // sah sie darum als "schon abgelöst" und
                        // aktualisierte nur Lage/dockIndex -- ein
                        // show() stand nie dabei, weil ein bereits
                        // abgelöstes Fenster bislang IMMER sichtbar war.
                        // Das Profil verlangt es sichtbar -- show() holt
                        // es aus genau diesem Versteck zurück. Steht
                        // weiterhin keine Verbindung, versteckt der
                        // Vorher-verstecken-Block ein paar Zeilen weiter
                        // unten (im selben Lambda, laeuft IMMER danach)
                        // es ohnehin gleich wieder -- die Reihenfolge
                        // passt zu beiden Regeln zugleich: das Profil
                        // wird treu hergestellt, UND es bleibt vor der
                        // ConnectMaske nichts sichtbar.
                        //
                        // Review-Fund 2026-09-01 (adversarial
                        // bestaetigt): NICHT bedingungslos zeigen. Ein
                        // abgeloestes Fenster kann mit abgeschalteter
                        // Sichtbarkeit existieren (Haken im Auswaehler
                        // aus, Fenster bleibt in m_floatingApplets) --
                        // das Profil traegt dann BEIDES: einen
                        // floatingApplets-Eintrag UND visible=false.
                        // Ein unbedingtes show() haette das Fenster im
                        // VERBUNDENEN Zustand entgegen dem Profil
                        // aufgerissen (der Versteck-Block unten laeuft
                        // dann nicht), mit widersprechendem Haken im
                        // Auswaehler.
                        const bool wantShown = !m_appletVis
                            || m_appletVis->isEffectivelyVisible(
                                   it.key());
                        w->setVisible(wantShown);
                        if (wantShown) {
                            m_floatingContainersHiddenPreConnect
                                .removeAll(w);
                        }
                        w->setDockIndex(dockIndex);
                        if (rect.isValid()) { w->setGeometry(rect); }
                        ensureOnVisibleScreen(w, this,
                                              QSize(Style::kAppletPanelW, 120));
                    } else if (auto* a = appletForKey(it.key())) {
                        detachApplet(a, dockIndex, rect, screen);
                    }
                    // Eine Kennung ohne Applet dahinter wird
                    // übergangen: eine Aufnahme von vor einem Update
                    // kann Widgets nennen, die es nicht mehr gibt.
                }

                qWarning() << "[ProfileApply:Step] 4/6 Panadapter";
                // ── Panadapter-Schwebefenster herstellen ────────────
                //
                // Betreiber 2026-09-01: siehe die Capture-Seite. Das
                // contains()-Tor ist Absicht: eine Aufnahme von VOR
                // diesem Update kennt den Schluessel nicht -- dann
                // nichts andocken und nichts abloesen, der globale
                // PanFloating_<id>-Stand bleibt fuer sie massgeblich
                // (dasselbe Migrations-Muster wie bei rotorDockVisible
                // weiter unten). Steht der Schluessel drin, gilt
                // dieselbe Vorhandensein-Regel wie bei floatingApplets:
                // Eintrag -> schwebt an dieser Stelle, kein Eintrag ->
                // angedockt.
                if (m_panStack
                    && s.contains(QStringLiteral("floatingPans"))) {
                    const QVariantMap fpans =
                        s.value(QStringLiteral("floatingPans")).toMap();
                    const QStringList panIds =
                        m_panStack->panIdsForTesting();

                    // ZWEI Durchgaenge, Reihenfolge tragend
                    // (Review-Fund 2026-09-01, adversarial bestaetigt):
                    // dockPanadapter() ist NICHT auf einen Panadapter
                    // begrenzt -- sein dockRequested-Weg ruft
                    // applyLayout(), dessen allererster Schritt
                    // dockAllFloatingPans() ist und damit JEDES
                    // Schwebefenster abraeumt. Ein einzelner Durchgang
                    // in Kennungs-Reihenfolge haette eine in Runde 1
                    // hergestellte Schwebe-Lage (pan-0) in Runde 2
                    // (pan-1 andocken) sofort wieder zerstoert -- und
                    // der naechste Beenden-Schnappschuss haette den
                    // kaputten Stand dauerhaft ins Profil geschrieben.
                    // Also: ERST alles andocken (die Kollateral-Docks
                    // duerfen dabei passieren), DANACH schweben lassen
                    // und die Lage setzen.
                    for (const QString& panId : panIds) {
                        if (!fpans.contains(panId)
                            && m_panStack->isPanFloating(panId)) {
                            m_panStack->dockPanadapter(panId);
                        }
                    }
                    for (const QString& panId : panIds) {
                        if (!fpans.contains(panId)) { continue; }
                        const QVariantMap one =
                            fpans.value(panId).toMap();
                        if (!m_panStack->isPanFloating(panId)) {
                            m_panStack->floatPanadapter(panId);
                        }
                        if (auto* pf = m_panStack
                                ->floatingWindowForTest(panId)) {
                            const QRect r(
                                one.value(QStringLiteral("x")).toInt(),
                                one.value(QStringLiteral("y")).toInt(),
                                one.value(QStringLiteral("w")).toInt(),
                                one.value(QStringLiteral("h")).toInt());
                            // Untergrenzen -- ein kaputter Eintrag
                            // soll kein 0x0-Fenster erzeugen. Der
                            // mitgesicherte "screen"-Schluessel wird
                            // hier bewusst (noch) nicht ausgewertet:
                            // Einzelmonitor-Betrieb; bei Mehrschirm
                            // greift ensureOnVisibleScreen() als Netz.
                            if (r.width() >= 100 && r.height() >= 80) {
                                pf->setGeometry(r);
                            }
                            ensureOnVisibleScreen(pf, this,
                                                  QSize(420, 240));
                        }
                    }
                    // Globale PanFloating_<id>-Schluessel nachziehen:
                    // die Kollateral-Docks aus dockAllFloatingPans()
                    // emittieren kein panFloatStateChanged (Signale
                    // vorher getrennt), und das Signal ist der einzige
                    // Schreiber dieser Schluessel -- ohne diese
                    // Schleife koennte der naechste Programmstart
                    // einen laengst angedockten Panadapter wieder
                    // schweben lassen.
                    for (const QString& panId : panIds) {
                        AppSettings::instance().setValue(
                            QStringLiteral("PanFloating_%1").arg(panId),
                            m_panStack->isPanFloating(panId)
                                ? QStringLiteral("True")
                                : QStringLiteral("False"));
                    }
                }

                qWarning() << "[ProfileApply:Step] 5/6 Rotor/Log";
                // ── Rotor/Log-Form herstellen ───────────────────────
                //
                // Dasselbe contains()-Tor. Der "floating"-Zweig ist
                // zusaetzlich auf die Sichtbarkeits-Map gebunden:
                // detachRotorPanel() schaltet WinRotorLog im
                // Controller auf sichtbar -- bei einem Profil, das den
                // Rotor ausdruecklich NICHT zeigt, waere das ein
                // Widerspruch, den erst der naechste Quit als
                // Dauerzustand festschriebe.
                if (s.contains(QStringLiteral("rotor"))) {
                    const QVariantMap rotor =
                        s.value(QStringLiteral("rotor")).toMap();
                    const QString form =
                        rotor.value(QStringLiteral("form")).toString();
                    const bool rotorWanted =
                        s.value(QStringLiteral("visible")).toMap()
                            .value(QStringLiteral("WinRotorLog"), true)
                            .toBool();
                    if (form == QLatin1String("floating")
                        && rotorWanted) {
                        detachRotorPanel();
                        if (m_rotorWindow) {
                            // Betreiber 2026-09-01: "das ROTOR Fenster im
                            // Format anders, auch die Positionierung" --
                            // ToolWindow's eigener Konstruktor liest VOR
                            // diesem Aufruf schon per restoreGeometryState()
                            // einen GLOBALEN, profil-unabhaengigen
                            // AppSettings-Schluessel (ToolWindowGeometry_
                            // RotorLog) und setzt m_sizedOnce -- ein
                            // synchrones setGeometry() direkt danach sollte
                            // eigentlich gewinnen, tut es an dieser Stelle
                            // aber nachweislich nicht zuverlaessig (derselbe
                            // Verdacht besteht fuer PanFloatingWindow, das
                            // dasselbe Muster traegt, dort aber noch nicht
                            // gemeldet wurde). Dieselbe Kur wie beim
                            // Panadapter-Reparenting (refreshAfterReparent,
                            // PanadapterStack.cpp): auf den naechsten
                            // Ereignisschleifen-Durchlauf verschieben, NACH
                            // allem, was der Konstruktor/show() noch
                            // nachreicht. QPointer schuetzt gegen ein
                            // zwischenzeitliches dockRotorPanel()
                            // (Andocken/Profilwechsel/Beenden).
                            const QRect r(
                                rotor.value(QStringLiteral("x")).toInt(),
                                rotor.value(QStringLiteral("y")).toInt(),
                                rotor.value(QStringLiteral("w")).toInt(),
                                rotor.value(QStringLiteral("h")).toInt());
                            QPointer<ToolWindow> guard(m_rotorWindow);
                            QTimer::singleShot(0, this, [this, guard, r]() {
                                if (!guard) { return; }
                                if (r.width() >= 100 && r.height() >= 80) {
                                    guard->setGeometry(r);
                                }
                                // Betreiber 2026-09-01: "immer falsch
                                // gespeichert, zu groß, andere
                                // Koordinaten" -- 420x240 ist die
                                // Mindestgroesse von PanFloatingWindow
                                // (dort per setMinimumSize() erzwungen),
                                // NICHT von ToolWindow/Rotor-Log, das
                                // gar keine eigene Mindestgroesse setzt.
                                // Blind kopiert von der Panadapter-Stelle
                                // direkt darueber. Eine echte,
                                // schmalere gespeicherte Breite (hier
                                // z.B. 270px) wurde dadurch bei JEDEM
                                // Wiederherstellen wieder auf 420
                                // aufgeblasen und neu positioniert --
                                // exakt das gemeldete Symptom. Dieselbe
                                // 100x80-Untergrenze wie in der
                                // Kaputt-Eintrag-Pruefung direkt darueber,
                                // nicht die Panadapter-Konstante.
                                ensureOnVisibleScreen(guard, this,
                                                      QSize(100, 80));
                            });
                        }
                    } else if (form == QLatin1String("below")) {
                        // Erst den Schluessel, dann andocken:
                        // dockRotorPanel() liest RotorPanelBelow am
                        // Ende selbst und legt das Panel entsprechend
                        // ab.
                        AppSettings::instance().setValue(
                            QStringLiteral("RotorPanelBelow"),
                            QStringLiteral("True"));
                        if (m_rotorWindow) {
                            dockRotorPanel();
                        } else {
                            setRotorPanelBelow(true);
                        }
                        // NACH setRotorPanelBelow(): das setzt den
                        // aeusseren Splitter bedingungslos auf
                        // 2/3-1/3 zurueck -- die gesicherte Teilung
                        // muss danach kommen, sonst gewinnt die
                        // Vorgabe (Review-Fund 2026-09-01).
                        const QVariantList split =
                            rotor.value(QStringLiteral("belowSplit"))
                                .toList();
                        if (m_outerSplitter && split.size() >= 2) {
                            QList<int> px;
                            for (const QVariant& v : split) {
                                px << v.toInt();
                            }
                            m_outerSplitter->setSizes(px);
                        }
                    } else if (form == QLatin1String("docked")) {
                        AppSettings::instance().setValue(
                            QStringLiteral("RotorPanelBelow"),
                            QStringLiteral("False"));
                        if (m_rotorWindow) {
                            dockRotorPanel();
                        } else {
                            setRotorPanelBelow(false);
                        }
                    }
                }

                // Betreiber 2026-09-01: "kein Funkgerät hier, alles nicht
                // verbunden" -- die schwebenden Fenster von Applets
                // (m_floatingApplets), Panadapter (PanadapterStack) und
                // Rotor/Log (m_rotorWindow) werden beim Profil-Anwenden
                // NICHT mehr wegen fehlender Verbindung versteckt (bis
                // 2026-09-01 waren das drei getrennte "Vorher-verstecken"
                // Zweige genau hier). Betreiber-Entscheidung: Layout-
                // Arbeit (Panadapter/Applets/Rotor anordnen) muss auch
                // ohne Radio am Tisch moeglich sein -- ein Widerruf der
                // frueheren "ALLE fliegenden Fenster gehören hinter die
                // ConnectMaske"-Weisung fuer genau diese drei Kategorien.
                // NUR das Antenna-Fenster (SWR-Sweep, ohne Funkgeraet
                // sicherheitsrelevant bedeutungslos) bleibt hinter der
                // Connect-Maske -- eigener, aelterer Grund
                // (applyWindowVisibility()'s WinAntenna-Wache), nicht
                // Teil dieser Entscheidung.
                if ((!m_radioModel
                     || m_radioModel->connectionState()
                            != ConnectionState::Connected)
                    && m_antennaWindow && m_antennaWindow->isVisible()) {
                    m_antennaWindow->hide();
                    m_floatingContainersHiddenPreConnect.append(
                        m_antennaWindow);
                }

                if (m_appletPanel) {
                    QList<AppletWidget*> order;
                    for (const QVariant& v :
                         s.value(QStringLiteral("order")).toList()) {
                        if (auto* a = appletForKey(v.toString())) {
                            order.append(a);
                        }
                    }
                    m_appletPanel->setAppletOrder(order);
                }

                if (m_containerManager
                    && s.contains(QStringLiteral("containerGeometry"))) {
                    m_containerManager->applyFloatingGeometries(
                        s.value(QStringLiteral("containerGeometry")).toMap());
                }

                // Rotor / Log -- see the capture side's own comment.
                // Missing key (a snapshot from before this fix) means
                // "unknown", not "hide": default to visible so an
                // existing profile's dock doesn't vanish just because it
                // predates this key.
                //
                // Betreiber 2026-08-28: "das ist bevor ich mich einlogge
                // - das rotor fenster gehört weg" -- am Start, solange
                // noch kein Funkgeraet verbunden ist, ist der Rotor/Log-
                // Kompass ohnehin bedeutungslos (000°, kein Ziel). Der
                // gespeicherte Wunsch bleibt gemerkt, kommt aber erst mit
                // der ersten Verbindung zur Geltung -- siehe die
                // connectionStateChanged-Bindung weiter unten.
                m_rotorDockWantedVisible =
                    s.value(QStringLiteral("rotorDockVisible"), true).toBool();
                if (m_rotorDock) {
                    m_rotorDock->setVisible(
                        m_radioModel
                        && m_radioModel->connectionState()
                               == ConnectionState::Connected
                        && m_rotorDockWantedVisible);
                }

                const QVariantList sizes =
                    s.value(QStringLiteral("splitter")).toList();
                if (m_mainSplitter && !sizes.isEmpty()) {
                    QList<int> px;
                    for (const QVariant& v : sizes) { px << v.toInt(); }
                    m_mainSplitter->setSizes(px);
                }
                qWarning() << "[ProfileApply:Step] 6/6 fertig";
            });

        m_layoutProfiles->load();
        // Betreiber 2026-09-01: "letzter Zustand nie beim Öffnen
        // sichtbar" -- zeigt, was TATSAECHLICH von der Platte kam,
        // bevor applyCurrent() irgendetwas damit tut. Vergleich mit
        // [ProfileSaveOnQuit:*] aus dem letzten Beenden beantwortet
        // die Frage, ob das Problem beim Sichern oder beim Laden liegt.
        if (!m_layoutProfiles->current().isEmpty()) {
            const QVariantMap snap =
                m_layoutProfiles->snapshot(m_layoutProfiles->current());
            qWarning() << "[ProfileLoadOnStartup]"
                       << m_layoutProfiles->current()
                       << "floatingApplets="
                       << snap.value(QStringLiteral("floatingApplets")).toMap().size()
                       << "floatingPans="
                       << snap.value(QStringLiteral("floatingPans")).toMap().size()
                       << "rotor=" << snap.value(QStringLiteral("rotor")).toMap();
        }
        if (m_layoutProfiles->names().isEmpty()) {
            // Beim allerersten Start gibt es genau ein Profil, und es
            // hält, was gerade zu sehen ist. Ohne das stünde die
            // Schiene leer da und das Plus hieße „leg dir erst mal
            // etwas an" — dabei hat der Betreiber schon ein Fenster.
            m_layoutProfiles->create(QStringLiteral("Standard"));
        } else if (!m_layoutProfiles->current().isEmpty()) {
            // NICHT activate(current()): dessen Namensgleich-Wache
            // ("schon aktiv? nichts tun") gibt hier sofort zurück, weil
            // load() m_current bereits auf denselben Namen gesetzt hat —
            // die gespeicherte Umgestaltung käme nie an. applyCurrent()
            // löst m_apply ohne diese Wache aus. Bug + Fix 2026-08-28.
            m_layoutProfiles->applyCurrent();
        }
        wireProfileRail();

        // Der Rotor/Log-Dock kommt erst mit der ERSTEN Verbindung nach
        // diesem Start auf den vom Profil gewuenschten Sichtbarkeitsstand
        // (siehe m_rotorDockWantedVisible oben) -- danach entscheidet der
        // Betreiber selbst wieder per Klick/Menue, darum trennt sich die
        // Bindung nach dem ersten Treffer.
        auto rotorShowConn = std::make_shared<QMetaObject::Connection>();
        *rotorShowConn = connect(m_radioModel,
            &RadioModel::connectionStateChanged, this,
            [this, rotorShowConn](Longpath::ConnectionState state) {
                if (state == Longpath::ConnectionState::Connected) {
                    // Welche der drei Formen (Dock, unter dem Panadapter,
                    // eigenes Fenster) gerade aktiv ist, wurde beim Start
                    // schon entschieden (RotorPanelBelow/RotorFloating) --
                    // hier nur wieder zeigen, was zuvor versteckt wurde.
                    // m_rotorWindow zuerst: RotorFloating ist der
                    // Standardfall, und ein sichtbares m_rotorWindow lässt
                    // ein daneben existierendes, aber leeres m_rotorDock
                    // ohnehin unbeachtet.
                    //
                    // Betreiber 2026-08-31, per Log/Einstellungsdatei
                    // bestaetigt: m_rotorDockWantedVisible durfte hier NIE
                    // das schwebende Fenster sperren. Der Wert kommt aus
                    // m_rotorDock->isVisible() zum Sicherungszeitpunkt
                    // (siehe "rotorDockVisible" Erfassung oben) -- steht
                    // Rotor/Log gerade schwebend (der dokumentierte
                    // Normalfall), ist das Dock leer und unbenutzt, seine
                    // isVisible() also ganz legitim false. Genau dieses
                    // false unterdrueckte danach auch m_rotorWindow->show(),
                    // obwohl das schwebende Fenster laengst existierte und
                    // nur auf die erste Verbindung wartete. Ein
                    // existierendes m_rotorWindow soll nach der ersten
                    // Verbindung IMMER wieder erscheinen -- das Flag gilt
                    // nur noch fuer den m_rotorDock-Zweig, wo es tatsaechlich
                    // die richtige Frage beantwortet.
                    if (m_rotorWindow) {
                        m_rotorWindow->show();
                        m_rotorWindow->raise();
                    } else if (m_rotorDock) {
                        m_rotorDock->setVisible(m_rotorDockWantedVisible);
                    }

                    // Dieselbe Freigabe fuer die schwebenden Meter/Applet-
                    // Fenster, die restoreState() versteckt hatte (siehe
                    // dort). QPointer haelt fest, ob eines inzwischen ganz
                    // geschlossen (nicht nur versteckt) wurde -- dann bleibt
                    // es aus, statt aus dem Nichts wiederzukommen.
                    for (const QPointer<QWidget>& w
                         : std::as_const(m_floatingContainersHiddenPreConnect)) {
                        if (w) { w->show(); w->raise(); }
                    }
                    m_floatingContainersHiddenPreConnect.clear();

                    QObject::disconnect(*rotorShowConn);
                }
            });
    }

    // ── Das Plus ─────────────────────────────────────────────────────
    //
    // OE5SOS: „Ich möchte mit einem PLUS Widget hinzufügen können und
    // entfernen, um so mein eigenes Profil selbst zu gestalten."
    //
    // Ein- und Ausblenden konnte AppletVisibilityController schon; es
    // steckte nur im Menü „Containers", und ein Menüeintrag lädt
    // niemanden ein, sein Fenster umzubauen. Erst hier, nachdem alle
    // registerApplet()- und describeApplet()-Aufrufe durch sind, kennt
    // der Verwalter die Kategorien, die der Auswähler links anzeigt.
    if (m_commandBar) {
        m_addWidget = new AddWidgetButton(m_appletVis, m_commandBar);
        m_commandBar->addTrailing(m_addWidget);
    }

    // Apply initial visibility state from the controller (in case
    // AppSettings already had values from a prior session).
    // Uses effective visibility (user pref AND available).
    //
    // Betreiber 2026-08-31, nach langer Suche: "rotor war auch wieder
    // kein eigenes fenster", trotz sauber beendeter Sitzungen ohne jede
    // Absturzspur -- die Ursache war genau diese Schleife. Fuer
    // "WinRotorLog" bedeutet ein/aus nicht bloss sichtbar/unsichtbar,
    // sondern welche FORM das Fenster hat (siehe applyWindowVisibility():
    // "on" ruft detachRotorPanel(), "off" ruft dockRotorPanel() -- beide
    // aendern die Form, nicht nur die Sichtbarkeit). Die Form ist aber
    // schon LAENGST entschieden, weiter oben im selben Konstruktor, aus
    // der eigentlich zustaendigen Quelle (RotorFloating/RotorPanelBelow).
    // Diese Schleife hier liest stattdessen den generischen, im PROFIL
    // gespeicherten "WinRotorLog"-Haken -- der oft genug einen anderen
    // (aelteren, oder nie synchronisierten) Stand traegt -- und rief
    // dockRotorPanel() jedes Mal auf, wenn der davon abwich. Ergebnis:
    // die richtige Form von oben wurde hier unten, im selben Start,
    // sofort wieder verworfen, UND RotorFloating gleich mit ueberschrieben
    // (dockRotorPanel() schreibt es). "WinRotorLog" ist deshalb hier
    // ausgenommen; seine Sichtbarkeit lebt fuer den Sonderfall Rotor/Log
    // ausschliesslich in RotorFloating/RotorPanelBelow, nicht hier.
    // Betreiber 2026-09-01: "kein Funkgerät hier, alles nicht verbunden" --
    // schwebende Applet-Fenster (m_floatingApplets: TX, S-Meter,
    // Mitschrift & Co.) werden beim Start nicht mehr wegen fehlender
    // Verbindung uebersprungen (widerruft die fruehere "ALLE fliegenden
    // Fenster gehören hinter die ConnectMaske"-Weisung fuer diese
    // Kategorie). "WinAntenna" haengt nicht an m_floatingApplets (eigener,
    // dritter Mechanismus fuer Werkzeugfenster wie Antenne/Logbuch/
    // Kanalzug) und bleibt weiterhin ausgenommen: SWR-Sweep ist ohne
    // Funkgeraet sicherheitsrelevant bedeutungslos ("no radio" steht
    // selbst im Fenster), nicht nur uninformativ wie ein Applet.
    for (const QString& id : m_appletVis->registeredIds()) {
        if (id == QLatin1String("WinRotorLog")) { continue; }
        if ((!m_radioModel
             || m_radioModel->connectionState() != ConnectionState::Connected)
            && id == QLatin1String("WinAntenna")) {
            continue;
        }
        applyAppletVisibility(id, m_appletVis->isEffectivelyVisible(id));
    }

    // Und die Kacheln an ihre Plaetze. NACH der Sichtbarkeit: ein
    // Fenster, das gar nicht sichtbar ist, bekommt keine Kachel.
    //
    // Der Aufruf steht ausserhalb der Schleife darueber, weil
    // moveAppletToCanvas das Applet aus dem Stapel NIMMT — waehrend
    // derselben Schleife haette applyAppletVisibility es gleich danach
    // wieder hineingelegt.
    restoreCanvasLayout();

    // Pump future EFFECTIVE-visibility changes from the controller into
    // the panel. effectiveVisibilityChanged fires when either the user
    // toggle or the availability gate flips the net visibility, so we
    // catch both menu clicks and external capability changes (e.g. 4O3A).
    connect(m_appletVis, &AppletVisibilityController::effectiveVisibilityChanged,
            this, [this](const QString& id, bool effective) {
        applyAppletVisibility(id, effective);
    });

    // Live-track 4O3A master toggle so Amp/Tuner availability updates
    // without an app restart. RadioModel::setFourO3AEnabled emits the
    // signal whenever the persisted value changes.
    if (m_radioModel) {
        connect(m_radioModel, &RadioModel::fourO3AEnabledChanged,
                this, [this](bool enabled) {
            if (!m_appletVis) { return; }
            m_appletVis->setAvailable(QStringLiteral("Amp"),   enabled);
            m_appletVis->setAvailable(QStringLiteral("Tuner"), enabled);
        });

        // Phase 3P-III Task 14: live-track RF-Kit master toggle so the
        // RfKit applet availability updates without an app restart.
        connect(m_radioModel, &RadioModel::rfKitEnabledChanged,
                this, [this](bool enabled) {
            if (!m_appletVis) { return; }
            m_appletVis->setAvailable(QStringLiteral("RfKit"), enabled);
        });
    }

    // ── Das ☰-Menue am Panel ist weggefallen (2026-08-18) ───────────
    //
    // Hier stand ein zweites Applet-Menue mit Haekchen je Eintrag, das
    // am ☰ in der Titelleiste des festen S-Meter-Kopfes hing. Mit dem
    // Kopf faellt beides weg.
    //
    // Kein Verlust: das + in der Kopfleiste oeffnet den Auswaehler (mit
    // Kategorien, Suche und Schlagwoertern), und Ansicht > Container >
    // Applets fuehrt dieselbe Liste mit Haekchen. Zwei Menues fuer
    // dieselbe Entscheidung waren ohnehin eine Doppelung — und die
    // Woche hat gezeigt, was aus Doppelungen wird.
    //
    // Vom Betreiber am 2026-08-18 bestaetigt: „Das ≡ fällt weg, ja.
    // Deine Begründung trägt — + in der Kopfleiste und Ansicht →
    // Container → Applets machen dieselbe Arbeit."

    // Ghost applets: constructed but not added to the panel or the Containers menu
    // until their feature phases ship. Uncomment the construction + addContainerToggle
    // call (in buildMenuBar) together when the feature lands.
    //
    // m_digitalApplet    = new DigitalApplet(m_radioModel, nullptr);    // TODO 3-VAX
    // m_diversityApplet  = new DiversityApplet(m_radioModel, nullptr);  // TODO 3F (multi-RX)
    // m_cwxApplet        = new CwxApplet(m_radioModel, nullptr);        // TODO 3M-2 (CW TX)
    // m_catApplet        = new CatApplet(m_radioModel, nullptr);        // TODO 3J/3K/3-VAX

    c0->setContent(panel);
    qCDebug(lcMeter) << "Installed default meter layout: S-Meter + Power/SWR + ALC";
    qCDebug(lcContainer) << "Container #0: Meters + RxApplet + TxApplet + PhoneCwApplet + VaxApplet + TciApplet + ClientChainApplet";
}

void MainWindow::buildMenuBar()
{
    // =========================================================================
    // FILE
    // =========================================================================
    QMenu* fileMenu = menuBar()->addMenu(QStringLiteral("&File"));

    {
        QAction* settingsAction = fileMenu->addAction(QStringLiteral("&Settings..."),
            this, [this]() {
                auto* dialog = new SetupDialog(m_radioModel, this);
                dialog->setAttribute(Qt::WA_DeleteOnClose);
                wireSetupDialog(dialog);
                dialog->show();
            });
        settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
        settingsAction->setMenuRole(QAction::NoRole);  // Keep in File menu, don't let macOS move it
        settingsAction->setToolTip(QStringLiteral("Open application settings"));
    }

    {
        QMenu* profilesMenu = fileMenu->addMenu(QStringLiteral("&Profiles"));
        QAction* txProfilesAction = profilesMenu->addAction(QStringLiteral("&TX Profiles..."));
        txProfilesAction->setEnabled(false);
        txProfilesAction->setToolTip(QStringLiteral("NYI — Phase X"));
        QAction* micProfilesAction = profilesMenu->addAction(QStringLiteral("&Mic Profiles..."));
        micProfilesAction->setEnabled(false);
        micProfilesAction->setToolTip(QStringLiteral("NYI — Phase X"));
        profilesMenu->addSeparator();
        QAction* importAction = profilesMenu->addAction(QStringLiteral("&Import..."));
        importAction->setEnabled(false);
        importAction->setToolTip(QStringLiteral("NYI — Phase X"));
        QAction* exportAction = profilesMenu->addAction(QStringLiteral("&Export..."));
        exportAction->setEnabled(false);
        exportAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    fileMenu->addSeparator();

    // Betreiber 2026-08-30, nach zwei Fehlschlaegen am Rotor/Log-Fix:
    // "habe ich gemacht, leider nein". Der Grund: Qt garantiert NICHT,
    // dass MainWindow::closeEvent() (wo m_shuttingDown bisher gesetzt
    // wurde) vor den Schliessereignissen der schwebenden Fenster
    // (Rotor/Log, Applets) laeuft -- closeAllWindows() geht die
    // Top-Level-Fenster in einer Reihenfolge durch, auf die sich kein
    // Aufrufer verlassen darf. Traf es zuerst ein schwebendes Fenster,
    // stand m_shuttingDown dort noch auf false, und dessen closeEvent
    // dockte sich selbst an, bevor MainWindow ueberhaupt zum Zug kam.
    //
    // Dieser Cmd+Q-Menuepunkt ist der EINE Ort, an dem das Beenden auf
    // macOS tatsaechlich beginnt (Qt zieht ihn per Rollen-Erkennung an
    // "Quit" automatisch ins Anwendungsmenue) -- alles Weitere
    // (closeAllWindows(), jedes einzelne closeEvent) folgt erst DANACH.
    // Die Sperre hier zu setzen, bevor qApp->quit() ueberhaupt aufgerufen
    // wird, macht die Reihenfolge der einzelnen Fenster bedeutungslos.
    fileMenu->addAction(QStringLiteral("&Quit"), QKeySequence(Qt::CTRL | Qt::Key_Q),
                        this, [this]() {
        m_shuttingDown = true;
        qApp->quit();
    });

    // =========================================================================
    // RADIO
    // =========================================================================
    // ── Radio menu — 3Q-9: role-based items with state-aware enablement ──────
    QMenu* radioMenu = menuBar()->addMenu(QStringLiteral("&Radio"));

    // Connect (⌘K) — reconnects to the last-used radio. Greyed out when there
    // is no actionable target (currently connected, or no lastConnected MAC,
    // or the lastConnected MAC isn't in saved radios). Manage Radios is the
    // ONLY menu item whose job is to open the Connection Panel; Connect is
    // strictly a one-click reconnect.
    m_actConnect = radioMenu->addAction(QStringLiteral("&Connect"),
        QKeySequence(Qt::CTRL | Qt::Key_K),
        this, [this]() {
            if (m_radioModel->isConnected()) {
                return;
            }
            AppSettings& s = AppSettings::instance();
            const QString lastMac = s.lastConnected();
            const auto saved = s.savedRadio(lastMac);
            if (!saved.has_value()) {
                return;  // enablement should have prevented this
            }
            // Unicast probe targeted at the saved IP — cleaner than a
            // broadcast scan + radioDiscovered listener: no leaked listeners
            // when the radio doesn't reply, and works across VPN tunnels
            // that drop broadcast traffic. Phase 3Q-2 wired probeAddress;
            // this menu item now uses it directly. (Earlier broadcast-listen
            // implementation leaked a connect-on-mac-match listener that
            // would auto-reconnect to LOCAL radio on later scans even after
            // the user explicitly disconnected — bug reported 2026-04-30.)
            RadioDiscovery* disc = m_radioModel->discovery();
            QMetaObject::Connection* connPtr = new QMetaObject::Connection;
            QMetaObject::Connection* failPtr = new QMetaObject::Connection;
            auto cleanup = [connPtr, failPtr]() {
                QObject::disconnect(*connPtr);
                delete connPtr;
                QObject::disconnect(*failPtr);
                delete failPtr;
            };
            *connPtr = connect(disc, &RadioDiscovery::radioDiscovered,
                this, [this, lastMac, cleanup](const RadioInfo& found) {
                    if (found.macAddress != lastMac) {
                        return;  // probe reply for a different radio — wait
                    }
                    if (m_radioModel->isConnected()) {
                        cleanup();
                        return;
                    }
                    cleanup();
                    RadioInfo ri = found;
                    HPSDRModel mo = AppSettings::instance().modelOverride(ri.macAddress);
                    if (mo != HPSDRModel::FIRST) {
                        ri.modelOverride = mo;
                    }
                    m_radioModel->connectToRadio(ri);
                });
            *failPtr = connect(disc, &RadioDiscovery::probeFailed,
                this, [cleanup, lastMac](const QHostAddress&, quint16) {
                    qCInfo(lcConnection) << "Connect: probe failed for"
                                         << lastMac;
                    cleanup();
                });
            disc->probeAddress(saved->info.address, saved->info.port);
        });
    m_actConnect->setToolTip(QStringLiteral(
        "Reconnect to the last-used radio (greyed out when there's nothing to reconnect to)"));

    // Disconnect (⌘⇧K) — disabled while disconnected.
    m_actDisconnect = radioMenu->addAction(QStringLiteral("&Disconnect"),
        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_K),
        this, [this]() { m_radioModel->disconnectFromRadio(); });
    m_actDisconnect->setToolTip(QStringLiteral("Disconnect from the current radio"));

    radioMenu->addSeparator();

    // Manage Radios — always enabled; sole purpose is to open the panel
    // (which has its own ↻ Scan button for fresh broadcast discovery).
    m_actManageRadios = radioMenu->addAction(QStringLiteral("&Manage Radios…"),
        this, &MainWindow::showConnectionPanel);
    m_actManageRadios->setToolTip(QStringLiteral(
        "Open the Connection Panel (radio list + ↻ Scan)"));

    radioMenu->addSeparator();

    {
        QAction* antennaSetupAction = radioMenu->addAction(QStringLiteral("&Antenna Setup…"));
        antennaSetupAction->setEnabled(false);
        antennaSetupAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* transvertersAction = radioMenu->addAction(QStringLiteral("Trans&verters…"));
        transvertersAction->setEnabled(false);
        transvertersAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    radioMenu->addSeparator();

    // ── KiwiSDR (Stufe 6b, 2026-08-23) ───────────────────────────────
    //
    // Bis hierher war der KiwiSDR vollstaendig gebaut und vollstaendig
    // UNERREICHBAR: Protokoll, Verzeichnis, Verwaltung, Anzeige, Ton
    // und Wasserfall standen, aber es gab in der Oberflaeche keinen
    // Weg, einen Empfaenger auszuwaehlen. Das ist die kleinste Luecke
    // mit dem groessten Ausmass — gebaut und nicht erreichbar ist so
    // gut wie nicht gebaut.
    //
    // Der Eintrag steht im Radio-Menue und nicht unter Werkzeuge: ein
    // KiwiSDR IST ein Empfaenger, kein Hilfsmittel.
    {
        QMenu* kiwiMenu = radioMenu->addMenu(QStringLiteral("&KiwiSDR"));

        kiwiMenu->addAction(QStringLiteral("Öffentliche Empfänger…"),
            this, [this]() {
                auto* picker = new KiwiPublicReceiverPicker(this);
                picker->setAttribute(Qt::WA_DeleteOnClose);
                connect(picker, &QDialog::accepted, this, [this, picker]() {
                    addKiwiSdrReceiver(picker->selectedName(),
                                       picker->selectedEndpoint());
                });
                picker->show();
            });

        kiwiMenu->addAction(QStringLiteral("Empfänger von Hand hinzufügen…"),
            this, [this]() {
                // Fuer den eigenen Kiwi im Heimnetz. Er steht in keinem
                // oeffentlichen Verzeichnis, und genau der ist fuer
                // einen Funker der interessanteste.
                bool ok = false;
                const QString ep = QInputDialog::getText(
                    this, QStringLiteral("KiwiSDR hinzufügen"),
                    QStringLiteral("Adresse (Rechnername oder IP, "
                                   "wahlweise mit :Port):"),
                    QLineEdit::Normal, QStringLiteral("kiwisdr.local:8073"),
                    &ok);
                if (!ok || ep.trimmed().isEmpty()) { return; }
                addKiwiSdrReceiver(QString(), ep.trimmed());
            });

        kiwiMenu->addSeparator();
        kiwiMenu->addAction(QStringLiteral("Alle trennen"), this, [this]() {
            if (m_kiwiSdrManager) { m_kiwiSdrManager->disconnectAll(); }
        });
    }

    // ── SunSDR (TCI-Client, 2026-08-24) ───────────────────────────────
    //
    // Derselbe Grund wie beim KiwiSDR-Eintrag direkt darueber: ein
    // SunSDR IST ein Empfaenger, kein Hilfsmittel, darum im Radio-Menue.
    //
    // Bewusst kein Verzeichnis-Eintrag wie "Oeffentliche Empfaenger" bei
    // Kiwi -- fuer ein privates SunSDR2 QRP im eigenen Netz gibt es kein
    // Aequivalent dazu. Nur "Verbinden" und "Trennen", siehe
    // MainWindow_SunSdr.cpp fuer die Verdrahtung.
    {
        QMenu* sunSdrMenu = radioMenu->addMenu(QStringLiteral("&SunSDR (TCI)"));

        sunSdrMenu->addAction(QStringLiteral("&Verbinden…"), this, [this]() {
            auto& s = AppSettings::instance();
            // Vorschlag 127.0.0.1, NICHT die Adresse der SunSDR-Hardware:
            // TCI ist ein Dienst von ExpertSDR2 (der Software), die in
            // aller Regel auf demselben Rechner laeuft wie Longpath.
            // Gemessen bei OE5SOS am 2026-08-24 (docs/TCI-SunSDR-gemessen.md,
            // tools/tci_probe.cpp lief durchgehend gegen 127.0.0.1) -- der
            // erste Verbindungsversuch schlug fehl, weil die Hardware-
            // Adresse (192.168.16.200, aus dem "Add Custom Radio"-Dialog
            // uebernommen) hier eingetragen war. Falscher Ort fuer diese
            // Adresse: die spricht nur mit der Hardware, nicht mit TCI.
            const QString lastEndpoint = s.value(
                QStringLiteral("SunSdrEndpoint"),
                QStringLiteral("127.0.0.1:40001")).toString();
            bool ok = false;
            const QString ep = QInputDialog::getText(
                this, QStringLiteral("SunSDR verbinden"),
                QStringLiteral("Adresse von ExpertSDR2 (IP oder Rechnername, "
                               "wahlweise mit :Port -- Voreinstellung 40001).\n"
                               "Meist 127.0.0.1, wenn ExpertSDR2 auf diesem "
                               "Rechner laeuft -- NICHT die Adresse der "
                               "SunSDR-Hardware selbst."),
                QLineEdit::Normal, lastEndpoint, &ok);
            if (!ok || ep.trimmed().isEmpty()) { return; }
            connectSunSdr(ep.trimmed());
        });

        sunSdrMenu->addAction(QStringLiteral("&Trennen"), this, [this]() {
            disconnectSunSdr();
        });
    }

    radioMenu->addSeparator();

    // Protocol Info — disabled while disconnected; shows a QMessageBox with
    // the connected radio's protocol, firmware, and address info.
    m_actProtocolInfo = radioMenu->addAction(QStringLiteral("&Protocol Info"),
        this, [this]() {
            if (!m_radioModel->isConnected()) {
                return;
            }
            RadioInfo info = m_radioModel->connection()->radioInfo();
            const QString proto =
                info.protocol == ProtocolVersion::Protocol2
                    ? QStringLiteral("P2") : QStringLiteral("P1");
            const QString msg =
                QStringLiteral("Radio:    %1\nProtocol: %2\nFirmware: %3\nMAC:      %4\nIP:       %5")
                    .arg(info.displayName())
                    .arg(proto)
                    .arg(info.firmwareVersion)
                    .arg(info.macAddress, info.address.toString());
            QMessageBox::information(this, QStringLiteral("Protocol Info"), msg);
        });
    m_actProtocolInfo->setToolTip(QStringLiteral(
        "Show connected radio protocol, firmware, and address details"));

    // Initial enablement (before any connectionStateChanged fires). Connect
    // is enabled only when there's a last-used radio in saved entries — i.e.
    // when "reconnect" actually has a target.
    {
        AppSettings& s = AppSettings::instance();
        const QString lastMac = s.lastConnected();
        const bool hasReconnectTarget =
            !lastMac.isEmpty() && s.savedRadio(lastMac).has_value();
        m_actConnect->setEnabled(hasReconnectTarget);
    }
    m_actDisconnect->setEnabled(false);
    m_actProtocolInfo->setEnabled(false);

    // =========================================================================
    // VIEW
    // =========================================================================
    QMenu* viewMenu = menuBar()->addMenu(QStringLiteral("&View"));

    // Phase 3F Sub-Epic D Task 14: live Pan Layout / Add slice / Float
    // entries. Replace the NYI submenu and disabled Add/Remove placeholders
    // with the working stack actions. The bottom-bar +PAN icon (a drawn
    // pixmap since Task B4, not a dropdown menu) is kept as the operator-
    // facing primary and shares showPanLayoutDialog() with the menu action
    // below it; these menu items are for operators who prefer the menubar
    // / keyboard shortcuts.
    {
        QAction* panLayoutAct = viewMenu->addAction(QStringLiteral("Pan &Layout…"));
        panLayoutAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+L")));
        panLayoutAct->setToolTip(QStringLiteral(
            "Pick a panadapter layout template"));
        // Delegates to showPanLayoutDialog() rather than duplicating dialog
        // construction here: this call site used to build its own
        // PanLayoutDialog inline with no isConnected() guard, so with no
        // radio it opened a dialog gated on a stale maxSlices()-only
        // fallback (final-fix-wave finding 12). showPanLayoutDialog() is
        // already connection-gated and DDC-axis-correct (finding 2); the
        // bottom-bar +PAN icon uses the same method.
        connect(panLayoutAct, &QAction::triggered, this,
                &MainWindow::showPanLayoutDialog);

        // ── Ausgegraut statt stillschweigend wirkungslos ─────────────
        //
        // Der Betreiber, 2026-08-20: „pan layout geht ueber den reiter
        // oben nicht."
        //
        // Er hatte recht, und es war schlimmer als ein Fehler:
        // showPanLayoutDialog() kehrt ohne Verbindung SOFORT zurueck.
        // Der Eintrag sah benutzbar aus, liess sich anklicken und tat
        // nichts — ohne ein Wort dazu. Ein Weg, der nichts sagt, ist
        // schlimmer als einer, der „geht gerade nicht" sagt.
        //
        // Die Sperre selbst bleibt richtig: wie viele Panadapter
        // moeglich sind, haengt an den DDCs des angeschlossenen
        // Geraets (siehe die Begruendung an showPanLayoutDialog). Ohne
        // Geraet gibt es keine Zahl, gegen die man pruefen koennte.
        //
        // Also grau, mit Grund im Tooltip.
        auto syncPanLayoutEnabled = [this, panLayoutAct]() {
            const bool on = m_radioModel && m_radioModel->isConnected();
            panLayoutAct->setEnabled(on);
            panLayoutAct->setToolTip(on
                ? QStringLiteral("Pick a panadapter layout template")
                : QStringLiteral(
                      "Erst verbinden: wie viele Panadapter moeglich "
                      "sind, sagt das Geraet (DDC-Anzahl)."));
        };
        syncPanLayoutEnabled();
        if (m_radioModel) {
            connect(m_radioModel, &RadioModel::connectionStateChanged, this,
                    [syncPanLayoutEnabled](Longpath::ConnectionState) {
                syncPanLayoutEnabled();
            });
        }
    }

    {
        QAction* addSliceAct = viewMenu->addAction(
            QStringLiteral("&Add slice on active pan"));
        addSliceAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+R")));
        addSliceAct->setToolTip(QStringLiteral(
            "Create a new slice on the active panadapter (up to maxSlices())"));
        connect(addSliceAct, &QAction::triggered, this, [this]() {
            if (m_panStack && m_radioModel) {
                m_radioModel->addSliceOnPan(m_panStack->activePanId());
            }
        });
    }

    {
        // ── Das fehlende Gegenstueck zu "Add slice" ──────────────────
        //
        // Der Betreiber am 2026-08-22: "40 meter hört sich an, als ich
        // 2 frequenzen gleichzeitig höre und ggf 2 bänder."
        //
        // Genau so war es: seine Kopfleiste zeigte A 7.144.100 und
        // B 14.225.000 — zwei Empfaenger, 40 m und 20 m, und der
        // Mischer nimmt JEDE Scheibe, die nicht stumm ist. Anlegen ging
        // (Strg+R), SCHLIESSEN ging nicht. Das steht sogar als Luecke
        // im Quelltext: "sechs Signale des Mehrfach-Panadapters
        // (Scheibe schliessen, ... Scheibe entfernen) — sie gehoeren zu
        // Phase 3F". Wer einen zweiten Empfaenger aufmacht, hoert ihn
        // seither fuer immer.
        //
        // Der letzte Empfaenger bleibt: eine App ohne Empfaenger ist
        // kein Zustand, den man versehentlich herstellen koennen soll.
        QAction* removeSliceAct = viewMenu->addAction(
            QStringLiteral("&Remove active slice"));
        removeSliceAct->setShortcut(
            QKeySequence(QStringLiteral("Ctrl+Shift+R")));
        removeSliceAct->setToolTip(QStringLiteral(
            "Close the active receiver. Its audio stops immediately."));
        connect(removeSliceAct, &QAction::triggered, this, [this]() {
            if (!m_radioModel) { return; }
            const QList<SliceModel*> list = m_radioModel->slices();
            if (list.size() <= 1) {
                showToast(tr("Der letzte Empfänger bleibt — sonst gäbe es "
                             "nichts mehr zu hören."),
                          ToastSeverity::Info, 4000);
                return;
            }
            SliceModel* s = m_radioModel->activeSlice();
            if (!s) { return; }
            const QString letter = QString(s->sliceLetter());
            m_radioModel->removeSlice(s->sliceIndex());
            showToast(tr("Empfänger %1 geschlossen.").arg(letter),
                      ToastSeverity::Info, 3000);
        });
    }

    {
        QAction* floatAct = viewMenu->addAction(
            QStringLiteral("&Float active pan…"));
        floatAct->setToolTip(QStringLiteral(
            "Detach the active panadapter into its own floating window"));
        connect(floatAct, &QAction::triggered, this, [this]() {
            if (m_panStack) {
                m_panStack->floatPanadapter(m_panStack->activePanId());
            }
        });
    }

    // ── Applet-Leiste neben oder unter den Panadapter ────────────────
    //
    // 2026-08-19, auf Ansage des Betreibers: „jedes Fenster sollte man
    // frei ändern können in der Größe. beim Panadapter sehe ich keine
    // Möglichkeit" — und als Zielbild Zeus Link, wo der Panadapter ein
    // mittleres Feld ist und andere Fenster darunter liegen.
    //
    // Die Ursache war nicht ein fehlender Griff, sondern seine RICHTUNG:
    // der Hauptsplitter steht waagerecht, also gibt es einen Griff
    // links/rechts und keinen oben/unten. Ein QSplitter kann seine
    // Richtung wechseln, und damit liegt die Applet-Leiste unter dem
    // Panadapter — mit ziehbarem Griff dazwischen.
    //
    // Das ist der erste Schritt zum Zielbild, nicht das Zielbild: dort
    // liegen auch Fenster ÜBER dem Panadapter. Das braucht eine echte
    // Kachelfläche und ist eine eigene Arbeit.
    {
        QAction* belowAct = viewMenu->addAction(
            QStringLiteral("Applets &below panadapter"));
        belowAct->setCheckable(true);
        belowAct->setToolTip(QStringLiteral(
            "Put the applet panel under the panadapter instead of beside "
            "it. The handle between them then resizes the panadapter's "
            "height."));
        belowAct->setChecked(m_mainSplitter
                             && m_mainSplitter->orientation() == Qt::Vertical);
        connect(belowAct, &QAction::toggled, this, [this](bool below) {
            setAppletPanelBelow(below);
        });

        // Rotor/Log unter den Panadapter: gibt der unteren Flaeche des
        // aeusseren Splitters Inhalt, und damit ist der Panadapter in
        // BEIDEN Achsen ziehbar — Breite am inneren Griff, Hoehe am
        // aeusseren (2026-08-19, auf Ansage des Betreibers).
        QAction* rotorBelowAct = viewMenu->addAction(
            QStringLiteral("&Rotor / Log below panadapter"));
        rotorBelowAct->setCheckable(true);
        rotorBelowAct->setToolTip(QStringLiteral(
            "Move the Rotor / Log panel out of its dock and under the "
            "panadapter. The handle between them then resizes the "
            "panadapter's height."));
        rotorBelowAct->setChecked(
            AppSettings::instance().value(QStringLiteral("RotorPanelBelow"),
                                          QStringLiteral("False")).toString()
            == QStringLiteral("True"));
        connect(rotorBelowAct, &QAction::toggled, this, [this](bool below) {
            setRotorPanelBelow(below);
        });
    }

    viewMenu->addSeparator();

    // From AetherSDR MainWindow.cpp:4098-4130 [@0cd4559]
    {
        QMenu* bandPlanMenu = viewMenu->addMenu(QStringLiteral("&Band Plan"));

        const int savedBpSize = AppSettings::instance()
                                    .value(QStringLiteral("BandPlanFontSize"),
                                           QStringLiteral("6"))
                                    .toInt();

        QActionGroup* bpGroup = new QActionGroup(bandPlanMenu);
        bpGroup->setExclusive(true);
        struct BpOption { const char* label; int pt; };
        const BpOption bpModes[] = {
            { "&Off",    0  },
            { "&Small",  6  },
            { "&Medium", 10 },
            { "&Large",  12 },
            { "&Huge",   16 },
        };
        for (const auto& opt : bpModes) {
            QAction* a = bandPlanMenu->addAction(QString::fromUtf8(opt.label));
            a->setCheckable(true);
            a->setChecked(opt.pt == savedBpSize);
            bpGroup->addAction(a);
            const int pt = opt.pt;
            connect(a, &QAction::triggered, this, [this, pt]() {
                if (activeSpectrumWidget()) {
                    activeSpectrumWidget()->setBandPlanFontSize(pt);
                }
                AppSettings::instance().setValue(QStringLiteral("BandPlanFontSize"),
                                                 QString::number(pt));
            });
        }

        bandPlanMenu->addSeparator();

        QActionGroup* planGroup = new QActionGroup(bandPlanMenu);
        planGroup->setExclusive(true);
        const auto& mgr = m_radioModel->bandPlanManager();
        const QString activePlan = mgr.activePlanName();
        for (const QString& name : mgr.availablePlans()) {
            QAction* a = bandPlanMenu->addAction(name);
            a->setCheckable(true);
            a->setChecked(name == activePlan);
            planGroup->addAction(a);
            connect(a, &QAction::triggered, this, [this, name]() {
                m_radioModel->bandPlanManagerMutable().setActivePlan(name);
            });
        }
    }

    {
        QMenu* displayModeMenu = viewMenu->addMenu(QStringLiteral("&Display Mode"));
        QAction* placeholder = displayModeMenu->addAction(QStringLiteral("(NYI placeholder)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    {
        QMenu* uiScaleMenu = viewMenu->addMenu(QStringLiteral("&UI Scale"));
        QActionGroup* scaleGroup = new QActionGroup(this);
        scaleGroup->setExclusive(true);
        const struct { const char* label; bool isDefault; } scales[] = {
            { "&75%",  false },
            { "&100%", true  },
            { "&125%", false },
            { "&150%", false },
            { "&175%", false },
            { "&200%", false },
        };
        for (const auto& s : scales) {
            QAction* a = uiScaleMenu->addAction(QString::fromUtf8(s.label));
            a->setCheckable(true);
            a->setEnabled(false);
            a->setToolTip(QStringLiteral("NYI — Phase X"));
            if (s.isDefault) { a->setChecked(true); }
            scaleGroup->addAction(a);
        }
    }

    // Phase 23 "View > Network Applets" submenu removed: TCI Server and
    // TCI Clients are now driven by AppletVisibilityController, accessible
    // via Containers > Applets and the panel's ☰ banner menu. Two
    // independent controls (old direct-setVisible + new controller path)
    // would drift out of sync. CAT + MIDI greyed placeholders deferred to
    // their feature phases (3K-1 / 3K-3) — re-add at that time wired
    // through the controller.

    viewMenu->addSeparator();

    m_darkThemeAction = viewMenu->addAction(QStringLiteral("&Dark Theme"));
    m_darkThemeAction->setCheckable(true);
    m_darkThemeAction->setChecked(true);
    m_darkThemeAction->setToolTip(QStringLiteral("Toggle dark theme (NYI — Phase X)"));
    connect(m_darkThemeAction, &QAction::toggled, this, [](bool /*on*/) {
        qCDebug(lcConnection) << "Dark Theme toggle NYI";
    });

    {
        QAction* minimalAction = viewMenu->addAction(QStringLiteral("&Minimal Mode"));
        minimalAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
        minimalAction->setEnabled(false);
        minimalAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // 2026-05-26 KG4VCF perf instrumentation: toggle the in-spectrum
    // perf overlay (paint/gap/fft/overlay timings + audio underruns
    // + UDP drops + memory pressure).  Persisted via AppSettings
    // "ShowPerfOverlay" by SpectrumWidget::setShowPerfOverlay; the
    // menu item just exposes the toggle.
    {
        QAction* perfAction = viewMenu->addAction(
            QStringLiteral("&Performance Overlay"));
        perfAction->setCheckable(true);
        perfAction->setChecked(activeSpectrumWidget() && activeSpectrumWidget()->showPerfOverlay());
        perfAction->setToolTip(QStringLiteral(
            "Show paint/gap/fft/overlay timings + audio underruns + UDP drops"
            " + memory pressure in a corner of the spectrum panel."
            "  Useful for diagnosing jitter under system load."));
        connect(perfAction, &QAction::toggled, this, [this](bool on) {
            if (activeSpectrumWidget()) {
                activeSpectrumWidget()->setShowPerfOverlay(on);
            }
        });
    }

    viewMenu->addSeparator();

    {
        QAction* kbAction = viewMenu->addAction(QStringLiteral("&Keyboard Shortcuts..."));
        kbAction->setEnabled(false);
        kbAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // DSP
    // =========================================================================
    QMenu* dspMenu = menuBar()->addMenu(QStringLiteral("&DSP"));

    // ── NR submenu — full slot bank, mutual exclusion via QActionGroup ─────
    // Mirrors VfoWidget's 7-button NR bank. Off/NR1/NR2/NR3/NR4/DFNR are
    // always present; MNR is gated by HAVE_MNR (macOS only) and BNR by
    // HAVE_BNR (NVIDIA build, currently never defined). Hidden actions
    // remain in the group so the activeNrChanged sync handler can find
    // them by index.
    {
        QMenu* nrMenu = dspMenu->addMenu(QStringLiteral("&NR"));
        m_nrGroup = new QActionGroup(this);
        m_nrGroup->setExclusive(true);

        using Slot = Longpath::NrSlot;
        struct Entry { const char* label; Slot slot; bool hidden; };
        const Entry nrSlots[] = {
            { "&Off",   Slot::Off,  false },
            { "NR&1",   Slot::NR1,  false },
            { "NR&2",   Slot::NR2,  false },
            { "NR&3",   Slot::NR3,  false },
            { "NR&4",   Slot::NR4,  false },
            { "&DFNR",  Slot::DFNR, false },
            { "&MNR",   Slot::MNR,
#ifdef HAVE_MNR
                false
#else
                true
#endif
            },
            { "&BNR",   Slot::BNR,
#ifdef HAVE_BNR
                false
#else
                true
#endif
            },
        };
        for (const auto& nr : nrSlots) {
            Slot slot = nr.slot;
            QAction* a = nrMenu->addAction(QString::fromUtf8(nr.label),
                this, [this, slot]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setActiveNr(slot); }
                });
            a->setCheckable(true);
            if (nr.hidden) { a->setVisible(false); }
            m_nrGroup->addAction(a);
        }
    }

    // ── NB submenu — Off/NB/NB2 mutual exclusion ───────────────────────────
    // Maps to SliceModel::setNbMode(NbMode). Mirrors VfoWidget's cycling
    // NB button (Off → NB → NB2 → Off) but as discrete menu items.
    {
        QMenu* nbMenu = dspMenu->addMenu(QStringLiteral("N&B"));
        m_nbGroup = new QActionGroup(this);
        m_nbGroup->setExclusive(true);

        using Mode = Longpath::NbMode;
        const struct { const char* label; Mode mode; } nbModes[] = {
            { "&Off",  Mode::Off },
            { "&NB",   Mode::NB  },
            { "NB&2",  Mode::NB2 },
        };
        for (const auto& nb : nbModes) {
            Mode mode = nb.mode;
            QAction* a = nbMenu->addAction(QString::fromUtf8(nb.label),
                this, [this, mode]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setNbMode(mode); }
                });
            a->setCheckable(true);
            m_nbGroup->addAction(a);
        }
    }

    // ── Single-toggle DSP actions ──────────────────────────────────────────
    // ANF now goes through SliceModel::anfEnabled (Phase 3F Sub-Epic J
    // Task 3) and is kept in sync below. SNB / APF / BIN go through
    // SliceModel and are synced too.
    {
        QAction* anfAction = dspMenu->addAction(QStringLiteral("&ANF"));
        anfAction->setCheckable(true);
        connect(anfAction, &QAction::toggled, this, [this](bool on) {
            // A control attached to no flag targets the active slice, which
            // is whichever flag the operator last clicked. Resolved at
            // invocation, not captured, so it follows focus.
            if (SliceModel* slice = m_radioModel->activeSlice()) {
                slice->setAnfEnabled(on);
            }
        });

        // Reflect the active slice when focus moves, without re-emitting
        // toggled back into the handler above.
        connect(m_radioModel, &RadioModel::activeSliceChanged, this,
                [this, anfAction](int) {
            if (SliceModel* slice = m_radioModel->activeSlice()) {
                QSignalBlocker block(anfAction);
                anfAction->setChecked(slice->anfEnabled());
            }
        });
    }

    m_snbAction = dspMenu->addAction(QStringLiteral("&SNB"));
    m_snbAction->setCheckable(true);
    connect(m_snbAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setSnbEnabled(on); }
    });

    m_apfAction = dspMenu->addAction(QStringLiteral("AP&F"));
    m_apfAction->setCheckable(true);
    connect(m_apfAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setApfEnabled(on); }
    });

    m_binAction = dspMenu->addAction(QStringLiteral("B&IN"));
    m_binAction->setCheckable(true);
    connect(m_binAction, &QAction::toggled, this, [this](bool on) {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setBinauralEnabled(on); }
    });

    // TNF: enable or bypass every notch at once. Global rather than per-slice
    // because the notch list itself is global (design decision D1), which is
    // also how Thetis models it: TNFActive is a single flag despite the
    // per-rx command shape (console.cs:52317-52326 [v2.10.3.15], where GetMNF
    // is documented "mnf enabled globally").
    m_tnfAction = dspMenu->addAction(QStringLiteral("&TNF"));
    m_tnfAction->setCheckable(true);
    m_tnfAction->setShortcut(tnfToggleShortcut());
    m_tnfAction->setToolTip(
        QStringLiteral("Enable or bypass all tunable notch filters"));
    if (NotchModel* notches = m_radioModel->notchModel()) {
        m_tnfAction->setChecked(notches->globalEnabled());
        connect(m_tnfAction, &QAction::toggled,
                notches, &NotchModel::setGlobalEnabled);
        // The blocker is what keeps one operator gesture from reaching the
        // model twice: setChecked re-emits toggled, which would write the
        // model again.
        connect(notches, &NotchModel::globalEnabledChanged,
                m_tnfAction, [this](bool on) {
            QSignalBlocker block(m_tnfAction);
            m_tnfAction->setChecked(on);
        });
    }

    dspMenu->addSeparator();

    {
        // AGC submenu — checkable exclusive via QActionGroup.
        // AGCMode enum from WdspTypes.h: Off=0, Long=1, Slow=2, Med=3, Fast=4, Custom=5
        // From Thetis dsp.cs AGCMode — all 6 modes wired to SliceModel::setAgcMode().
        QMenu* agcMenu = dspMenu->addMenu(QStringLiteral("&AGC"));
        m_agcGroup = new QActionGroup(this);
        m_agcGroup->setExclusive(true);

        const struct { const char* label; AGCMode mode; } agcModes[] = {
            { "&Off",    AGCMode::Off    },
            { "&Long",   AGCMode::Long   },
            { "&Slow",   AGCMode::Slow   },
            { "&Med",    AGCMode::Med    },
            { "&Fast",   AGCMode::Fast   },
            { "&Custom", AGCMode::Custom },
        };
        for (const auto& agc : agcModes) {
            AGCMode agcMode = agc.mode;
            QAction* a = agcMenu->addAction(QString::fromUtf8(agc.label),
                this, [this, agcMode]() {
                    SliceModel* slice = m_radioModel->activeSlice();
                    if (slice) { slice->setAgcMode(agcMode); }
                });
            a->setCheckable(true);
            m_agcGroup->addAction(a);
        }

        // Sync AGC checked state when SliceModel changes
        connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
            if (index != 0) { return; }
            SliceModel* slice = m_radioModel->activeSlice();
            if (!slice) { return; }
            connect(slice, &SliceModel::agcModeChanged, this, [this](AGCMode mode) {
                QList<QAction*> acts = m_agcGroup->actions();
                const AGCMode agcOrder[] = {
                    AGCMode::Off, AGCMode::Long, AGCMode::Slow,
                    AGCMode::Med, AGCMode::Fast, AGCMode::Custom
                };
                for (int i = 0; i < acts.size() && i < 6; ++i) {
                    acts[i]->setChecked(agcOrder[i] == mode);
                }
            });
        });
    }

    // ── Sync NR / NB / SNB / APF / BIN checked state from slice 0 ──────────
    // Mirrors the AGC sync pattern above. Wired via sliceAdded so the
    // connection survives slice teardown/re-create. ANF is not synced here:
    // its check state follows activeSliceChanged instead (wired beside the
    // action's creation above), since it must track whichever slice is
    // active, not just slice 0.
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }

        // NR submenu sync — actions appended to m_nrGroup in this fixed order.
        const Longpath::NrSlot nrOrder[] = {
            Longpath::NrSlot::Off,  Longpath::NrSlot::NR1,
            Longpath::NrSlot::NR2,  Longpath::NrSlot::NR3,
            Longpath::NrSlot::NR4,  Longpath::NrSlot::DFNR,
            Longpath::NrSlot::MNR,  Longpath::NrSlot::BNR,
        };
        auto syncNr = [this, nrOrder](Longpath::NrSlot slot) {
            QList<QAction*> acts = m_nrGroup->actions();
            const int n = static_cast<int>(std::size(nrOrder));
            for (int i = 0; i < acts.size() && i < n; ++i) {
                QSignalBlocker b(acts[i]);
                acts[i]->setChecked(nrOrder[i] == slot);
            }
        };
        syncNr(slice->activeNr());
        connect(slice, &SliceModel::activeNrChanged, this, syncNr);

        // NB submenu sync — three actions in m_nbGroup: Off, NB, NB2.
        const Longpath::NbMode nbOrder[] = {
            Longpath::NbMode::Off, Longpath::NbMode::NB, Longpath::NbMode::NB2,
        };
        auto syncNb = [this, nbOrder](Longpath::NbMode mode) {
            QList<QAction*> acts = m_nbGroup->actions();
            for (int i = 0; i < acts.size() && i < 3; ++i) {
                QSignalBlocker b(acts[i]);
                acts[i]->setChecked(nbOrder[i] == mode);
            }
        };
        syncNb(slice->nbMode());
        connect(slice, &SliceModel::nbModeChanged, this, syncNb);

        // Single-toggle initial sync.
        { QSignalBlocker b(m_snbAction); m_snbAction->setChecked(slice->snbEnabled()); }
        { QSignalBlocker b(m_apfAction); m_apfAction->setChecked(slice->apfEnabled()); }
        { QSignalBlocker b(m_binAction); m_binAction->setChecked(slice->binauralEnabled()); }

        connect(slice, &SliceModel::snbEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_snbAction);
            m_snbAction->setChecked(v);
        });
        connect(slice, &SliceModel::apfEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_apfAction);
            m_apfAction->setChecked(v);
        });
        connect(slice, &SliceModel::binauralEnabledChanged, this, [this](bool v) {
            QSignalBlocker b(m_binAction);
            m_binAction->setChecked(v);
        });
    });

    dspMenu->addSeparator();

    {
        QAction* eqAction = dspMenu->addAction(QStringLiteral("&Equalizer..."));
        eqAction->setEnabled(false);
        eqAction->setToolTip(QStringLiteral("NYI — Phase 3I-3"));
    }
    {
        // Phase 3M-4 Task 8: wire DSP > PureSignal... to the modeless dialog.
        // Both this entry and Tools > PureSignal... below open the same
        // singleton dialog (DSP for discoverability under the existing
        // DSP-feature menu, Tools per the per-task plan §8.4).
        QAction* psAction = dspMenu->addAction(QStringLiteral("&PureSignal..."));
        psAction->setToolTip(
            QStringLiteral("Open the PureSignal pre-distortion control dialog."));
        connect(psAction, &QAction::triggered,
                this, &MainWindow::openPureSignalDialog);
    }
    {
        QAction* divAction = dspMenu->addAction(QStringLiteral("&Diversity..."));
        divAction->setEnabled(false);
        divAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // BAND
    // =========================================================================
    QMenu* bandMenu = menuBar()->addMenu(QStringLiteral("&Band"));

    {
        QMenu* hfMenu = bandMenu->addMenu(QStringLiteral("&HF"));
        // Frequency values from Thetis console.cs band definitions
        const struct { const char* label; double freqHz; } hfBands[] = {
            { "160m (1.8 MHz)",    1.8e6   },
            { "80m (3.5 MHz)",     3.5e6   },
            { "60m (5.3 MHz)",     5.3e6   },
            { "40m (7.0 MHz)",     7.0e6   },
            { "30m (10.1 MHz)",   10.1e6   },
            { "20m (14.0 MHz)",   14.0e6   },
            { "17m (18.068 MHz)", 18.068e6 },
            { "15m (21.0 MHz)",   21.0e6   },
            { "12m (24.89 MHz)",  24.89e6  },
            { "10m (28.0 MHz)",   28.0e6   },
            { "6m (50.0 MHz)",    50.0e6   },
        };
        for (const auto& band : hfBands) {
            double freq = band.freqHz;
            hfMenu->addAction(QString::fromUtf8(band.label), this, [this, freq]() {
                SliceModel* slice = m_radioModel->activeSlice();
                if (slice) { slice->setFrequency(freq); }
            });
        }
    }

    {
        QMenu* vhfMenu = bandMenu->addMenu(QStringLiteral("&VHF"));
        QAction* placeholder = vhfMenu->addAction(QStringLiteral("(NYI — Phase X)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("VHF bands NYI — Phase X"));
    }

    {
        QMenu* genMenu = bandMenu->addMenu(QStringLiteral("&GEN"));
        QAction* placeholder = genMenu->addAction(QStringLiteral("(NYI — Phase X)"));
        placeholder->setEnabled(false);
        placeholder->setToolTip(QStringLiteral("GEN coverage NYI — Phase X"));
    }

    bandMenu->addAction(QStringLiteral("&WWV (10.0 MHz)"), this, [this]() {
        SliceModel* slice = m_radioModel->activeSlice();
        if (slice) { slice->setFrequency(10.0e6); }
    });

    bandMenu->addSeparator();

    {
        QAction* bandStackAction = bandMenu->addAction(QStringLiteral("Band &Stacking..."));
        bandStackAction->setEnabled(false);
        bandStackAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    // =========================================================================
    // MODE
    // =========================================================================
    QMenu* modeMenu = menuBar()->addMenu(QStringLiteral("&Mode"));

    // 12 Thetis-faithful modes + the NereusSDR-native RADE-U / RADE-L
    // entries (Phase 3R L3).  Display order: LSB, USB, DSB, CWL, CWU,
    // AM, SAM, FM, DIGL, DIGU, DRM, SPEC, RADE-U, RADE-L.  Maps to
    // DSPMode enum values from WdspTypes.h.
    // From Thetis dsp.cs DSPMode enum — enum values used directly, not indices.
    // RADE-U / RADE-L are NereusSDR-native entries (DSPMode::RADE_U = 12,
    // DSPMode::RADE_L = 13; not WDSP modes; routes the slice through
    // RadeChannel).  Like USB/LSB, RADE has upper/lower sideband
    // variants with mirrored 1700 Hz passbands.
    const struct { const char* label; DSPMode mode; } modes[] = {
        { "LSB",    DSPMode::LSB    },
        { "USB",    DSPMode::USB    },
        { "DSB",    DSPMode::DSB    },
        { "CWL",    DSPMode::CWL    },
        { "CWU",    DSPMode::CWU    },
        { "AM",     DSPMode::AM     },
        { "SAM",    DSPMode::SAM    },
        { "FM",     DSPMode::FM     },
        { "DIGL",   DSPMode::DIGL   },
        { "DIGU",   DSPMode::DIGU   },
        { "DRM",    DSPMode::DRM    },
        { "SPEC",   DSPMode::SPEC   },
        { "RADE-U", DSPMode::RADE_U },  // Phase 3R L3, NereusSDR-native upper
        { "RADE-L", DSPMode::RADE_L },  // Phase 3R L3, NereusSDR-native lower
    };

    m_modeActionGroup = new QActionGroup(this);
    m_modeActionGroup->setExclusive(true);

    for (int i = 0; i < 14; ++i) {
        DSPMode mode = modes[i].mode;
        QAction* act = modeMenu->addAction(QString::fromUtf8(modes[i].label),
                                           this, [this, mode]() {
            SliceModel* slice = m_radioModel->activeSlice();
            if (slice) { slice->setDspMode(mode); }
        });
        act->setCheckable(true);
        m_modeActionGroup->addAction(act);
        m_modeActions[i] = act;
    }

    // Sync checked mode action when SliceModel reports a mode change.
    // Connection is deferred until slice 0 is available (sliceAdded signal).
    connect(m_radioModel, &RadioModel::sliceAdded, this, [this](int index) {
        if (index != 0) { return; }
        SliceModel* slice = m_radioModel->activeSlice();
        if (!slice) { return; }
        connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
            const DSPMode displayOrder[] = {
                DSPMode::LSB, DSPMode::USB, DSPMode::DSB, DSPMode::CWL,
                DSPMode::CWU, DSPMode::AM,  DSPMode::SAM,  DSPMode::FM,
                DSPMode::DIGL, DSPMode::DIGU, DSPMode::DRM, DSPMode::SPEC,
                DSPMode::RADE_U,  // Phase 3R L3, index 12
                DSPMode::RADE_L,  // Phase 3R L3, index 13
            };
            for (int i = 0; i < 14; ++i) {
                if (m_modeActions[i]) {
                    m_modeActions[i]->setChecked(displayOrder[i] == mode);
                }
            }
        });
    });

    // =========================================================================
    // CONTAINERS
    // =========================================================================
    QMenu* containersMenu = menuBar()->addMenu(QStringLiteral("Contai&ners"));

    {
        // New Container: creates a floating container with a fresh MeterWidget,
        // then opens the settings dialog so the user can pick a preset or add items.
        // From Thetis setup.cs:24358 — btnAddRX1Container_Click → AddMeterContainer(1, false)
        QAction* newContAction = containersMenu->addAction(QStringLiteral("&New Container..."));
        connect(newContAction, &QAction::triggered, this, [this]() {
            if (!m_containerManager) { return; }

            ContainerWidget* c = m_containerManager->createContainer(1, DockMode::Floating);
            c->setNotes(QStringLiteral("Meter"));

            // Give it a MeterWidget as content (replaces the default placeholder label)
            MeterWidget* meter = new MeterWidget();
            c->setContent(meter);

            // Open settings dialog so user can configure it
            ContainerSettingsDialog dialog(c, this, m_containerManager);
            if (dialog.exec() == QDialog::Rejected) {
                // User cancelled — destroy the container
                m_containerManager->destroyContainer(c->id());
            }
        });
    }
    {
        // Phase 3G-6 block 6 commit 45: dynamic "Edit Container ▸"
        // submenu populated from ContainerManager::allContainers().
        // Replaces the old static "Container Settings..." action that
        // could only edit Container #0. Rebuilds on
        // containerAdded / containerRemoved / containerTitleChanged
        // signals so menu entries stay in sync with the live
        // container set.
        m_editContainerMenu = containersMenu->addMenu(
            QStringLiteral("&Edit Container"));
        rebuildEditContainerSubmenu();
        if (m_containerManager) {
            connect(m_containerManager, &ContainerManager::containerAdded,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerRemoved,
                    this, [this](const QString&) { rebuildEditContainerSubmenu(); });
            connect(m_containerManager, &ContainerManager::containerTitleChanged,
                    this, [this](const QString&, const QString&) {
                rebuildEditContainerSubmenu();
            });
        }
    }
    {
        // Phase 3G-6 block 6 commit 46: Reset Default Layout — now
        // functional. Destroys every non-panel container and
        // rebuilds the submenu.
        QAction* resetAction = containersMenu->addAction(QStringLiteral("&Reset Default Layout"));
        connect(resetAction, &QAction::triggered, this,
                &MainWindow::resetDefaultLayout);
    }

    // ── Freie Flaeche, dauerhaft ─────────────────────────────────────
    //
    // Der Schalter, nicht die einmalige Aktion. Steht er an, bekommt
    // JEDES Fenster, das ueber das Plus dazukommt, sofort eine eigene
    // Lage — statt im Stapel zu landen und dort auf ein Freistellen von
    // Hand zu warten.
    if (m_appletPanel) {
        m_freeCanvasAction = containersMenu->addAction(
            QStringLiteral("Neue Fenster gleich ablösen"));
        m_freeCanvasAction->setCheckable(true);
        m_freeCanvasAction->setChecked(m_freeCanvasMode);
        m_freeCanvasAction->setToolTip(QStringLiteral(
            "Jedes Fenster bekommt eine eigene Lage auf der Flaeche — "
            "ziehen an der Titelleiste, Groesse an der Ecke, Schloss im "
            "Kopf. Neue Fenster kommen gleich so."));
        connect(m_freeCanvasAction, &QAction::toggled, this, [this](bool on) {
            setFreeCanvasMode(on);
            if (on) { moveAllAppletsToCanvas(); }
        });
    }

    // ── Alles frei stellen ───────────────────────────────────────────
    //
    // Der Weg zu „alles x-beliebig verschiebbar" in einem Klick. Jedes
    // Applet verlaesst den Stapel und bekommt eine Kachel mit eigener
    // Lage — ziehen an der Titelleiste, Groesse an der Ecke, Schloss
    // im Kopf.
    if (m_appletPanel) {
        QAction* freeAll = containersMenu->addAction(
            QStringLiteral("Alle Fenster als eigene Fenster ablösen"));
        freeAll->setToolTip(QStringLiteral(
            "Jedes Fenster wird ein eigenes Betriebssystem-Fenster: "
            "ueberall hinschiebbar, in der Groesse aenderbar, auch auf "
            "einen zweiten Schirm. Zurueck ueber das ✕ am Fenster."));
        connect(freeAll, &QAction::triggered,
                this, &MainWindow::moveAllAppletsToCanvas);
    }

    // ── Alles ausblenden ─────────────────────────────────────────────
    //
    // Der Weg zur leeren Flaeche, ohne Einstellungen zu loeschen. Die
    // neue Vorgabe (alles aus) gilt nur fuer eine frische
    // Einstellungsdatei; wer schon eine Anordnung hat, behaelt sie —
    // und braucht deshalb einen Knopf, der sie EINMAL wegraeumt.
    //
    // Umkehrbar: das Plus unten rechts holt jedes Fenster zurueck.
    if (m_appletVis) {
        QAction* clearAll = containersMenu->addAction(
            QStringLiteral("Alle Fenster ausblenden"));
        clearAll->setToolTip(QStringLiteral(
            "Leere Flaeche. Mit dem + unten rechts holst du jedes "
            "Fenster einzeln zurueck."));
        connect(clearAll, &QAction::triggered, this, [this]() {
            for (const QString& id : m_appletVis->registeredIds()) {
                // Rahmen bleibt: Knopfleiste und Statuszeile sind keine
                // Fenster, die man hinzufuegt.
                if (id == QLatin1String(kChromeOverlayId)) { continue; }
                if (id == QLatin1String(kChromeStatusId))  { continue; }
                m_appletVis->setVisible(id, false);
            }
        });
    }

    containersMenu->addSeparator();

    // ── Containers > Applets section ─────────────────────────────────────
    // Show/hide toggles for each currently-wired applet. Backed by
    // m_appletVis (AppletVisibilityController). Two-way sync with the
    // ☰ menu on AppletPanelWidget happens via the controller's
    // visibilityChanged signal.
    //
    // Predecessor: dead lambda was disabled in 25597df because its 7
    // entries were all ghost applets. The new section ships only
    // currently-wired applets. Add new entries here as additional
    // applets ship (default visible per design §5.2).
    if (m_appletVis) {
        // Section header. addSection is the idiomatic Qt API; falls back
        // gracefully on platforms where it renders as a plain label.
        containersMenu->addSection(QStringLiteral("Applets"));

        for (const QString& id : m_appletVis->registeredIds()) {
            QAction* act = containersMenu->addAction(
                m_appletVis->displayName(id));
            act->setCheckable(true);
            act->setChecked(m_appletVis->isVisible(id));
            // Grey out when the applet is currently unavailable (e.g.
            // Amp/Tuner when 4O3A is disabled). Check state still
            // reflects the user preference.
            act->setEnabled(m_appletVis->isAvailable(id));
            // User-visible tooltip — plain English, no source cites.
            act->setToolTip(QStringLiteral("Show or hide the %1 applet")
                            .arg(m_appletVis->displayName(id)));

            connect(act, &QAction::toggled, this, [this, id](bool checked) {
                if (m_appletVis) { m_appletVis->setVisible(id, checked); }
                // Betreiber 2026-08-30, ueber einen Regressionstest
                // gefunden: dieser Weg fehlte im Gegensatz zum
                // Ausblenden-Kreuz (appletHideRequested oben) das
                // sofortige captureIntoCurrent()+save() -- ein Haken hier
                // ueberlebte bislang nur, wenn die App normal ueber
                // closeEvent() beendet wurde, sonst ging er beim
                // naechsten Start wieder verloren, lautlos.
                if (m_layoutProfiles) {
                    m_layoutProfiles->captureIntoCurrent();
                    m_layoutProfiles->save();
                }
            });
            m_topMenuAppletActions.insert(id, act);
        }

        // Sync checkmark when the controller's state changes (e.g. via
        // the banner ☰ menu in Task 6). QSignalBlocker prevents
        // recursive toggle.
        connect(m_appletVis, &AppletVisibilityController::visibilityChanged,
                this, [this](const QString& id, bool visible) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                QSignalBlocker block(act);
                act->setChecked(visible);
            }
        });

        // Grey/un-grey top-menu entries when an applet's availability
        // changes (e.g. 4O3A master toggle flipped in Setup).
        connect(m_appletVis, &AppletVisibilityController::availabilityChanged,
                this, [this](const QString& id, bool available) {
            if (auto* act = m_topMenuAppletActions.value(id, nullptr)) {
                act->setEnabled(available);
            }
        });
    }

    // =========================================================================
    // TOOLS
    // =========================================================================
    QMenu* toolsMenu = menuBar()->addMenu(QStringLiteral("&Tools"));

    // Phase 3J-2 H1: Spot Hub (DX cluster / RBN / POTA / WSJT-X / FreeDV /
    // PSK Reporter). Modeless singleton dialog; lazy-constructed in
    // openSpotHub() with all 7 clients + SpotModel + DxccColorProvider
    // injected from RadioModel.
    {
        QAction* spotHubAction = toolsMenu->addAction(QStringLiteral("Spot &Hub..."));
        spotHubAction->setObjectName(QStringLiteral("actSpotHub"));
        spotHubAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+S")));
        spotHubAction->setToolTip(QStringLiteral(
            "Open the Spot Hub dialog (DX cluster, RBN, POTA, WSJT-X, "
            "FreeDV Reporter, PSK Reporter, Spot Collector)."));
        connect(spotHubAction, &QAction::triggered, this, &MainWindow::openSpotHub);
    }

    // Rotor dial — step 1 of the logbook/rotator work. A modeless
    // window rather than a dock or a splitter pane: the surrounding
    // layout stays untouched while the instrument itself is reviewed.
    {
        QAction* rotorAction = toolsMenu->addAction(QStringLiteral("&Rotor..."));
        rotorAction->setObjectName(QStringLiteral("actRotorDial"));
        // User-visible tooltip — plain English, no source cites.
        rotorAction->setToolTip(QStringLiteral(
            "Open the antenna rotator dial."));
        connect(rotorAction, &QAction::triggered,
                this, &MainWindow::openRotorDial);
    }

    // The channel strip. Next to the voice check because they are two
    // halves of one job: one measures the voice, the other changes it.
    {
        QAction* stripAction =
            toolsMenu->addAction(QStringLiteral("&Channel strip..."));
        stripAction->setObjectName(QStringLiteral("actChannelStrip"));
        stripAction->setToolTip(QStringLiteral(
            "Gate, EQ, de-esser, compressor, tube, exciter, reverb and "
            "limiter, ahead of the radio's own processing."));
        connect(stripAction, &QAction::triggered,
                this, &MainWindow::openChannelStrip);
    }

    // Transmit audio, measured rather than guessed at. Under Tools
    // because it is a thing you go and do, not a control you leave
    // sitting on screen.
    {
        QAction* voiceAction =
            toolsMenu->addAction(QStringLiteral("&Voice check..."));
        voiceAction->setObjectName(QStringLiteral("actVoiceCheck"));
        voiceAction->setToolTip(QStringLiteral(
            "Hear yourself off air, record fifteen seconds, and get an "
            "equaliser suggestion measured from your own voice."));
        connect(voiceAction, &QAction::triggered,
                this, &MainWindow::openVoiceCheck);
    }

    // Setting the rotator up, reachable without first finding the dock
    // and the small button inside it. This is also where Hamlib gets
    // installed, so it is the first place an operator with a rotator
    // and no rotctld needs to arrive at — putting it behind two other
    // discoveries was the reason the feature went unused.
    {
        QAction* rotorSetupAction =
            toolsMenu->addAction(QStringLiteral("Rotator &setup..."));
        rotorSetupAction->setObjectName(QStringLiteral("actRotorSetup"));
        rotorSetupAction->setToolTip(QStringLiteral(
            "Choose the rotator controller, install Hamlib if it is "
            "missing, and connect."));
        connect(rotorSetupAction, &QAction::triggered,
                this, &MainWindow::openRotorSetup);
    }

    // The antenna window. Next to the rotator setup because the two are
    // the same errand — the mast and what is on top of it — and because
    // an operator who has just found one will look here for the other.
    {
        QAction* antAction =
            toolsMenu->addAction(QStringLiteral("&Antenna..."));
        antAction->setObjectName(QStringLiteral("actAntenna"));
        antAction->setToolTip(QStringLiteral(
            "Open a sweep from your analyser and see where the antenna "
            "is resonant, and how much wire to add or remove."));
        connect(antAction, &QAction::triggered,
                this, &MainWindow::openAntennaWindow);
    }

    // The logbook, reachable without going through the dock. It is the
    // same window the dock's button opens — one window over one file.
    {
        QAction* logAction =
            toolsMenu->addAction(QStringLiteral("&Logbook..."));
        logAction->setObjectName(QStringLiteral("actLogbook"));
        // Not Ctrl+L: the pan-layout dialog already has it, and two
        // actions on one shortcut means one of them silently never
        // fires.
        logAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+L")));
        logAction->setToolTip(QStringLiteral(
            "Open the logbook: search, correct, import, export and map."));
        connect(logAction, &QAction::triggered,
                this, &MainWindow::openLogbookWindow);
    }

    // Where logged contacts can be sent on to. Separate from the QRZ
    // account entry, because these are separate services with separate
    // credentials and one combined dialog would invite mixing them up.
    {
        QAction* svcAction =
            toolsMenu->addAction(QStringLiteral("&Logging services..."));
        svcAction->setObjectName(QStringLiteral("actLoggingServices"));
        svcAction->setToolTip(QStringLiteral(
            "Set up Cloudlog / Wavelog and a local logger such as Log4OM."));
        connect(svcAction, &QAction::triggered,
                this, &MainWindow::openLoggingServicesDialog);
    }

    // Phase 3J-2 H1: FreeDV Reporter live station map.
    // Modeless singleton dialog; lazy-constructed in openFreeDVReporter()
    // with FreeDVStationModel + FreeDVReporterClient from RadioModel.
    {
        QAction* fdvAction = toolsMenu->addAction(QStringLiteral("&FreeDV Reporter..."));
        fdvAction->setObjectName(QStringLiteral("actFreeDVReporter"));
        fdvAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+R")));
        fdvAction->setToolTip(QStringLiteral(
            "Open the FreeDV Reporter dialog (live stations on qso.freedv.org)."));
        connect(fdvAction, &QAction::triggered, this, &MainWindow::openFreeDVReporter);
    }

    toolsMenu->addSeparator();

    // TX Equalizer: modeless singleton dialog (Phase 3M-3a-i Batch 3 A.1).
    {
        QAction* txEqAction = toolsMenu->addAction(QStringLiteral("TX &Equalizer..."));
        txEqAction->setToolTip(QStringLiteral(
            "Open the 10-band TX EQ dialog (preamp + 10 band gains + center frequencies)."));
        connect(txEqAction, &QAction::triggered, this, [this]() {
            if (!m_radioModel) { return; }
            TxEqDialog* dlg = TxEqDialog::instance(m_radioModel, this);
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    // Phase 3M-4 Task 8: PureSignal — modeless singleton dialog.
    // Same target as DSP > PureSignal... above; this entry per design doc
    // §4 #3 ("Tools > PureSignal..." for higher discoverability than the
    // DSP-buried path).
    {
        m_actPureSignal = toolsMenu->addAction(QStringLiteral("&PureSignal..."));
        m_actPureSignal->setToolTip(QStringLiteral(
            "Open the PureSignal pre-distortion control dialog."));
        connect(m_actPureSignal, &QAction::triggered,
                this, &MainWindow::openPureSignalDialog);
    }

    // Phase 3F Sub-Epic G T4: Diversity dialog (bench minimum).
    // Lazy-singleton: one dialog instance per RadioModel, kept alive
    // across close so the dialog's own state survives a re-open
    // (SliceModel persistence handles real settings round-trip in T2).
    {
        QAction* divDlgAct = toolsMenu->addAction(QStringLiteral("&Diversity..."));
        divDlgAct->setShortcut(QKeySequence(QStringLiteral("Ctrl+Shift+D")));
        divDlgAct->setToolTip(QStringLiteral(
            "Open the Diversity dialog (Slice A: enable, phase, gain)."));
        connect(divDlgAct, &QAction::triggered, this, [this]() {
            static DiversityDialog* dlg = nullptr;
            if (!dlg) {
                dlg = new DiversityDialog(m_radioModel, this);
                dlg->setAttribute(Qt::WA_DeleteOnClose, false);
            }
            dlg->show();
            dlg->raise();
            dlg->activateWindow();
        });
    }

    toolsMenu->addSeparator();

    {
        QAction* cwxAction = toolsMenu->addAction(QStringLiteral("C&WX..."));
        cwxAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_X));
        cwxAction->setEnabled(false);
        cwxAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* memAction = toolsMenu->addAction(QStringLiteral("&Memory Manager..."));
        memAction->setEnabled(false);
        memAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* catAction = toolsMenu->addAction(QStringLiteral("&CAT Control..."));
        catAction->setEnabled(false);
        catAction->setToolTip(QStringLiteral("NYI — Phase 3K"));
    }
    {
        // Phase 23: TCI Server action — enabled, opens Setup → TCI Server.
        QAction* tciAction = toolsMenu->addAction(QStringLiteral("&TCI Server..."));
        tciAction->setToolTip(QStringLiteral("Open TCI Server Setup"));
        connect(tciAction, &QAction::triggered, this, &MainWindow::openTciSetupPage);
    }
    {
        QAction* daxAction = toolsMenu->addAction(QStringLiteral("&VAX Audio..."));
        daxAction->setEnabled(false);
        daxAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* midiAction = toolsMenu->addAction(QStringLiteral("&MIDI Mapping..."));
        midiAction->setEnabled(false);
        midiAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* macroAction = toolsMenu->addAction(QStringLiteral("Macro &Buttons..."));
        macroAction->setEnabled(false);
        macroAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    toolsMenu->addSeparator();

    {
        QAction* netDiagAction = toolsMenu->addAction(QStringLiteral("&Network Diagnostics..."));
        netDiagAction->setEnabled(false);
        netDiagAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    toolsMenu->addAction(QStringLiteral("&Support Bundle..."), this,
                         &MainWindow::showSupportDialog);

    // Phase 3F closeout — operator-visible test entries for Sub-Epic E
    // consumer surfaces. Lets the user verify the toast and TX-bound
    // re-route dialog render correctly without needing to trigger a real
    // antenna conflict. Removed when full conflict-detection state machine
    // lands and the test surfaces become unnecessary.
    toolsMenu->addSeparator();
    {
        QAction* testToastAct = toolsMenu->addAction(
            QStringLiteral("Test antenna switch &toast"));
        testToastAct->setToolTip(
            QStringLiteral("Phase 3F closeout: fire the AntennaSwitchToast surface "
                            "for visual verification. Real auto-switch firing wires "
                            "when the conflict-detection state machine ships."));
        connect(testToastAct, &QAction::triggered, this, [this]() {
            if (m_radioModel) {
                m_radioModel->emitAntennaAutoSwitched(
                    0, QStringLiteral("ANT1"), QStringLiteral("ANT2"));
            }
        });
    }
    {
        QAction* testReRouteAct = toolsMenu->addAction(
            QStringLiteral("Test TX-bound &re-route dialog"));
        testReRouteAct->setToolTip(
            QStringLiteral("Phase 3F closeout: open the TxBoundConfirmDialog surface "
                            "for visual verification. Real emission from addSliceOnPan "
                            "wires when the conflict-detection state machine ships."));
        connect(testReRouteAct, &QAction::triggered, this, [this]() {
            if (m_radioModel) {
                m_radioModel->requestTxBoundReRoute(
                    QStringLiteral("ANT2"), QStringLiteral("ANT1"));
            }
        });
    }

    // =========================================================================
    // HELP
    // =========================================================================
    QMenu* helpMenu = menuBar()->addMenu(QStringLiteral("&Help"));

    {
        QAction* gettingStartedAction = helpMenu->addAction(QStringLiteral("&Getting Started"));
        gettingStartedAction->setEnabled(false);
        gettingStartedAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* helpAction = helpMenu->addAction(QStringLiteral("&NereusSDR Help"));
        helpAction->setEnabled(false);
        helpAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }
    {
        QAction* dataModesAction = helpMenu->addAction(QStringLiteral("Understanding &Data Modes"));
        dataModesAction->setEnabled(false);
        dataModesAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    helpMenu->addSeparator();

    {
        QAction* whatsNewAction = helpMenu->addAction(QStringLiteral("What's &New"));
        whatsNewAction->setEnabled(false);
        whatsNewAction->setToolTip(QStringLiteral("NYI — Phase X"));
    }

    helpMenu->addSeparator();

#if defined(Q_OS_LINUX)
    // PipeWire / pactl diagnostic dialog — Linux-only feature.
    helpMenu->addAction(QStringLiteral("&Diagnose audio backend…"),
                        this, &MainWindow::showAudioDiagnoseDialog);

    helpMenu->addSeparator();
#endif

    helpMenu->addAction(QStringLiteral("&About NereusSDR"), this, [this]() {
        AboutDialog dlg(this);
        dlg.exec();
    });

    // Phase 3J-2 H1: Ctrl+Shift+K clears all rows in SpotModel. Mirrors the
    // "Clear All Spots" button on SpotHubDialog's Display tab so the user
    // can wipe stale spots without opening the dialog. Application-scoped
    // QShortcut so it fires regardless of which child widget has focus.
    {
        auto* clearSpotsShortcut = new QShortcut(
            QKeySequence(QStringLiteral("Ctrl+Shift+K")), this);
        clearSpotsShortcut->setContext(Qt::ApplicationShortcut);
        connect(clearSpotsShortcut, &QShortcut::activated, this, [this]() {
            if (m_radioModel && m_radioModel->spotModel()) {
                m_radioModel->spotModel()->clear();
            }
        });
    }

    // Betreiber 2026-09-02: "kann ich das Fenster auch nicht kleiner und
    // größer machen" -- enterBorderlessFullSize() (2026-09-01) nimmt dem
    // Fenster Qt::FramelessWindowHint weg und damit die nativen
    // Ziehgriffe; es gibt aber keinen Knopf und keine Taste, die zurueck
    // in den normalen, groessenveraenderbaren Rahmen fuehrt -- und der
    // Zustand kann schon beim Start lautlos aus einem gespeicherten
    // Profil (fullScreen=true) kommen, ohne dass irgendetwas auf dem
    // Schirm sagt, warum das Fenster jetzt starr ist. Escape ist der
    // Fluchtweg, den jedes Vollbild kennt -- Standardkontext
    // (Qt::WindowShortcut), damit ein fokussierter Dialog sein eigenes
    // Escape (schliessen) zuerst bekommt.
    {
        auto* exitBorderlessShortcut = new QShortcut(
            QKeySequence(Qt::Key_Escape), this);
        connect(exitBorderlessShortcut, &QShortcut::activated, this, [this]() {
            exitBorderlessFullSize();
        });
    }
}

// Reserved safety slot dim helper (design §4.5). Static so both
// buildStatusBar()'s construction-time state and setTxInhibited() (a
// separately-wired Task 17 slot outside buildStatusBar()) can drive the
// same badge through the same opacity-only state change -- a plain
// setVisible() no longer represents "inactive" once a badge lives in a
// permanently allocated slot.
void MainWindow::dimSafetyBadge(QWidget* w, bool active)
{
    auto* fx = qobject_cast<QGraphicsOpacityEffect*>(w->graphicsEffect());
    if (!fx) {
        fx = new QGraphicsOpacityEffect(w);
        w->setGraphicsEffect(fx);
    }
    fx->setOpacity(active ? 1.0 : 0.14);
}

// Task B4 (design §8.2): AetherSDR's connection gate on the +PAN affordance
// is a silent early return, which reads as a dead click. Ours dims the icon
// and names the reason in the tooltip BEFORE the click, so unavailability
// is visible ahead of time rather than discovered by clicking and getting
// nothing. Called at construction (buildStatusBar) and on every
// connectionStateChanged transition.
void MainWindow::updateAddPanButtonState()
{
    if (!m_addPanButton) { return; }
    const bool connected = m_radioModel && m_radioModel->isConnected();
    m_addPanButton->setEnabled(connected);
    auto* fx = qobject_cast<QGraphicsOpacityEffect*>(
        m_addPanButton->graphicsEffect());
    if (!fx) {
        fx = new QGraphicsOpacityEffect(m_addPanButton);
        m_addPanButton->setGraphicsEffect(fx);
    }
    fx->setOpacity(connected ? 1.0 : 0.35);
    m_addPanButton->setToolTip(connected
        ? tr("Change panadapter layout")
        : tr("Connect a radio to change pan layout"));
}

// ── TNF operator controls (design sections 7, 7.5, 10.2) ──────────────────
//
// Four pure statics behind the status-bar light, the DSP-menu chord and the
// rejected-add notice. Static because MainWindow boots WDSP, the audio engine
// and the discovery thread, so nothing that needs an instance can be tested.

QString MainWindow::tnfIndicatorStyleSheet(bool globalEnabled, int notchCount)
{
    // ON reads accent cyan, matching AetherSDR's status-bar TNF light
    // (MainWindow_Wiring.cpp:3306-3311 [@c6481cbf], #00b4d8 on / #3a4a5a
    // off).
    //
    // The OFF half diverges from upstream, deliberately. AetherSDR's notch
    // list mirrors radio state and defaults its global flag ON, so its off
    // state is a rare, transient thing. Ours ships OFF (maintainer decision
    // D-a, matching Thetis's unchecked chkTNF and WDSP's master run 0 at
    // RXA.c:87), which means the operator's very first notch does nothing
    // until they find this switch. A dim grey label sitting in a row of
    // three permanently dim NYI labels does not communicate that, so:
    //
    //   off, no notches  -> dim #3a4a5a + struck through. Idle, matched to
    //                       the CWX / DVK / FDX siblings but visibly a
    //                       toggle rather than a stub.
    //   off, notches set -> amber + struck through. Notches exist and are
    //                       being bypassed; that is the D-a hazard and it
    //                       gets the palette's warning colour.
    //   on               -> accent cyan, no strike, whatever the count.
    const bool bypassing = (!globalEnabled && notchCount > 0);
    const QString color = globalEnabled
                              ? QString::fromLatin1(Style::kAccent)
                              : (bypassing ? QString::fromLatin1(Style::kAmberWarn)
                                           : QStringLiteral("#3a4a5a"));
    const QString decoration = globalEnabled ? QStringLiteral("none")
                                             : QStringLiteral("line-through");
    return QStringLiteral("QLabel { color: %1; font-weight: bold; "
                          "font-size: 11px; text-decoration: %2; }")
        .arg(color, decoration);
}

QString MainWindow::tnfIndicatorTooltip(int notchCount, bool globalEnabled)
{
    // Shape from AetherSDR buildTnfTooltip (MainWindowHelpers.cpp:233-247
    // [@c6481cbf]): name the feature, say how many notches exist, say the
    // click toggles them. Upstream renders an HTML table of every notch;
    // ours stays a single plain line because the Settings > DSP > MNF table
    // (design section 9) is where the per-notch list lives.
    if (notchCount <= 0) {
        return QStringLiteral("Tunable Notch Filter: no notches. "
                              "Click to toggle all notches.");
    }
    return QStringLiteral("Tunable Notch Filter: %1 notch%2, %3. "
                          "Click to toggle all notches.")
        .arg(notchCount)
        .arg(notchCount == 1 ? QString() : QStringLiteral("es"),
             globalEnabled ? QStringLiteral("enabled")
                           : QStringLiteral("bypassed"));
}

QKeySequence MainWindow::tnfToggleShortcut()
{
    // Design section 10.2: KeyboardSetupPages.cpp is a 100% NYI stub, no
    // ShortcutManager or registerAction exists in src/, and every shipped
    // shortcut is a plain QAction::setShortcut in this file. AetherSDR
    // registers "tnf_toggle" with an empty default sequence
    // (MainWindow_Shortcuts.cpp:1093 [@c6481cbf]) precisely because it HAS a
    // manager to bind it later; we ship a fixed chord instead. Building the
    // assignment subsystem is a separate epic and explicitly out of scope.
    //
    // Ctrl+Shift+N (maintainer decision D-f), consistent with the existing
    // Ctrl+Shift+S and Ctrl+Shift+R chords and verified unclaimed.
    return QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_N);
}

QString MainWindow::tnfAddRejectedNotice(const QString& reason)
{
    // NotchModel's reject reasons are already operator-legible sentences
    // ("A notch already exists within 10 Hz"), so this only names what was
    // refused. Without it a +TNF press inside the dedupe window is entirely
    // silent (plan correction 16).
    return QStringLiteral("Notch not added: %1.").arg(reason);
}

void MainWindow::buildStatusBar()
{
    // AetherSDR double-height status bar (46px fixed height, 3-section layout)
    QStatusBar* sb = statusBar();
    sb->setFixedHeight(46);
    sb->setSizeGripEnabled(false);
    // ── Gleiches Design wie die obere Leiste (2026-08-19) ────────────
    //
    // Auf Ansage des Betreibers: „die Taskleiste unten sollte auch wie
    // die Taskleiste oben aussehen, gleiches Design."
    //
    // Die untere Leiste trug eigene harte Farben (#0a0a14, #203040), die
    // obere zieht ihre aus Style::. Zwei Leisten mit getrennt gefuehrten
    // Farben sehen nach dem naechsten Feinschliff wieder verschieden
    // aus — also dieselben Konstanten, und die Pillen kommen aus
    // CommandBar::pillStyle(), damit es nur EINE Optik gibt.
    //
    // Der Rahmen bleibt oben abgesetzt statt rundum: die Statusleiste
    // sitzt am Fensterrand, ein Rahmen dort hinterliesse eine
    // Doppellinie.
    //
    // 2026-08-20, Nachtrag: hier stand kPanelBg (#0c0c0e), die obere
    // Leiste nimmt kStatusBarBg (#0a0a0c). Zwei Toene, die sich um
    // zwei Stufen unterscheiden — auf dem Schirm kaum zu benennen und
    // trotzdem der Grund, warum die beiden Leisten nicht wie ein Paar
    // wirkten. Jetzt dieselbe Konstante wie oben.
    sb->setStyleSheet(Style::themed(QStringLiteral(
        "QStatusBar { background: %1; border-top: 1px solid %2; }"
        "QStatusBar::item { border: none; }")
        .arg(QString::fromLatin1(Style::kStatusBarBg),
             QString::fromLatin1(Style::kBorderSubtle))));

    // Wrapper widget for the full-width custom layout. Stored as a
    // member so resizeEvent can read its width for m_chromeBar->relayout().
    m_chromeBarWidget = new QWidget(sb);
    m_chromeBarWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    QWidget* barWidget = m_chromeBarWidget;   // local alias keeps existing code below tidy
    QHBoxLayout* hbox = new QHBoxLayout(barWidget);
    hbox->setContentsMargins(6, 0, 6, 0);
    hbox->setSpacing(6);

    // ── Gruppentrenner: ein duenner Strich, kein Punkt ───────────────
    //
    // Zeus setzt zwischen die Gruppen der unteren Leiste einen
    // senkrechten Haarstrich. Wir hatten ein „ · " in 22 Punkt — das
    // liest sich als Satzzeichen und traegt kein Gewicht; zwischen
    // zwanzig kleinen Pillen sah man ueberhaupt nicht mehr, wo eine
    // Gruppe aufhoert.
    //
    // Ein Strich ist ruhiger UND deutlicher, weil er senkrecht steht,
    // wo alles andere waagerecht laeuft.
    auto makeSep = [&]() -> QLabel* {
        auto* sep = new QLabel(barWidget);
        sep->setFixedWidth(1);
        sep->setStyleSheet(QStringLiteral(
            "QLabel { background: %1; margin: 9px 7px; }")
            .arg(QString::fromLatin1(Style::kBorder)));
        return sep;
    };

    // ── Left section ──────────────────────────────────────────────────────────

    // Band Stack: three grey circles (NYI — clickable placeholder). Added to
    // m_placeholderGroup below rather than hbox directly, so it folds
    // together with TNF/CWX/DVK/FDX at rung 10 (design §6) instead of
    // sitting at its own fixed early position.
    auto* bandStackLabel = new QLabel(barWidget);
    bandStackLabel->setFixedSize(10, 22);
    {
        QPixmap pm(10, 22);
        pm.fill(Qt::transparent);
        QPainter painter(&pm);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setBrush(QColor(0x40, 0x48, 0x58));
        painter.setPen(Qt::NoPen);
        for (int i = 0; i < 3; ++i) {
            painter.drawEllipse(0, i * 7, 9, 6);
        }
        painter.end();
        bandStackLabel->setPixmap(pm);
    }
    bandStackLabel->setToolTip(QStringLiteral("Band Stack (NYI)"));
    bandStackLabel->setCursor(Qt::PointingHandCursor);

    // +PAN icon. From AetherSDR MainWindow.cpp:4368-4396 [@c6481cb]: a jagged
    // spectrum polyline with a plus in the upper right. An icon reads as a
    // control where a text pill reads as a label, and the trace says what
    // kind of thing it adds.
    auto* panBtn = new QLabel(barWidget);
    panBtn->setObjectName(QStringLiteral("addPanButton"));
    panBtn->setAccessibleName(tr("Add panadapter"));
    QPixmap pm(36, 28);
    {
        pm.fill(Qt::transparent);
        QPainter pp(&pm);
        pp.setRenderHint(QPainter::Antialiasing);
        const QColor stroke(255, 255, 255, 210);
        pp.setPen(QPen(stroke, 1.6));
        const QPointF pts[] = {
            { 0, 22}, { 1, 21}, { 2, 22}, { 3, 19}, { 4, 22},
            { 5, 21}, { 6, 18}, { 7, 12}, { 8, 17}, { 9, 22},
            {10, 21}, {11, 22}, {12, 16}, {13, 22},
            {14, 21}, {15, 19}, {16, 22},
            {17, 20}, {18, 12}, {19,  4}, {20, 11}, {21, 21},
            {22, 22}, {23, 21}, {24, 17}, {25, 22},
            {26, 21}, {27, 22}, {28, 18}, {29, 22}, {30, 22}
        };
        pp.drawPolyline(pts, sizeof(pts) / sizeof(pts[0]));
        pp.setPen(QPen(stroke, 2.2));
        pp.drawLine(30, 4, 30, 14);
        pp.drawLine(25, 9, 35, 9);
        pp.end();
    }
    panBtn->setPixmap(pm);
    panBtn->setCursor(Qt::PointingHandCursor);
    panBtn->installEventFilter(this);
    panBtn->setProperty("isAddPanButton", true);
    // Band-stack dots lead the bar, ahead of +PAN, as they did before the
    // bottom-banner epic. Folding them is rung 10's job; where they sit is
    // this layout's job, and the two are independent.
    hbox->addWidget(bandStackLabel);
    hbox->addWidget(panBtn);
    m_addPanButton = panBtn;
    m_bandStackLabel = bandStackLabel;

    // ── Aufgeraeumt auf Ansage (2026-08-20) ──────────────────────────
    //
    // Der Betreiber: „weiters kannst du in der fussleiste auch
    // A-TX-LSB-2,9k, M, NR und ATN loeschen, dafuer gibt es widgets,
    // 40m kannst du auch loeschen. daneben sind ganz links noch
    // zeichen, bitte weg."
    //
    // VERSTECKT, nicht geloescht. Die Elemente haengen an lebender
    // Verdrahtung (Bandstapel, Kettenzustand, Slice-Zeile); sie
    // herauszuschneiden hiesse, diese Wege mit zu entfernen und beim
    // naechsten Sinneswandel neu zu bauen. Ein setVisible(false) ist
    // umkehrbar, kostet nichts und laesst die Verdrahtung heil.
    //
    // Was jeweils an ihre Stelle tritt:
    //   Bandstapel-Punkte  ->  Band-Menue in der Menueleiste
    //   +PAN-Symbol        ->  View -> Pan Layout… (Ctrl+L)
    //   CH 0 / 40m         ->  die Pille im Panadapter-Kopf
    //   A/TX/LSB/2.9k/…    ->  die Applets RX, Filter, Frequenz
    // Das Ausblenden selbst steht weiter unten, NACH
    // registerChromeBarItems — siehe die Notiz dort. Hier waere es zu
    // frueh und ausserdem am Regler vorbei.
    updateAddPanButtonState();

    // Phase 3F Sub-Epic D Task 11: per-chain (ADC) BPF state indicators
    // in the bottom status bar. Two stacked labels per chain ("CH N"
    // header + reasonText body), wired to AlexController::bpfStateChanged
    // below so the body text + colour reflect the live per-ADC BPF
    // state. CH 0 is always shown; CH 1 is shown only when the
    // connected radio's BoardCapabilities reports rxFilterChainCount >= 2
    // (gated in the currentRadioChanged handler below).
    auto makeChainIndicator = [&](int adc) -> QWidget* {
        auto* w  = new QWidget(barWidget);
        auto* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);

        auto* topLbl = new QLabel(QStringLiteral("CH %1").arg(adc), w);
        topLbl->setStyleSheet(
            QStringLiteral("color: %1; font-size: 11px; font-weight: bold;")
                .arg(Style::kTextScale));
        vl->addWidget(topLbl);

        auto* botLbl = new QLabel(QStringLiteral("idle"), w);
        botLbl->setObjectName(QStringLiteral("chainIndicator%1").arg(adc));
        botLbl->setStyleSheet(
            QStringLiteral("color: %1; font-size: 9px; font-weight: bold;")
                .arg(Style::kTextInactive));
        vl->addWidget(botLbl);

        return w;
    };

    auto* chain0Widget = makeChainIndicator(0);
    hbox->addWidget(chain0Widget);
    m_chain0IndicatorWidget = chain0Widget;
    auto* chain1Widget = makeChainIndicator(1);
    chain1Widget->setVisible(false);  // shown only on 2-ADC SKUs
    hbox->addWidget(chain1Widget);
    m_chain1IndicatorWidget = chain1Widget;

    // Panel toggle (☰) — wired to QSplitter right pane visibility
    auto* panelToggleLabel = new QLabel(QStringLiteral("☰"), barWidget);
    panelToggleLabel->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }")));
    panelToggleLabel->setToolTip(QStringLiteral("Toggle container panel"));
    panelToggleLabel->setCursor(Qt::PointingHandCursor);
    hbox->addWidget(panelToggleLabel);

    // Wire ☰ click: toggle QSplitter right pane (widget index 1) visibility.
    // When hiding: save sizes so we can restore them. When showing: restore.
    connect(panelToggleLabel, &QLabel::linkActivated, this, [](const QString&){});
    // QLabel doesn't emit click directly — install event filter via lambda via
    // a helper QObject. Use mousePressEvent via event filter on the label.
    panelToggleLabel->installEventFilter(this);
    // We need to store the label pointer to recognise it in eventFilter.
    // Use a property to mark it.
    panelToggleLabel->setProperty("isPanelToggle", true);

    // TNF light. Click toggles every notch at once; colour and tooltip follow
    // NotchModel::globalEnabled.
    // From AetherSDR MainWindow.cpp:4422-4436 [@c6481cbf] (indicator label +
    // tooltip refresh on list changes) and MainWindow_Wiring.cpp:3306-3311
    // [@c6481cbf] (globalEnabledChanged drives the stylesheet).
    //
    // Not gated on isConnected the way AetherSDR's is: under design decision
    // D3 the notch list is persisted client-side operator state, not a mirror
    // of radio state, so it is meaningful before a radio is attached.
    m_tnfLabel = new QLabel(QStringLiteral("TNF"), barWidget);
    m_tnfLabel->setCursor(Qt::PointingHandCursor);
    m_tnfLabel->setProperty("isTnfToggle", true);
    m_tnfLabel->installEventFilter(this);
    // TNF is NOT part of m_placeholderGroup. It was an inert NYI label when
    // the bottom-banner epic classified it as fold-last chrome, but the
    // tunable-notch-filter work that landed on main (#313) made it a live
    // toggle that turns amber when notches exist and are being bypassed.
    // That is an operationally meaningful warning, so it is registered as
    // its own item and folds with live state, not with the stubs.
    hbox->addWidget(m_tnfLabel);

    // ── Die vier Sendeschalter ────────────────────────────────────────
    //
    // Betreiber-Entscheidung 2026-08-18 (Fussleisten-Entwurf, Zuschnitt
    // A): MOX, VOX, TUNE und PS kommen nach unten und BLEIBEN ZUGLEICH
    // in der TxApplet. Beide Flaechen haengen am selben Modell und
    // kennen einander nicht — wer eine umlegt, sieht die andere
    // mitgehen, weil beide auf dasselbe Signal hoeren.
    m_txMoxSwitch  = new TxSwitch(TxSwitch::Kind::Mox,  m_radioModel, barWidget);
    m_txVoxSwitch  = new TxSwitch(TxSwitch::Kind::Vox,  m_radioModel, barWidget);
    m_txTuneSwitch = new TxSwitch(TxSwitch::Kind::Tune, m_radioModel, barWidget);
    m_txPsSwitch   = new TxSwitch(TxSwitch::Kind::Ps,   m_radioModel, barWidget);
    // Reihenfolge im Bild wie bei Zeus: MOX VOX TUNE PS. Die Faltung
    // laeuft andersherum (PS zuerst), das ist Absicht — die Stelle im
    // Bild und die Wichtigkeit sind zwei verschiedene Ordnungen.
    hbox->addWidget(m_txMoxSwitch);
    hbox->addWidget(m_txVoxSwitch);
    hbox->addWidget(m_txTuneSwitch);
    hbox->addWidget(m_txPsSwitch);

    if (NotchModel* notches = m_radioModel->notchModel()) {
        // Named slot, not a lambda: Qt::UniqueConnection only works on a
        // pointer-to-member target -- on a lambda Qt6 warns and refuses the
        // connect entirely. Every one of these five signals can fire while
        // the bar is being rebuilt, so the dedup has to actually hold.
        connect(notches, &NotchModel::globalEnabledChanged, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchAdded, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchRemoved, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchesReset, this,
                &MainWindow::refreshTnfIndicator, Qt::UniqueConnection);
        connect(notches, &NotchModel::notchAddRejected, this,
                &MainWindow::onNotchAddRejected, Qt::UniqueConnection);
        // Seed from whatever restoreFromSettings already loaded.
        refreshTnfIndicator();
    }

    // CWX
    auto* cwxLabel = new QLabel(QStringLiteral("CWX"), barWidget);
    cwxLabel->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: #3a4a5a; font-weight: bold; font-size: 11px; }")));
    cwxLabel->setToolTip(QStringLiteral("CW Keyer (NYI)"));
    cwxLabel->setCursor(Qt::PointingHandCursor);

    // DVK
    auto* dvkLabel = new QLabel(QStringLiteral("DVK"), barWidget);
    dvkLabel->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: #3a4a5a; font-weight: bold; font-size: 11px; }")));
    dvkLabel->setToolTip(QStringLiteral("Digital Voice Keyer (NYI)"));
    dvkLabel->setCursor(Qt::PointingHandCursor);

    // FDX
    auto* fdxLabel = new QLabel(QStringLiteral("FDX"), barWidget);
    fdxLabel->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: #3a4a5a; font-weight: bold; font-size: 11px; }")));
    fdxLabel->setToolTip(QStringLiteral("Full Duplex (NYI)"));
    fdxLabel->setCursor(Qt::PointingHandCursor);

    // Rung 10, last resort (design §6). Grouped so the ladder folds them as
    // one unit rather than dribbling them out one label at a time. Each
    // label is reparented here; the group owns them.
    //
    // Two members that used to be here are deliberately NOT:
    //
    //   bandStackLabel  -- it leads the bar, ahead of +PAN, and moving it
    //   into this group silently reordered the left section. Restored to
    //   its original position below; it still folds at rung 10 with the
    //   rest of the stubs, because rung governs visibility and the layout
    //   governs position, which are independent.
    //
    //   m_tnfLabel      -- no longer a stub. The tunable-notch-filter work
    //   on main (#313) made it a live toggle that turns amber when notches
    //   exist and are being bypassed, so it folds later than the stubs.
    m_placeholderGroup = new QWidget(barWidget);
    auto* phRow = new QHBoxLayout(m_placeholderGroup);
    phRow->setContentsMargins(0, 0, 0, 0);
    phRow->setSpacing(6);
    phRow->addWidget(cwxLabel);
    phRow->addWidget(dvkLabel);
    phRow->addWidget(fdxLabel);
    hbox->addWidget(m_placeholderGroup);

    // Trailing separator, paired with m_placeholderGroup so it folds
    // alongside the group (design §6) instead of dangling on its own.
    m_placeholderSep = makeSep();
    hbox->addWidget(m_placeholderSep);

    // Design §4.1: the old left-section model+firmware pair
    // (radioInfoWidget / m_radioModelLabel / m_radioFwLabel) is retired.
    // It had no click affordance and sat in the banner's unprotected left
    // section, so its width changes were what shoved its neighbours.
    // Radio identity now renders once, on StationBlock's second row
    // (Task A4's setHardwareLine), wired from onConnectionStateChanged()
    // below. m_connStatusLabel's legacy alias is retired with it — grep
    // confirms nothing else in the tree reads it.

    // ── Phase 3Q Sub-PR-6 (F.1): RxDashboard ────────────────────────────────
    // Replaces the Phase 3Q-7 verbose connection-info strip (those fields now
    // live in the segment tooltip / NetworkDiagnosticsDialog).
    // Follows the ACTIVE slice (Task A5's rebindDashboard lambda, wired
    // below to RadioModel::sliceAdded / activeSliceChanged) rather than a
    // fixed slice(0); no slice exists yet at construction time so there is
    // nothing to bind here. When disconnected the badges show placeholder
    // "—" until the slice receives live values from the radio.
    m_rxDashboard = new RxDashboard(barWidget);
    hbox->addWidget(m_rxDashboard);

    // ── Phase 3M-4 Task 10: PSA bottom-banner pair (FB + PS) ──────────────────
    // Source-first port of Thetis ucInfoBar.cs:820-1098 [v2.10.3.13].
    // The widget auto-wires to RadioModel's PureSignal coordinator and
    // MoxController on construction; click signals route back to
    // PureSignal::setInvertRedBlue / setHideFeedback below.
    //
    // Phase 3M-4 bench-fix: visibility is gated on
    //   caps.hasPureSignal && pureSignal->isAutoCalEnabled()
    // (NereusSDR-specific UX: hide the banner unless the user has
    // explicitly armed PS-A; reduces clutter for non-PS workflows on
    // PS-capable boards).  updatePsaIndicatorVisibility() centralises the
    // condition; called from autoCalEnabledChanged + connection-state +
    // pureSignalCoordinatorReady (late-bind seam, Task 13).
    m_psaIndicator = new PsaIndicatorWidget(m_radioModel, barWidget);
    m_psaIndicator->setVisible(false);
    auto wirePsaCoordinator = [this](PureSignal* ps) {
        if (!ps) return;
        connect(m_psaIndicator,
                &PsaIndicatorWidget::invertRedBlueRequested,
                this, [ps]() {
                    ps->setInvertRedBlue(!ps->invertRedBlue());
                });
        connect(m_psaIndicator,
                &PsaIndicatorWidget::hideFeedbackToggleRequested,
                this, [ps]() {
                    ps->setHideFeedback(!ps->hideFeedback());
                });
        connect(ps, &PureSignal::autoCalEnabledChanged,
                this, &MainWindow::updatePsaIndicatorVisibility);

        // ── Die Stabilitaetsregel sichtbar machen (2026-08-23) ──────
        //
        // Ohne diese Verbindung waere die Regel eine Behauptung: sie
        // haelt die Korrektur zurueck oder friert sie ein, und die
        // Anzeige sagt weiter "Correcting". Das ist die Art stiller
        // Falschaussage, die man erst am Messplatz bemerkt — und ich
        // konnte die Regel nicht gegen Hardware pruefen, also muss sie
        // wenigstens sichtbar sein.
        connect(ps, &PureSignal::stabilityActionChanged, this,
                [this](PsCorrectionAction action) {
            if (m_psaIndicator) { m_psaIndicator->setStabilityAction(action); }
        });
        if (m_psaIndicator) {
            m_psaIndicator->setStabilityAction(ps->stabilityAction());
        }
    };
    wirePsaCoordinator(m_radioModel->pureSignal());
    connect(m_radioModel, &RadioModel::pureSignalCoordinatorReady,
            this, [this, wirePsaCoordinator](PureSignal* ps) {
                wirePsaCoordinator(ps);
                updatePsaIndicatorVisibility();
            });
    hbox->addWidget(m_psaIndicator);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Center section: STATION — radio-name anchor (Sub-PR-7 G.1) ───────────
    // The old cyan "STATION: NereusSDR" box is replaced by a StationBlock that
    // shows the connected radio's name. Click → opens ConnectionPanel. Right-
    // click → Disconnect / Edit radio… / Forget radio. Disconnected appearance:
    // dashed-red border + italic "Click to connect" placeholder.
    // The StationCallsign AppSettings key is preserved on disk for a potential
    // future operator-callsign surface; it is no longer shown in status chrome.
    m_stationBlock = new StationBlock(barWidget);
    connect(m_stationBlock, &StationBlock::clicked,
            this, &MainWindow::showConnectionPanel);
    connect(m_stationBlock, &StationBlock::contextMenuRequested,
            this, &MainWindow::showStationContextMenu);
    // Update the block's name on connection state changes.
    connect(m_radioModel, &RadioModel::currentRadioChanged, this,
            [this](const Longpath::RadioInfo& info) {
        const bool connected =
            (m_radioModel->connectionState() == ConnectionState::Connected);
        m_stationBlock->setRadioName(connected ? info.name : QString());
        // setRadioName(QString()) also clears the hardware row
        // (StationBlock.cpp:57-59), so sizeHint may have changed either
        // way. Report it (final-fix-wave finding 3) so the fold budget
        // does not keep charging the connected width after this label
        // shrinks back to "Click to connect".
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(
                m_stationBlock, m_stationBlock->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    });
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this](ConnectionState s) {
        if (s != ConnectionState::Connected) {
            m_stationBlock->setRadioName(QString());
            if (m_chromeBar && m_chromeBarWidget) {
                m_chromeBar->setNaturalWidth(
                    m_stationBlock, m_stationBlock->sizeHint().width());
                m_chromeBar->relayout(m_chromeBarWidget->width());
            }
        }
    });

    // ── ADC Overload alarm: lives in the reserved safety group ─────────────
    // Earlier revisions of the Phase 3Q chrome work parked the alarm
    // between the dashboard and STATION block. That violated layout-
    // stability rule §278.4 ("STATION sits between two flex:1 spacers.
    // Activity in the middle or right sections never moves it.")
    // because the label's text width grew when overload fired and
    // pushed STATION sideways. The alarm is now an AdcOverloadBadge built
    // further down as one of the four permanently-allocated 50 px safety
    // slots (design doc §4.5: INH / PA / OVL / TX) -- an inactive slot
    // dims rather than collapsing, so its appearance changes opacity, not
    // width, and nothing else on the bar moves when it lights up.

    hbox->addWidget(m_stationBlock);

    // ── Stretch ───────────────────────────────────────────────────────────────
    hbox->addStretch(1);

    // ── Right section: indicators ────────────────────────────────────────────

    // Helper lambda: create a stacked indicator pair (top label + bottom label)
    // Returns the widget; sets topLbl/botLbl via out-params for wiring.
    auto makeIndicator = [&](const QString& top, const QString& bottom,
                              QLabel** outTop = nullptr, QLabel** outBot = nullptr) -> QWidget* {
        QWidget* w = new QWidget(barWidget);
        w->setMinimumWidth(60);
        QVBoxLayout* vl = new QVBoxLayout(w);
        vl->setContentsMargins(0, 0, 0, 0);
        vl->setSpacing(0);
        auto* topLbl = new QLabel(top, w);
        topLbl->setStyleSheet(Style::themed(QStringLiteral(
            "QLabel { color: #607080; font-size: 11px; }")));
        auto* botLbl = new QLabel(bottom, w);
        botLbl->setStyleSheet(Style::themed(QStringLiteral(
            "QLabel { color: #3a4a5a; font-size: 11px; }")));
        vl->addWidget(topLbl);
        vl->addWidget(botLbl);
        if (outTop) { *outTop = topLbl; }
        if (outBot) { *outBot = botLbl; }
        return w;
    };

    // CAT Serial — NYI until Phase 3K; kept as static indicator, no live signal
    m_catIndicator = makeIndicator(QStringLiteral("CAT"), QStringLiteral("Off"));
    hbox->addWidget(m_catIndicator);
    m_catSep = makeSep();
    hbox->addWidget(m_catSep);

    // TCI — Phase 23: capture bottom label for updateTciIndicator() + install
    // event filter for click-to-Setup navigation.
    m_tciIndicator = makeIndicator(QStringLiteral("TCI"), QStringLiteral("Off"),
                                   nullptr, &m_tciIndicatorBotLabel);
    m_tciIndicator->installEventFilter(this);
    hbox->addWidget(m_tciIndicator);
    m_tciSep = makeSep();
    hbox->addWidget(m_tciSep);

    // ── System tile: PA telemetry + CPU, merged (design §4.3) ────────────
    // Earlier revisions showed only a single "PSU" widget driven by the
    // supply_volts (AIN6) channel.  Source-first audit against Thetis
    // [v2.10.3.13] proved that channel is never displayed in Thetis —
    // computeHermesDCVoltage() exists but has zero callers, and the only
    // voltage status indicator (toolStripStatusLabel_Volts) reads
    // _MKIIPAVolts which is convertToVolts(getUserADC0()) — i.e. the PA
    // drain voltage on AIN3.  On a G2 / 8000D / 7000DLE the PA drain IS
    // the supply voltage minus a small drop, so this single number
    // covers what the user wants to know.
    //
    // The HL2 fork (mi0bot) extends this slot for HermesLite by reusing
    // the volts label to show FPGA on-die temperature (HL2 has no PA
    // volts ADC, but does carry a temperature ADC value in the C&C
    // exciter_power AIN5 field).  See mi0bot console.cs:26758-26762
    // [v2.10.3.13-beta2 @c26a8a4]:
    //   if (HardwareSpecific.Model == HPSDRModel.HERMESLITE)
    //   {
    //       toolStripStatusLabel_Volts.Text = String.Format("{0:#0.0}C", _MKIIHL2Temp);
    //       ...
    //   }
    //
    // Bottom-banner cleanup Task A3 merged the 2-row PA stack (PA-V over
    // PA-T, mutually exclusive per board) with the standalone CPU
    // MetricLabel into one 2-row SystemTile: row one carries whichever PA
    // reading(s) the board publishes (both share row one when a board
    // publishes both, rather than evicting CPU per design §4.3), row two
    // is always CPU. The tile itself never hides — a board with neither
    // PA reading still shows CPU-only, matching design §4.3's degenerate
    // case ("shows a CPU-only tile, not an empty one").
    m_systemTile = new SystemTile(barWidget);
    hbox->addWidget(m_systemTile);
    m_systemTileSep = makeSep();
    hbox->addWidget(m_systemTileSep);

    // Phase 3P-II Task 21: TGXL presence chip. Registered with m_chromeBar
    // at rung 2 (design §6), so it folds under width pressure, but
    // presence is not a fold concept -- it is reported to the controller
    // via setItemAvailable, straight from the signal that changes it, per
    // ChromeBarController::setItemAvailable's own doc comment. Hidden
    // (available=false) until TunerModel::presenceChanged fires true;
    // text reflects operate/bypass/standby state via stateChanged.
    m_tgxlChip = new QLabel(QStringLiteral("TGXL"), barWidget);
    m_tgxlChip->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { background:#204060; border:1px solid #205070; "
        "padding:1px 8px; border-radius:6px; color:#cfe2f5; }")));
    m_tgxlChip->setVisible(false);
    hbox->addWidget(m_tgxlChip);

    connect(m_radioModel->tunerModel(), &TunerModel::presenceChanged,
            this, [this](bool present) {
        if (!m_chromeBar || !m_chromeBarWidget) { return; }
        m_chromeBar->setItemAvailable(m_tgxlChip, present);
        m_chromeBar->relayout(m_chromeBarWidget->width());
    });
    connect(m_radioModel->tunerModel(), &TunerModel::stateChanged,
            this, [this]() {
        TunerModel* t = m_radioModel->tunerModel();
        QString s = t->isOperate()
                    ? (t->isBypass() ? QStringLiteral("BYPS")
                                     : QStringLiteral("OPER"))
                    : QStringLiteral("SBY");
        m_tgxlChip->setText(QStringLiteral("TGXL ") + s);
        // TGXL / TGXL OPER / TGXL BYPS / TGXL SBY are different widths
        // (Task A8 fix round 1 finding 4); report the new one.
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_tgxlChip,
                                         m_tgxlChip->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    });

    // Helper: SystemTile's content just changed width (a reading gained or
    // lost digits, a row appeared/disappeared). Report the new width to
    // m_chromeBar and let it re-decide — content-change sites call
    // setNaturalWidth then relayout() instead of the old force-refresh
    // drop-priority pattern (design §5.2).
    auto refreshChromeBarForSystemTile = [this]() {
        if (!m_chromeBar || !m_chromeBarWidget || !m_systemTile) { return; }
        m_chromeBar->setNaturalWidth(m_systemTile,
                                     m_systemTile->sizeHint().width());
        m_chromeBar->relayout(m_chromeBarWidget->width());
    };

    // Wire voltage signals: re-bind on every new connection, reset on disconnect.
    connect(m_radioModel, &RadioModel::connectionStateChanged, this,
            [this, refreshChromeBarForSystemTile](ConnectionState s) {
        if (s != ConnectionState::Connected) {
            m_systemTile->clearPaVolts();
            m_systemTile->clearPaTemp();
            refreshChromeBarForSystemTile();
        }
        if (auto* conn = m_radioModel->connection()) {
            // conn is a new object on each reconnect — no deduplication
            // needed. Qt::UniqueConnection would be no help here in any
            // case: on a lambda target Qt6 warns and refuses the connect,
            // rather than making it once-only.
            //
            // 2026-05-25 KG4VCF G2E bench finding: ANAN-G2E (HermesC10)
            // firmware leaves user_adc0 (AIN3 / status bytes 53-54) dark.
            // Bench reading was 0.1 V against an actual 13.4 V supply.
            // Route supply_volts (AIN6 / bytes 45-46) to the tile on G2E
            // and rename "PA" to "PSU".  Other MKII boards keep the
            // existing user_adc0 path -- on those SKUs user_adc0 IS the
            // PA drain sense, which is what the "PA" label means.
            //
            // Implementation note: we bind BOTH signals unconditionally
            // and gate inside each slot on the CURRENT model.  The outer
            // lambda fires on every connectionStateChanged transition
            // (Connecting / Probing / Connected), and at Connecting time
            // hardwareProfile.model may not yet be set to ANAN_G2E -- so
            // a branch-at-bind-time approach picked the wrong slot and
            // the tile stayed dark.  Gating inside the slot reads the
            // model at each signal emission, when it is guaranteed to be
            // set (status frames only arrive after the Connected handler
            // has populated the profile).
            auto onUserAdc0 = [this, refreshChromeBarForSystemTile](float v) {
                const auto model = m_radioModel->hardwareProfile().model;
                if (model == HPSDRModel::ANAN_G2E) {
                    return;  // G2E uses supply_volts; ignore user_adc0.
                }
                // Task 3.6: ANAN-8000DLE user preference gate.
                // For ANAN-8000D radios, consult the "Show volts/amps in title
                // bar" AppSettings key (default true). For other MKII-class
                // boards (7000DLE, AnvelinaPro3) the gate is always open —
                // those boards don't have the per-SKU preference checkbox.
                const bool is8000D = (model == HPSDRModel::ANAN8000D);
                const bool showVolts = !is8000D ||
                    AppSettings::instance().value(
                        QStringLiteral("HardwareAnan8000DleShowVoltsAmps"),
                        QStringLiteral("True")).toString() == QStringLiteral("True");
                if (!showVolts) { return; }
                m_systemTile->setPaLabel(QStringLiteral("PA"));
                m_systemTile->setPaVolts(static_cast<double>(v));
                refreshChromeBarForSystemTile();
                qInfo() << "PA tile updated via userAdc0:" << v << "V";
            };
            connect(conn, &RadioConnection::userAdc0Changed, this, onUserAdc0);

            auto onSupplyVolts = [this, refreshChromeBarForSystemTile](float v) {
                const auto model = m_radioModel->hardwareProfile().model;
                if (model != HPSDRModel::ANAN_G2E) {
                    return;  // Non-G2E uses user_adc0 path.
                }
                m_systemTile->setPaLabel(QStringLiteral("PSU"));
                m_systemTile->setPaVolts(static_cast<double>(v));
                refreshChromeBarForSystemTile();
                qInfo() << "PSU tile updated via supplyVolts:" << v << "V";
            };
            connect(conn, &RadioConnection::supplyVoltsChanged, this, onSupplyVolts);

            // 2026-08-03 KG4VCF G2E bench finding: neither qInfo above ever
            // printed against a live G2E, although it stayed Connected for
            // minutes. Root cause: P2RadioConnection starts parsing
            // High-Priority status frames (and calling handleSupplyRaw /
            // handleUserAdc0Raw) as soon as its UDP socket is live -- the
            // bench log's first "P2: UDP packet: port 1025 ... size 60"
            // trace lands about 19 ms BEFORE RadioModel reaches Connected.
            // The connect() calls just above cannot exist before this exact
            // lambda runs, so that first sample's userAdc0Changed /
            // supplyVoltsChanged emission fires with nobody listening.
            // handleSupplyRaw/handleUserAdc0Raw then suppress every later
            // re-emit of an unchanged value (identical-raw suppression), so
            // a steady supply never gives the tile a second chance. Pull
            // whatever the connection already computed instead of waiting
            // on a change that will never come.
            if (conn->lastUserAdc0Volts() >= 0.0f) {
                onUserAdc0(conn->lastUserAdc0Volts());
            }
            if (conn->lastSupplyVolts() >= 0.0f) {
                onSupplyVolts(conn->lastSupplyVolts());
            }
        }
    });

    // PA temperature row — driven by RadioStatus::paTemperatureChanged
    // (HL2 publishes via the handlePaTelemetry HL2 branch; future boards
    // may publish via the same RadioStatus signal).  The label
    // formatting respects PaTempUnitNotifier::currentUnit() so a
    // °C / °F toggle reformats live.
    connect(&m_radioModel->radioStatus(), &RadioStatus::paTemperatureChanged,
            this, [this, refreshChromeBarForSystemTile](double celsius) {
        m_systemTile->setPaTempCelsius(celsius);
        refreshChromeBarForSystemTile();
    });

    // Live re-format on °C / °F toggle without waiting for the next
    // telemetry sample. There is no toggle(); flip explicitly, matching
    // the SystemTile::paTempClicked handler just below.
    connect(&PaTempUnitNotifier::instance(),
            &PaTempUnitNotifier::unitChanged, this,
            [this, refreshChromeBarForSystemTile](PaTempUnit) {
        m_systemTile->refreshPaRow();
        refreshChromeBarForSystemTile();
    });
    connect(m_systemTile, &SystemTile::paTempClicked, this, []() {
        const PaTempUnit cur = PaTempUnitNotifier::currentUnit();
        PaTempUnitNotifier::setUnit(cur == PaTempUnit::Celsius
                                        ? PaTempUnit::Fahrenheit
                                        : PaTempUnit::Celsius);
    });

    // ── sub-PR-8: CPU, now SystemTile's row two ──────────────────────────
    // Replaces the old standalone CPU MetricLabel.
    m_systemTile->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_systemTile, &QWidget::customContextMenuRequested,
            this, &MainWindow::onCpuMenuRequested);
    // No whole-tile tooltip set here (unlike the old m_cpuMetric): the
    // merge means m_systemTile's tooltip is already owned by
    // SystemTile::refreshPaRow(), which sets it to the °C/°F click hint
    // whenever a temperature reading is present and clears it otherwise.
    // Setting a second, competing tooltip here would just get clobbered
    // by the next PA reading, unpredictably.

    // TX Inhibit has no slot of its own any more.
    //
    // It used to be an "INH" pill dimmed to 14%. Two problems, both found on
    // a bench: the abbreviation meant nothing to the operator, and the pill
    // carried a 1 px #ff6060 border that survived the dimming while its text
    // did not, so the safety corner sat there showing an empty alarm-red
    // outline while nothing was wrong. That is the opposite of what the
    // corner is for.
    //
    // Inhibit is a property OF transmit, not a peer indicator beside it, so
    // it now paints onto the TX badge itself: a prohibition symbol replaces
    // the TX dot and a toast names the reason at the moment it asserts. See
    // setTxInhibited(). That reclaims a whole reserved slot as well.

    // ── sub-PR-8: PA Status StatusBadge ──────────────────────────────────
    // Variant::On (green ✓ PA OK) / Variant::Tx (red ✓ PA FAULT).
    // Driven by RadioModel::paTripped(); setPaTripped() flips the variant.
    // Signal wiring lands in Task 17 (same as the original QLabel).
    m_paStatusBadge = new StatusBadge(barWidget);
    m_paStatusBadge->setObjectName(QStringLiteral("paStatusBadge"));
    // SVG-backed icon — earlier revisions used the U+2713 CHECK MARK
    // glyph, which renders inconsistently across the SF Mono / Menlo /
    // monospace fallback chain (boxed or kerned wrong on platforms
    // without SF Mono installed). The SVG is rendered at 14 logical
    // px and tinted with the variant's foreground color.
    m_paStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-check.svg"));
    m_paStatusBadge->setLabel(QStringLiteral("PA"));
    m_paStatusBadge->setVariant(StatusBadge::Variant::On);
    m_paStatusBadge->setToolTip(tr("PA Status — OK"));

    // ── ADC overload alarm: reserved slot between PA and TX ──────────────
    // Dimmed by default; shown at full opacity when StepAttenuatorController
    // emits an overload event, dimmed again 2 s after the latest event by
    // the timer below.
    //
    // Source-first port of Thetis pollOverloadSyncSeqErr + ucInfoBar.Warning
    // [@501e3f5]:
    //   console.cs:21323        adc_names[] = { "ADC0", "ADC1", "ADC2" }
    //   console.cs:21359-21389  per-ADC level counter; level>0 → warn,
    //                            any level>3 → red_warning
    //   ucInfoBar.cs:911-933    Warning(msg, red_warning, show_duration):
    //                            ForeColor = red ? Red : Yellow;
    //                            Visible=true; _warningTimer.Start()
    m_adcOvlBadge = new AdcOverloadBadge(barWidget);
    m_adcOvlBadge->setObjectName(QStringLiteral("adcOvlBadge"));
    dimSafetyBadge(m_adcOvlBadge, false);

    // Auto-hide timer mirrors Thetis ucInfoBar._warningTimer — single-shot
    // 2000 ms, restarts on each overload event so a single hit keeps the
    // alarm visible for the full 2 s even after the per-ADC level decays.
    // Source: ucInfoBar.cs:927-932 [v2.10.3.13] + console.cs:21388 show_duration=2000
    // [@501e3f5].
    m_adcOvlHideTimer = new QTimer(this);
    m_adcOvlHideTimer->setSingleShot(true);
    m_adcOvlHideTimer->setInterval(2000);
    connect(m_adcOvlHideTimer, &QTimer::timeout, this, [this]() {
        if (m_adcOvlBadge) { dimSafetyBadge(m_adcOvlBadge, false); }
        // No relayout() needed: m_safetyGroup is registered with
        // m_chromeBar as one rung-0 item at its pinned 4x50 px sizeHint
        // (design §4.5), so dimming a badge inside it never changes the
        // group's own required width (finding routed from Task A6).
    });

    connect(m_stepAttController, &StepAttenuatorController::overloadStatusChanged,
            this, [this](int /*adc*/, OverloadLevel /*level*/) {
        // Thetis adc_names table — console.cs:21323 [@501e3f5]
        static const char* const kAdcNames[3] = { "ADC0", "ADC1", "ADC2" };

        // Build the alarm state: which ADCs are firing, plus highest
        // severity. Thetis console.cs:21359-21389 [@501e3f5] —
        // red_warning is any level > 3; our levelToSeverity() maps that
        // to OverloadLevel::Red.
        bool anyRed = false;
        QString shownAdcs;
        QString tip;
        for (int i = 0; i < 3; ++i) {
            const OverloadLevel lvl = m_stepAttController->overloadLevel(i);
            if (lvl == OverloadLevel::None) { continue; }
            if (lvl == OverloadLevel::Red) { anyRed = true; }
            if (!shownAdcs.isEmpty()) { shownAdcs += QStringLiteral("/"); }
            shownAdcs += QString::number(i);
            if (!tip.isEmpty()) { tip += QStringLiteral("\n"); }
            tip += QStringLiteral("%1: overload").arg(
                QString::fromLatin1(kAdcNames[i]));
        }

        if (shownAdcs.isEmpty()) {
            // No ADC currently above level 0 — let the 2 s auto-hide
            // timer expire so a just-cleared overload stays visible
            // for the remainder of its window. Matches Thetis.
            return;
        }

        m_adcOvlBadge->setAdcs(shownAdcs);
        // ucInfoBar.cs:928 [@501e3f5] — red_warning ? Red : Yellow.
        m_adcOvlBadge->setVariant(anyRed ? AdcOverloadBadge::Variant::Tx
                                         : AdcOverloadBadge::Variant::Warn);
        m_adcOvlBadge->setToolTip(tip);
        dimSafetyBadge(m_adcOvlBadge, true);

        // No relayout() needed here either -- same reasoning as the
        // auto-hide timer above.

        // Restart auto-hide — Thetis: _warningTimer.Stop(); .Start();
        // (ucInfoBar.cs:927+932 [@501e3f5]).
        m_adcOvlHideTimer->start();
    });

    // ── sub-PR-8: Canonical TX StatusBadge ───────────────────────────────
    // Solid red (Variant::Tx) when MoxController emits moxStateChanged(true).
    // Dim (Variant::Off) at rest. No flash per design spec.
    m_txStatusBadge = new StatusBadge(barWidget);
    m_txStatusBadge->setObjectName(QStringLiteral("txStatusBadge"));
    // SVG-backed icon — see PA badge note above for rationale. The dot
    // shape matches the U+25CF BLACK CIRCLE glyph it replaces.
    m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    // ── „ON AIR", nicht „TX" ─────────────────────────────────────────
    //
    // Hier stand „TX" — genau wie am Abzeichen der Scheibe weiter
    // links, das RxDashboard fuehrt. Die beiden sagen Verschiedenes:
    // links „diese Scheibe ist der Sender", hier „es wird gerade
    // gesendet". Im Ruhezustand stehen beide matt und gleich
    // beschriftet nebeneinander, und der Betreiber hat sie am
    // 2026-08-20 zu Recht fuer eine Doppelung gehalten.
    //
    // Geloescht wird keines: dann fehlte eine der beiden Aussagen. Ein
    // Wort loest es. „ON AIR" ist im Funkbetrieb die uebliche Anzeige
    // dafuer, dass gerade gesendet wird, und niemand verwechselt es
    // mit der Angabe, welche Scheibe der Sender ist.
    m_txStatusBadge->setLabel(QStringLiteral("ON AIR"));
    m_txStatusBadge->setVariant(StatusBadge::Variant::Off);
    m_txStatusBadge->setToolTip(tr("Receive (MOX off)"));

    // ── Reserved safety slots (design §4.5) ──────────────────────────────
    // Every slot is permanently allocated. Only the badge inside changes.
    // The old code inserted the overload badge BETWEEN the PA and TX badges
    // and made it visible on overload, so TX slid sideways at the exact
    // moment something went wrong. Reserving the slot fixes that: an alarm
    // now lights up in a pixel the operator has already learned.
    m_safetyGroup = new QWidget(barWidget);
    m_safetyGroup->setObjectName(QStringLiteral("safetyGroup"));
    m_safetyGroup->setStyleSheet(Style::themed(QStringLiteral(
        "QWidget#safetyGroup { border-left: 1px solid #203040; }")));
    auto* safetyRow = new QHBoxLayout(m_safetyGroup);
    safetyRow->setContentsMargins(8, 0, 0, 0);
    safetyRow->setSpacing(6);

    auto addSlot = [&](QWidget* badge, int widthPx = kSafetySlotWidthPx) {
        auto* slot = new QWidget(m_safetyGroup);
        slot->setObjectName(QStringLiteral("safetySlot"));
        slot->setFixedWidth(widthPx);
        auto* sl = new QHBoxLayout(slot);
        sl->setContentsMargins(0, 0, 0, 0);
        sl->addWidget(badge);
        badge->setParent(slot);
        safetyRow->addWidget(slot);
    };

    addSlot(m_paStatusBadge);
    // The alarm gets a slot sized to its own content, not to its
    // neighbours. PA and TX stay narrow and learnable by position.
    addSlot(m_adcOvlBadge, kOverloadSlotWidthPx);
    addSlot(m_txStatusBadge);
    hbox->addWidget(m_safetyGroup);

    // Wire TX badge to MoxController. MoxController lives on m_radioModel;
    // both are created before buildStatusBar() runs.
    if (MoxController* mox = m_radioModel->moxController()) {
        // Qt::UniqueConnection is not usable on a lambda target — Qt6 warns
        // and refuses such a connect outright. Not needed here either: this
        // runs once at construction, so there is nothing to deduplicate.
        connect(mox, &MoxController::moxStateChanged, this, [this](bool tx) {
            // While inhibited the badge is showing the prohibition symbol and
            // must keep showing it; a MOX transition underneath must not
            // repaint over an active interlock.
            if (m_txInhibited) { return; }
            m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
            m_txStatusBadge->setVariant(tx ? StatusBadge::Variant::Tx
                                           : StatusBadge::Variant::Off);
            m_txStatusBadge->setToolTip(tx ? tr("Transmitting (MOX engaged)")
                                           : tr("Receive (MOX off)"));
        });
    }

    // ── OverflowChip: surfaces folded-item contents when the strip is tight
    // Sits at the right end of the strip; the "…" appears whenever
    // m_chromeBar has folded >= 1 item to fit the bar width. Hidden
    // (unavailable) when nothing is folded. The clock lives on TitleBar
    // now (Task A7), not here.
    m_overflowChip = new OverflowChip(barWidget);
    hbox->addWidget(m_overflowChip);

    // ── CPU usage timer ──────────────────────────────────────────────────────
    // Two sources, user-toggleable via right-click on m_systemTile:
    //   System  (default) — host_processor_info / whole-machine CPU,
    //                       mirrors Thetis _total_cpu_usage PerformanceCounter.
    //   App     — getrusage(RUSAGE_SELF), this process only,
    //                       mirrors Thetis Common.ProcessCPUUsage.
    // 1 s tick rate matches Thetis cpu_meter_delay (console.cs:20102).
    // Smoothed value via 0.8/0.2 mix per Thetis console.cs:26224.
    // Restore persisted toggle (default System per Thetis default).
    // Wired on every supported platform — readSystemCpuPercent /
    // readProcessCpuPercent are cross-platform (macOS / Linux / Windows).
    // The context-menu policy + connect are wired above, next to
    // m_systemTile's construction.
    m_cpuShowSystem = (AppSettings::instance()
                          .value(QStringLiteral("CpuShowSystem"),
                                 QStringLiteral("True"))
                          .toString() == QStringLiteral("True"));
    if (m_systemTile) { m_systemTile->setCpuSource(m_cpuShowSystem); }

    m_cpuTimer = new QTimer(this);
    connect(m_cpuTimer, &QTimer::timeout, this, [this]() {
        const double pct = m_cpuShowSystem ? readSystemCpuPercent()
                                           : readProcessCpuPercent();
        // Thetis smoothing: smoothed = smoothed*0.8 + new*0.2
        m_cpuSmoothedPct = m_cpuSmoothedPct * 0.8 + pct * 0.2;
        if (m_systemTile) {
            m_systemTile->setCpuPercent(m_cpuSmoothedPct);
            // CPU's digit count varies (0-100%), so its row can change
            // width tick to tick. Cheap: relayout() no-ops unless the
            // computed rung actually changes (ChromeBarController::relayout).
            if (m_chromeBar && m_chromeBarWidget) {
                m_chromeBar->setNaturalWidth(m_systemTile,
                                             m_systemTile->sizeHint().width());
                m_chromeBar->relayout(m_chromeBarWidget->width());
            }
        }
    });
    // Task 3.6: restore persisted rate (default 1 Hz = 1000 ms interval).
    {
        const int savedHz = AppSettings::instance().value(
            QStringLiteral("GeneralCpuMeterUpdateRateHz"), 1).toInt();
        const int clampedHz = qBound(1, savedHz, 30);
        m_cpuTimer->start(1000 / clampedHz);
    }

    // ── Layout authority (design §5) ───────────────────────────────────
    // One controller replaces RxDashboard's internal ladder, the old
    // right-strip drop-priority pass, and Qt's squeeze in the left section.
    // The rung assignments live in registerChromeBarItems so they can be
    // tested without constructing MainWindow; do not inline them here.
    m_chromeBar = new ChromeBarController(this);

    ChromeBarWidgets bar;
    bar.panButton        = panBtn;
    bar.panelToggle      = panelToggleLabel;
    bar.stationBlock     = m_stationBlock;
    bar.safetyGroup      = m_safetyGroup;
    bar.psaIndicator     = m_psaIndicator;
    bar.overflowChip     = m_overflowChip;
    bar.systemTile       = m_systemTile;
    bar.systemTileSep    = m_systemTileSep;
    bar.tgxlChip         = m_tgxlChip;
    bar.catIndicator     = m_catIndicator;
    bar.catSep           = m_catSep;
    bar.tciIndicator     = m_tciIndicator;
    bar.tciSep           = m_tciSep;
    bar.chain0           = m_chain0IndicatorWidget;
    // chain1 is always constructed (below), never null; single-ADC SKUs
    // are gated via setItemAvailable on rxFilterChainCount, not by
    // omitting this widget.
    bar.chain1           = m_chain1IndicatorWidget;
    bar.rxDashRow        = m_rxDashboard;
    bar.placeholderGroup = m_placeholderGroup;
    bar.placeholderSep   = m_placeholderSep;
    bar.bandStackLabel   = m_bandStackLabel;
    bar.tnfLabel         = m_tnfLabel;
    bar.txMox            = m_txMoxSwitch;
    bar.txVox            = m_txVoxSwitch;
    bar.txTune           = m_txTuneSwitch;
    bar.txPs             = m_txPsSwitch;
    // 5..12: die fuenf DSP-Pillen plus die drei Zugaenge vom 2026-08-17
    // (VAX 10, ANT 11, RIT 12). TX ist absichtlich nicht dabei — es
    // faltet nie und zaehlt in residualWidth mit.
    for (int rung = 5; rung <= 12; ++rung) {
        bar.pillByRung[rung] = m_rxDashboard->badgeForRung(rung);
    }

    registerChromeBarItems(*m_chromeBar, bar);

    // ── Aufgeraeumt auf Ansage — ueber den REGLER ────────────────────
    //
    // Der Betreiber, 2026-08-20: „A-TX-LSB-2,9k, M, NR und ATN
    // loeschen, dafuer gibt es widgets, 40m kannst du auch loeschen.
    // daneben sind ganz links noch zeichen, bitte weg."
    //
    // Beim ersten Versuch stand hier ein setVisible(false) direkt an
    // den Widgets — und mit MOX, VOX und TUNE verschwand auch, was
    // bleiben sollte. Der Grund steht im Kopf von ChromeBarController:
    //
    //   „assumes it is the sole writer of visibility for every item it
    //    registers; calling setVisible directly on a registered widget
    //    … will desync it from the next relayout's decision."
    //
    // Die Faltungsrechnung arbeitet mit gemeldeten Breiten; ein Widget,
    // das ohne ihr Wissen verschwindet, verschiebt die Rechnung, und
    // sie faltet weiter unten in der Leiter weg, was noch gepasst
    // haette. setItemAvailable ist der dafuer vorgesehene Weg — der
    // Regler weiss dann, dass das Element nicht da ist, und rechnet
    // ohne es.
    //
    // Versteckt, nicht geloescht: die Verdrahtung bleibt heil, ein
    // Wiedereinschalten ist eine Zeile. Was an ihre Stelle tritt:
    //   Bandstapel-Punkte  ->  Band-Menue in der Menueleiste
    //   +PAN-Symbol        ->  View -> Pan Layout… (Ctrl+L)
    //   CH 0 / 40m         ->  die Pille im Panadapter-Kopf
    //   A/TX/LSB/2.9k/…    ->  die Applets RX, Filter, Frequenz
    for (QWidget* w : {static_cast<QWidget*>(bar.bandStackLabel),
                       static_cast<QWidget*>(bar.panButton),
                       static_cast<QWidget*>(bar.chain0),
                       static_cast<QWidget*>(bar.rxDashRow)}) {
        if (w) { m_chromeBar->setItemAvailable(w, false); }
    }

    // registerChromeBarItems just measured w.rxDashRow's raw sizeHint(),
    // which at this point in construction happens to equal tag + mode +
    // filter + AGC (the four badges visible before any slice binds) --
    // wrong on two counts: it double-counts AGC (registered separately at
    // rung 9 above) and it would go stale the moment any pill's
    // visibility changes. Override with the pill-independent residual
    // immediately (final-fix-wave finding 5).
    if (m_rxDashboard) {
        m_chromeBar->setNaturalWidth(m_rxDashboard,
                                     m_rxDashboard->residualWidth());
    }

    // Items that start unavailable until their owning signal says
    // otherwise (Task A8 fix round 1, findings 1-3). No radio has
    // connected and no slice has bound yet at this point in construction,
    // so nothing is known to be present, armed or DSP-active. Without
    // this, availability defaults to true (addItem's default) and the
    // FIRST relayout() -- which always runs a full pass, since
    // m_foldedThrough starts at -1 -- would force-show a blank PSA
    // indicator and stray "TGXL" / "CH 1" tiles, and RxDashboard's four
    // toggle pills would pop up empty on every cold launch.
    m_chromeBar->setItemAvailable(m_psaIndicator, false);
    m_chromeBar->setItemAvailable(m_tgxlChip, false);
    m_chromeBar->setItemAvailable(m_chain1IndicatorWidget, false);
    m_chromeBar->setItemAvailable(m_overflowChip, false);
    for (int rung = 5; rung <= 8; ++rung) {  // SQL, APF, NB, NR
        m_chromeBar->setItemAvailable(m_rxDashboard->badgeForRung(rung), false);
    }
    // Die drei Zugaenge starten ebenfalls aus: eine Antenne, ein
    // RIT-Versatz und ein VAX-Kanal gibt es erst, wenn eine Scheibe
    // etwas davon sagt.
    for (int rung = 10; rung <= 12; ++rung) {
        m_chromeBar->setItemAvailable(m_rxDashboard->badgeForRung(rung), false);
    }
    // AGC (rung 9) has no off state and keeps the default available=true.

    // RxDashboard's on*Changed handlers report DSP-active state (and
    // hence width, since StatusBadge::setLabel changes minimum width
    // live) through this signal instead of calling setVisible directly
    // (RxDashboard.h doc comment).
    connect(m_rxDashboard, &RxDashboard::badgeAvailabilityChanged,
            this, [this](int rung, bool available) {
        if (!m_chromeBar || !m_chromeBarWidget) { return; }
        StatusBadge* badge = m_rxDashboard->badgeForRung(rung);
        if (!badge) { return; }
        m_chromeBar->setItemAvailable(badge, available);
        m_chromeBar->setNaturalWidth(badge, badge->sizeHint().width());
        m_chromeBar->relayout(m_chromeBarWidget->width());
    });

    // The slice tag, mode or filter content changed, so the residual
    // registered above is stale (final-fix-wave finding 5). Mirrors the
    // badgeAvailabilityChanged handler just above, minus the availability
    // half -- the row itself never folds.
    connect(m_rxDashboard, &RxDashboard::residualWidthChanged, this, [this]() {
        if (!m_chromeBar || !m_chromeBarWidget || !m_rxDashboard) { return; }
        m_chromeBar->setNaturalWidth(m_rxDashboard,
                                     m_rxDashboard->residualWidth());
        m_chromeBar->relayout(m_chromeBarWidget->width());
    });

    // m_overflowChip is registered with m_chromeBar at rung 0 (final-fix-
    // wave finding 4), so the controller is the sole writer of its
    // visibility; setDroppedItems() no longer calls setVisible() itself.
    // This handler feeds it content AND reports the resulting
    // available/unavailable fact back through setItemAvailable, same
    // shape as updatePsaIndicatorVisibility(). The nested relayout() call
    // is safe: the chip's width can only ever push the required rung UP
    // (never down), so once its availability flips true the folded set
    // stays a superset and the chain settles in one extra pass; the
    // reverse direction (labels empty -> chip goes unavailable) is
    // self-consistent at rung 0, which never has anything folded, so no
    // pass beyond that one is triggered either.
    connect(m_chromeBar, &ChromeBarController::foldStateChanged, this,
            [this](const QStringList& labels) {
        m_overflowChip->setDroppedItems(labels);
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setItemAvailable(m_overflowChip, !labels.isEmpty());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    });

    // Add the full-width bar widget to the status bar.
    //
    // Deliberately addWidget rather than addPermanentWidget: the bar takes
    // the full width with stretch 1, so as a permanent widget it would
    // leave a showMessage no room to render and the notice would be lost
    // silently. Nothing calls showMessage any more for exactly that
    // reason; notices go through showToast() below. If you are about to
    // add a showMessage here, it will blank this entire bar. Use
    // showToast().
    sb->addWidget(barWidget, 1);
}

// ── Transient notices ────────────────────────────────────────────────────────
//
// Bench report 2026-07-30 (JJ, KG4VCF): pressing TUNE with PureSignal
// active replaced the whole bottom bar with a single line of text for
// six seconds. Root cause is not the message, it is the surface:
// QStatusBar::showMessage hides every non-permanent widget while a
// message is up, and buildStatusBar adds the entire bar as one such
// widget. So any notice cost the operator the CH pill, the PureSignal
// indicator, the radio name, CAT and TCI state, the PA and TX badges,
// and the clock (which lived on this bar at the time; it has since
// moved to TitleBar, Task A7), all at once, mid-transmit.
//
// The notices themselves are worth keeping. They move here instead.
StatusToast* MainWindow::showToast(const QString& message,
                                   ToastSeverity severity,
                                   int timeoutMs)
{
    if (message.isEmpty()) { return nullptr; }

    // Drop dead entries first so a repeat check and the restack below
    // both see only live toasts.
    m_toasts.removeIf([](const QPointer<StatusToast>& t) { return t.isNull(); });

    // A repeat of something already on screen restarts its countdown
    // rather than stacking a second copy. Several of these fire from
    // signals that can repeat while the condition persists, and a column
    // of identical toasts is worse than the message it replaced.
    for (const QPointer<StatusToast>& existing : m_toasts) {
        if (existing && existing->message() == message) {
            existing->refresh(timeoutMs);
            return existing;
        }
    }

    auto* toast = new StatusToast(message, severity, timeoutMs, this);
    connect(toast, &QObject::destroyed, this, [this]() {
        m_toasts.removeIf([](const QPointer<StatusToast>& t) { return t.isNull(); });
        restackToasts();
    });
    m_toasts.append(toast);
    toast->show();
    restackToasts();
    return toast;
}

void MainWindow::restackToasts()
{
    // Bottom-right, newest nearest the bar, growing upward. Offset clears
    // the status bar itself so a toast never covers the thing it was
    // introduced to stop covering.
    const QRect geom = frameGeometry();
    const int rightEdge = geom.right() - 20;
    int bottom = geom.bottom() - 60;

    for (auto it = m_toasts.crbegin(); it != m_toasts.crend(); ++it) {
        StatusToast* toast = *it;
        if (!toast) { continue; }
        toast->move(rightEdge - toast->width(), bottom - toast->height());
        bottom -= toast->height() + 8;
    }
}

// ── Phase 3M-0 Task 14 / sub-PR-8: PA trip badge update ──────────────────────
// Called by Task 17 wiring when RadioModel::paTrippedChanged fires.
// Flips the StatusBadge variant (On = green ✓ PA OK; Tx = red ✓ PA FAULT)
// and updates the tooltip atomically.
void MainWindow::setPaTripped(bool tripped)
{
    if (!m_paStatusBadge) { return; }
    if (tripped) {
        m_paStatusBadge->setVariant(StatusBadge::Variant::Tx);
        m_paStatusBadge->setToolTip(tr("PA Status — FAULT (PA tripped, MOX dropped)"));
    } else {
        m_paStatusBadge->setVariant(StatusBadge::Variant::On);
        m_paStatusBadge->setToolTip(tr("PA Status — OK"));
    }
}

// ── Phase 3M-0 Task 14: TX Inhibit label state ───────────────────────────────
// Called by Task 17 wiring when TxInhibitMonitor::txInhibitedChanged fires.
// The "INH" pill lives in a permanently allocated safety slot (design
// §4.5) -- dims to 14% opacity when inactive rather than hiding, so this
// never resizes the slot. setVisible() would be a no-op here in the wrong
// direction: the label stays Qt-visible at all times once inside its slot.
void MainWindow::setTxInhibited(bool inhibited)
{
    if (!m_txStatusBadge) { return; }
    if (m_txInhibited == inhibited) { return; }
    m_txInhibited = inhibited;

    if (inhibited) {
        // A prohibition symbol over TX, not a separate "INH" pill. Inhibit is
        // a property of transmit, so it belongs on the transmit indicator;
        // and the pill's abbreviation meant nothing to an operator who had
        // not read the source (bench report, 2026-08-03).
        m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-prohibited.svg"));
        m_txStatusBadge->setVariant(StatusBadge::Variant::Tx);
        m_txStatusBadge->setToolTip(
            tr("Transmit blocked by an external TX Inhibit signal."));

        // The symbol says transmit is blocked; the toast says why, once, at
        // the moment it happens. Error severity because an interlock is
        // actively refusing the operator, which is what that level means.
        // Held so it can be taken down the instant inhibit clears rather
        // than aging out and leaving a stale notice on screen.
        m_txInhibitToast = showToast(
            tr("Transmit blocked: external TX Inhibit asserted."),
            ToastSeverity::Error, 8000);
        return;
    }

    // Cleared. Hand the badge back to whatever MOX currently says, so the
    // operator does not have to key up to get a truthful indicator again.
    if (m_txInhibitToast) {
        m_txInhibitToast->close();
        m_txInhibitToast = nullptr;
    }
    const bool tx = m_radioModel && m_radioModel->moxController()
                    && m_radioModel->moxController()->isMox();
    m_txStatusBadge->setSvgIcon(QStringLiteral(":/icons/badge-dot.svg"));
    m_txStatusBadge->setVariant(tx ? StatusBadge::Variant::Tx
                                   : StatusBadge::Variant::Off);
    m_txStatusBadge->setToolTip(tx ? tr("Transmitting (MOX engaged)")
                                   : tr("Receive (MOX off)"));
}

// ── Phase 23: TCI indicator update + Setup navigation ────────────────────────
//
// updateTciIndicator() — 4 states per design doc §8.4.
// Reads m_tciServerRunning / m_tciClientCount / m_tciHasTxClient (all updated
// by TciServer signal lambdas) and applies color + text + tooltip.
//
// Colors:
//   Off:       #3a4a5a (dim grey)
//   On:        #6f6    (green)
//   On · N:    #6cf    (cyan)
//   On · N TX: #ec6    (orange)
void MainWindow::updateTciIndicator()
{
    if (!m_tciIndicatorBotLabel) { return; }

    QString text;
    QString color;
    QString tooltip;

#ifdef HAVE_WEBSOCKETS
    const quint16 port = m_tciServer ? m_tciServer->port() : 50001;
    const QString addr = QStringLiteral("127.0.0.1:%1").arg(port);
#else
    const QString addr = QStringLiteral("127.0.0.1:50001");
#endif

    if (!m_tciServerRunning) {
        text    = QStringLiteral("Off");
        color   = QStringLiteral("#3a4a5a");
        tooltip = QStringLiteral("TCI Server stopped. Click to open Setup.");
    } else if (m_tciClientCount == 0) {
        text    = QStringLiteral("On");
        color   = QStringLiteral("#6f6");
        tooltip = QStringLiteral("TCI Server listening on %1. No clients.").arg(addr);
    } else if (!m_tciHasTxClient) {
        text    = QStringLiteral("On · %1").arg(m_tciClientCount);
        color   = QStringLiteral("#6cf");
        tooltip = QStringLiteral("TCI Server listening on %1. %2 client%3 connected.")
                      .arg(addr)
                      .arg(m_tciClientCount)
                      .arg(m_tciClientCount == 1 ? QString{} : QStringLiteral("s"));
    } else {
        text    = QStringLiteral("On · %1 ▸TX").arg(m_tciClientCount);
        color   = QStringLiteral("#ec6");
        QString txPeer;
#ifdef HAVE_WEBSOCKETS
        if (m_tciServer) {
            txPeer = m_tciServer->activeTxClientPeer();
        }
#endif
        tooltip = QStringLiteral("TCI Server listening on %1. %2 client%3. %4 holds TX audio.")
                      .arg(addr)
                      .arg(m_tciClientCount)
                      .arg(m_tciClientCount == 1 ? QString{} : QStringLiteral("s"))
                      .arg(txPeer.isEmpty() ? QStringLiteral("A client") : txPeer);
    }

    m_tciIndicatorBotLabel->setText(text);
    m_tciIndicatorBotLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(color));
    if (m_tciIndicator) {
        m_tciIndicator->setToolTip(tooltip);
        // Bottom row text ranges from "Off" to "On · 3 ▸TX" -- a real
        // width change (Task A8 fix round 1 finding 4).
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_tciIndicator,
                                         m_tciIndicator->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    }
}

// openTciSetupPage() — open Setup dialog at "TCI Server" page.
// Pattern-matched from the many other "open setup" sites in MainWindow.cpp
// (e.g. vfoWidget::openSetupRequested, m_overlayPanel::openSetupRequested).
void MainWindow::openTciSetupPage()
{
    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);
    dialog->selectPage(QStringLiteral("TCI Server"));
    dialog->show();
}

// openSetup(pageKey) -- Phase 3P-II Phase 4 Task 90
//
// Generic navigation entry point wired to applet right-click menus.
// Maps a well-known key string to a SetupDialog tree label and opens the
// dialog at that page. If the key is not recognised, logs a warning and
// opens the dialog at the default page (first leaf).
//
// Key -> tree label mapping (Option 1 per plan Task 90):
//   "pgxlAdvanced"  -> "PGXL Advanced"
//   "tgxlAdvanced"  -> "TGXL Advanced"
//   "pgxlInterlock" -> "PGXL Interlock"
//   "peripherals"   -> "Peripherals"
//
// Pattern matches openTciSetupPage(): fresh SetupDialog with WA_DeleteOnClose
// so geometry is not preserved across opens (consistent with all other Setup
// entry points in this file).
void MainWindow::openSetup(const QString& pageKey)
{
    static const QHash<QString, QString> kKeyToLabel = {
        {QStringLiteral("pgxlAdvanced"),  QStringLiteral("PGXL Advanced")},
        {QStringLiteral("tgxlAdvanced"),  QStringLiteral("TGXL Advanced")},
        {QStringLiteral("pgxlInterlock"), QStringLiteral("PGXL Interlock")},
        {QStringLiteral("peripherals"),   QStringLiteral("Peripherals")},
        // Phase 3P-III Task 14: RF-Kit setup page (Setup > CAT & Network > RF-Kit).
        {QStringLiteral("rfKit"),         QStringLiteral("RF-Kit")},
    };

    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);

    const QString label = kKeyToLabel.value(pageKey);
    if (label.isEmpty()) {
        qWarning("MainWindow::openSetup: unknown pageKey '%s' -- opening at default page",
                 qUtf8Printable(pageKey));
    } else {
        dialog->selectPage(label);
    }
    dialog->show();
    dialog->raise();
}

// Phase 3P-II Phase 4 Task 97: PGXL power cap soft-alert toast.
//
// Fires a 5-second QStatusBar toast when peak forward power exceeds the
// operator-configured PGXL cap.  De-bounced: one toast per exceedance event
// (re-armed when fwd drops back below the cap threshold so a subsequent
// exceedance fires a fresh toast).
//
// Design reference:
//   docs/architecture/2026-05-18-pgxl-tgxl-and-analog-smeter-plan.md
//   Task 97 / design ss5.6.2 "TX power cap: soft alert only".
//
// Keys:
//   PGXL_PowerCapEnabled  -- "True"/"False", default "False"
//   PGXL_PowerCapW        -- int watts, default 1500
//
// Connected to RadioModel::ampMetersChanged in buildUI() near Task 43.
void MainWindow::onAmpMetersForPowerCap(float fwd, float /*swr*/)
{
    const bool enabled = AppSettings::instance()
        .value(QStringLiteral("PGXL_PowerCapEnabled"), QStringLiteral("False"))
        .toString() == QStringLiteral("True");
    if (!enabled) {
        m_powerCapToastShown = false;   // keep re-arm state sane if feature toggled
        return;
    }

    const float capW = AppSettings::instance()
        .value(QStringLiteral("PGXL_PowerCapW"), 1500).toFloat();

    if (fwd <= capW) {
        m_powerCapToastShown = false;   // re-arm: fwd is back below cap
        return;
    }

    if (m_powerCapToastShown) { return; }   // de-bounce: already toasted this exceedance
    m_powerCapToastShown = true;

    const QString msg = QStringLiteral("PGXL power %1 W exceeds cap %2 W")
        .arg(static_cast<int>(fwd))
        .arg(static_cast<int>(capW));
    showToast(msg, ToastSeverity::Error, 5000);
    qCWarning(lcMeter) << msg;
}

// ── Phase 3P-II review fix C2: TX interlock warning/denial toasts ────────────
// Both slots display a 5-second notice so the operator knows why TX was
// warned or blocked.  The distinction: warning allows TX to proceed; denial
// means MOX was rejected.  Pattern mirrors onAmpMetersForPowerCap above
// (showToast + qCWarning(lcMeter)).
//
// These are the strongest argument for not using QStatusBar::showMessage
// here: both fire during transmit, and blanking the bottom bar would take
// the PA and TX badges away at the exact moment they matter.
void MainWindow::onTxInterlockWarning(const QString& reason)
{
    const QString msg = QString("TX interlock warning: %1").arg(reason);
    showToast(msg, ToastSeverity::Warning, 5000);
    qCWarning(lcMeter) << msg;
}

void MainWindow::onTxInterlockDenial(const QString& reason)
{
    const QString msg = QString("TX interlock blocked: %1").arg(reason);
    showToast(msg, ToastSeverity::Error, 5000);
    qCWarning(lcMeter) << msg;
}

// ── Task 3.6: CPU meter rate ─────────────────────────────────────────────────
// Live-applies the CPU meter update interval from GeneralOptionsPage spinbox.
// Restarts m_cpuTimer with the new period. hz is clamped to [1, 30] so a
// zero or negative value from a misconfigured spinbox cannot stop the timer.
void MainWindow::setCpuTimerIntervalHz(int hz)
{
    if (!m_cpuTimer) { return; }
    const int clamped = qBound(1, hz, 30);
    m_cpuTimer->setInterval(1000 / clamped);
}

// ── Task 3.6: ANAN-8000DLE volts/amps visibility ────────────────────────────
// Called when the "Show volts/amps in title bar" checkbox on Hardware →
// Radio Info is toggled.  Only has visible effect for ANAN-8000D radios
// (SystemTile's PA row is already hidden for non-MKII boards by the
// hardware gate in buildStatusBar(); this gives the user an additional
// opt-out).
void MainWindow::setVoltsAmpsVisible(bool visible)
{
    if (!m_systemTile) { return; }
    // SystemTile::clearPaVolts() clears only the volts flag; if a
    // temperature reading is also present (HL2-class row-sharing, design
    // §4.3) it keeps the tile's PA row alive, same intent as the old
    // refreshPaStackVisibility() symmetry. Idempotent, so no isVisible()
    // guard is needed the way the old m_paVoltLabel check had.
    if (!visible) {
        m_systemTile->clearPaVolts();
        if (m_chromeBar && m_chromeBarWidget) {
            m_chromeBar->setNaturalWidth(m_systemTile,
                                         m_systemTile->sizeHint().width());
            m_chromeBar->relayout(m_chromeBarWidget->width());
        }
    }
    // Note: toggling back to visible=true does not force-show the reading;
    // the next userAdc0Changed will re-show it. This avoids showing a stale
    // "—" value before the first ADC reading arrives.
}

// ---------------------------------------------------------------------------
// Phase 3M-3a-ii Batch 6 (Task 3) — wireSetupDialog
//
// Centralized helper called from every SetupDialog construction site.  Wires
// the dialog's cfcDialogRequested signal (forwarded from CfcSetupPage's
// [Configure CFC bands…] button) to TxApplet::requestOpenCfcDialog so the
// modeless TxCfcDialog instance owned by the TxApplet is reused.
//
// Pre-condition: m_txApplet is set (TxApplet is created during early UI
// build-out, well before any of the user-triggered SetupDialog opens).
// ---------------------------------------------------------------------------
void MainWindow::wireSetupDialog(SetupDialog* dialog)
{
    if (!dialog) { return; }
    if (m_txApplet) {
        connect(dialog, &SetupDialog::cfcDialogRequested,
                m_txApplet, &TxApplet::requestOpenCfcDialog);
    }
    // Phase 3P-II Phase 4 Task 95: propagate TGXL antenna label edits from
    // Setup -> Network -> TGXL Advanced -> Antenna Labels to the TunerApplet
    // antenna buttons so they update live without restarting.
    if (m_tunerApplet) {
        connect(dialog, &SetupDialog::tgxlAntennaLabelChanged,
                m_tunerApplet, &TunerApplet::onAntennaLabelChanged);
    }
    // Task 3.6: CPU meter rate live-apply.
    connect(dialog, &SetupDialog::cpuMeterRateChanged,
            this,   &MainWindow::setCpuTimerIntervalHz);
    // Task 3.6: ANAN-8000DLE volts/amps live-apply.
    connect(dialog, &SetupDialog::anan8000DleVoltsAmpsChanged,
            this,   &MainWindow::setVoltsAmpsVisible);

#ifdef HAVE_WEBSOCKETS
    // Phase 3J-1 review P2.4: live-wire Setup → Network → TCI Server enable
    // checkbox to the running TciServer.  Previously the checkbox only wrote
    // AppSettings; the server required a manual restart to pick up the change.
    // Now toggling the checkbox immediately starts or stops the server.
    //
    // Auto-connect (QueuedConnection for cross-dialog safety): when `on` is
    // true, start on the persisted port; when false, stop.  If the server is
    // already in the requested state the calls are no-ops (double-start
    // returns false; stop() on a non-running server returns immediately).
    if (m_tciServer) {
        connect(dialog, &SetupDialog::tciServerEnableToggled,
                this, [this](bool on, quint16 port) {
                    if (on) {
                        // Re-read bind address from AppSettings so the start
                        // honors whatever the operator last picked in the
                        // bind-interface dropdown.  CatTciServerPage persists
                        // TciServerBindAddress on every combo change.
                        auto& s = AppSettings::instance();
                        const QString bindStr = s.value(
                            QStringLiteral("TciServerBindAddress"),
                            QStringLiteral("127.0.0.1")).toString();
                        QHostAddress bindAddr;
                        if (!bindAddr.setAddress(bindStr)) {
                            bindAddr = QHostAddress(QHostAddress::LocalHost);
                        }
                        m_tciServer->start(bindAddr, port);
                    } else {
                        m_tciServer->stop();
                    }
                });
        // Phase 3J-1 closeout Item 1 (2026-05-12): live-restart on bind /
        // port change.  CatTciServerPage emits this whenever the operator
        // picks a different interface from the dropdown or edits the port
        // spinbox.  If the server is running, restart it in place; if
        // stopped, the new values are already in AppSettings for next
        // start.  Mirrors the enable-toggled pattern above.
        connect(dialog, &SetupDialog::tciServerBindOrPortChanged,
                this, [this](const QString& bindStr, quint16 port) {
                    if (!m_tciServer->isRunning()) {
                        return;
                    }
                    QHostAddress bindAddr;
                    if (!bindAddr.setAddress(bindStr)) {
                        bindAddr = QHostAddress(QHostAddress::LocalHost);
                    }
                    m_tciServer->stop();
                    m_tciServer->start(bindAddr, port);
                });
        // Phase 3J-1 closeout Item 2 (2026-05-12): "Show Log..." button.
        // The window is owned by MainWindow (lazy-constructed) so it
        // outlives this dialog closing -- WSJT-X sessions can take an
        // hour to settle and the operator wants the log window pinned.
        connect(dialog, &SetupDialog::tciShowLogRequested,
                this,   &MainWindow::showTciLogWindow);

        // Phase 3J-1 bench fix (2026-05-11): forward the TciServer reference
        // into the dialog so CatTciServerPage's Server group box title +
        // Status label update live as clients connect/disconnect and the
        // server starts/stops.  Mirrors Thetis Setup.cs:9491-9494
        // [v2.10.3.13] — TCIClientsConnectedChange updates `grpTCIServer.Text`.
        dialog->setTciServer(m_tciServer);
    }
#endif // HAVE_WEBSOCKETS
}

#ifdef HAVE_WEBSOCKETS
// Phase 3J-1 closeout Item 2 (2026-05-12): lazy-construct the TciLogWindow
// on first request, connect it to TciServer::messageLogged, and show it.
// Subsequent clicks just raise the existing window.  The window is owned
// by MainWindow so it survives Setup dialog close/reopen.
//
// Qt::QueuedConnection on the signal hookup ensures the TciServer's
// emit-side never blocks while the log view processes the entry --
// important during a busy WSJT-X session where ~10 frames/sec arrive
// from each direction.
void MainWindow::showTciLogWindow()
{
    if (!m_tciServer) {
        return;  // No-op in builds without HAVE_WEBSOCKETS or before init.
    }
    if (!m_tciLogWindow) {
        m_tciLogWindow = new TciLogWindow(this);
        connect(m_tciServer, &TciServer::messageLogged,
                m_tciLogWindow, &TciLogWindow::appendEntry,
                Qt::QueuedConnection);
    }
    m_tciLogWindow->show();
    m_tciLogWindow->raise();
    m_tciLogWindow->activateWindow();
}
#else
void MainWindow::showTciLogWindow() {}  // no-op in non-WebSocket builds
#endif // HAVE_WEBSOCKETS

void MainWindow::wireSliceToSpectrum()
{
    SliceModel* slice = m_radioModel->activeSlice();
    if (!slice || !activeSpectrumWidget()) {
        return;
    }

    // Set initial spectrum display. Phase 3G-12: preserve the user's
    // persisted zoom level if present. SpectrumWidget::loadSettings()
    // has already read "DisplayBandwidth" from AppSettings into
    // m_bandwidthHz by this point. If the loaded value is sensible
    // (between 10 kHz and the DDC sample rate), keep it; otherwise
    // fall back to the full-span default (768 kHz = sample rate).
    double freq = slice->frequency();
    const double loadedBw = activeSpectrumWidget()->bandwidth();
    const double initialBw = (loadedBw >= 10000.0 && loadedBw <= 768000.0)
                             ? loadedBw : 768000.0;
    activeSpectrumWidget()->setFrequencyRange(freq, initialBw);
    activeSpectrumWidget()->setDdcCenterFrequency(freq);
    activeSpectrumWidget()->setSampleRate(768000.0);
    activeSpectrumWidget()->setVfoFrequency(freq);
    activeSpectrumWidget()->setFilterOffset(slice->filterLow(), slice->filterHigh());
    activeSpectrumWidget()->setStepSize(slice->stepHz());

    // Hier entstand bis 2026-08-18 die schwebende VFO-Flagge. Sie ist
    // geloescht; siehe die Notiz an der Stelle, wo createSliceFlag
    // stand, fuer die vollstaendige Liste dessen, was umgezogen ist.

    // Mode-driven applet surfaces. These are SINGLE global widgets (one RADE
    // applet, one PhoneCw applet), so they follow the ACTIVE slice's mode and
    // stay here rather than moving into the per-flag path.
    //
    // Restored after the flag-path unification deleted the handler that
    // carried them alongside the flag's own setMode: switching to CW or FM
    // stopped changing the PhoneCw page, and RADE stopped revealing its
    // applet.
    // ── Squelch-Linie im Panadapter ───────────────────────────────────
    //
    // Die Linie erscheint, wenn die Schwelle EINGESTELLT wird, und
    // blendet sich nach drei Sekunden aus (SpectrumWidget::setSquelchLine).
    // Beide Quellen fuehren dorthin: das Ein- und Ausschalten und jede
    // Aenderung des Wertes.
    //
    // Ohne diese Verdrahtung waere die Linie der naechste Fall von
    // „gebaut und an keiner Flaeche" — zwei davon hatten wir an zwei
    // Tagen (Modusgruppen, Bandplan).
    {
        auto pushSquelch = [this, slice] {
            if (auto* sw = activeSpectrumWidget()) {
                sw->setSquelchLine(slice->ssqlEnabled(), slice->amsqThresh());
            }
        };
        connect(slice, &SliceModel::ssqlEnabledChanged, this,
                [pushSquelch](bool) { pushSquelch(); });
        connect(slice, &SliceModel::amsqThreshChanged, this,
                [pushSquelch](double) { pushSquelch(); });
    }

    // ── Squelch-Automatik ─────────────────────────────────────────────
    //
    // Der Rueckweg der Linie darueber: das Panadapter meldet aus dem
    // Rauschboden eine Schwelle, sie geht ins Modell, und das Modell
    // zieht ueber amsqThreshChanged die Linie nach. Der Betreiber sieht
    // die Automatik damit arbeiten, statt ihr glauben zu muessen.
    //
    // Nur wenn der Squelch auch eingeschaltet ist: eine Automatik, die
    // eine Schwelle stellt, die niemand benutzt, verstellt dem Betreiber
    // nur seinen gemerkten Wert.
    if (auto* sw = activeSpectrumWidget()) {
        connect(sw, &SpectrumWidget::autoSquelchThresholdSuggested, this,
                [slice](double dbm) {
            if (slice->ssqlEnabled()) {
                slice->setAmsqThresh(dbm);
            }
        });
    }

    connect(slice, &SliceModel::dspModeChanged, this, [this](DSPMode mode) {
        // Phase 3R L2: RADE applet shows for either RADE sideband, IN ADDITION
        // to PhoneCwApplet -- bench feedback showed PhoneCw hosts the mic gain
        // slider, which RADE TX still needs. Routed through the visibility
        // controller so the wrapper and the menu entry agree, and the user's
        // persisted preference survives the mode change.
        const bool isRade = (mode == DSPMode::RADE_U
                             || mode == DSPMode::RADE_L);
        if (m_appletVis) {
            m_appletVis->setAvailable(QStringLiteral("Rade"), isRade);
        }
        if (m_phoneCwApplet) {
            m_phoneCwApplet->setVisible(true);  // always visible
            switch (mode) {
                case DSPMode::CWL:
                case DSPMode::CWU:
                    m_phoneCwApplet->showPage(1);  // CW page
                    break;
                case DSPMode::FM:
                    m_phoneCwApplet->showPage(2);  // FM page
                    break;
                default:
                    m_phoneCwApplet->showPage(0);  // Phone page
                                                   // (incl. RADE_U / RADE_L)
                    break;
            }
        }
    });

    // Was hier stand, hing an der VFO-Flagge und ist mit ihr gegangen:
    // Boardfaehigkeiten und SKU fuer die ANT/BYPS-Knoepfe, das
    // TX-Abzeichen fuer die Uebergabe zwischen Scheiben, die vier
    // Signale ihres Rechtsklickmenues (Abtastrate, Filterpolitik,
    // Scheibe entfernen, Antennenwahl) und die Anbindung an
    // AlexController::rxOutOnTx.
    //
    // Antennen und Bypass stehen in der RxApplet (ueber
    // AntennaPopupBuilder, der den BYPS-Eintrag unter „Special" fuehrt
    // und an derselben Hardware-Bedingung gattert). Die vier
    // Mehrfach-Panadapter-Signale gehoeren zu Phase 3F und brauchen
    // dort ohnehin eine neue Flaeche.

    // 2026-05-11 bench: wire EOO-decoded RADE speaker callsign to the
    // VFO flag SNR row so "<call> ● <snr>dB" replaces "RADE ● <snr>dB"
    // whenever we have a decoded callsign for the current slice.  Sticky
    // until next decode replaces it; SliceModel clears the field when
    // setDspMode leaves RADE_U/RADE_L (A + D semantics per bench design
    // 2026-05-11).  Seed the current cached value once so a slice that
    // already holds a decoded callsign (e.g. from before the user opened
    // a panadapter container) paints correctly on first show.

    // --- Slice → spectrum display ---

    // VFO frequency change → move VFO marker
    // In CTUN mode (SmartSDR-style): pan stays fixed, VFO moves within it.
    // In traditional mode: pan follows VFO (auto-scroll handled in setVfoFrequency).
    // Band changes (large jumps) always recenter regardless of mode.
    //
    // ── Die Ueberschrift stand hier, die Verbindung NICHT ────────────
    //
    // Gefunden am 2026-08-22, und sie erklaert mehrere Befunde des
    // Betreibers auf einmal:
    //
    //   "balken spring nicht dort hin"  — ein Klick aenderte die
    //   Scheibe, aber niemand sagte es dem Panadapter; der tuerkise
    //   Balken blieb, wo er war.
    //
    //   "cursor tastatur sollte auch die frequenz ändern" — die
    //   Pfeiltaste rechnet vom VFO des WIDGETS. Der stand noch auf dem
    //   Vorgabewert. Gemessen: nach setFrequency(7,1 MHz) ergab ein
    //   Tastendruck 14.225.100 Hz statt 7.100.100 — also ein Schritt
    //   von 14,225 MHz, dem Startwert, den das Widget nie verlassen
    //   hatte.
    //
    // Dasselbe galt fuer jede andere Abstimmquelle: Bandknoepfe,
    // CAT, Speicher. Der Balken folgte nur dem, was man DIREKT an ihm
    // zog.
    connect(slice, &SliceModel::frequencyChanged, this,
            [this, slice](double hz) {
        if (!slice) { return; }
        SpectrumWidget* sw = nullptr;
        const QString panKey = slice->panKey();
        if (m_panStack && !panKey.isEmpty()) {
            sw = m_panStack->spectrum(panKey);
        }
        if (!sw) { sw = activeSpectrumWidget(); }
        if (sw) { sw->setVfoFrequency(hz); }
    });


    // Task 42 (Phase 3P-II): reconfigure the Max Bin detector whenever the
    // IF passband changes so the passband-strongest-bin reading follows the
    // active filter window.
    //
    // 100 ms QTimer::singleShot debounce: rapid filter edge drags (e.g.
    // VFO flag drag) would otherwise call SetupDetectMaxBin on every
    // intermediate sample, which re-initialises the WDSP analyzer DSP
    // block at display-interrupt rate and wastes CPU.
    //
    // WdspEngine::setupMaxBinDetector wraps Thetis Console/dsp.cs:846-847
    // [@501e3f5] SetupDetectMaxBin; display channel = 0 (single panadapter).
    // Sample rate: primaryFftEngine()->sampleRate() at call time, which
    // reflects the currently active DDC bandwidth.  Primary engine because
    // the detector is set up with disp=0 (single display channel).
    // Frame rate: primaryFftEngine()->outputFps() * 1.1 matches Thetis
    //   console.cs:51150 [@501e3f5]: (int)Math.Max(1, _display_fps * 1.1f).
    // ── Die neue Breite auch ZEIGEN ─────────────────────────────────
    //
    // Der Betreiber am 2026-08-22: "auch wenn ich die bandbreite oben
    // ändere sehe ich keine änderung im diagramm."
    //
    // Stimmt: setFilterOffset() wurde nur an ZWEI Stellen gerufen —
    // beim Anlegen einer Scheibe und beim Wechsel der aktiven. Bei
    // einer Aenderung der Breite nirgends. Der Wert im Modell stimmte,
    // die DSP bekam ihn (der Zeitgeber unten), nur der tuerkise Balken
    // im Panadapter blieb, wie er war.
    //
    // Sofort, nicht verzoegert: der Balken ist Anzeige, kein
    // Geraetebefehl. Die 100 ms unten sind fuer WDSP da.
    connect(slice, &SliceModel::filterChanged, this,
            [this, slice](int low, int high) {
        if (!slice) { return; }
        SpectrumWidget* sw = nullptr;
        const QString panKey = slice->panKey();
        if (m_panStack && !panKey.isEmpty()) {
            sw = m_panStack->spectrum(panKey);
        }
        if (!sw) { sw = activeSpectrumWidget(); }
        if (sw) { sw->setFilterOffset(low, high); }
    });

    connect(slice, &SliceModel::filterChanged, this, [this, slice](int low, int high) {
        QTimer::singleShot(100, this, [this, slice, low, high]() {
            FFTEngine* fft = primaryFftEngine();
            if (!m_radioModel || !fft) { return; }
            WdspEngine* eng = m_radioModel->wdspEngine();
            if (!eng) { return; }
            const double rate = fft->sampleRate();
            const int fps = qMax(1, static_cast<int>(fft->outputFps() * 1.1f));
            eng->setupMaxBinDetector(/*disp=*/0, /*ss=*/0, /*LO=*/0,
                                     rate,
                                     static_cast<double>(low),
                                     static_cast<double>(high),
                                     /*tauSeconds=*/0.5,
                                     fps);
            // Re-sync the CTUN slice offset after every setup call.  Filter
            // changes don't move the slice, but they re-run setupMaxBinDetector
            // and we want the offset to be authoritative against the current
            // slice freq vs DDC center -- not whatever stale offset was last
            // pushed by frequencyChanged.  Without this re-sync, a filter
            // change immediately after a CTUN tune could leave the detector
            // pointing at the wrong bins until the user nudges the VFO again.
            const double ddcCenter = activeSpectrumWidget()->ddcCenterFrequency();
            const double sliceFreq = slice ? slice->frequency() : ddcCenter;
            eng->setMaxBinSliceOffsetHz(/*disp=*/0, sliceFreq - ddcCenter);
        });
    });

    // Plan 4 D9 (Cluster E): initial TX mode push so the overlay has the right
    // IQ-space sign convention before the first paint.
    if (activeSpectrumWidget()) {
        activeSpectrumWidget()->setTxMode(slice->dspMode());
        // Initial XIT offset push + signal wires below so the TX overlay
        // centers on the actual TX frequency (RX VFO + XIT) rather than the
        // RX VFO alone.  Codex review feedback on PR #166.
        const int initialXitOffset = slice->xitEnabled() ? slice->xitHz() : 0;
        activeSpectrumWidget()->setTxVfoOffsetHz(initialXitOffset);
    }

    // XIT-enabled toggle and XIT-Hz changes both feed the spectrum's TX
    // overlay center.  When enabled flips off, the offset goes to zero;
    // when on, the offset tracks xitHz.
    auto pushXitOffset = [this, slice]() {
        if (!activeSpectrumWidget()) { return; }
        activeSpectrumWidget()->setTxVfoOffsetHz(slice->xitEnabled() ? slice->xitHz() : 0);
    };
    connect(slice, &SliceModel::xitEnabledChanged, this,
            [pushXitOffset](bool /*enabled*/) { pushXitOffset(); });
    connect(slice, &SliceModel::xitHzChanged, this,
            [pushXitOffset](int /*hz*/) { pushXitOffset(); });








    // ── Hier standen zwoelf Abschnittskoepfe der VFO-Flagge ──────────────
    //
    // `SliceModel -> VfoWidget: RIT/XIT inbound`, `... DSP tab inbound`,
    // `VfoWidget -> Setup -> DSP -> NR/ANF page` und neun weitere. Die
    // Verbindungen darunter gingen mit der Flagge (75cc2c35), die Koepfe
    // blieben stehen — Wegweiser auf Wege, die es nicht mehr gibt.
    //
    // Ein Grabstein statt zwoelf: die Routen fuehren jetzt ueber die
    // RxApplet, die ihre Scheibe selbst bindet (RxApplet::connectSlice)
    // und dafuer keine Verdrahtung in MainWindow braucht. Wer die alten
    // Kanten sehen will:
    //   git show 75cc2c35^:src/gui/MainWindow.cpp
    // --- Noise floor fast-attack triggers (slice is guaranteed non-null here) ---
    {
        auto* nfTracker = m_radioModel->noiseFloorTracker();
        if (nfTracker) {
            // From Thetis v2.10.3.13 display.cs:905 — freq change > 0.5
            connect(slice, &SliceModel::frequencyChanged,
                    this, [nfTracker](double /*hz*/) {
                nfTracker->triggerFastAttack();
            });
            // From Thetis v2.10.3.13 display.cs:880 — mode change
            connect(slice, &SliceModel::dspModeChanged,
                    this, [nfTracker](Longpath::DSPMode /*mode*/) {
                nfTracker->triggerFastAttack();
            });
        }
    }

    // Hier stand die Verdrahtung des schwebenden ✕ von Scheibe A auf
    // removeSlice (Phase 3F, Fehler 2). Sie ist mit der Flagge gegangen
    // (75cc2c35), und zwar ersatzlos MIT ABSICHT: das ✕ war ein Knopf AUF
    // der Flagge. Es gibt keine schwebende Flagge mehr, also auch kein ✕
    // zu verdrahten — keine Luecke, sondern ein entfallener Knopf.
    // Scheiben schliesst man jetzt ueber die RxApplet.


    // The four spectrum controls that act on a slice (click-to-tune, filter-
    // edge drag, pan drag, CTUN toggle) used to be connected here, to
    // activeSpectrumWidget(), with lambdas capturing the `slice` above by
    // value. That pointer is Slice A and never moved, so on pan-0 all four
    // drove Slice A for the life of the session however many times the
    // operator selected another flag. Bench-caught 2026-07-28.
    //
    // They now live in wireSpectrumSliceControls, which ensureOverlayPanels
    // runs for every pan including this one, and which resolves its target
    // through sliceForPan(panId) on each signal instead of capturing it.
    // verify-no-captured-slice-spectrum-wiring.py keeps them from coming back.

    // --- dBm range strip → PanadapterModel (per-band grid storage + AppSettings) ---
    connect(activeSpectrumWidget(), &SpectrumWidget::dbmRangeChangeRequested,
            this, [this](float minDbm, float maxDbm) {
        if (m_radioModel && !m_radioModel->panadapters().isEmpty()) {
            PanadapterModel* pan = m_radioModel->panadapters().first();
            pan->setdBmFloor(static_cast<int>(minDbm));
            pan->setdBmCeiling(static_cast<int>(maxDbm));
        }
    });

    // --- PanadapterModel → dBm range: die Richtung, die nie existierte ---
    //
    // 2026-08-15 am Gerät: dB Max −30 und dB Min −190 in Setup → Display
    // eingetragen, und die Skala im Hauptfenster blieb stehen.
    //
    // levelChanged wurde an zwei Stellen ausgelöst und im ganzen
    // Quellbaum von niemandem gehört. Die Setup-Seite schreibt ins
    // Modell, gezeichnet wird aus SpectrumWidget, und dazwischen war
    // nichts. Aufgefallen ist es erst, als die beiden verschiedene
    // Vorgaben bekamen — vorher standen sie zufällig nah beieinander und
    // der fehlende Draht sah aus wie ein träges Feld.
    //
    // Keine Rückkopplung: setDbmRange() löst dbmRangeChangeRequested
    // nicht aus (das tun nur Ziehen und Radschritt, Zeilen 7309 / 7912 /
    // 8017), der Weg endet also hier. Ein Zug an der Skala läuft
    // Widget → Modell → Widget und kommt beim zweiten Mal auf denselben
    // Zahlen an.
    if (m_radioModel && !m_radioModel->panadapters().isEmpty()) {
        PanadapterModel* pan = m_radioModel->panadapters().first();
        connect(pan, &PanadapterModel::levelChanged,
                this, [this, pan]() {
            if (auto* sw = activeSpectrumWidget()) {
                sw->setDbmRange(static_cast<float>(pan->dBmFloor()),
                                static_cast<float>(pan->dBmCeiling()));
                // setDbmRange speichert nicht selbst — siehe die Notiz
                // dort. Das Modell hat seinen Bandeintrag schon
                // geschrieben; hier geht es um den Schlüssel des Widgets,
                // der beim nächsten Start gelesen wird.
                sw->requestSettingsSave();
            }
        });
    }

    // Set initial lock state
    m_radioModel->receiverManager()->setDdcFrequencyLocked(
        activeSpectrumWidget()->ctunEnabled());

    // Position the VFO flag
    activeSpectrumWidget()->updateVfoPositions();

    // Der S-Meter-Balken der Flagge hing an MeterPoller::smeterUpdated.
    // Er ist mit ihr gegangen; das Zeigerinstrument holt sich seinen
    // Wert ueber die Kennung MeterBinding::SignalAvg wie jede andere
    // Groesse.

    // --- Wire RxApplet to active slice ---
    if (m_rxApplet) {
        m_rxApplet->setSlice(slice);

        // AUTO button toggle → SliceModel
        connect(m_rxApplet, &RxApplet::autoAgcToggled,
                slice, &SliceModel::setAutoAgcEnabled);

        // Right-click AGC-T slider → open Setup dialog to AGC/ALC page
        connect(m_rxApplet, &RxApplet::openSetupRequested, this, [this]() {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(QStringLiteral("AGC/ALC"));
            dialog->show();
        });

        // ── Der Schnellregler-Rechtsklick (2026-08-18) ──────────────
        //
        // Hier stand seit dem 2026-04-22: „RxApplet openNbSetupRequested
        // wiring removed — RxApplet no longer hosts any NB controls
        // (strict Thetis parity). VfoWidget::openNbSetupRequested above
        // handles the NB→Setup hop."
        //
        // Die Begruendung von damals setzt die Flagge voraus. Sie faellt
        // ersatzlos weg (Zielbild Punkt 1), und von den drei Thetis-
        // Flaechen blieben sonst zwei uebrig, die beide Einstellungen
        // sind und keine Bedienung. Entscheidung des Betreibers am
        // 2026-08-18: die Rauschminderung zieht mit.
        //
        // Wortgleich mit der Verdrahtung der Flagge weiter oben — es ist
        // dieselbe Handlung, nur von einer anderen Flaeche aus.
        connect(m_rxApplet, &RxApplet::openNbSetupRequested, this, [this]() {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(QStringLiteral("NB/SNB"));
            dialog->show();
        });
        connect(m_rxApplet, &RxApplet::openNrSetupRequested,
                this, &MainWindow::openNrSetupPage);
    }

    // Dieselbe Stelle aus der oberen Leiste: der Rechtsklick auf eine
    // NR-Pille zeigt die Schnellregler, und deren „mehr…" fuehrt hier
    // hinein. EINE Funktion fuer beide Wege — zwei Kopien desselben
    // Lambdas waeren zwei Stellen, an denen der naechste Seitenname
    // vergessen wird.
    if (m_commandBar) {
        connect(m_commandBar, &CommandBar::openNrSetupRequested,
                this, &MainWindow::openNrSetupPage,
                Qt::UniqueConnection);
    }

    // --- PhoneCwApplet → Setup → Transmit → DEXP/VOX page (Phase 3M-3a-iii Task 15).
    // Right-click on the DEXP [ON] button on the Phone tab opens the
    // SetupDialog and jumps to the DexpVoxPage leaf (Task 14).  Mirrors
    // the SpeechProcessorPage cross-link pattern (TransmitSetupPages.h:201).
    // (VOX-button right-click moved to TxApplet 2026-05-04 with the rest
    // of the VOX surface — see the TxApplet connect just below.)
    if (m_phoneCwApplet) {
        connect(m_phoneCwApplet, &PhoneCwApplet::openSetupRequested, this,
                [this](const QString& /*category*/, const QString& page) {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(page);
            dialog->show();
            dialog->raise();
        });
    }

    // --- TxApplet → Setup → Transmit → DEXP/VOX page (3M-3a-iii bench polish 2026-05-04).
    // Right-click on the VOX button (relocated from PhoneCwApplet) opens
    // the same DexpVoxPage leaf.  Same lambda body as the PhoneCwApplet
    // connect above.
    if (m_txApplet) {
        connect(m_txApplet, &TxApplet::openSetupRequested, this,
                [this](const QString& /*category*/, const QString& page) {
            auto* dialog = new SetupDialog(m_radioModel, this);
            dialog->setAttribute(Qt::WA_DeleteOnClose);
            wireSetupDialog(dialog);
            dialog->selectPage(page);
            dialog->show();
            dialog->raise();
        });
    }

    // --- Wire overlay Band flyout to RadioModel band-click handler (#118) ---
    // The signal still carries legacy (name, freqHz, mode) args for
    // backwards-compat with SpectrumOverlayPanel's kBands table, but the
    // handler now owns seed/restore policy — only the name is consulted.
    // Previously this lambda called setFrequency and silently discarded
    // the mode arg, which was the #118 reproducer (80m click moved VFO
    // but left mode stale).
    // Wired per strip in ensureOverlayPanels() now, so a band click acts on the
    // pan it was clicked on rather than on whichever slice happens to be
    // active. The handler there keeps this one's semantics (name only; the
    // legacy freqHz / mode args stay for SpectrumOverlayPanel's kBands table).

    // Phase 3P-II Task 65: notify PGXL of band changes so the amplifier can
    // switch its bias / antenna profile when the operator crosses a band boundary.
    // Gate: no-op if PGXL is not connected at the time of the band change.
    // Use the slice's actual frequency rather than a band center lookup because
    // Band.h has no centerFreqHz() helper (not needed elsewhere).
    connect(slice, &SliceModel::bandChanged,
            this, [this](Longpath::Band /*b*/) {
        PgxlConnection* pgxl = m_radioModel->pgxlConnection();
        if (!pgxl || !pgxl->isConnected()) { return; }
        SliceModel* s = m_radioModel->activeSlice();
        if (!s) { return; }
        pgxl->setBand(static_cast<int>(s->frequency()));
    });

    // Bench-fix 2026-05-19: also push on within-band frequency changes so
    // PGXL sees every tune, not just band-boundary crossings.
    // bandChanged fires only when the slice crosses a band edge; within-band
    // tunes (e.g. 7.200 -> 7.250 MHz) never trigger it, leaving PGXL's
    // bandA field stale until the operator crosses into an adjacent band.
    // Operator confirmed on-bench: after pairing PGXL via the serial field,
    // frequency changes from the tune wheel were not visible in PGXL status.
    // Debounced: a 200 ms QTimer::singleShot coalesces a burst of tune-wheel
    // clicks into one outbound command. m_pgxlBandPushTokenMs is the last
    // token; only the most recently scheduled callback fires the push.
    connect(slice, &SliceModel::frequencyChanged,
            this, [this](qint64 hz) {
        Q_UNUSED(hz);
        const qint64 nowMs = QDateTime::currentMSecsSinceEpoch();
        m_pgxlBandPushTokenMs = nowMs;
        QTimer::singleShot(200, this, [this, nowMs]() {
            if (m_pgxlBandPushTokenMs != nowMs) { return; }
            PgxlConnection* pgxl = m_radioModel->pgxlConnection();
            if (!pgxl || !pgxl->isConnected()) { return; }
            SliceModel* s = m_radioModel->activeSlice();
            if (!s) { return; }
            pgxl->setBand(static_cast<int>(s->frequency()));
        });
    });

    // SmartSDR API responder: also push slice freq/mode and TX state into the
    // TCP 4992 listener so PGXL/TGXL (acting as SmartSDR clients) pull current
    // band data via the documented API path rather than relying solely on the
    // explicit `flexradio band=N` push above. This is what `bsrcA=FLEX` on the
    // PGXL status frame consumes: the FlexRadio's slice 0 RF_frequency, mode,
    // and transmit MOX state.
    if (auto* l = m_radioModel->smartSdrListener()) {
        // Push current state immediately so the listener doesn't sit on
        // the constructor default (14.250 MHz USB) until the operator first
        // turns the dial. Without this, PGXL would see a misleading band
        // until the first frequency change.
        l->setSliceFrequencyHz(/*sliceId=*/0, slice->frequency());
        l->setSliceMode(/*sliceId=*/0, SliceModel::modeName(slice->dspMode()));
    }
    connect(slice, &SliceModel::frequencyChanged,
            this, [this](qint64 hz) {
        if (auto* l = m_radioModel->smartSdrListener()) {
            l->setSliceFrequencyHz(/*sliceId=*/0, hz);
        }
    });
    connect(slice, &SliceModel::dspModeChanged,
            this, [this](Longpath::DSPMode mode) {
        if (auto* l = m_radioModel->smartSdrListener()) {
            l->setSliceMode(/*sliceId=*/0, SliceModel::modeName(mode));
        }
    });

    // Phase 3P-II review fix C1: keep TunerApplet m_currentBand in sync so
    // right-click Save/Recall/Clear actions always address the actual current
    // (antenna, band) slot rather than the Band::Band20m default.
    if (m_tunerApplet) {
        connect(slice, &SliceModel::bandChanged,
                m_tunerApplet, &TunerApplet::setBand);
        // Seed with the slice's current band so the first context-menu open
        // before any band crossing is already correct.
        m_tunerApplet->setBand(bandFromFrequency(slice->frequency()));
    }
}

// ── CPU usage source toggle ──────────────────────────────────────────────────
// Right-click menu on m_systemTile — System / App radio choice.
// Mirrors Thetis's toolStripDropDownButton_CPU with systemToolStripMenuItem
// and thetisOnlyToolStripMenuItem (console.cs:44230-44247). Persists the
// choice in AppSettings under "CpuShowSystem".
void MainWindow::onCpuMenuRequested(const QPoint& localPos)
{
    if (!m_systemTile) { return; }

    QMenu menu(this);
    QAction* sysAct = menu.addAction(tr("System"));
    sysAct->setCheckable(true);
    sysAct->setChecked(m_cpuShowSystem);
    QAction* appAct = menu.addAction(tr("App (Longpath)"));
    appAct->setCheckable(true);
    appAct->setChecked(!m_cpuShowSystem);

    QAction* chosen = menu.exec(m_systemTile->mapToGlobal(localPos));
    if (!chosen) { return; }

    const bool newSys = (chosen == sysAct);
    if (newSys == m_cpuShowSystem) { return; }

    m_cpuShowSystem = newSys;
    if (m_systemTile) { m_systemTile->setCpuSource(newSys); }
    AppSettings::instance().setValue(
        QStringLiteral("CpuShowSystem"),
        newSys ? QStringLiteral("True") : QStringLiteral("False"));

    // Reset delta state and smoothing so the next reading starts cleanly.
    m_cpuSmoothedPct = 0.0;
    m_cpuProcPrevWallUs = 0;
    m_cpuProcPrevUserUs = 0;
    m_cpuProcPrevSysUs = 0;
    m_cpuSysPrevTotal = 0;
    m_cpuSysPrevIdle = 0;
    // SystemTile's CPU row does start with a "—" placeholder
    // (SystemTile.cpp constructor), but setCpuPercent() only takes a
    // numeric value and there's no way to ask for that placeholder again
    // once the timer is running; 0% is the closest equivalent to a
    // reset-to-placeholder display until the next timer tick.
    m_systemTile->setCpuPercent(0.0);
}

double MainWindow::readProcessCpuPercent()
{
    // Per-platform "process CPU time since boot" readers — return user +
    // kernel time consumed by this process expressed in microseconds.
    // POSIX (macOS / Linux) uses getrusage; Windows uses GetProcessTimes
    // and converts the FILETIME tick counter (100 ns) to microseconds.
    qint64       userUs = 0;
    qint64       sysUs  = 0;
    const qint64 nowUs  = QDateTime::currentMSecsSinceEpoch() * 1000LL;

#if defined(Q_OS_MAC) || defined(Q_OS_LINUX)
    struct rusage ru{};
    if (getrusage(RUSAGE_SELF, &ru) != 0) { return 0.0; }
    auto toUs = [](const struct timeval& tv) -> qint64 {
        return static_cast<qint64>(tv.tv_sec) * 1'000'000LL
             + static_cast<qint64>(tv.tv_usec);
    };
    userUs = toUs(ru.ru_utime);
    sysUs  = toUs(ru.ru_stime);
#elif defined(Q_OS_WIN)
    FILETIME ftCreation{}, ftExit{}, ftKernel{}, ftUser{};
    if (!GetProcessTimes(GetCurrentProcess(),
                         &ftCreation, &ftExit, &ftKernel, &ftUser)) {
        return 0.0;
    }
    auto fileTimeToUs = [](const FILETIME& ft) -> qint64 {
        ULARGE_INTEGER u{};
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return static_cast<qint64>(u.QuadPart / 10);  // 100 ns -> µs
    };
    userUs = fileTimeToUs(ftUser);
    sysUs  = fileTimeToUs(ftKernel);
#else
    return 0.0;
#endif

    if (m_cpuProcPrevWallUs == 0) {
        // First-call sentinel — capture baseline, return 0 this round.
        m_cpuProcPrevWallUs = nowUs;
        m_cpuProcPrevUserUs = userUs;
        m_cpuProcPrevSysUs  = sysUs;
        return 0.0;
    }

    const qint64 wallDelta = nowUs - m_cpuProcPrevWallUs;
    if (wallDelta <= 0) { return 0.0; }

    const qint64 cpuDelta = (userUs - m_cpuProcPrevUserUs)
                          + (sysUs  - m_cpuProcPrevSysUs);

    m_cpuProcPrevWallUs = nowUs;
    m_cpuProcPrevUserUs = userUs;
    m_cpuProcPrevSysUs  = sysUs;

    return 100.0 * static_cast<double>(cpuDelta)
                 / static_cast<double>(wallDelta);
}

double MainWindow::readSystemCpuPercent()
{
    // Per-platform "system CPU time since boot" readers. The CPU usage
    // formula is the same across all three: percent = 100 * (1 - dIdle / dTotal).
    // What differs is how each OS exposes the underlying tick counters.
    //
    // - macOS: host_processor_info(PROCESSOR_CPU_LOAD_INFO) → per-CPU
    //   tick counters; sum across cores.
    // - Linux: /proc/stat first line "cpu  user nice system idle iowait
    //   irq softirq steal guest guest_nice" — total = sum, idle = the
    //   `idle` field (not iowait, matching `top`/`htop` convention).
    // - Windows: GetSystemTimes → idle/kernel/user as FILETIMEs (100 ns).
    //   Note kernel time on Windows *includes* idle, so total = kernel +
    //   user; the percent formula above still holds.
    quint64 totalNow = 0;
    quint64 idleNow  = 0;

#if defined(Q_OS_MAC)
    natural_t                 cpuCount = 0;
    processor_info_array_t    info     = nullptr;
    mach_msg_type_number_t    numInfo  = 0;

    if (host_processor_info(mach_host_self(), PROCESSOR_CPU_LOAD_INFO,
                            &cpuCount, &info, &numInfo) != KERN_SUCCESS) {
        return 0.0;
    }

    auto* cpus = reinterpret_cast<processor_cpu_load_info_t>(info);
    for (natural_t i = 0; i < cpuCount; ++i) {
        for (int s = 0; s < CPU_STATE_MAX; ++s) {
            totalNow += cpus[i].cpu_ticks[s];
        }
        idleNow += cpus[i].cpu_ticks[CPU_STATE_IDLE];
    }

    vm_deallocate(mach_task_self(),
                  reinterpret_cast<vm_address_t>(info),
                  static_cast<vm_size_t>(numInfo) * sizeof(integer_t));
#elif defined(Q_OS_LINUX)
    QFile f(QStringLiteral("/proc/stat"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return 0.0; }
    const QByteArray line = f.readLine();
    f.close();

    // Tokenize on whitespace; first token is "cpu", remaining are tick
    // counts. Empty entries from the doubled space after "cpu" get filtered.
    const QList<QByteArray> rawParts = line.split(' ');
    QList<quint64> vals;
    vals.reserve(10);
    for (int i = 1; i < rawParts.size() && vals.size() < 10; ++i) {
        if (rawParts[i].isEmpty()) { continue; }
        bool ok = false;
        const quint64 v = rawParts[i].toULongLong(&ok);
        if (ok) { vals.append(v); }
    }
    if (vals.size() < 4) { return 0.0; }
    for (auto v : vals) { totalNow += v; }
    idleNow = vals[3];   // idle field; iowait NOT counted as idle (top convention)
#elif defined(Q_OS_WIN)
    FILETIME ftIdle{}, ftKernel{}, ftUser{};
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) { return 0.0; }
    auto fileTimeToTicks = [](const FILETIME& ft) -> quint64 {
        ULARGE_INTEGER u{};
        u.LowPart  = ft.dwLowDateTime;
        u.HighPart = ft.dwHighDateTime;
        return static_cast<quint64>(u.QuadPart);
    };
    const quint64 idle   = fileTimeToTicks(ftIdle);
    const quint64 kernel = fileTimeToTicks(ftKernel);   // includes idle
    const quint64 user   = fileTimeToTicks(ftUser);
    totalNow = kernel + user;
    idleNow  = idle;
#else
    return 0.0;
#endif

    if (m_cpuSysPrevTotal == 0) {
        // First-call sentinel — capture baseline, return 0 this round.
        m_cpuSysPrevTotal = totalNow;
        m_cpuSysPrevIdle  = idleNow;
        return 0.0;
    }

    const quint64 totalDelta = totalNow - m_cpuSysPrevTotal;
    const quint64 idleDelta  = idleNow  - m_cpuSysPrevIdle;
    m_cpuSysPrevTotal = totalNow;
    m_cpuSysPrevIdle  = idleNow;

    if (totalDelta == 0) { return 0.0; }
    return 100.0 * (1.0 - static_cast<double>(idleDelta)
                              / static_cast<double>(totalDelta));
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);

    // Das schwebende Plus hat keine Anordnung, die es mitzieht.
    positionAddWidgetButton();

    // Update axis-lock positions for overlay-docked containers
    if (m_mainSplitter && m_containerManager) {
        // Use the spectrum pane (first splitter child) as reference
        QWidget* spectrumPane = m_mainSplitter->widget(0);
        if (spectrumPane) {
            m_hDelta = spectrumPane->width();
            m_vDelta = spectrumPane->height();
            m_containerManager->updateDockedPositions(m_hDelta, m_vDelta);
        }
    }

    // Single layout authority for the banner (design §5). One relayout()
    // call per resize; no re-measure mid-decision, no deadband. Presence/
    // DSP-active facts (TGXL, CH1, PSA, RX pills) are reported to the
    // controller via setItemAvailable at the signal that changes them,
    // not re-derived here -- see ChromeBarController::setItemAvailable.
    if (m_chromeBar && m_chromeBarWidget) {
        m_chromeBar->relayout(m_chromeBarWidget->width());
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Phase 3Q Sub-PR-4 D.3: TitleBar ConnectionSegment hover tooltip.
    // The segment has installEventFilter(this) in the D.2 wiring block.
    // We intercept QHelpEvent (ToolTip) and delegate to RadioModel for the
    // formatted multi-line string so the segment stays a thin paint layer.
    if (m_titleBar && watched == m_titleBar->connectionSegment()
     && event->type() == QEvent::ToolTip) {
        auto* helpEvent = static_cast<QHelpEvent*>(event);
        QToolTip::showText(helpEvent->globalPos(),
                           m_radioModel->buildConnectionTooltip(),
                           m_titleBar->connectionSegment());
        return true;
    }

    // Phase 23: m_tciIndicator click → open Setup → TCI Server.
    // The indicator is a QWidget (not a QLabel) so we match by pointer identity.
    if (event->type() == QEvent::MouseButtonPress) {
        if (watched == m_tciIndicator) {
            openTciSetupPage();
            return true;  // event consumed
        }
    }

    // Handle ☰ panel toggle click — label has property "isPanelToggle"
    if (event->type() == QEvent::MouseButtonPress) {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label && label->property("isPanelToggle").toBool()) {
            // Toggle QSplitter right pane (index 1) visibility.
            // Save/restore sizes so spectrum expands when panel is hidden.
            if (!m_mainSplitter || m_mainSplitter->count() < 2) {
                return QMainWindow::eventFilter(watched, event);
            }
            QWidget* rightPane = m_mainSplitter->widget(1);
            if (rightPane->isVisible()) {
                // Hide: save current sizes, then collapse right to 0
                m_splitterSizesBeforeHide = m_mainSplitter->sizes();
                rightPane->hide();
                label->setStyleSheet(Style::themed(QStringLiteral(
                    "QLabel { color: #3a4a5a; font-weight: bold; font-size: 16px; }")));
            } else {
                // Show: restore saved sizes (or default 80/20 if none saved)
                rightPane->show();
                if (!m_splitterSizesBeforeHide.isEmpty()) {
                    m_mainSplitter->setSizes(m_splitterSizesBeforeHide);
                } else {
                    m_mainSplitter->setSizes({1024, 256});
                }
                label->setStyleSheet(Style::themed(QStringLiteral(
                    "QLabel { color: #8aa8c0; font-weight: bold; font-size: 16px; }")));
            }
            return true;  // event consumed
        }
    }

    // Task B4: +PAN icon click; label has property "isAddPanButton".
    // updateAddPanButtonState() disables the label while disconnected, so
    // this fires only when a layout change is actually possible; the
    // showPanLayoutDialog() body re-checks the same guard defensively.
    // Left button only (final-fix-wave finding 12): a bare
    // QEvent::MouseButtonPress check fires on right-click and middle-
    // click too, which is not how every other clickable icon on this bar
    // behaves (compare StationBlock::mousePressEvent's explicit button
    // check).
    if (watched->property("isAddPanButton").toBool()
        && event->type() == QEvent::MouseButtonPress
        && static_cast<QMouseEvent*>(event)->button() == Qt::LeftButton) {
        showPanLayoutDialog();
        return true;
    }

    // Status-bar TNF light: click toggles every notch at once.
    // From AetherSDR MainWindow_Shortcuts.cpp:612-614 [@c6481cbf], which
    // flips the model flag straight from the indicator's mouse press. The
    // DSP > TNF menu item follows through globalEnabledChanged, so either
    // surface can originate the flip and neither echoes it back.
    if (event->type() == QEvent::MouseButtonPress) {
        auto* label = qobject_cast<QLabel*>(watched);
        if (label && label->property("isTnfToggle").toBool()) {
            auto* me = static_cast<QMouseEvent*>(event);

            // ── Rechtsklick: die Filter selbst ───────────────────────
            //
            // Der Betreiber, 2026-08-21: „finde keinen button fuer die
            // aenderung des notch filter" — und vorher schon: „man
            // koennte auch unten in der taskleiste das menue erweitern
            // mit notchfiltereigenschaften."
            //
            // Bis hierher ging Bearbeiten nur AM BALKEN: Rechtsklick
            // oder Doppelklick auf die gelbe Flaeche im Spektrum. Das
            // setzt voraus, dass man sie trifft — und dass sie
            // ueberhaupt im sichtbaren Ausschnitt liegt. Ein Filter auf
            // 7,19 MHz ist unerreichbar, solange man 7,05 ansieht.
            //
            // Die TNF-Anzeige ist der richtige Ort dafuer: sie ist
            // immer da, sie zeigt ohnehin schon, ob Filter liegen und
            // ob sie wirken.
            if (me->button() == Qt::RightButton) {
                showNotchBarMenu(me->globalPosition().toPoint());
                return true;
            }

            NotchModel* notches =
                m_radioModel ? m_radioModel->notchModel() : nullptr;
            if (notches) {
                notches->setGlobalEnabled(!notches->globalEnabled());
            }
            return true;  // event consumed
        }
    }

    return QMainWindow::eventFilter(watched, event);
}

// ── Die Notch-Filter aus der Fussleiste erreichen ────────────────────
//
// Siehe die Notiz im Ereignisfilter: der Weg am Balken taugt nur fuer
// Filter, die man gerade sieht. Hier stehen alle, mit Frequenz und
// Breite, und jeder laesst sich von hier aus oeffnen, stummschalten
// oder wegwerfen.
//
// Der Editor ist derselbe wie am Balken (SpectrumWidget::
// openNotchEditor), und die Wuensche gehen denselben Weg ins Modell.
// Ein zweiter Bearbeitungspfad waere ein zweiter Ort, an dem etwas
// auseinanderlaufen kann.
void MainWindow::showNotchBarMenu(const QPoint& globalPos)
{
    NotchModel* notches = m_radioModel ? m_radioModel->notchModel() : nullptr;
    if (!notches) { return; }

    auto* menu = new QMenu(this);
    menu->setAttribute(Qt::WA_DeleteOnClose);

    const QList<Notch>& list = notches->notches();

    QAction* head = menu->addAction(
        list.isEmpty()
            ? tr("Keine Notch-Filter")
            : tr("%n Notch-Filter", "", static_cast<int>(list.size())));
    head->setEnabled(false);

    QAction* global = menu->addAction(tr("TNF aktiv"));
    global->setCheckable(true);
    global->setChecked(notches->globalEnabled());
    global->setToolTip(tr(
        "Alle Notch-Filter auf einmal stumm schalten, ohne sie zu "
        "verlieren. Dasselbe wie ein Linksklick auf TNF."));
    connect(global, &QAction::toggled, this, [notches](bool on) {
        notches->setGlobalEnabled(on);
    });

    if (list.isEmpty()) {
        menu->addSeparator();
        QAction* hint = menu->addAction(
            tr("Anlegen: Cmd-Klick ins Spektrum"));
        hint->setEnabled(false);
        menu->popup(globalPos);
        return;
    }

    menu->addSeparator();

    for (const Notch& n : list) {
        // Frequenz in MHz mit sechs Stellen — dieselbe Schreibweise wie
        // im Kopf des Menues am Balken, damit man dieselbe Zeile
        // wiedererkennt.
        QMenu* sub = menu->addMenu(
            tr("%1 MHz · %2 Hz")
                .arg(n.centerHz / 1.0e6, 0, 'f', 6)
                .arg(qRound(n.widthHz)));

        const int id = n.id;

        QAction* edit = sub->addAction(tr("Bearbeiten…"));
        connect(edit, &QAction::triggered, this, [this, id, globalPos]() {
            SpectrumWidget* w =
                m_radioModel ? m_radioModel->spectrumWidget() : nullptr;
            if (w) { w->openNotchEditor(id, globalPos); }
        });

        QAction* act = sub->addAction(tr("Aktiv"));
        act->setCheckable(true);
        act->setChecked(n.active);
        connect(act, &QAction::toggled, this, [notches, id](bool on) {
            notches->setActive(id, on);
        });

        sub->addSeparator();
        QAction* del = sub->addAction(tr("Löschen"));
        connect(del, &QAction::triggered, this, [notches, id]() {
            notches->removeNotch(id);
        });
    }

    if (list.size() > 1) {
        menu->addSeparator();
        QAction* all = menu->addAction(
            tr("Alle %1 entfernen…").arg(list.size()));
        connect(all, &QAction::triggered,
                this, &MainWindow::onNotchRemoveAll);
    }

    menu->popup(globalPos);
}

// Die Einstellungsseite einer Rauschminderung oeffnen und gleich auf
// deren Reiter springen. Aus dem RX-Feld UND aus der oberen Leiste
// erreichbar; beide gehen denselben Weg.
void MainWindow::openNrSetupPage(Longpath::NrSlot slot)
{
    auto* dialog = new SetupDialog(m_radioModel, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    wireSetupDialog(dialog);
    dialog->selectPage(QStringLiteral("NR/ANF"));
    if (auto* nrPage = dialog->findChild<NrAnfSetupPage*>()) {
        nrPage->selectSubtab(slot);
    }
    dialog->show();
}

void MainWindow::applyDarkTheme()
{
    // ── Der Dock-Griff neben Rotor/Log ───────────────────────────────
    //
    // Betreiber, 2026-08-30: "roto log ist wieder nicht
    // größenveränderbar" -- angedockt ist Rotor/Log ein echtes
    // QDockWidget (ensureRotorPanel()), dessen Ziehgriff Qts eigener,
    // UNGESTALTETER QMainWindow::separator ist. Gegen das fast
    // schwarze Hausstil-Grau ist der praktisch unsichtbar und schwer
    // zu treffen -- derselbe Fehler wie bei den drei Pixel breiten
    // Splitter-Griffen, den Style::splitterStyle() schon einmal
    // behoben hat (siehe dort), nur diesmal am QMainWindow selbst statt
    // an einem QSplitter. Dieselben Masse, damit sich beide Griffe
    // gleich anfuehlen.
    const QString separatorStyle = QStringLiteral(
        "QMainWindow::separator { background: %1; width: %2px; "
        "  height: %2px; border: none; }"
        "QMainWindow::separator:hover { background: %3; }")
            .arg(Style::hexRole(Style::kPanelBg))
            .arg(Style::kSplitterHandlePx)
            .arg(Style::hexRole(Style::kAccent));

    setStyleSheet(Style::themed(separatorStyle + QStringLiteral(
        "QMainWindow { background: #0f0f1a; }"
        "QMenuBar {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border-bottom: 1px solid #203040;"
        "}"
        "QMenuBar::item:selected { background: #4a7ba8; }"
        "QMenu {"
        "  background: #1a2a3a;"
        "  color: #c8d8e8;"
        "  border: 1px solid #203040;"
        "}"
        "QMenu::item:selected { background: #4a7ba8; }"
        "QLabel { color: #c8d8e8; }"
        "QStatusBar {"
        "  background: #1a2a3a;"
        "  color: #8090a0;"
        "  border-top: 1px solid #203040;"
        "}")));
}

// ── Alle Schwebefenster hinter die Connect-Maske ─────────────────────
//
// Betreiber 2026-09-01: "ALLE fliegenden Fenster gehören hinter die
// ConnectMaske oder hier gelöscht." EINE Methode fuer alle fuenf
// Schwebe-Mechanismen (ContainerManager-Container, abgeloeste Applet-
// Fenster, Antennen-Werkzeugfenster, Panadapter-Schwebefenster,
// Rotor/Log-ToolWindow) -- die Zaehlung selbst ist die Lehre dieses
// Tages: jede Kopie dieser Liste an einer anderen Stelle hat
// mindestens einen Mechanismus vergessen (erst die Applets, dann die
// Antenne, dann die Pans, zuletzt den Rotor -- alle vier Luecken
// einzeln vom Betreiber oder vom adversarialen Review gefunden).
// Alles Versteckte landet in m_floatingContainersHiddenPreConnect und
// kommt an genau zwei Stellen zurueck: mit der naechsten Verbindung
// (onConnectionStateChanged, Connected-Zweig) oder beim SCHLIESSEN der
// Maske ohne Verbindung (ConnectionPanel-destroyed-Handler unten) --
// Betreiber 2026-09-01: "panadapter kann ich nicht finden / profil 01
// ist leer": nach dem Wegklicken der Maske blieb sonst eine leere
// Flaeche, das Layout des Betreibers unauffindbar versteckt.
void MainWindow::hideFloatingWindowsBehindConnectMask()
{
    for (ContainerWidget* c : m_containerManager->allContainers()) {
        if (!c) { continue; }
        if (c->isFloating()) {
            QWidget* win = c->window();
            if (win && win != this && win->isVisible()) {
                win->hide();
                m_floatingContainersHiddenPreConnect.append(win);
            }
        } else if (c->isOverlayDocked()) {
            if (c->isVisible()) {
                c->hide();
                m_floatingContainersHiddenPreConnect.append(c);
            }
        }
    }
    // Betreiber 2026-09-01, korrigiert nach "es liegt da wieder alles
    // durcheinander, bevor man connected. dies hatte ich dir schon 30
    // mal gesagt": DIESE Methode (nur noch aufgerufen, wenn der
    // Connect-Dialog tatsaechlich aufgeht -- siehe showConnectionPanel()
    // unten, der einzige verbliebene Aufrufer) versteckt weiterhin ALLE
    // schwebenden Fenster einschliesslich Applets/Panadapter/Rotor. Die
    // fruehere Entfernung dieser drei war zu weit gefasst: sie sollten
    // nicht mehr wegen blosser Trennung ohne offenen Dialog verschwinden
    // (das war der eigentliche Wunsch von "kein Funkgerät hier, alles
    // nicht verbunden" -- siehe die entfernten Aufrufe im Profil-
    // Anwenden-Lambda und im Konstruktor), aber SEHR WOHL, solange die
    // "Connect to Radio"-Maske selbst offen ist und den Bildschirm
    // bedeckt -- genau die urspruengliche, wiederholt eingeforderte
    // Regel ("ALLE fliegenden Fenster gehören hinter die ConnectMaske").
    for (AppletFloatingWindow* w : std::as_const(m_floatingApplets)) {
        if (w && w->isVisible()) {
            w->hide();
            m_floatingContainersHiddenPreConnect.append(w);
        }
    }
    if (m_antennaWindow && m_antennaWindow->isVisible()) {
        m_antennaWindow->hide();
        m_floatingContainersHiddenPreConnect.append(m_antennaWindow);
    }
    if (m_panStack) {
        for (const QString& panId : m_panStack->panIdsForTesting()) {
            if (m_panStack->isPanFloating(panId)) {
                if (auto* pf = m_panStack->floatingWindowForTest(panId)) {
                    if (pf->isVisible()) {
                        pf->hide();
                        m_floatingContainersHiddenPreConnect.append(pf);
                    }
                }
            }
        }
    }
    if (m_rotorWindow && m_rotorWindow->isVisible()) {
        m_rotorWindow->hide();
        m_floatingContainersHiddenPreConnect.append(m_rotorWindow);
    }
}

void MainWindow::showConnectionPanel()
{
    // Maske auf, Fenster weg -- egal, WER sie oeffnet (automatisch
    // nach einer Trennung oder von Hand ueber Menue/Klick auf die
    // Statuszeile). Vorher galt das nur fuer den automatischen Weg.
    if (!m_radioModel
        || m_radioModel->connectionState() != ConnectionState::Connected) {
        hideFloatingWindowsBehindConnectMask();
    }
    if (!m_connectionPanel) {
        m_connectionPanel = new ConnectionPanel(m_radioModel, this);
        m_connectionPanel->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_connectionPanel, &QObject::destroyed, this, [this]() {
            m_connectionPanel = nullptr;
            // Betreiber 2026-09-01: "panadapter kann ich nicht finden
            // ... profil 01 ist leer" -- die Maske ist zu, aber ohne
            // Verbindung blieb ALLES dauerhaft versteckt und der
            // Betreiber sah nur noch eine leere Flaeche. Die Regel
            // heisst "hinter die ConnectMaske", nicht "weg bis zur
            // Verbindung": schliesst der Betreiber die Maske, gehoert
            // ihm sein Layout zurueck. Beim Verbinden uebernimmt
            // stattdessen der Connected-Zweig in
            // onConnectionStateChanged dieselbe Liste (dann ist sie
            // hier schon leer -- doppeltes show() droht nicht).
            if (m_shuttingDown) { return; }
            if (m_radioModel
                && m_radioModel->connectionState()
                       == ConnectionState::Connected) {
                return;
            }
            for (const QPointer<QWidget>& w
                 : std::as_const(m_floatingContainersHiddenPreConnect)) {
                if (w) { w->show(); w->raise(); }
            }
            m_floatingContainersHiddenPreConnect.clear();
        });
    }
    m_connectionPanel->show();
    m_connectionPanel->raise();
    m_connectionPanel->activateWindow();
}

// Phase 3Q Sub-PR-4 D.2 — right-click context menu on the TitleBar
// ConnectionSegment. "Reconnect" is intentionally absent: RadioModel has no
// public reconnect() API (tryAutoReconnect() is private to MainWindow and
// starts a full probe + discovery cycle, which is not appropriate to invoke
// from a context menu that the user might trigger mid-session). The user can
// use "Connect to other radio…" to re-select the same radio.
void MainWindow::showSegmentContextMenu(const QPoint& globalPos)
{
    QMenu menu(this);

    menu.addAction(tr("Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });
    menu.addAction(tr("Connect to other radio…"), this, [this]() {
        showConnectionPanel();
    });
    menu.addSeparator();
    menu.addAction(tr("Network diagnostics…"), this, [this]() {
        auto* dlg = new NetworkDiagnosticsDialog(
            m_radioModel, m_radioModel->audioEngine(), this);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->show();
    });
    menu.addSeparator();
    menu.addAction(tr("Copy IP address"), this, [this]() {
        QGuiApplication::clipboard()->setText(m_radioModel->connectionIpText());
    });
    menu.addAction(tr("Copy MAC address"), this, [this]() {
        QGuiApplication::clipboard()->setText(m_radioModel->connectionMacText());
    });

    menu.exec(globalPos);
}

void MainWindow::showStationContextMenu(const QPoint& globalPos)
{
    // Only show when connected — StationBlock only emits contextMenuRequested
    // in connected appearance, but guard here defensively.
    if (m_radioModel->connectionState() != ConnectionState::Connected) {
        return;
    }

    QMenu menu(this);

    menu.addAction(tr("Disconnect"), this, [this]() {
        m_radioModel->disconnectFromRadio();
    });

    // "Edit radio…" — open ConnectionPanel so the user can edit the currently
    // connected radio's settings (model override, etc.). The panel pre-selects
    // by highlighted MAC when available; if not connected, user clicks the row.
    menu.addAction(tr("Edit radio…"), this, [this]() {
        showConnectionPanel();
        if (m_connectionPanel) {
            const QString mac =
                m_radioModel->connection()
                    ? m_radioModel->connection()->radioInfo().macAddress
                    : QString();
            if (!mac.isEmpty()) {
                m_connectionPanel->highlightMac(mac);
            }
        }
    });

    menu.addAction(tr("Forget radio"), this, [this]() {
        const QString mac =
            m_radioModel->connection()
                ? m_radioModel->connection()->radioInfo().macAddress
                : QString();
        m_radioModel->disconnectFromRadio();
        if (!mac.isEmpty()) {
            AppSettings::instance().forgetRadio(mac);
        }
    });

    menu.exec(globalPos);
}

void MainWindow::showSupportDialog()
{
    if (!m_supportDialog) {
        m_supportDialog = new SupportDialog(m_radioModel, this);
        m_supportDialog->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_supportDialog, &QObject::destroyed, this, [this]() {
            m_supportDialog = nullptr;
        });
    }
    m_supportDialog->show();
    m_supportDialog->raise();
    m_supportDialog->activateWindow();
}

// Phase 3M-4 Task 8: open the modeless PureSignal dialog (Tools >
// PureSignal... and DSP > PureSignal...).  Lazy-constructs on the first
// call; subsequent calls show + raise the existing instance so geometry
// persists across opens.  Source-first port of Thetis console.cs:43099-
// 43104 linearityToolStripMenuItem_Click [v2.10.3.13]:
//
//   if (psform == null) psform = new PSForm(this);
//   psform.Show();
//   psform.Focus();
//
// NereusSDR mirrors via raise()+activateWindow() instead of Focus().
void MainWindow::openPureSignalDialog()
{
    if (!m_psForm) {
        // PureSignal coordinator is owned by RadioModel; pass it directly so
        // the dialog can wire signal/slot bindings even before connect.
        // RadioModel is the owner; we keep a non-owning pointer so the
        // dialog tolerates RadioModel-less startup (covered by tst_psform
        // construction-time test).
        PureSignal* coordinator =
            (m_radioModel ? m_radioModel->pureSignal() : nullptr);
        m_psForm = new PsForm(m_radioModel, coordinator, this);
    }
    m_psForm->show();
    m_psForm->raise();
    m_psForm->activateWindow();
}

// Phase 3J-2 H1: open the modeless SpotHubDialog.
//
// Lazy-constructs on first invocation, wiring all 7 spot-ingest clients
// + SpotModel + DxccColorProvider from RadioModel (Task H2 made these
// accessible via getters). Subsequent calls show + raise the existing
// instance so geometry, table sort state, and per-tab settings persist
// across opens. QPointer guards the pointer in case the dialog is ever
// deleted by some external path; lazy reconstruction is then automatic.
//
// Mirrors the modeless-singleton pattern at AetherSDR
// src/gui/MainWindow.cpp openDxClusterDialog() [@0cd4559].
// ── QRZ account ─────────────────────────────────────────────────────

void MainWindow::ensureQrzClient()
{
    if (m_qrzClient) { return; }
    m_qrzClient = new QrzClient(this);

    // Username lives in settings; the password goes to the platform
    // credential store, never into the settings file.
    const QString user =
        AppSettings::instance().value(QStringLiteral("QrzUsername"),
                                      QString{}).toString();
    if (!user.isEmpty()) {
        m_qrzClient->setCredentials(
            user, CredentialStore::retrieve(QStringLiteral("qrz.password"),
                                            user));
    }
}

void MainWindow::ensureQrzUploader()
{
    if (m_qrzUploader) { return; }
    m_qrzUploader = new QrzLogbookUploader(this);
    // The logbook API key is a different credential from the XML
    // login, so it gets its own keychain entry.
    m_qrzUploader->setApiKey(
        CredentialStore::retrieve(QStringLiteral("qrz.logbookkey"),
                                  QStringLiteral("logbook")));
}

void MainWindow::ensureExtraUploaders()
{
    if (!m_cloudlogUploader) {
        m_cloudlogUploader = new CloudlogUploader(this);
        AppSettings& s = AppSettings::instance();
        m_cloudlogUploader->setBaseUrl(
            s.value(QStringLiteral("CloudlogUrl"), QString{}).toString());
        m_cloudlogUploader->setStationProfileId(
            s.value(QStringLiteral("CloudlogStationId"), QString{}).toString());
        // The key is a credential, so it lives in the keychain. Settings
        // files end up in backups, screenshots and support bundles.
        m_cloudlogUploader->setApiKey(
            CredentialStore::retrieve(QStringLiteral("cloudlog.apikey"),
                                      QStringLiteral("cloudlog")));
    }
    if (!m_localLogUploader) {
        m_localLogUploader = new AdifNetworkUploader(this);
        AppSettings& s = AppSettings::instance();
        const QString host =
            s.value(QStringLiteral("LocalLoggerHost"), QString{}).toString();
        const int port =
            s.value(QStringLiteral("LocalLoggerPort"), 0).toInt();
        const bool tcp =
            s.value(QStringLiteral("LocalLoggerTcp"), false).toBool();
        m_localLogUploader->setTarget(host, static_cast<quint16>(port),
            tcp ? AdifNetworkUploader::Transport::Tcp
                : AdifNetworkUploader::Transport::Udp);
    }
}

QVector<QsoUploader*> MainWindow::qsoUploaders()
{
    ensureQrzUploader();
    ensureExtraUploaders();
    return {m_qrzUploader, m_cloudlogUploader, m_localLogUploader};
}

void MainWindow::openLoggingServicesDialog()
{
    ensureExtraUploaders();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("Logging services"));
    Style::applyDarkPageStyle(&dlg);

    auto* col = new QVBoxLayout(&dlg);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(10);

    AppSettings& s = AppSettings::instance();

    // ── Cloudlog / Wavelog ───────────────────────────────────────────
    auto* clTitle = new QLabel(QStringLiteral("Cloudlog / Wavelog"), &dlg);
    QFont bold = clTitle->font();
    bold.setBold(true);
    clTitle->setFont(bold);
    col->addWidget(clTitle);

    auto* clForm = new QFormLayout;
    auto* urlEdit = new QLineEdit(m_cloudlogUploader->baseUrl(), &dlg);
    urlEdit->setPlaceholderText(
        QStringLiteral("address of your instance, e.g. log.example.org"));
    clForm->addRow(QStringLiteral("Instance URL"), urlEdit);

    auto* keyEdit = new QLineEdit(&dlg);
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setPlaceholderText(QStringLiteral("API key with write access"));
    keyEdit->setText(CredentialStore::retrieve(
        QStringLiteral("cloudlog.apikey"), QStringLiteral("cloudlog")));
    clForm->addRow(QStringLiteral("API key"), keyEdit);

    auto* stationEdit = new QLineEdit(
        m_cloudlogUploader->stationProfileId(), &dlg);
    stationEdit->setPlaceholderText(
        QStringLiteral("number from Station Profiles"));
    clForm->addRow(QStringLiteral("Station profile"), stationEdit);
    col->addLayout(clForm);

    auto* clNote = new QLabel(QStringLiteral(
        "The station profile decides which locator and grid the contact "
        "is filed under. An operator with a home and a portable profile "
        "has two — the wrong number files the QSO in the wrong place."),
        &dlg);
    clNote->setWordWrap(true);
    clNote->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                              .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(clNote);

    // ── Local logger ─────────────────────────────────────────────────
    auto* llTitle = new QLabel(
        QStringLiteral("Local logger (Log4OM, DXKeeper, …)"), &dlg);
    llTitle->setFont(bold);
    col->addWidget(llTitle);

    auto* llForm = new QFormLayout;
    auto* hostEdit = new QLineEdit(m_localLogUploader->host(), &dlg);
    hostEdit->setPlaceholderText(
        QStringLiteral("127.0.0.1 if it runs on this machine"));
    llForm->addRow(QStringLiteral("Host"), hostEdit);

    auto* portEdit = new QLineEdit(
        m_localLogUploader->port() ? QString::number(m_localLogUploader->port())
                                   : QString{}, &dlg);
    portEdit->setPlaceholderText(QStringLiteral("port the logger listens on"));
    llForm->addRow(QStringLiteral("Port"), portEdit);

    auto* transport = new QComboBox(&dlg);
    transport->addItem(QStringLiteral("UDP — send only, no confirmation"));
    transport->addItem(QStringLiteral("TCP — confirms the logger took it"));
    transport->setCurrentIndex(
        m_localLogUploader->transport() == AdifNetworkUploader::Transport::Tcp
            ? 1 : 0);
    llForm->addRow(QStringLiteral("Transport"), transport);
    col->addLayout(llForm);

    auto* llNote = new QLabel(QStringLiteral(
        "Over UDP nothing comes back, so a successful send means the "
        "datagram left — not that the logger filed it. TCP at least "
        "confirms the bytes were accepted."), &dlg);
    llNote->setWordWrap(true);
    llNote->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                              .arg(QString::fromLatin1(Style::kTextSecondary)));
    col->addWidget(llNote);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Save | QDialogButtonBox::Cancel, &dlg);
    col->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    if (dlg.exec() != QDialog::Accepted) { return; }

    m_cloudlogUploader->setBaseUrl(urlEdit->text());
    m_cloudlogUploader->setApiKey(keyEdit->text());
    m_cloudlogUploader->setStationProfileId(stationEdit->text());
    s.setValue(QStringLiteral("CloudlogUrl"), urlEdit->text().trimmed());
    s.setValue(QStringLiteral("CloudlogStationId"),
               stationEdit->text().trimmed());
    // Key to the keychain only — never to the settings XML.
    CredentialStore::store(QStringLiteral("cloudlog.apikey"),
                           QStringLiteral("cloudlog"), keyEdit->text());

    const bool tcp = transport->currentIndex() == 1;
    m_localLogUploader->setTarget(
        hostEdit->text(), static_cast<quint16>(portEdit->text().toUInt()),
        tcp ? AdifNetworkUploader::Transport::Tcp
            : AdifNetworkUploader::Transport::Udp);
    s.setValue(QStringLiteral("LocalLoggerHost"), hostEdit->text().trimmed());
    s.setValue(QStringLiteral("LocalLoggerPort"),
               portEdit->text().trimmed().toInt());
    s.setValue(QStringLiteral("LocalLoggerTcp"), tcp);
}

void MainWindow::openQrzCredentialsDialog()
{
    ensureQrzClient();
    ensureQrzUploader();

    QDialog dlg(this);
    dlg.setWindowTitle(QStringLiteral("QRZ account"));
    Style::applyDarkPageStyle(&dlg);

    auto* col = new QVBoxLayout(&dlg);
    col->setContentsMargins(14, 14, 14, 14);
    col->setSpacing(8);

    auto* form = new QFormLayout;
    auto* userEdit = new QLineEdit(
        AppSettings::instance().value(QStringLiteral("QrzUsername"),
                                      QString{}).toString(), &dlg);
    // NOT a realistic callsign: a grey example in an empty required
    // field reads as already filled in, which cost an afternoon of
    // chasing a login that was never being sent.
    userEdit->setPlaceholderText(QStringLiteral("your QRZ callsign"));
    form->addRow(QStringLiteral("Callsign"), userEdit);

    auto* passEdit = new QLineEdit(&dlg);
    passEdit->setEchoMode(QLineEdit::Password);
    passEdit->setPlaceholderText(QStringLiteral("your QRZ password"));
    passEdit->setText(CredentialStore::retrieve(
        QStringLiteral("qrz.password"), userEdit->text()));
    form->addRow(QStringLiteral("Password"), passEdit);

    // Separate credential, separate service: uploads go to
    // logbook.qrz.com with an API key from the logbook settings page,
    // not the password above. Confusing the two is the usual cause of
    // an unexplained rejection.
    auto* keyEdit = new QLineEdit(&dlg);
    keyEdit->setEchoMode(QLineEdit::Password);
    keyEdit->setPlaceholderText(QStringLiteral("for uploading QSOs"));
    keyEdit->setText(m_qrzUploader->apiKey());
    form->addRow(QStringLiteral("Logbook API key"), keyEdit);
    col->addLayout(form);

    // Automatic lookup while typing. On by default because the locator
    // is what the log wants and nobody remembers to press a button for
    // it — but switchable, because a contest is several hundred
    // callsigns and an XML subscription is metered.
    auto* autoLookup = new QCheckBox(
        QStringLiteral("Look up automatically while typing a callsign"), &dlg);
    autoLookup->setChecked(
        AppSettings::instance()
            .value(QStringLiteral("QrzAutoLookupWhileTyping"), true).toBool());
    autoLookup->setToolTip(QStringLiteral(
        "Fills in the station's locator so the contact is logged with "
        "one. Answers are remembered for the session, so a callsign "
        "worked twice costs one lookup."));
    col->addWidget(autoLookup);

    auto* keyNote = new QLabel(QStringLiteral(
        "The logbook key is separate from the password above — "
        "find it on your QRZ logbook settings page."), &dlg);
    keyNote->setWordWrap(true);
    keyNote->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                               .arg(Style::kTextSecondary));
    col->addWidget(keyNote);

    auto* note = new QLabel(CredentialStore::backendDescription(), &dlg);
    note->setWordWrap(true);
    note->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                            .arg(Style::kTextSecondary));
    col->addWidget(note);

    auto* status = new QLabel(QString{}, &dlg);
    status->setWordWrap(true);
    status->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }")
                              .arg(Style::kTextSecondary));
    col->addWidget(status);

    // Empty required fields get an amber border, not only a sentence:
    // a status line is easy to read past when the field looks filled.
    const QString kNeedsInput = Style::lineEditStyle()
        + QStringLiteral("QLineEdit { border: 1px solid %1; }")
              .arg(Style::kAmberBorder);
    auto markEmpty = [kNeedsInput](QLineEdit* e) {
        e->setStyleSheet(e->text().trimmed().isEmpty()
            ? kNeedsInput : Style::lineEditStyle());
    };
    for (QLineEdit* e : {userEdit, passEdit}) {
        markEmpty(e);
        connect(e, &QLineEdit::textChanged, &dlg, [markEmpty, e]() {
            markEmpty(e);
        });
    }

    auto* row = new QHBoxLayout;
    auto* testBtn = new QPushButton(QStringLiteral("Test"), &dlg);
    row->addWidget(testBtn);
    row->addStretch();
    auto* cancelBtn = new QPushButton(QStringLiteral("Cancel"), &dlg);
    row->addWidget(cancelBtn);
    auto* saveBtn = new QPushButton(QStringLiteral("Save"), &dlg);
    saveBtn->setDefault(true);
    row->addWidget(saveBtn);
    col->addLayout(row);

    // A dialog that says nothing back is indistinguishable from a
    // broken one. Every path ends in a sentence.
    auto* timeout = new QTimer(&dlg);
    timeout->setSingleShot(true);
    timeout->setInterval(20000);
    connect(timeout, &QTimer::timeout, &dlg, [status]() {
        status->setText(QStringLiteral(
            "No answer from QRZ after 20 seconds — check the network"));
    });

    connect(testBtn, &QPushButton::clicked, &dlg, [&, timeout]() {
        const QString user = userEdit->text().trimmed();
        if (user.isEmpty() || passEdit->text().isEmpty()) {
            status->setText(QStringLiteral(
                "Enter your QRZ callsign and password first"));
            return;
        }
        status->setText(QStringLiteral("Testing %1…").arg(user));
        timeout->start();
        m_qrzClient->testLogin(user, passEdit->text());
    });
    connect(m_qrzClient, &QrzClient::loginTestFinished, &dlg,
            [status, timeout](bool ok, const QString& message) {
        timeout->stop();
        if (ok) {
            status->setText(message.isEmpty()
                ? QStringLiteral("Login accepted")
                : QStringLiteral("Login accepted — %1").arg(message));
            return;
        }
        QString hint;
        if (message.contains(QLatin1String("incorrect"), Qt::CaseInsensitive)
            || message.contains(QLatin1String("password"), Qt::CaseInsensitive)) {
            hint = QStringLiteral(
                " — QRZ wants your callsign as the username, not your email");
        }
        status->setText(QStringLiteral("Login failed — %1%2").arg(message, hint));
    });
    connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
    connect(saveBtn, &QPushButton::clicked, &dlg, [&]() {
        const QString user = userEdit->text().trimmed();
        AppSettings::instance().setValue(QStringLiteral("QrzUsername"), user);
        CredentialStore::store(QStringLiteral("qrz.password"), user,
                               passEdit->text());
        m_qrzClient->setCredentials(user, passEdit->text());

        CredentialStore::store(QStringLiteral("qrz.logbookkey"),
                               QStringLiteral("logbook"), keyEdit->text());
        m_qrzUploader->setApiKey(keyEdit->text());

        AppSettings::instance().setValue(
            QStringLiteral("QrzAutoLookupWhileTyping"), autoLookup->isChecked());
        dlg.accept();
    });

    dlg.exec();
}

// ── Rotor + logbook dock ────────────────────────────────────────────
//
// A dock rather than a free window: QDockWidget already gives the area
// show / hide / float / re-dock and a checkable menu action that tracks
// its real visibility, so the panel behaves like the rest of the app
// instead of being a stray window the operator has to keep track of.
// Build the dock if it does not exist yet and hand back the panel.
// Split out of openRotorDial so the Logbook menu entry can reach the
// panel's logbook window without also forcing the dock into view — a
// menu item called Logbook should open a logbook, not rearrange the
// operator's screen.
RotorLogbookPanel* MainWindow::ensureRotorPanel()
{
    if (!m_rotorDock) {
        ensureQrzClient();
        ensureQrzUploader();

        m_rotorDock = new QDockWidget(QStringLiteral("Rotor / Log"), this);
        m_rotorDock->setObjectName(QStringLiteral("rotorLogDock"));
        m_rotorDock->setAllowedAreas(Qt::AllDockWidgetAreas);
        // Kein natives Schweben: Qt's eigenes Los-Ziehen macht daraus ein
        // Betriebssystem-Fenster ohne unsere Titelleiste, ohne Schloss und
        // ohne Anfasser -- genau das Bild, das der Betreiber wiederholt als
        // "altes Format, laesst sich nicht veraendern" gemeldet hat, zuletzt
        // beim Start ueber dem Verbinden-Dialog haengend und nur ueber die
        // Taskleiste erreichbar. Der richtige Weg nach draussen bleibt
        // detachRotorPanel() -> ToolWindow, das dieselbe Titelleiste +
        // Anfasser + Schloss traegt wie jedes andere Fenster im Programm.
        // Review-Fund 2026-08-28, live geprueft und WIEDER verworfen:
        // DockWidgetClosable allein nimmt zwar auch DockWidgetMovable mit
        // (setFeatures() ERSETZT die ganze Menge statt nur ein Bit zu
        // loeschen) -- das sieht nach einem Versehen aus, und
        // DockWidgetClosable | DockWidgetMovable schien der richtige Fix.
        // Am laufenden Programm gezeigt: mit Movable gesetzt erzeugt ein
        // Zug am Titel trotz FEHLENDEM DockWidgetFloatable ein natives,
        // schlossloses Betriebssystem-Fenster -- genau das Bild, das der
        // Betreiber weiter oben als "altes Format, laesst sich nicht
        // veraendern" gemeldet hat. Qt erlaubt das Los-Ziehen auf macOS
        // offenbar schon mit Movable allein, nicht erst mit Floatable.
        // Unbeweglich ist der kleinere Fehler gegenueber dem groesseren:
        // zurueck auf Closable allein, bis ein Fix gefunden ist, der
        // wirklich nur innerhalb des Fensters umsetzt.
        m_rotorDock->setFeatures(QDockWidget::DockWidgetClosable);
        auto* panel = new RotorLogbookPanel(m_radioModel, m_qrzClient,
                                            m_qrzUploader, m_rotorDock);
        // Live logging still goes to QRZ alone; the extra destinations
        // are for sending contacts on afterwards from the logbook
        // window, where the operator can see what is being sent where.
        panel->setUploadTargets(qsoUploaders());
        m_rotorPanel = panel;
        m_rotorDock->setWidget(panel);
        addDockWidget(Qt::RightDockWidgetArea, m_rotorDock);
    }
    // NICHT ueber m_rotorDock->widget(): das Panel kann unter dem
    // Panadapter liegen (setRotorPanelBelow), und dann ist das Dock leer.
    // Frueher stand hier genau das — und der naechste Aufruf haette ein
    // ZWEITES Panel gebaut, mit eigener QRZ-Abfrage und eigenem Logbuch.
    return m_rotorPanel;
}

void MainWindow::raiseRotorPanel()
{
    ensureRotorPanel();
    // m_rotorWindow zuerst: RotorFloating ist der Standardfall, und ein
    // sichtbares m_rotorWindow laesst ein daneben existierendes, aber
    // leeres m_rotorDock ohnehin unbeachtet (derselbe Vorrang wie in der
    // connectionStateChanged-Bindung weiter unten).
    if (m_rotorWindow) {
        m_rotorWindow->show();
        m_rotorWindow->raise();
    } else if (m_rotorDock) {
        m_rotorDock->show();
        m_rotorDock->raise();
    }
}

// ── Rotor/Log als eigenes Fenster ────────────────────────────────────
//
// Derselbe Weg wie bei den Applets: das Panel wandert in ein Fenster
// mit eigener Leiste, Schloss und Anfasser, und kommt auf Wunsch
// zurueck. Es wird NICHT kopiert — ein zweites Logbuch mit eigenem
// Zustand waere genau die Art Fehler, die niemand bemerkt, bis zwei
// Eintraege auseinanderlaufen.
void MainWindow::detachRotorPanel()
{
    RotorLogbookPanel* panel = ensureRotorPanel();
    if (!panel) { return; }
    if (m_rotorWindow) {                 // schon draussen: nach vorn
        m_rotorWindow->show();
        m_rotorWindow->raise();
        return;
    }

    // Wo stand es? Das Fenster geht dort auf — der Klick liest sich
    // dann als „aufheben" statt als „woanders neu oeffnen".
    QRect pickedUpAt;
    if (panel->isVisible()) {
        pickedUpAt = QRect(panel->mapToGlobal(QPoint(0, 0)), panel->size());
    }

    if (auto* col = m_belowPane
                        ? qobject_cast<QVBoxLayout*>(m_belowPane->layout())
                        : nullptr) {
        col->removeWidget(panel);
    }
    if (m_rotorDock && m_rotorDock->widget() == panel) {
        m_rotorDock->setWidget(nullptr);
    }
    if (m_rotorHeader) { m_rotorHeader->hide(); }
    if (m_belowPane)   { m_belowPane->hide(); }
    if (m_rotorDock)   { m_rotorDock->hide(); }
    syncOuterSplitterHandle();

    m_rotorWindow = new ToolWindow(panel, QStringLiteral("RotorLog"),
                                   QStringLiteral("Rotor / Log"), this);
    connect(m_rotorWindow, &ToolWindow::dockRequested, this,
            [this](const QString&) { dockRotorPanel(); });
    m_rotorWindow->show();
    m_rotorWindow->applyDefaultSize(
        pickedUpAt.isValid() ? pickedUpAt.size() : QSize(900, 420));
    if (pickedUpAt.isValid()) { m_rotorWindow->move(pickedUpAt.topLeft()); }
    m_rotorWindow->raise();

    // Gemerkt, damit es beim naechsten Start wieder frei schwebt --
    // der Betreiber, 2026-08-28: soll standardmaessig ein eigenes
    // Fenster mit Titelleiste, Schloss und Anfasser sein, kein Dock.
    AppSettings::instance().setValue(QStringLiteral("RotorFloating"),
                                     QStringLiteral("True"));

    // Betreiber 2026-08-30, in der Nacht gefunden: RotorFloating war nie
    // das einzige "wie steht das Fenster"-Signal. m_appletVis fuehrt
    // fuer "WinRotorLog" eine EIGENE, persistierte Sichtbarkeit
    // (AppletWinRotorLogVisible, plus die Kopie in der aktiven
    // Profil-JSON ueber captureIntoCurrent()'s "visible"-Map) -- und
    // die wurde von detachRotorPanel()/dockRotorPanel() nie
    // nachgefuehrt. Ergebnis: das Fenster stand sichtbar offen, aber
    // der Controller hielt es fuer unsichtbar, und genau DIESEN
    // veralteten Stand schrieb jeder Quit in die Profil-JSON. Ohne
    // diese Zeile bleibt "RotorFloating=True" zwar korrekt, aber die
    // Startreihenfolge in buildUI() liest die Profil-JSON VOR der
    // RotorFloating-Wiederherstellung -- ein zweiter, unabhaengiger Weg
    // zu genau demselben Symptom wie der Cmd+Q-Wettlauf oben in
    // dockRotorPanel(). setVisible() ist ein No-Op, wenn der Wert schon
    // stimmt, also keine zusaetzliche Arbeit im Normalfall.
    if (m_appletVis) {
        m_appletVis->setVisible(QStringLiteral("WinRotorLog"), true);
    }
}

void MainWindow::dockRotorPanel()
{
    // ── Beim Beenden: NICHTS mehr andocken ───────────────────────────
    //
    // Betreiber 2026-08-30, wieder: "Rotor log wieder kein eigenes
    // Fenster. das hatten wir auch schon mehrmals." DER Grund, derselbe
    // wie beim Profil-nicht-gespeichert-Fund vom selben Tag: Cmd+Q
    // schickt auch dem schwebenden Rotor/Log-Fenster ein Schliess-
    // ereignis, und ToolWindow::closeEvent() behandelt "geschlossen"
    // als "andocken" -- genau wie AppletFloatingWindow es tat. Dieser
    // Andock-Weg hier unten schrieb danach RotorFloating=False in die
    // Einstellungen, JEDES Mal beim Beenden, egal wie das Fenster
    // gerade stand. Der Fix fuer AppletFloatingWindow/dockAppletBack()
    // (m_shuttingDown-Sperre) galt nur dort -- diese zweite, aehnlich
    // gebaute Klasse hatte ihn nie bekommen.
    //
    // Betreiber 2026-08-31, per Log geklaert: qApp->quit() (Cmd+Q, ueber
    // die "&Quit"-Handlung) liefert GAR KEIN QCloseEvent an irgendein
    // Fenster -- weder an MainWindow noch an dieses ToolWindow. Es
    // beendet nur die Ereignisschleife (aboutToQuit) und raeumt danach
    // per normaler QObject-Elternschaft ab. dockRotorPanel() laeuft ueber
    // diesen Weg also nie, diese Sperre bleibt trotzdem als Schutz fuer
    // jeden ANDEREN Weg stehen, der tatsaechlich ein QCloseEvent
    // ausloest (z.B. ein spaeter hinzugefuegter nativer Schliessen-Knopf).
    if (m_shuttingDown) { return; }

    if (!m_rotorWindow) { return; }
    // Betreiber 2026-09-01 (Untersuchung nach einem Haenger/OOM-Verdacht
    // beim Profilwechsel): erst verstecken, DANN zerlegen -- dasselbe
    // Muster, das PanadapterStack.cpp fuer sein eigenes Schwebefenster
    // bewusst einhaelt ("erst den GPU-Schutz, DANN das Umhaengen -- sonst
    // bricht der doppelte NSView-Lebenszyklus die NSResponder-Kette
    // (#1344)", PanadapterStack.cpp ~Zeile 395-397). m_rotorWindow ist
    // seit heute (schwebende Fenster bleiben auch ohne Verbindung
    // sichtbar) zum ersten Mal regelmaessig noch SICHTBAR/gemappt, wenn
    // releaseContent()+deleteLater() darauf laufen -- vorher war es an
    // dieser Stelle durch die inzwischen entfernte "hinter die
    // ConnectMaske"-Logik praktisch immer schon unsichtbar, und
    // setParent(nullptr) auf einem ungemappten NSPanel ist ein reiner
    // Buchhaltungsvorgang ohne Fenster-Server-Roundtrip.
    m_rotorWindow->hide();
    QWidget* panel = m_rotorWindow->releaseContent();
    m_rotorWindow->deleteLater();
    m_rotorWindow = nullptr;
    if (!panel) { return; }
    AppSettings::instance().setValue(QStringLiteral("RotorFloating"),
                                     QStringLiteral("False"));
    // Gegenstueck zum Sync in detachRotorPanel() -- siehe dortigen
    // Kommentar. m_shuttingDown ist hier oben schon abgefangen, also
    // laeuft diese Zeile nie beim Beenden; sie haelt m_appletVis nur im
    // normalen Betrieb (Klick auf den Andocken-Pfeil) synchron.
    if (m_appletVis) {
        m_appletVis->setVisible(QStringLiteral("WinRotorLog"), false);
    }
    // Zurueck an den Ort, den die Einstellung nennt. Vorgabe "False"
    // wie beim Programmstart (MainWindow-Konstruktor) -- "unten" ist
    // ein bewusst gewaehlter Zustand, kein Standard. Stand hier bis
    // 2026-08-28 auf "True": ein noch nie gesetztes RotorPanelBelow
    // haette das Andocken auf einen ANDEREN Platz gelegt als das, wo
    // der Programmstart es hingesetzt haette.
    const bool below = AppSettings::instance()
                           .value(QStringLiteral("RotorPanelBelow"),
                                  QStringLiteral("False"))
                           .toString() == QStringLiteral("True");
    setRotorPanelBelow(below);
}

// Betreiber 2026-09-02: "Vertikal ist da noch eine Linie inkl. blauer
// Punkt" ueber der CAT-Anzeige — sichtbar, obwohl m_belowPane leer und
// verborgen war. QSplitter hebt seinen Griff nicht von selbst auf, nur
// weil das benachbarte Kind hide() bekommt; auf macOS zeichnet der
// native Stil obendrein einen kleinen Griff-Punkt in die Mitte des
// Balkens, egal was Style::splitterStyle() an Hintergrundfarbe setzt.
// Der Griff selbst bleibt: er gehoert m_outerSplitter, dem Splitter
// zwischen Panadapter/Applet-Leiste (Index 0) und m_belowPane
// (Index 1) — und m_belowPane ist kein totes Feld, sondern das Ziel
// von setRotorPanelBelow(true) weiter unten. Nur seine SICHTBARKEIT
// soll der von m_belowPane folgen.
void MainWindow::syncOuterSplitterHandle()
{
    if (!m_outerSplitter || !m_belowPane) { return; }
    // isHidden() statt isVisible(): Letzteres haengt auch von der
    // Sichtbarkeit des ganzen Vorfahrenpfads ab und liefert vor dem
    // ersten show() des Hauptfensters immer false — unabhaengig davon,
    // ob m_belowPane gerade selbst show() oder hide() bekommen hat.
    if (auto* handle = m_outerSplitter->handle(1)) {
        handle->setHidden(m_belowPane->isHidden());
    }
}

// ── Rotor/Log unter den Panadapter ───────────────────────────────────
//
// Die untere Flaeche des aeusseren Splitters bekommt Inhalt. Ohne
// Inhalt waere der senkrechte Griff da, aber sinnlos: man zieht einen
// leeren Streifen groesser.
//
// Das Panel wandert dabei aus dem Dock heraus und wieder hinein. Es
// wird nicht kopiert — ein zweites Logbuch mit eigenem Zustand waere
// genau die Art Fehler, die niemand bemerkt, bis zwei Eintraege
// auseinanderlaufen.
void MainWindow::setRotorPanelBelow(bool below)
{
    RotorLogbookPanel* panel = ensureRotorPanel();
    if (!panel || !m_belowPane || !m_rotorDock) { return; }

    auto* col = qobject_cast<QVBoxLayout*>(m_belowPane->layout());
    if (!col) { return; }

    if (below) {
        m_rotorDock->setWidget(nullptr);
        // ── Eine Kopfleiste, wie sie jedes andere Feld hat ───────────
        //
        // Der Betreiber, 2026-08-20: „rotor noch immer in keinem
        // window welches man wie alle andern verschieben und
        // vergroessern kann."
        //
        // Er hatte recht: unter dem Panadapter lag das Panel nackt im
        // Splitter — keine Marke, kein ↗, kein Schloss. Alles andere
        // im Programm traegt inzwischen dieselbe Leiste; dies war das
        // letzte Feld ohne.
        if (!m_rotorHeader) {
            m_rotorHeader = new WindowTitleBar(
                QStringLiteral("Rotor / Log"), m_belowPane);
            // Andocken und Schliessen heissen hier dasselbe: das Panel
            // gehoert unter den Panadapter, das ist sein Zuhause.
            connect(m_rotorHeader, &WindowTitleBar::dockRequested, this,
                    [this]() { detachRotorPanel(); });
            connect(m_rotorHeader, &WindowTitleBar::closeRequested, this,
                    [this]() { setRotorPanelBelow(false); });
            col->addWidget(m_rotorHeader);
        }
        m_rotorHeader->show();
        col->addWidget(panel);
        panel->show();
        m_belowPane->show();
        m_rotorDock->hide();
        syncOuterSplitterHandle();

        // Zwei Drittel Panadapter, ein Drittel darunter — dieselbe
        // Aufteilung wie beim waagerechten Splitter.
        const int h = m_outerSplitter ? m_outerSplitter->height() : 0;
        if (h > 0 && m_outerSplitter) {
            m_outerSplitter->setSizes({h * 2 / 3, h / 3});
        }
    } else {
        col->removeWidget(panel);
        if (m_rotorHeader) { m_rotorHeader->hide(); }
        m_rotorDock->setWidget(panel);
        m_belowPane->hide();
        m_rotorDock->show();
        m_rotorDock->raise();
        syncOuterSplitterHandle();
    }

    AppSettings::instance().setValue(
        QStringLiteral("RotorPanelBelow"),
        below ? QStringLiteral("True") : QStringLiteral("False"));
}

// ── Applet-Leiste neben oder unter den Panadapter ────────────────────
//
// Begruendung und Zielbild stehen beim Menueeintrag (View → Applets
// below panadapter). Hier nur die Mechanik.
//
// Der spectrumPane hat eine Mindestbreite von 400 Bildpunkten, damit er
// waagerecht nicht zur Leiste zerdrueckt wird. Untereinander ist die
// Breite kein Thema mehr, aber die HOEHE: ohne Mindesthoehe laesst sich
// der Panadapter auf null ziehen, und dann ist das Fenster leer und
// niemand weiss, warum.
void MainWindow::setAppletPanelBelow(bool below)
{
    if (!m_mainSplitter) { return; }

    const Qt::Orientation want = below ? Qt::Vertical : Qt::Horizontal;
    if (m_mainSplitter->orientation() == want) { return; }

    m_mainSplitter->setOrientation(want);

    if (QWidget* spectrumPane = m_mainSplitter->widget(0)) {
        if (below) {
            spectrumPane->setMinimumWidth(0);
            spectrumPane->setMinimumHeight(200);
        } else {
            spectrumPane->setMinimumHeight(0);
            spectrumPane->setMinimumWidth(400);
        }
    }

    // Neue Richtung, neue Groessen: die gemerkten Werte stammen aus der
    // anderen Richtung und waeren hier sinnlos (eine Breite als Hoehe
    // gelesen). 70 zu 30 ist die Aufteilung, die auch waagerecht die
    // Vorgabe ist.
    const int total = below ? m_mainSplitter->height() : m_mainSplitter->width();
    if (total > 0) {
        m_mainSplitter->setSizes({total * 7 / 10, total * 3 / 10});
    }

    AppSettings::instance().setValue(
        QStringLiteral("AppletPanelBelow"),
        below ? QStringLiteral("True") : QStringLiteral("False"));
}

void MainWindow::openRotorDial()
{
    raiseRotorPanel();
}

// Ein Eintrag der Auswahl, der ein Fenster meint. Einschalten oeffnet,
// Ausschalten schliesst — mehr Bedeutung hat der Haken nicht.
void MainWindow::applyWindowVisibility(const QString& id, bool on)
{
    auto closeIf = [](QWidget* w) { if (w) { w->hide(); } };

    if (id == QLatin1String("WinLogbook")) {
        // 2026-08-27: "schliesst sich selbst" only held for the operator
        // clicking the window's own close button. A profile switch that
        // turns this off (e.g. activating a deliberately empty new
        // profile while the logbook is open from the previous one) needs
        // an actual hide -- otherwise the window keeps showing on top of
        // a profile that never opened it.
        if (on) {
            openLogbookWindow();
        } else if (m_rotorDock || m_rotorWindow) {
            // Nur zumachen, was schon existiert. ensureRotorPanel() legt
            // das (sichtbare!) Rotor/Log-Dock bei Bedarf neu an -- ohne
            // diese Wache haette ein Profilwechsel, der WinLogbook als
            // ERSTES in der Sitzung abschaltet, das Dock allein durch
            // diesen Aufruf aus dem Nichts erscheinen lassen (Review-Fund
            // 2026-08-28).
            if (RotorLogbookPanel* panel = ensureRotorPanel()) {
                panel->hideLogbook();
            }
        }
        return;
    }
    if (id == QLatin1String("WinRotorLog")) {
        if (on) { detachRotorPanel(); } else { dockRotorPanel(); }
        return;
    }
    if (id == QLatin1String("WinChannelStrip")) {
        if (on) { openChannelStrip(); } else { closeIf(m_stripWindow); }
        return;
    }
    if (id == QLatin1String("WinAntenna")) {
        // Betreiber 2026-09-01: "ALLE fliegenden Fenster gehören hinter
        // die ConnectMaske oder hier gelöscht" -- diese eine Stelle ist
        // der einzige Weg, ueber den JEDER Aufrufer (die einmalige
        // Start-Schleife, ein Profil-Anwenden/-Import, und der staendig
        // laufende effectiveVisibilityChanged-Signalpumpen-Anschluss --
        // DREI unabhaengige Wege, alle drei fuehren hier durch) das
        // Fenster oeffnet. Ohne Funkgeraet zeigt es selbst "no radio";
        // ein Import, dessen Profil "sichtbar" gespeichert hat, riss es
        // trotz der Start-Ausnahme weiter auf, weil jene Ausnahme nur
        // die einmalige Start-Schleife traf, nicht diese gemeinsame
        // Endstelle.
        if (on
            && (!m_radioModel
                || m_radioModel->connectionState()
                       != ConnectionState::Connected)) {
            return;
        }
        if (on) { openAntennaWindow(); } else { closeIf(m_antennaWindow); }
        return;
    }
    // Die QSO-Karte steht bewusst NICHT in der Liste: sie lebt im
    // Logbuchfenster (LogbookWindow.cpp:1213) und hat von hier keinen
    // eigenen Weg. Einen zu erfinden hiesse, ein zweites Kartenfenster
    // mit eigenem Zustand zu bauen — dieselbe Falle wie ein zweites
    // Logbuch.
    if (id == QLatin1String("WinSpotHub")) {
        if (on) { openSpotHub(); } else { closeIf(m_spotHubDialog); }
        return;
    }
}

void MainWindow::openLogbookWindow()
{
    if (RotorLogbookPanel* panel = ensureRotorPanel()) {
        panel->showLogbook();
    }
}

void MainWindow::openAntennaWindow()
{
    // One window, reused and never destroyed on close. The workflow is
    // measure, walk to the antenna, adjust, measure again — a window
    // that forgot the wire length and the target between sweeps would
    // make the operator retype them every round.
    if (!m_antennaWindow) {
        m_antennaWindow = new AntennaWindow(this);
        m_antennaWindow->setAttribute(Qt::WA_DeleteOnClose, false);
        // Betreiber 2026-09-01: "Das Öffnen weiterer Fenster wie zum
        // Beispiel Antenne darf sich nicht hinter anderen Fenstern
        // verstecken, sondern muss ebenfalls im Vordergrund stehen."
        // raise()/activateWindow() unten standen laengst da und
        // wirkten trotzdem nicht: die schwebenden Werkzeugfenster
        // (Panadapter, Rotor/Log, Bandwidth Filter ...) sind Qt::Tool
        // -- auf macOS ein NSPanel auf einer HOEHEREN Fensterebene als
        // ein gewoehnlicher QDialog. Ein raise() hebt nur innerhalb
        // der eigenen Ebene; gegen ein NSPanel dardrueber ist es
        // machtlos. Also dieselbe Ebene und dasselbe Space-Verhalten
        // wie die anderen vier Fensterklassen.
        m_antennaWindow->setWindowFlag(Qt::Tool, true);
        enableFullScreenAuxiliaryBehavior(m_antennaWindow);
        // Betreiber 2026-09-01: "passiert die ganze Zeit!" -- siehe
        // AntennaWindow::closed() fuer die volle Begruendung. Der native
        // rote Knopf schloss das Fenster bisher, ohne den Controller
        // davon zu unterrichten, also stand "sichtbar" fuer immer fest
        // und der Konstruktor riss es bei jedem Start wieder auf.
        connect(m_antennaWindow, &AntennaWindow::closed, this, [this]() {
            m_appletVis->setVisible(QStringLiteral("WinAntenna"), false);
        });
        // 2026-08-13: wire the radio-as-analyzer backend into the
        // "Sweep (Radio)" tab. Without a RadioModel the tab stays
        // inert with its explanatory status line.
        if (m_radioModel && m_antennaWindow->sweepPanel()) {
            SwrSweepPanel::Backend backend;
            backend.controller = m_radioModel->swrSweepController();
            backend.guard      = &m_radioModel->bandPlan();
            backend.txMode     = [rm = m_radioModel]() {
                const SliceModel* s = rm->txBoundSlice();
                return s ? s->dspMode() : DSPMode::USB;
            };
            // ── Which slider TUNE actually reads ──────────────────────
            //
            // This used to be a bare tunePowerForBand(), which is only
            // the right answer when tuneDrivePowerSource() happens to
            // be TuneSlider. The default is DriveSlider — TUNE reads
            // the RF Power slider and Tune Pwr does nothing — so the
            // Antenna window displayed and gated on a number the radio
            // was not using.
            //
            // 2026-08-14: that cost a morning. The operator raised Tune
            // Pwr twice, the panel dutifully agreed, and the sweep went
            // on transmitting at the RF Power slider's 1 W and
            // measuring 0.01 W forward. He diagnosed it himself, from
            // the outside, while I was reading the wrong end of the
            // chain.
            //
            // Mirrors the switch in TransmitModel::setPowerUsingTargetDbm
            // (txMode 1). If a fourth source is ever added there, this
            // has to follow — hence the name of the control travelling
            // with the number, so a mismatch is visible on screen
            // instead of silent.
            backend.tuneDrive =
                [rm = m_radioModel](Band b) -> SwrSweepPanel::TuneDrive {
                const TransmitModel& tx = rm->transmitModel();
                // Same ceiling RadioModel::setTune applies before it
                // keys, so the panel shows what will actually go out
                // rather than what happens to be stored. A label that
                // says 50 while 5 leaves the radio is the exact kind of
                // confident wrong number this window has produced all
                // day.
                const auto cap = [](int w) {
                    return std::min(w, TransmitModel::kMaxTunePowerW);
                };
                switch (tx.tuneDrivePowerSource()) {
                case DrivePowerSource::TuneSlider:
                    return { cap(tx.tunePowerForBand(b)),
                             QStringLiteral("Tune-Pwr-Regler") };
                case DrivePowerSource::Fixed:
                    return { cap(tx.tunePower()),
                             QStringLiteral("festen Wert im Setup") };
                case DrivePowerSource::DriveSlider:
                    break;
                }
                // DriveSlider is the default source, so this is the one
                // most stations actually tune on — it needs the cap
                // just as much as the other two.
                return { cap(tx.power()),
                         QStringLiteral("RF-Power-Regler") };
            };
            backend.rawAdc = [rm = m_radioModel]() {
                return qMakePair(rm->lastFwdAdcRaw(), rm->lastRevAdcRaw());
            };

            // The model the coupler arithmetic actually uses — see
            // SwrSweepPanel::Backend::couplerProfile for why that is not
            // the same thing as the board named in the status bar.
            backend.couplerProfile = [rm = m_radioModel]() {
                return QString::fromLatin1(
                    displayName(rm->hardwareProfile().model));
            };

            // ── Band, both ways ──────────────────────────────────────
            //
            // Asked for directly, and it is a correctness matter rather
            // than a convenience: the panel could otherwise offer to
            // sweep 80 m while the radio sat on 20, and the operator
            // would read a curve for a band the antenna was never
            // switched to.
            //
            // Read from the TX-bound slice, because that is the one the
            // sweep will actually key on — not whichever slice happens
            // to have focus.
            backend.radioBand = [rm = m_radioModel]() -> Band {
                const SliceModel* s = rm->txBoundSlice();
                return s ? bandFromFrequency(s->frequency()) : Band::GEN;
            };
            // Written through the same handler the band buttons use, so
            // a band change from here does everything one from the panel
            // does — filters, antenna relays, per-band power, the lot.
            // RadioModel's handler, not MainWindow's — I read the
            // declaration out of RadioModel.h and reached for it as
            // though it were local. Same handler the band buttons and
            // the band combo already go through (MainWindow.cpp:2249,
            // :3116), so a band change from the antenna window does
            // everything one from the panel does.
            backend.setRadioBand = [rm = m_radioModel](Band b) {
                rm->onBandButtonClicked(b);
            };
            m_antennaWindow->sweepPanel()->setBackend(backend);
        }
    }
    m_antennaWindow->show();
    m_antennaWindow->raise();
    m_antennaWindow->activateWindow();
}

// ── The AetherSDR monitor, wired the AetherSDR way (2026-08-11) ──────
//
// Structure copied 1:1 from upstream MainWindow (@31b29583):
//   - MainWindow owns the ClientPuduMonitor;
//   - the strip surface hosts two buttons and three state setters;
//   - muteRxRequested gates the live RX feed for the whole
//     record→auto-play cycle (NereusSDR's gate is
//     AudioEngine::setRxMutedForMonitor — one call, idempotent);
//   - recordingStopped auto-starts playback.
// NereusSDR-specific: the capture feed. Upstream's engine pushes int16
// stereo 24 kHz from its client chain tail; here the TX worker's
// post-strip tap delivers float mono 48 kHz, so the feed lambda
// converts (average adjacent samples → 24 kHz, duplicate L=R) before
// ClientPuduMonitor::feedTxPostDsp — the ported class stays untouched.
// The tap is (re)wired per recording because the worker is rebuilt on
// every connect.
void MainWindow::wirePuduMonitor()
{
    if (m_finalMonitor || !m_stripWindow) { return; }
    m_finalMonitor = new ClientPuduMonitor(this);

    connect(m_finalMonitor, &ClientPuduMonitor::muteRxRequested,
            this, [this](bool mute) {
        if (m_radioModel && m_radioModel->audioEngine()) {
            m_radioModel->audioEngine()->setRxMutedForMonitor(mute);
        }
    });

    // Strip buttons → monitor, same toggle logic as upstream. The stop
    // side runs through finishPuduTake(), which is where the captured
    // device audio meets the offline strip pass before the monitor
    // finalises — see the capture note below.
    connect(m_stripWindow.data(), &StripWindow::monitorRecordClicked,
            this, [this]() {
        if (m_finalMonitor->isRecording()) {
            finishPuduTake();
        } else {
            if (m_finalMonitor->isPlaying()) m_finalMonitor->stopPlayback();
            m_finalMonitor->startRecording();
        }
    });
    connect(m_stripWindow.data(), &StripWindow::monitorPlayClicked,
            this, [this]() {
        if (m_finalMonitor->isPlaying()) m_finalMonitor->stopPlayback();
        else                             m_finalMonitor->startPlayback();
    });

    // ── Capture: the SOUND CARD paces it, never the radio ────────────
    //
    // The first feed rode the TX worker's post-strip tap, and the tap's
    // own cadence diagnostic condemned it on this bench: 3190-3639
    // pump ticks where 3755 belong — the radio's mic-frame stream loses
    // 3-15% of its blocks over the network, and a sample that never
    // arrived cannot be buffered back into existence. That loss is why
    // every self-listening construction today stuttered, whatever sat
    // downstream.
    //
    // This is also the real reason AetherSDR's monitor sounds right:
    // its entire client chain runs on the PC's audio device clock and
    // never ticks on the radio. Copied properly now: a QAudioSource on
    // the default input captures the take gapless at 48 kHz mono, and
    // when recording stops the channel strip runs OFFLINE over the
    // whole take (a private StripChain configured from the same
    // persisted settings — never the live one, which the worker owns),
    // then the result is decimated to the monitor's 24 kHz stereo diet
    // and fed in one pass. No real-time seam anywhere in the loop.
    connect(m_finalMonitor, &ClientPuduMonitor::recordingStarted,
            this, [this]() {
        if (m_stripWindow) { m_stripWindow->setMonitorRecording(true); }

        m_puduRawTake.clear();
        QAudioFormat f;
        f.setSampleRate(48000);
        f.setChannelCount(1);
        f.setSampleFormat(QAudioFormat::Int16);
        const QAudioDevice dev = QMediaDevices::defaultAudioInput();
        if (dev.isNull()) {
            qCWarning(lcAudio) << "PUDU capture: no audio input device";
            return;
        }
        m_puduCapture = new QAudioSource(dev, f, this);
        m_puduCaptureIo = m_puduCapture->start();
        if (!m_puduCaptureIo) {
            qCWarning(lcAudio) << "PUDU capture failed to start:"
                               << static_cast<int>(m_puduCapture->error());
            m_puduCapture->deleteLater();
            m_puduCapture = nullptr;
            return;
        }
        connect(m_puduCaptureIo, &QIODevice::readyRead, this, [this]() {
            if (!m_puduCaptureIo || !m_finalMonitor) { return; }
            m_puduRawTake += m_puduCaptureIo->readAll();
            // Same 30-second cap as the monitor's own buffer.
            constexpr int kCapBytes = 30 * 48000 * 2;
            if (m_puduRawTake.size() >= kCapBytes
                && m_finalMonitor->isRecording()) {
                finishPuduTake();
            }
        });
    });
    connect(m_finalMonitor, &ClientPuduMonitor::recordingStopped,
            this, [this](int /*durationMs*/) {
        if (m_stripWindow) {
            m_stripWindow->setMonitorRecording(false);
            m_stripWindow->setMonitorHasRecording(
                m_finalMonitor->hasRecording());
        }
        // Auto-start playback — the mute stays installed across the
        // transition because the monitor only emits muteRxRequested
        // (false) at stopPlayback().
        m_finalMonitor->startPlayback();
    });
    connect(m_finalMonitor, &ClientPuduMonitor::playbackStarted,
            this, [this]() {
        if (m_stripWindow) { m_stripWindow->setMonitorPlaying(true); }
    });
    connect(m_finalMonitor, &ClientPuduMonitor::playbackStopped,
            this, [this]() {
        if (m_stripWindow) { m_stripWindow->setMonitorPlaying(false); }
    });

    // Seed the strip with the monitor's current state.
    m_stripWindow->setMonitorRecording(m_finalMonitor->isRecording());
    m_stripWindow->setMonitorPlaying(m_finalMonitor->isPlaying());
    m_stripWindow->setMonitorHasRecording(m_finalMonitor->hasRecording());
}

// Stop the device capture, run the channel strip offline over the whole
// take, feed the monitor, finalise. See the capture note in
// wirePuduMonitor for why nothing here is real-time.
void MainWindow::finishPuduTake()
{
    if (m_puduCapture) {
        m_puduCapture->stop();
        m_puduCapture->deleteLater();
        m_puduCapture = nullptr;
        m_puduCaptureIo = nullptr;   // owned by the source; dies with it
    }
    if (!m_finalMonitor || !m_finalMonitor->isRecording()) { return; }

    const int inFrames = m_puduRawTake.size()
                         / static_cast<int>(sizeof(int16_t));
    if (inFrames > 0) {
        // int16 mono 48 kHz → float for the strip.
        std::vector<float> mono(static_cast<size_t>(inFrames));
        const auto* s16 =
            reinterpret_cast<const int16_t*>(m_puduRawTake.constData());
        for (int i = 0; i < inFrames; ++i) {
            mono[static_cast<size_t>(i)] = s16[i] / 32768.0f;
        }

        // Offline strip pass. A PRIVATE chain, configured from the
        // same persisted settings the live one writes on every edit —
        // the live chain is owned by the worker thread and must not be
        // touched from here. The master switch is deliberately not
        // persisted (loads off), so it is mirrored from the live chain
        // (an atomic read); stage enables come with the settings. When
        // the strip master is off, the take stays raw — which is the
        // honest A of the A/B.
        StripChain* live =
            m_radioModel ? m_radioModel->stripChain() : nullptr;
        if (live && live->isEnabled()) {
            StripChain offline;
            // 2026-08-11 crash fix (first Voice-Check take on this
            // bench, SIGSEGV in ClientReverb::process): prepare()
            // allocates every stage's delay lines and was never
            // called on the offline instance. Stages that are
            // DISABLED bypass before touching buffers — which is why
            // the earlier bench (reverb off) never hit it — but any
            // ENABLED stage with internal state dereferenced empty
            // vectors. Capture format is fixed 48 kHz mono, so the
            // rate is a constant here, matching the QAudioFormat
            // above.
            offline.prepare(48000.0);
            StripSettings::restore(offline);
            offline.setEnabled(true);
            constexpr int kBlock = 64;
            for (int at = 0; at + kBlock <= inFrames; at += kBlock) {
                offline.processMono(mono.data() + at, kBlock);
            }
        }

        // float mono 48 kHz → int16 stereo 24 kHz, the ported
        // monitor's native diet: average adjacent samples (the correct
        // 2:1 decimator for a voice-band signal), L = R.
        const int outFrames = inFrames / 2;
        QByteArray pcm(outFrames * ClientPuduMonitor::kBytesPerFrame,
                       Qt::Uninitialized);
        auto* dst = reinterpret_cast<int16_t*>(pcm.data());
        for (int i = 0; i < outFrames; ++i) {
            const float v = 0.5f * (mono[static_cast<size_t>(2 * i)]
                                    + mono[static_cast<size_t>(2 * i + 1)]);
            const auto q = static_cast<int16_t>(
                std::lround(std::clamp(v, -1.0f, 1.0f) * 32767.0f));
            *dst++ = q;
            *dst++ = q;
        }
        m_finalMonitor->feedTxPostDsp(pcm);
    }
    m_puduRawTake.clear();
    m_finalMonitor->stopRecording();
}

void MainWindow::openVoiceCheck()
{
    if (!m_radioModel) { return; }
    // 2026-08-11: the voice check lives inside the channel strip window
    // now — asked for at the bench, so the change-record-listen loop is
    // one window instead of two. The menu entry stays and simply lands
    // on the right tab.
    openChannelStrip();
    if (m_stripWindow) { m_stripWindow->showVoiceCheckTab(); }
}

void MainWindow::openChannelStrip()
{
    if (!m_radioModel) { return; }
    // Modeless, like the voice check and for the same reason: the loop
    // is turn a knob, listen, turn it back.
    if (!m_stripWindow) {
        m_stripWindow = new StripWindow(m_radioModel, this);
        wirePuduMonitor();
    }
    m_stripWindow->show();
    m_stripWindow->raise();
    m_stripWindow->activateWindow();
}

void MainWindow::openRotorSetup()
{
    if (RotorLogbookPanel* panel = ensureRotorPanel()) {
        panel->showRotorSetup();
    }
}

void MainWindow::openSpotHub()
{
    if (!m_radioModel) { return; }
    if (!m_spotHubDialog) {
        m_spotHubDialog = new SpotHubDialog(
            m_radioModel->dxCluster(),
            m_radioModel->rbn(),
            m_radioModel->wsjtx(),
            m_radioModel->spotCollector(),
            m_radioModel->pota(),
            m_radioModel->sota(),
            m_radioModel->freeDvReporter(),
            m_radioModel->pskReporter(),
            m_radioModel->spotModel(),
            m_radioModel->spotTableModel(),
            m_radioModel->dxccColorProvider(),
            this);
        // Bridge spotsClearedAll (Display tab's "Clear All Spots" button)
        // to SpotModel::clear so the global QShortcut and the dialog
        // button share one truth-source.
        connect(m_spotHubDialog.data(), &SpotHubDialog::spotsClearedAll,
                m_radioModel->spotModel(), &SpotModel::clear);
        // Spot List double-click tuneRequested(double Mhz) drives the
        // active slice. SliceModel::setFrequency takes Hz (double), so
        // multiply by 1e6 to convert MHz to Hz.
        connect(m_spotHubDialog.data(), &SpotHubDialog::tuneRequested,
                this, [this](double freqMhz) {
                    if (auto* slice = m_radioModel->activeSlice()) {
                        slice->setFrequency(freqMhz * 1.0e6);
                    }
                });
        // 2026-08-10: Spot List right-click → aim the rotor. The panel
        // owns the bearing maths and the rotator link; bring the dock
        // into view so the operator sees the needle they just
        // commanded rather than trusting that something happened.
        connect(m_spotHubDialog.data(), &SpotHubDialog::rotorRequested,
                this, [this](const QString& dxCall) {
                    if (RotorLogbookPanel* panel = ensureRotorPanel()) {
                        raiseRotorPanel();
                        panel->workSpot(dxCall);
                    }
                });
        // NereusSDR-native (2026-08-27, operator-requested follow-up):
        // Spot List right-click → "Take Spot: <call>". Same takeSpot()
        // path as the panadapter's spotLogRequested double-click above
        // -- prefills the panel for the operator to review and log
        // themselves, does not turn the rotor or write a log entry.
        connect(m_spotHubDialog.data(), &SpotHubDialog::logSpotRequested,
                this, [this](const QString& dxCall) {
                    if (RotorLogbookPanel* panel = ensureRotorPanel()) {
                        raiseRotorPanel();
                        panel->takeSpot(dxCall);
                    }
                });
        // Phase 3J-2 + 3R M2: Display tab knob round-trip.
        // SpotHubDialog F4 writes every knob change to AppSettings and
        // emits settingsChanged. SpectrumWidget::loadSpotDisplaySettings
        // pulls the new values back out and pushes them into the spot
        // overlay setters in one go. Mirrors AetherSDR's refreshSpots
        // lambda (src/models/RadioModel.cpp [@0cd4559]) but the
        // NereusSDR shape lives on the widget so the test seam is local
        // (see tst_spothub_display_knobs).
        connect(m_spotHubDialog.data(), &SpotHubDialog::settingsChanged,
                this, [this] {
                    if (activeSpectrumWidget()) {
                        activeSpectrumWidget()->loadSpotDisplaySettings();
                    }
                });
        // Defensive re-seed.  The primary seed runs at MainWindow startup
        // (see the QTimer::singleShot(0, ...) lambda earlier in this file
        // that pairs loadSpotDisplaySettings + restoreSpotClientAutoStartState),
        // which guarantees the panadapter mask is populated before any
        // auto-started spot client emits its first spot.  This call is kept
        // as a belt-and-suspenders idempotent re-seed for the (rare) case
        // where the spectrum widget was not yet available at startup but
        // is now -- loadSpotDisplaySettings re-reads AppSettings and the
        // setter is no-op when the value is unchanged, so the cost is
        // bounded and the behaviour is correct either way.
        if (activeSpectrumWidget()) {
            activeSpectrumWidget()->loadSpotDisplaySettings();
        }

        // Phase 3R K-bench (bench feedback): wire the FreeDV tab's
        // Start / Stop button signals to the actual client lifecycle.
        // Previously the button emitted freedvStartRequested /
        // freedvStopRequested but nothing handled the Stop side, so
        // clicking Stop appeared to do nothing.
        if (auto* fdv = m_radioModel->freeDvReporter()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::freedvStartRequested,
                    fdv, &FreeDVReporterClient::startConnection);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::freedvStopRequested,
                    fdv, &FreeDVReporterClient::stopConnection);
        }

        // 2026-05-12 bench fix: wire the remaining 10 SpotHubDialog
        // lifecycle signals to their respective client methods.  Without
        // these connects the per-tab Connect / Start / Stop buttons in
        // SpotHubDialog emit signals into the void — the FreeDV pair
        // above was wired but DX Cluster, RBN, WSJT-X, SpotCollector,
        // and POTA buttons all silently no-op'd.  The auto-start path
        // (RadioModel::restoreSpotClientAutoStartState) calls the same
        // client methods directly and worked; only the manual-button
        // path was broken.  Clients themselves are correct — proven by
        // the spotReceived → spotModel wires at RadioModel.cpp:973-981.
        if (auto* dxc = m_radioModel->dxCluster()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::connectRequested,
                    dxc, &DxClusterClient::connectToCluster);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::disconnectRequested,
                    dxc, &DxClusterClient::disconnect);
        }
        if (auto* rbn = m_radioModel->rbn()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::rbnConnectRequested,
                    rbn, &DxClusterClient::connectToCluster);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::rbnDisconnectRequested,
                    rbn, &DxClusterClient::disconnect);
        }
        if (auto* wsjtx = m_radioModel->wsjtx()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::wsjtxStartRequested,
                    wsjtx, &WsjtxClient::startListening);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::wsjtxStopRequested,
                    wsjtx, &WsjtxClient::stopListening);
        }
        if (auto* sc = m_radioModel->spotCollector()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotCollectorStartRequested,
                    sc, &SpotCollectorClient::startListening);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotCollectorStopRequested,
                    sc, &SpotCollectorClient::stopListening);
        }
        if (auto* pota = m_radioModel->pota()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::potaStartRequested,
                    pota, &PotaClient::startPolling);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::potaStopRequested,
                    pota, &PotaClient::stopPolling);
        }
        if (auto* sota = m_radioModel->sota()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::sotaStartRequested,
                    sota, &SotaClient::startPolling);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::sotaStopRequested,
                    sota, &SotaClient::stopPolling);
        }

        // 2026-05-12 bench fix: PSK Reporter Start button source-first
        // port from freedv-gui.  The dialog emitted pskStartRequested
        // but nothing in MainWindow handled it.
        //
        // From freedv-gui main.cpp:2597 [@77e793a]:
        //   m_pskReporterTimer.Start(5 * 60 * 1000);
        // and main.cpp:1609-1616 [@77e793a]:
        //   if (timerId == ID_TIMER_PSKREPORTER) {
        //       for (auto& obj : wxGetApp().m_reporters) obj->send();
        //   }
        // PSK Reporter is a send-only IPFIX client (pskreporter.h:65-68
        // [@77e793a] — freqChange / transmit / inAnalogMode are no-ops).
        // "Start" = arm the 5-minute auto-send timer.
        //
        // From freedv-gui main.cpp:2694 [@77e793a]:
        //   m_pskReporterTimer.Stop();
        // "Stop" = disarm the timer.  Any queued records flush on
        // ~PskReporterClient when the client tears down (mirrors
        // pskreporter.cpp:171-181 [@77e793a]).
        if (auto* psk = m_radioModel->pskReporter()) {
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::pskStartRequested,
                    psk, [psk](const QString& call,
                               const QString& grid) {
                        // 2026-05-12 bench fix (PR #238 review P2):
                        // apply the freshly-validated identity to the
                        // live client BEFORE arming the timer.  Without
                        // this call, the client keeps the (often empty)
                        // identity set at RadioModel construction time
                        // and emits IPFIX datagrams with empty receiver
                        // fields.  pskreporter.cpp:148-169 [@77e793a].
                        psk->setIdentity(
                            call, grid,
                            QStringLiteral("NereusSDR ") +
                                QStringLiteral(NEREUSSDR_VERSION));
                        psk->setAutoSendIntervalSec(
                            PskReporterClient::kReportingIntervalSec);
                    });
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::pskStopRequested,
                    psk, [psk]() {
                        psk->setAutoSendIntervalSec(0);
                    });
        }

        // 2026-05-12 bench fix: Save & Propagate writes User/GridSquare
        // to AppSettings but the FreeDVStationModel only reads its
        // m_ourGrid once at RadioModel construction.  Without this
        // forward the Reporter dialog's Distance + Hdg columns stay
        // zeroed until app restart.  Push the new grid into the model
        // on every save so distance/heading recompute live.
        connect(m_spotHubDialog.data(), &SpotHubDialog::identitySaved,
                this, [this](const QString& /*call*/,
                             const QString& grid,
                             const QString& /*msg*/) {
                    if (auto* sm = m_radioModel
                            ? m_radioModel->freeDvStationModel()
                            : nullptr) {
                        if (!grid.isEmpty()) {
                            sm->setOurGridSquare(grid);
                        }
                    }
                });

        // 2026-05-12 bench fix (Gap #6 — spot list ↔ panadapter hover sync).
        // Bidirectional: panadapter hover highlights the Spot List row,
        // Spot List hover paints a halo on the matching panadapter
        // label.  Lazy-wired here because both widgets are needed; the
        // dialog is constructed on first open.
        if (activeSpectrumWidget()) {
            connect(activeSpectrumWidget(), &SpectrumWidget::spotHoverIndexChanged,
                    m_spotHubDialog.data(),
                    &SpotHubDialog::setHoveredPanadapterSpot);
            connect(m_spotHubDialog.data(),
                    &SpotHubDialog::spotListHoverChanged,
                    activeSpectrumWidget(),
                    &SpectrumWidget::setHoverSpotIndexExternal);
        }
    }
    m_spotHubDialog->show();
    m_spotHubDialog->raise();
    m_spotHubDialog->activateWindow();
}

// Phase 3J-2 H1: open the modeless FreeDVReporterDialog.
//
// Lazy-constructs on first invocation, wiring FreeDVStationModel +
// FreeDVReporterClient from RadioModel. Same singleton + show / raise
// pattern as openSpotHub.
//
// Wires three downstream connections:
//   qsyRequested -> FreeDVReporterClient::requestQSY (network QSY)
//   messageSendRequested -> FreeDVReporterClient::updateMessage
//   tuneRequested -> active SliceModel::setFrequency (local QSY)
//
// The dialog already calls setAttribute(Qt::WA_DeleteOnClose, false)
// in its own ctor so close + reopen preserves state.
void MainWindow::openFreeDVReporter()
{
    if (!m_radioModel) { return; }
    if (!m_freeDVReporterDialog) {
        m_freeDVReporterDialog = new FreeDVReporterDialog(
            m_radioModel->freeDvStationModel(),
            m_radioModel->freeDvReporter(),
            this);
        // QSY: dialog -> reporter client -> network broadcast.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::qsyRequested,
                m_radioModel->freeDvReporter(),
                &FreeDVReporterClient::requestQSY);
        // Message update: dialog -> reporter client.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::messageSendRequested,
                m_radioModel->freeDvReporter(),
                &FreeDVReporterClient::updateMessage);
        // Local QSY: dialog -> active slice tune. tuneRequested signature
        // is quint64 Hz so no MHz conversion needed.
        connect(m_freeDVReporterDialog.data(),
                &FreeDVReporterDialog::tuneRequested,
                this, [this](quint64 freqHz) {
                    if (auto* slice = m_radioModel->activeSlice()) {
                        slice->setFrequency(static_cast<double>(freqHz));
                    }
                });

        // Phase 3R K-bench (bench feedback): wire active slice VFO ->
        // dialog so the Band/Exact-freq filter actually tracks. Push
        // current value immediately + on every frequencyChanged.
        if (auto* slice = m_radioModel->activeSlice()) {
            m_freeDVReporterDialog->setActiveFrequency(
                static_cast<quint64>(slice->frequency()));
            connect(slice, &SliceModel::frequencyChanged,
                    m_freeDVReporterDialog.data(),
                    [this](double hz) {
                        if (m_freeDVReporterDialog) {
                            m_freeDVReporterDialog->setActiveFrequency(
                                static_cast<quint64>(hz));
                        }
                    });
        }
    }
    m_freeDVReporterDialog->show();
    m_freeDVReporterDialog->raise();
    m_freeDVReporterDialog->activateWindow();
}

// panIdsForLayout(): synthesize a "pan-0" .. "pan-(N-1)" id list sized to
// a layout template's pan count. Two callers: applyPanLayout() below,
// reached from PanLayoutDialog's thumbnail-grid accept path (Task B3/B4),
// and the launch-time restoredLayout call near the top of this file.
//
// This used to also carry a 20-line doc block for showPanMenu(), the old
// +PAN dropdown that built the same table inline across three sections
// (add-slice on the active pan / layout / float the active pan).
// showPanMenu() was replaced by PanLayoutDialog's thumbnail grid
// (Task B3), and its two per-pan actions moved to each pan's own
// right-click menu so they carry that pan's own id instead of routing
// through activePanId() (Task B5, design doc §8.5); neither reads this
// function's five-template comment any more, and the table itself has
// grown to the current nine layouts (design doc §8.3).
// Codex review round 3, PR #293. The template-to-pan-count table had three
// copies; this is the only one now.
QStringList MainWindow::panIdsForLayout(const QString& layoutId)
{
    // One table, so a new layout is a one-line addition here and a branch in
    // PanadapterStack::applyLayout, rather than a chain of ternaries that
    // silently defaults new ids to 2. Counts match design §8.3.
    static const QHash<QString, int> kPanCount = {
        {QStringLiteral("1"),   1},
        {QStringLiteral("2v"),  2},
        {QStringLiteral("2h"),  2},
        {QStringLiteral("2h1"), 3},
        {QStringLiteral("12h"), 3},
        {QStringLiteral("3v"),  3},
        {QStringLiteral("2x2"), 4},
        {QStringLiteral("4v"),  4},
        {QStringLiteral("3h2"), 5},
    };
    const int needed = kPanCount.value(layoutId, 1);
    QStringList ids;
    ids.reserve(needed);
    for (int i = 0; i < needed; ++i) {
        ids << QStringLiteral("pan-%1").arg(i);
    }
    return ids;
}

// Codex review round 3, PR #293. See MainWindow.h for why this exists.
void MainWindow::applyPanLayout(const QString& layoutId)
{
    if (!m_panStack) { return; }

    const QStringList ids = panIdsForLayout(layoutId);

    qCInfo(lcContainer) << "Pan layout: applying" << layoutId << "with ids" << ids;
    m_panStack->applyLayout(layoutId, ids);

    if (!m_radioModel) { return; }

    // Shrink first. Slices left on panes applyLayout just deleted would keep a
    // dangling panKey, lose their VFO widget, and hold a DDC, a stream and
    // audio the operator can no longer reach. Running this before the grow
    // step also means the occupancy question below sees the settled answer.
    const int rehomed = m_radioModel->rehomeSlicesToPans(ids);
    if (rehomed > 0) {
        qCInfo(lcContainer) << "Layout: rehomed" << rehomed
                            << "slice(s) onto" << ids.value(0);
    }

    // Grow. Every pan wants a slice so that it has a VfoWidget and an RX
    // applet entry (Phase 3F bench fix 2026-06-03).
    //
    // Asked as an occupancy question rather than as
    // `for (i = slices().size(); i < target; ++i)`. That form assumed the
    // slice COUNT is the first unoccupied pan index, which stops being true
    // the moment slices co-host or get rehomed: shrinking a 2x2 to one pane
    // puts all four slices on pan-0, and expanding back then saw
    // existing == target, added nothing, and left three panes empty.
    // (Codex review round 4, PR #293.)
    //
    // No maxSlices arithmetic here: addSliceOnPan enforces the cap itself and
    // emits sliceAddRejected with an operator-facing reason when it cannot,
    // so restating it would be a second copy of that policy.
    //
    // Spread before creating. After a shrink every slice is co-hosted on
    // pan-0, so the slices these empty pans need already exist. Creating new
    // ones instead spends the maxSlices budget filling one pan and leaves the
    // rest empty with a surplus slice in the model. (Codex review round 5.)
    const int spread = m_radioModel->spreadSlicesOntoEmptyPans(ids);
    if (spread > 0) {
        qCInfo(lcContainer) << "Layout: spread" << spread
                            << "co-hosted slice(s) onto empty pans";
    }

    // Whatever is still empty after the surplus has been used up genuinely
    // needs a new slice.
    populateEmptyPans();
}

// ---------------------------------------------------------------------------
// populateEmptyPans — every pan needs a slice to be worth anything
//
// A pan with no slice has no VfoWidget, no RX applet entry, and no stream
// feeding it: it renders as an empty box with a 0.0000 flag.
//
// Called from two places, and the second one is why this is a function.
// applyPanLayout calls it because a layout change can add panes. The connect
// handler calls it because the startup layout restore deliberately does NOT
// (MainWindow.cpp, the PanLayoutId block): at startup no radio is connected
// and the stream pool is unsized, so manufacturing slices there would bind
// nothing. Correct as far as it goes, but nothing finished the job once a
// radio did connect.
//
// Bench-caught 2026-08-01 (J.J. Boyd, KG4VCF): quit with a 2v layout, relaunch,
// connect, and the second pan is permanently dead until the operator notices
// they have to add Slice B by hand. The log gives it away by omission, with no
// "Pan layout: applying" line anywhere in the session.
//
// addSliceOnPan enforces the maxSlices cap itself and emits sliceAddRejected
// with an operator-facing reason, so there is no cap arithmetic here.
// ---------------------------------------------------------------------------
void MainWindow::populateEmptyPans()
{
    if (!m_radioModel || !m_panStack) { return; }

    // The pans that actually exist, not panIdsForLayout's template. After a
    // restore those are the same, but reading the live stack means a pan
    // created by any other route is covered too.
    QStringList ids;
    for (const PanadapterApplet* applet : m_panStack->allApplets()) {
        if (applet) { ids << applet->panId(); }
    }

    for (const QString& emptyPan : m_radioModel->pansWithoutSlices(ids)) {
        m_radioModel->addSliceOnPan(emptyPan);
    }
}

// Task B4: replaces the showPanMenu() context menu. Its layout section
// listed ids as bare strings and is superseded by PanLayoutDialog's
// thumbnail grid (Task B3); its two per-pan actions (add slice on active
// pan, float active pan) move to each pan's own right-click menu in
// Task B5, since both routed through activePanId() and a control drawn on
// a pan should target that pan, not "whichever one is active."
void MainWindow::showPanLayoutDialog()
{
    if (!m_radioModel || !m_radioModel->isConnected()) {
        return;
    }
    // Gate on the DDC-derived ceiling, not raw maxSlices: opening a NEW
    // pan always claims its own DDC (SliceStreamAllocator::placeSlice,
    // preferOwnStream=true; see the ruling comment at
    // SliceStreamAllocator.cpp:81-86), so a board like HL2 (maxSlices=5,
    // userDdcCount=2) can never fill more than 2 independent pans even
    // though it can host 5 slices total. Gating on maxSlices alone showed
    // tiles the board could paint but never fill (final-fix-wave finding 2).
    const auto& caps = m_radioModel->boardCapabilities();
    const int maxPanCount = qMin(caps.maxSlices, caps.userDdcCount);
    const QString boardName = m_radioModel->name();
    PanLayoutDialog dlg(maxPanCount,
                        m_panStack ? m_panStack->currentLayoutId()
                                   : QStringLiteral("1"),
                        boardName, this);
    if (dlg.exec() == QDialog::Accepted && !dlg.selectedLayout().isEmpty()) {
        applyPanLayout(dlg.selectedLayout());
    }
}

// Phase 3M-4 bench-fix: PSA bottom-banner indicator visibility
// gated on (caps.hasPureSignal && PureSignal::isAutoCalEnabled).
// Centralised so onConnectionStateChanged + autoCalEnabledChanged +
// pureSignalCoordinatorReady can all share one truth-source.
//
// m_psaIndicator is registered with m_chromeBar at rung 0 so its width
// (two QLabel minimumWidth pins, ~154 px) is counted in the fold budget
// on every PS-capable, PS-armed board (Task A8 fix round 1 finding 2).
// The armed fact itself is reported via setItemAvailable, not a direct
// setVisible call, per ChromeBarController::setItemAvailable's own doc
// comment.
void MainWindow::updatePsaIndicatorVisibility()
{
    if (!m_psaIndicator) { return; }
    const bool caps =
        m_radioModel
        && m_radioModel->isConnected()
        && m_radioModel->boardCapabilities().hasPureSignal;
    auto* ps = m_radioModel ? m_radioModel->pureSignal() : nullptr;
    const bool armed = ps && ps->isAutoCalEnabled();
    if (m_chromeBar && m_chromeBarWidget) {
        m_chromeBar->setItemAvailable(m_psaIndicator, caps && armed);
        m_chromeBar->relayout(m_chromeBarWidget->width());
    }
}

void MainWindow::showAudioDiagnoseDialog()
{
#if defined(Q_OS_LINUX)
    AudioEngine* eng = m_radioModel->audioEngine();
    if (!eng) {
        return;
    }
    auto* dlg = new VaxLinuxFirstRunDialog(eng, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->exec();
#endif
}

void MainWindow::onConnectionStateChanged()
{
    // Phase 3Q-8: forward state to the spectrum widgets for the disconnect
    // overlay.
    //
    // EVERY pan, not just the active one. SpectrumWidget::mousePressEvent
    // opens with a disconnected guard that emits disconnectedClickRequest()
    // and returns, so a widget left at its Disconnected default swallows every
    // press: no click-to-tune, no filter-edge drag, no pan drag. Only
    // mouseMoveEvent is ungated, which is why a second pan still tracked the
    // cursor readout and looked alive while being completely unclickable --
    // and why tuning appeared to work only with the pointer over the flag,
    // which is a separate widget with its own handlers.
    pushConnectionStateToPans();

    // Task B4: +PAN dims (and its tooltip explains why) on every
    // connection-state transition, connected or not.
    updateAddPanButtonState();

    if (m_radioModel->isConnected()) {
        // Betreiber 2026-09-01: "diese fliegenden Fenster sind zu
        // löschen auf der Connect Seite" -- Meter-/Applet-Fenster, die
        // der else-Zweig unten bei einer Trennung MITTEN in der
        // Sitzung versteckt hat, kommen bei JEDEM erneuten Verbinden
        // zurueck, nicht nur beim allerersten der Sitzung (das
        // behandelt bereits der eigene, einmalige Rotor/Log-Haken
        // weiter unten im Konstruktor -- diese Schleife hier ist
        // dieselbe Wiederherstellung, nur nicht auf "einmal" begrenzt).
        for (const QPointer<QWidget>& w
             : std::as_const(m_floatingContainersHiddenPreConnect)) {
            if (w) { w->show(); w->raise(); }
        }
        m_floatingContainersHiddenPreConnect.clear();

        // Review-Fund 2026-09-01 (adversarial bestaetigt): ein Profil
        // mit sichtbarem Antennen-Fenster, angewendet OHNE Verbindung,
        // lief in die WinAntenna-Sperre in applyWindowVisibility() --
        // das Fenster wurde dort nie ANGELEGT, stand also auch nie in
        // der Liste oben und kaeme sonst nach dem Verbinden nie
        // wieder; der gespeicherte Wunsch waere still verloren, obwohl
        // der Auswaehler-Haken weiter "sichtbar" sagte. Jetzt, MIT
        // Verbindung, laesst die Sperre den Weg frei.
        if (m_appletVis
            && m_appletVis->isEffectivelyVisible(
                   QStringLiteral("WinAntenna"))
            && (!m_antennaWindow || !m_antennaWindow->isVisible())) {
            applyWindowVisibility(QStringLiteral("WinAntenna"), true);
        }

        // Neuer Ausfall darf wieder einmal gemeldet werden.
        m_connectionPanelAutoOpenedThisEpisode = false;
        m_connectFailedToastShownThisEpisode = false;

        // Board code ("Saturn") not marketing name ("ANAN-G2 (Saturn)") —
        // the marketing name truncates at status-bar widths. boardCodeName()
        // returns the HPSDRHW enum label which is short and unambiguous.
        // Design §4.1: rendered on StationBlock's second row (Task A4)
        // instead of the retired left-section model+firmware pair.
        {
            const HPSDRHW board = m_radioModel->connection()->radioInfo().boardType;
            const QString code  = QString::fromLatin1(boardCodeName(board));
            m_stationBlock->setHardwareLine(
                code, QStringLiteral("v%1").arg(m_radioModel->version()));
            // The second row's text (and hence StationBlock's sizeHint)
            // just changed (Task A8 fix round 1 finding 4).
            if (m_chromeBar && m_chromeBarWidget) {
                m_chromeBar->setNaturalWidth(
                    m_stationBlock, m_stationBlock->sizeHint().width());
                m_chromeBar->relayout(m_chromeBarWidget->width());
            }
        }

        // Phase 3Q-6/D.1: setRadio() removed — radio identity moves to the
        // STATION block (sub-PR-7). Segment state is already driven by
        // connectionStateChanged → ConnectionSegment::setState (see D.2 wiring
        // block in the constructor).

        // RxDashboard follows the ACTIVE slice (Task A5's rebindDashboard
        // lambda, wired to RadioModel::sliceAdded / activeSliceChanged
        // below), not a fixed slice(0), so no per-connect rebind is needed
        // here specifically -- the rebind already happens on sliceAdded.
        // (Connection details moved to segment tooltip / NetworkDiagnosticsDialog.)

        // Wire step attenuator controller to the live radio connection
        // and set max attenuation from board capabilities.
        // From Thetis console.cs ucInfoBar Warning() + SetupForm attenuator init.
        m_stepAttController->setRadioConnection(m_radioModel->connection());
        const auto& caps = BoardCapsTable::forBoard(
            m_radioModel->connection()->radioInfo().boardType);
        m_stepAttController->setMaxAttenuation(caps.attenuator.maxDb);
        // Wire HPSDR-board flag — Atlas/Metis kit uses preamp save/restore on
        // MOX rather than per-band TX ATT (Thetis console.cs:29548 [v2.10.3.13]:
        //   if (HardwareSpecific.Model == HPSDRModel.HPSDR) { ... }).
        m_stepAttController->setIsHpsdrBoard(
            m_radioModel->connection()->radioInfo().boardType == HPSDRHW::Atlas);

        // Phase 3M-4 Task 10 + bench-fix: PSA bottom-banner indicator is
        // gated on caps.hasPureSignal AND pureSignal->isAutoCalEnabled().
        // Boards without PS support (HL2 / Atlas) hide the FB+PS pair
        // entirely; PS-capable boards (Hermes II / Angelia / Orion /
        // Saturn / G2) show it only when the user has armed PS-A.
        // updatePsaIndicatorVisibility centralises the condition.
        updatePsaIndicatorVisibility();
        // Phase 3M-4 Task 13: gate PureSignalApplet + TxApplet [PS-A] on
        // the same board capability.  PureSignalApplet hides itself; the
        // TxApplet [PS-A] button hides via its setBoardCapabilities slot.
        if (m_pureSignalApplet) {
            m_pureSignalApplet->setVisible(caps.hasPureSignal);
        }
        if (m_txApplet) {
            m_txApplet->setBoardCapabilities(caps);
        }
        // P1 full-parity §4.1: gate AutoAttMode::Adaptive on per-step
        // calibration support.  Must be set BEFORE loadSettings() so a
        // persisted "Adaptive" string is clamped to Classic when the
        // connected board lacks the feature.
        m_stepAttController->setHasStepAttenuatorCal(caps.hasStepAttenuatorCal);
        m_stepAttController->loadSettings(m_radioModel->connection()->radioInfo().macAddress);

        // Phase 3Q Task 5 — auto-close: 1 s after connect, accept() the panel if open.
        // Fires on transitions TO Connected only (not on repeated Connected emits).
        if (m_connectionPanel && m_connectionPanel->isVisible()) {
            QTimer::singleShot(1000, this, [this]() {
                if (m_connectionPanel && m_connectionPanel->isVisible()
                    && m_radioModel->isConnected()) {
                    m_connectionPanel->accept();
                }
            });
        }

        // Per-radio peripherals refactor (2026-05-26): the PGXL / TGXL
        // auto-connect-on-Connected block previously lived here in
        // MainWindow but read GLOBAL AppSettings keys.  The lifecycle
        // (gated on the per-MAC FourO3A flag) now lives in
        // RadioModel::applyPeripheralsForCurrentMac(), which is driven
        // from onConnectionStateChanged so MainWindow doesn't need to
        // touch the peripheral wires here.

        // Phase 3P-II Task 20: wire AmpApplet controls to PgxlConnection.
        // operateToggled: translate bool to "operate"/"standby" command string.
        // statusUpdated: fan the k=v map into AmpApplet setter slots.
        // These connects are made on every radio-connect. Qt lambda
        // connects do not support UniqueConnection, so guard with a flag
        // to avoid stacking connections across reconnects. The flag is
        // instance-local; resetFlag is intentional (first time = wire).
        if (m_ampApplet && !m_ampAppletWired) {
            m_ampAppletWired = true;

            connect(m_ampApplet, &AmpApplet::operateToggled,
                    this, [this](bool wantOperate) {
                // Bench-fix 2026-05-19: pcap stream 11 (.19 PowerGeniusDesktop
                // -> .235 PGXL :9008) shows the actually-used wire command
                // for OPERATE is `operate=1` (key=value), not bare `operate`.
                // PGXL rejected `operate` / `standby` with error 50000016
                // every click.
                m_radioModel->pgxlConnection()->sendCommand(
                    wantOperate ? QStringLiteral("operate=1")
                                : QStringLiteral("operate=0"));
            });

            connect(m_radioModel->pgxlConnection(),
                    &PgxlConnection::statusUpdated,
                    this, [this](const QMap<QString, QString>& kvs) {
                if (kvs.contains(QStringLiteral("temp")))
                    m_ampApplet->setTemp(kvs.value(QStringLiteral("temp")).toFloat());
                if (kvs.contains(QStringLiteral("id")))
                    m_ampApplet->setDrainCurrent(kvs.value(QStringLiteral("id")).toFloat());
                if (kvs.contains(QStringLiteral("vac")))
                    m_ampApplet->setMainsVoltage(kvs.value(QStringLiteral("vac")).toInt());
                if (kvs.contains(QStringLiteral("state")))
                    m_ampApplet->setState(kvs.value(QStringLiteral("state")));
                if (kvs.contains(QStringLiteral("meffa")))
                    m_ampApplet->setMeff(kvs.value(QStringLiteral("meffa")));

                // 2026-05-22 bench fix: PGXL's `peakfwd` and `swr`
                // status fields are HOLD values that latch the last TX
                // peak and DO NOT decay back to 0 when the amp leaves
                // TRANSMIT_A/B (PGXL's intent is "show the last QSO's
                // peak on the front panel"). For our applet gauges we
                // want the live keyed value during TX and a clean zero
                // between cycles, so we gate the peakfwd / swr writes
                // on the transmitting state. Without this gate the
                // previous bench-fix at this site (which forced fwd=0
                // / swr=1 when state changed to IDLE) was overwritten
                // 30 ms later by the next status response carrying the
                // stale latched peakfwd.
                //
                // Inferred transmitting state: if the status update
                // includes state=, use it; otherwise fall back to the
                // last cached state (m_ampApplet tracks it via
                // setState).
                bool transmitting = false;
                if (kvs.contains(QStringLiteral("state"))) {
                    const QString st = kvs.value(QStringLiteral("state"));
                    transmitting =
                        (st == QStringLiteral("TRANSMIT_A")
                         || st == QStringLiteral("TRANSMIT_B"));
                    if (!transmitting) {
                        m_ampApplet->setFwdPower(0.0f);
                        m_ampApplet->setSwr(1.0f);
                    }
                } else {
                    transmitting = m_ampApplet->isTransmitting();
                }

                // 2026-05-20 bench fix: peakfwd is dBm (not watts) and swr
                // is signed dB return loss (not an SWR ratio). Convert
                // here so the AmpApplet gauges read the same numbers
                // die TxApplet already gets via
                // RadioModel::ampMetersChanged.
                // 2026-05-22 bench fix: only forward the converted
                // peakfwd / swr when the amp is actually transmitting;
                // otherwise the stale latched peak would overwrite the
                // zero set by the state-edge block above.
                if (transmitting && kvs.contains(QStringLiteral("peakfwd"))) {
                    const float dbm   = kvs.value(QStringLiteral("peakfwd")).toFloat();
                    const float watts = std::pow(10.0f, dbm / 10.0f) / 1000.0f;
                    m_ampApplet->setFwdPower(watts);
                }
                if (transmitting && kvs.contains(QStringLiteral("swr"))) {
                    const float rlDbWire =
                        kvs.value(QStringLiteral("swr")).toFloat();
                    float ratio;
                    if (rlDbWire >= 0.0f) {
                        ratio = 99.0f;  // RL>=0 -> open/short, cap display
                    } else {
                        const float gamma = std::pow(10.0f, rlDbWire / 20.0f);
                        ratio = (gamma >= 0.999f)
                            ? 99.0f
                            : (1.0f + gamma) / (1.0f - gamma);
                    }
                    m_ampApplet->setSwr(ratio);
                }
            });

            // Phase 3P-II Phase 4 Task 88: track PGXL connected state for the
            // context menu Disconnect/Reconnect label.
            connect(m_radioModel->pgxlConnection(), &PgxlConnection::connected,
                    this, [this]() { m_ampApplet->setPgxlConnected(true); });
            connect(m_radioModel->pgxlConnection(), &PgxlConnection::disconnected,
                    this, [this]() { m_ampApplet->setPgxlConnected(false); });

            // Phase 3P-II Phase 4 Task 88: context menu right-click signals.

            // connectionToggleRequested: disconnect or reconnect PGXL.
            connect(m_ampApplet, &AmpApplet::connectionToggleRequested,
                    this, [this]() {
                PgxlConnection* pgxl = m_radioModel->pgxlConnection();
                if (!pgxl) { return; }
                if (pgxl->isConnected()) {
                    pgxl->disconnect();
                } else {
                    // PR #279 review #5 (2026-05-23): use the same
                    // PGXL_ManualIp / PGXL_ManualPort keys that
                    // auto-connect at line ~6331 + the Setup ->
                    // CAT & Network -> PGXL page persist.  The
                    // earlier reads of obsolete PGXL_IpAddress /
                    // PGXL_Port (default 50001) returned empty
                    // strings on every install that had only ever
                    // written the canonical keys, so the AmpApplet
                    // context-menu Connect did nothing.
                    //
                    // Per-radio peripherals refactor (2026-05-26):
                    // the keys are scoped under
                    // hardware/<mac>/peripherals/.  Default 9008
                    // matches the AppSettings.h documented default.
                    const QString ip = m_radioModel->peripheralValue(
                        QStringLiteral("PGXL_ManualIp"));
                    const quint16 port = static_cast<quint16>(
                        m_radioModel->peripheralValue(
                            QStringLiteral("PGXL_ManualPort"),
                            QStringLiteral("9008")).toUInt());
                    if (!ip.isEmpty()) {
                        pgxl->connectToPgxl(ip, port);
                    }
                }
            });

            // diagnosticsCopyRequested: build a brief diagnostic string and copy to clipboard.
            connect(m_ampApplet, &AmpApplet::diagnosticsCopyRequested,
                    this, [this]() {
                PgxlConnection* pgxl = m_radioModel->pgxlConnection();
                const QString text = QStringLiteral(
                    "PGXL Diagnostics\n"
                    "Connected: %1\n"
                    "IP: %2\n"
                ).arg(pgxl && pgxl->isConnected() ? QStringLiteral("Yes") : QStringLiteral("No"))
                 .arg(pgxl ? pgxl->peerAddress() : QStringLiteral("--"));
                QGuiApplication::clipboard()->setText(text);
            });

            // Phase 3P-II Phase 4 Task 90: wire navigationRequested to openSetup().
            connect(m_ampApplet, &AmpApplet::navigationRequested,
                    this, &MainWindow::openSetup);
        }

        // Phase 3P-II Phase 4 Task 89: wire TunerApplet context menu signals to
        // TgxlConnection. buildUI() runs once at startup, so no deduplication
        // guard is needed. Qt::UniqueConnection is intentionally NOT used
        // here: it requires a pointer-to-member-function of a QObject
        // subclass, and on a lambda Qt6 warns and returns an invalid
        // Connection — all four connects below would have been dead on
        // arrival, not merely un-deduplicated.
        if (m_tunerApplet) {
            // Track TGXL connected state for Disconnect/Reconnect label.
            connect(m_radioModel->tgxlConnection(), &TgxlConnection::connected,
                    this, [this]() { m_tunerApplet->setTgxlConnected(true); });
            connect(m_radioModel->tgxlConnection(), &TgxlConnection::disconnected,
                    this, [this]() { m_tunerApplet->setTgxlConnected(false); });

            // connectionToggleRequested: disconnect or reconnect TGXL.
            connect(m_tunerApplet, &TunerApplet::connectionToggleRequested,
                    this, [this]() {
                TgxlConnection* tgxl = m_radioModel->tgxlConnection();
                if (!tgxl) { return; }
                if (tgxl->isConnected()) {
                    tgxl->disconnect();
                } else {
                    // Per-radio peripherals refactor (2026-05-26): keys
                    // scoped under hardware/<mac>/peripherals/.
                    const QString ip = m_radioModel->peripheralValue(
                        QStringLiteral("TGXL_ManualIp"));
                    const quint16 port = static_cast<quint16>(
                        m_radioModel->peripheralValue(
                            QStringLiteral("TGXL_ManualPort"),
                            QStringLiteral("9010")).toUInt());
                    if (!ip.isEmpty()) {
                        tgxl->connectToTgxl(ip, port);
                    }
                }
            });

            // diagnosticsCopyRequested: build diagnostic string and copy to clipboard.
            connect(m_tunerApplet, &TunerApplet::diagnosticsCopyRequested,
                    this, [this]() {
                TgxlConnection* tgxl = m_radioModel->tgxlConnection();
                const QString text = QStringLiteral(
                    "TGXL Diagnostics\n"
                    "Connected: %1\n"
                    "IP: %2\n"
                ).arg(tgxl && tgxl->isConnected() ? QStringLiteral("Yes") : QStringLiteral("No"))
                 .arg(tgxl ? tgxl->peerAddress() : QStringLiteral("--"));
                QGuiApplication::clipboard()->setText(text);
            });

            // Phase 3P-II Phase 4 Task 90: wire navigationRequested to openSetup().
            connect(m_tunerApplet, &TunerApplet::navigationRequested,
                    this, &MainWindow::openSetup,
                    Qt::UniqueConnection);
        }
    } else {
        // No explicit hardware-line reset needed here: StationBlock clears
        // its own second row automatically whenever setRadioName(QString())
        // runs (Task A4), which the connectionStateChanged handler wired in
        // buildStatusBar() already does on every non-Connected transition.

        // Save step attenuator settings before disconnecting.
        if (m_radioModel->connection()) {
            m_stepAttController->saveSettings(m_radioModel->connection()->radioInfo().macAddress);
        }

        // Disconnect step attenuator from radio
        m_stepAttController->setRadioConnection(nullptr);

        // Phase 3M-4 Task 10 + bench-fix: hide the PSA indicator on
        // disconnect.  Re-evaluated via updatePsaIndicatorVisibility on
        // next reconnect (which now also checks PureSignal::isAutoCalEnabled).
        updatePsaIndicatorVisibility();
        // Phase 3M-4 Task 13: hide PureSignalApplet + TxApplet [PS-A] on
        // disconnect.  Same lifetime model as the PSA indicator above.
        // Re-evaluation happens on next reconnect via the connected-branch
        // gating block.
        if (m_pureSignalApplet) {
            m_pureSignalApplet->setVisible(false);
        }
        if (m_txApplet) {
            // Push the unknown-board defaults (hasPureSignal == false) so
            // [PS-A] hides.  RadioModel::boardCapabilities() returns the
            // unknown-board fallback when m_hardwareProfile.caps is null
            // (RadioModel.cpp:1016 [v2.10.3.13] equivalent).
            m_txApplet->setBoardCapabilities(m_radioModel->boardCapabilities());
        }

        // Phase 3Q Sub-PR-6 (F.1): RxDashboard shows placeholder "—" when
        // disconnected automatically (slice values reset to defaults). No
        // per-disconnect update needed here.
        // (The "last connected" breadcrumb moved to the segment tooltip in D.2.)

        // Phase 3Q Task 5 — auto-open: on disconnect (after having been connected),
        // open the ConnectionPanel so the user can reconnect.
        // Guard: m_shuttingDown suppresses it during ⌘Q so ConnectionPanel's
        // ctor doesn't restart discovery mid-close (would beach-ball the
        // close path for the full SafeDefault scan window — see
        // [shutdown-trace] log analysis 2026-05-02). The very first state read
        // at startup is Disconnected which should not open the panel either —
        // the radio-name check below handles that case.
        // The panel itself is non-modal (show/raise), matching the current pattern.
        if (!m_shuttingDown) {
            // Betreiber 2026-09-01, korrigiert nach "es liegt da wieder
            // alles durcheinander, bevor man connected": HIER nicht mehr
            // unbedingt verstecken -- eine blosse Trennung MITTEN in der
            // Sitzung soll die schwebenden Fenster nicht mehr anfassen,
            // solange die Connect-Maske gar nicht aufgeht (das war die
            // heutige "kein Funkgerät hier"-Entscheidung). Versteckt
            // wird nur noch, wenn showConnectionPanel() unten den Dialog
            // TATSAECHLICH oeffnet -- dessen eigener Aufruf von
            // hideFloatingWindowsBehindConnectMask() deckt genau diesen
            // Fall ab. Wiederhergestellt wird weiterhin bei JEDEM
            // erneuten Verbinden (oben im Connected-Zweig dieser
            // Funktion) sowie beim SCHLIESSEN der Maske (siehe
            // showConnectionPanel()).

            // Only open if we were previously connected (transition from Connected,
            // not the initial Disconnected state at startup). We detect this by
            // checking if the model has ever reported a radio name — set on connect.
            //
            // Review-Fund 2026-08-28: seit P2 (wie P1 schon vorher) bei
            // Funkstille automatisch und unbegrenzt neu verbindet, durchlaeuft
            // eine anhaltende Stoerung diesen Zweig immer wieder (LinkLost ->
            // Connecting -> LinkLost -> ...). Ohne die Wache hier waere der
            // Dialog bei jedem Durchlauf erneut nach vorn gekommen und haette
            // dem Bediener den Fokus geraubt -- genau die Art staendiger
            // Unterbrechung, die der automatische Wiederholversuch eigentlich
            // vermeiden sollte. Einmal je Ausfall genuegt; zurueckgesetzt,
            // sobald Connected wieder eintrifft (weiter oben in dieser Funktion).
            if (!m_radioModel->name().isEmpty()
                && !m_connectionPanelAutoOpenedThisEpisode) {
                m_connectionPanelAutoOpenedThisEpisode = true;
                showConnectionPanel();
            }
        }
    }

    // 3Q-9 (post-feedback simplification): Connect is "reconnect to last".
    // Greyed out when (a) we're already connected, OR (b) there's no
    // last-used radio in saved entries to reconnect to. Manage Radios is
    // the only way to pick a different radio.
    //
    // Use the model's authoritative connectionState (3Q-1) rather than
    // RadioModel::isConnected() — the latter dereferences m_connection
    // which can briefly disagree during teardown (m_connectionState
    // already Disconnected but m_connection->isConnected() still true
    // until the worker-thread teardown finishes). Without this, a
    // Radio→Disconnect would leave Connect greyed forever.
    if (m_actConnect && m_actDisconnect && m_actProtocolInfo) {
        const bool connected =
            (m_radioModel->connectionState() == ConnectionState::Connected);
        AppSettings& s = AppSettings::instance();
        const QString lastMac = s.lastConnected();
        const bool hasReconnectTarget =
            !connected
            && !lastMac.isEmpty()
            && s.savedRadio(lastMac).has_value();
        m_actConnect->setEnabled(hasReconnectTarget);
        m_actDisconnect->setEnabled(connected);
        m_actProtocolInfo->setEnabled(connected);
    }
}

// Phase 3I Task 17 / Phase 3Q Task 10 — was auto-reconnect-on-launch.
//
// The whole auto-connect-on-launch feature (the "Auto-connect to this radio
// on launch" checkbox, the SavedRadio::autoConnect flag, the pinToMac
// discovery-and-connect dance, all of it) was removed 2026-08-27 — operator
// decision (OE5SOS): the flag kept getting re-armed by the plain Connect
// button (ConnectionPanel::onConnectClicked used to hardcode autoConnect=true
// on every connect), so it silently reconnected to the Anvelina on every
// launch over a flaky WLAN link — actively unwanted, not just unreliable.
// What's left is just the cold-launch convenience this always also did:
// open the Connect-to-Radio panel so the user has a one-click path to a
// saved radio or Add Manually (design §6.1).
void MainWindow::openConnectionPanelOnLaunch()
{
    showConnectionPanel();
}

// =============================================================================
// Phase 3O Sub-Phase 11 Task 11b — VAX first-run / rescan hook
// =============================================================================
//
// Called once from the constructor via QTimer::singleShot(0, ...). Decides
// whether to show the VaxFirstRunDialog based on audio/FirstRunComplete
// and a SHA-256 diff of the current detected-cable set against the stored
// audio/LastDetectedCables fingerprint. The fingerprint is refreshed on
// every launch regardless of whether the dialog shows, so uninstall +
// reinstall of the same cable doesn't flag it as "new" forever.
//
// NereusSDR-original; no Thetis equivalent.
void MainWindow::checkVaxFirstRun()
{
    auto& s = AppSettings::instance();
    const bool firstRunDone =
        (s.value(QStringLiteral("audio/FirstRunComplete"),
                 QStringLiteral("False")).toString() == QStringLiteral("True"));

    // Platform-specific scan — see detectedForFirstRun() in the anonymous
    // namespace at the top of this file for the platform split rationale.
    const QVector<DetectedCable> detected = detectedForFirstRun();

    // Always refresh the stored fingerprint so a cable being removed +
    // later reinstalled doesn't permanently re-flag itself as "new".
    const QString newCsv = VirtualCableDetector::fingerprintCsv(detected);
    const QString lastCsv = s.value(QStringLiteral("audio/LastDetectedCables"),
                                    QString()).toString();
    s.setValue(QStringLiteral("audio/LastDetectedCables"), newCsv);
    s.save();

    FirstRunScenario scenario;
    QVector<DetectedCable> payload;

    if (!firstRunDone) {
#if defined(Q_OS_WIN)
        scenario = detected.isEmpty() ? FirstRunScenario::WindowsNoCables
                                       : FirstRunScenario::WindowsCablesFound;
#elif defined(Q_OS_MAC)
        scenario = FirstRunScenario::MacNative;
#else
        scenario = FirstRunScenario::LinuxNative;
#endif
        payload = detected;
    } else {
        // First-run already complete — only pop the dialog if NEW cables
        // have appeared since the last launch.
        const auto fresh = VirtualCableDetector::diffNewCables(detected, lastCsv);
        if (fresh.isEmpty()) {
            return;
        }
        scenario = FirstRunScenario::RescanNewCables;
        payload = fresh;
    }

    auto* dlg = new VaxFirstRunDialog(scenario, payload, this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);

    // "Apply suggested" / "Apply to VAX 3 & 4" — user accepted the
    // recommended bindings. Log-but-ignore any AudioEngine wiring failure;
    // the design interview explicitly settled that we still mark the
    // first-run complete so the user isn't re-ambushed on next launch.
    connect(dlg, &VaxFirstRunDialog::applySuggested, this,
            [this](const QVector<QPair<int, QString>>& bindings) {
        auto* engine = m_radioModel->audioEngine();
        if (!engine) {
            qCWarning(lcAudio)
                << "VAX first-run: applySuggested with no AudioEngine; "
                   "bindings dropped" << bindings.size();
            return;
        }

        // Remap dialog-suggested bindings onto the first VAX slots
        // whose audio/Vax<ch>/DeviceName is unset. VaxFirstRunDialog::
        // computeSuggestedBindings always numbers its payload starting
        // at VAX 1 regardless of scenario, with an explicit comment
        // that MainWindow is responsible for skipping slots the user
        // has already assigned. Applying the dialog's channel numbers
        // verbatim — the previous revision — clobbered existing slot-1
        // ..N mappings under FirstRunScenario::RescanNewCables. We
        // apply the same rule unconditionally since it is a no-op for
        // WindowsCablesFound (all four DeviceName keys are empty on a
        // fresh install, so remap resolves to the same 1..N order).
        //
        auto& settings = AppSettings::instance();
        int slot = 1;
        for (const auto& b : bindings) {
            while (slot <= 4) {
                const QString key = QStringLiteral("audio/Vax%1/DeviceName")
                                        .arg(slot);
                if (settings.value(key, QString()).toString().isEmpty()) {
                    break;
                }
                ++slot;
            }
            if (slot > 4) {
                qCWarning(lcAudio)
                    << "VAX first-run: no unassigned slots remain;"
                    << "dropping cable" << b.second;
                break;
            }
            AudioDeviceConfig cfg;
            cfg.deviceName = b.second;
            engine->setVaxConfig(slot, cfg);
            engine->setVaxEnabled(slot, true);
            ++slot;
        }
    });

    // Sub-Phase 12: wire "Customize…" / "Why do I need this?" → Setup → VAX.
    // Opens (or raises) the Setup dialog and navigates to Audio → VAX.
    connect(dlg, &VaxFirstRunDialog::openSetupAudioPage, this,
            [this](const QString& pageLabel) {
        auto* dialog = new SetupDialog(m_radioModel, this);
        dialog->setAttribute(Qt::WA_DeleteOnClose);
        wireSetupDialog(dialog);
        dialog->selectPage(pageLabel);
        dialog->show();
    });

    connect(dlg, &VaxFirstRunDialog::openInstallUrl, this,
            [](const QString& url) {
        QDesktopServices::openUrl(QUrl(url));
    });

    // Persist audio/FirstRunComplete on Accepted only (Apply / Skip /
    // Got-it). Rejected covers Escape, window-close, and Customize — none
    // of those should silence the dialog on next launch.
    connect(dlg, &QDialog::finished, this, [](int result) {
        if (result == QDialog::Accepted) {
            auto& settings = AppSettings::instance();
            settings.setValue(QStringLiteral("audio/FirstRunComplete"),
                              QStringLiteral("True"));
            settings.save();
        }
    });

    dlg->show();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    // Mark shutting-down BEFORE anything else so the "auto-open
    // ConnectionPanel on Disconnect" slot below doesn't re-trigger
    // discovery via ConnectionPanel's ctor while teardown runs.
    m_shuttingDown = true;

    // ── Profil-Stand sichern, SOFORT, nicht ueber aboutToQuit ────────
    //
    // Absturzbericht 2026-08-27 23:10 (Segfault beim Beenden, nach
    // erfolgreicher Verbindung und sauberem Trennen). Stack: dieses
    // closeEvent loest ueber die Cocoa-Terminate-Kaskade REENTRANT ein
    // qApp::aboutToQuit aus, waehrend dieses closeEvent noch auf dem
    // Stapel steht -- und wireProfileRail()'s aboutToQuit-Handler
    // („Beim Beenden den jetzigen Stand ins aktive Profil") lief dann
    // mitten im eigenen Abbau, mit Zeigern, die dieses closeEvent
    // gerade erst zur Haelfte aufgeraeumt hatte. Der Zeitpunkt hier,
    // ganz am Anfang, ist der einzige, an dem garantiert noch alles
    // lebt, was captureIntoCurrent() anfasst. Der aboutToQuit-Handler
    // selbst prueft jetzt m_shuttingDown und tut auf diesem Weg nichts
    // mehr -- er bleibt nur noch fuer den SIGTERM/Kill-Pfad zustaendig,
    // auf dem dieses closeEvent nie laeuft.
    if (m_layoutProfiles) {
        m_layoutProfiles->captureIntoCurrent();
        m_layoutProfiles->save();
        // Betreiber 2026-09-01: "letzter Zustand nie beim Öffnen
        // sichtbar" -- unbedingtes Log, damit sich Erfassen (hier),
        // Laden (Start, siehe [ProfileLoadOnStartup]) und die
        // tatsaechlich auf Platte stehende Datei direkt vergleichen
        // lassen, statt weiter zu vermuten.
        const QVariantMap snap =
            m_layoutProfiles->snapshot(m_layoutProfiles->current());
        qWarning() << "[ProfileSaveOnQuit:closeEvent]"
                   << m_layoutProfiles->current()
                   << "floatingApplets="
                   << snap.value(QStringLiteral("floatingApplets")).toMap().size()
                   << "floatingPans="
                   << snap.value(QStringLiteral("floatingPans")).toMap().size()
                   << "rotor=" << snap.value(QStringLiteral("rotor")).toMap();
    }

    // Die Schwebefenster SOFORT informieren — nicht erst unten bei
    // shutDownFloating(). Bei Cmd+Q schickt Qt jedem Fenster ein
    // Schliessereignis; ohne diese Zeile bittet das Schwebefenster
    // mitten im Abbau ums Zurueckhaengen und stirbt daran.
    // (AetherSDR MainWindow.cpp:2653 [@0cd4559])
    if (m_panStack) { m_panStack->setShuttingDown(true); }

    // Dasselbe fuer das Rotor/Log-Werkzeugfenster -- Betreiber
    // 2026-08-31, siehe ToolWindow::setShuttingDown()'s Kommentar: ohne
    // das schrieb ein ganz normales Beenden per rotem Punkt
    // RotorFloating=False in AppSettings, weil ToolWindow::closeEvent()
    // sonst IMMER ums Zurueckdocken bittet.
    if (m_rotorWindow) { m_rotorWindow->setShuttingDown(true); }

    // Force-run any pending coalesced slice save BEFORE we tear anything
    // down. The 500 ms debounce in RadioModel::scheduleSettingsSave can't
    // fire while this handler runs synchronously (event loop blocked on
    // QThread::wait below); without this flush the user's last AF / step /
    // freq / lock / RIT change is silently dropped on close.
    m_radioModel->flushPendingSettingsSave();

    // Stop discovery to prevent new signals during shutdown
    m_radioModel->discovery()->stopDiscovery();

    // Stop FFT thread
    //
    // Die begrenzte Wartezeit stand hier ohne Auswertung: lief sie ab,
    // ging es trotzdem weiter -- und ~MainWindow() zerstoerte spaeter
    // m_fftThread als Kind-QObject, worauf Qt mit qFatal abbricht, wenn
    // der Faden da noch laeuft ("QThread: Destroyed while thread
    // 'SpectrumThread' is still running").
    //
    // Der eigentliche Ausloeser dieses Abbruchs war ein anderer und ist
    // in 6e7c1cad behoben (die Aufraeumschleife weiter unten sammelte
    // ein FREMDES MainWindow ein und loeschte es synchron mitsamt
    // laufendem Faden). Das Auswerten bleibt trotzdem richtig: dass ein
    // stiller Zeitablauf hier direkt in einen Prozessabbruch fuehrt, ist
    // unabhaengig davon, wer den Faden gerade aufhaelt. Dieselbe Wahl
    // wie im ~MainWindow()-Sicherheitsnetz aus demselben Commit --
    // lieber laenger warten als abbrechen.
    if (m_fftThread && m_fftThread->isRunning()) {
        m_fftThread->quit();
        if (!m_fftThread->wait(2000)) {
            qWarning() << "[ShutdownFftThread] SpectrumThread did not stop"
                          " within 2s -- waiting without a timeout";
            m_fftThread->wait();
        }
    }

    // Save display settings before shutdown
    if (m_panStack) {
        for (PanadapterApplet* applet : m_panStack->allApplets()) {
            if (applet && applet->spectrumWidget()) {
                applet->spectrumWidget()->saveSettings();
            }
        }
    }

    // Tear down connection (sends stop command, closes sockets, joins thread)
    m_radioModel->disconnectFromRadio();

    // Save container layout
    if (m_containerManager) {
        m_containerManager->saveState();
    }

    // Phase 3F Sub-Epic D Task 15: persist pan layout id + per-splitter
    // sizes. PanadapterStack::saveSplitterState writes PanLayoutId +
    // PanLayoutSplitter_* keys to AppSettings; the matching restore runs
    // during MainWindow init.
    if (m_panStack) {
        m_panStack->saveSplitterState();
    }

    // Issue #206 — persist window geometry + maximized/fullscreen
    // state. Captured BEFORE close so the saved blob reflects the
    // user-visible state, not Qt's mid-teardown geometry.
    saveMainWindowGeometry();

    AppSettings::instance().save();
    qWarning() << "[ProfileSaveOnQuit:closeEvent] AppSettings::save() done";

    // ── Schwebende Fenster JETZT abraeumen ───────────────────────────
    //
    // Aus dem Absturzbericht vom 2026-08-21, 06:47 (und 06:29, gleicher
    // Ablauf). Der Faden mit dem Fehler ist der Hauptfaden, und der
    // Stapel liest sich von unten nach oben so:
    //
    //   NSApplication terminate → exit → __cxa_finalize
    //   → QThreadDataDestroyer::EarlyMainThread::~EarlyMainThread
    //   → sendPostedEvents → AppletFloatingWindow::~AppletFloatingWindow
    //   → WindowTitleBar::~WindowTitleBar → QWidget::destroy
    //   → QWindow::~QWindow → QSurface::~QSurface
    //   → QOpenGLContext::currentContext() → QThreadStorageData::get()
    //   → SIGSEGV
    //
    // Also: ein deleteLater auf ein schwebendes Fenster wurde nicht
    // mehr zugestellt, solange das Programm lief. Zugestellt wurde es
    // erst beim Abbau der Faden-Daten — da war der Faden-Speicher, den
    // der QWindow-Destruktor ueber den OpenGL-Kontext anfasst, schon
    // weg. Die Leiste hat ein eigenes Fenster (WA_NativeWindow, damit
    // sie ueber dem Spektrum steht), deshalb trifft es genau sie.
    //
    // Zwei Zeilen, die das ausschliessen: die Fenster hier selbst
    // abraeumen, und danach zustellen, was sonst noch aussteht —
    // solange das Programm noch da ist.
    // ── Offene Dialoge mit schliessen ───────────────────────────────
    //
    // Gefunden am 2026-08-21 beim Bau von
    // tst_quit_leaves_no_pending_deletes: ein VaxFirstRunDialog
    // ueberlebt das Schliessen des Hauptfensters. Er haengt zwar als
    // Kind daran und traegt WA_DeleteOnClose — aber close() auf das
    // Hauptfenster schliesst seine Dialoge NICHT mit. Sie bleiben
    // offen und ihr Loeschen faellt in den Programmabbau; das ist
    // dieselbe Familie wie c8d8161a und 1c781bae.
    //
    // Es ist ausserdem fuer sich richtig: ein Dialog, der laenger lebt
    // als das Fenster, zu dem er gehoert, ist ein Fenster ohne
    // Zuhause.
    for (QDialog* d : findChildren<QDialog*>()) {
        if (d->isVisible()) { d->close(); }
    }

    // Auch die abgeloesten PANADAPTER — eine eigene Sammlung, die beim
    // ersten Anlauf (c8d8161a) uebersehen wurde. Eine Probe im echten
    // Hauptfenster hat den Absturz dann geliefert: abloesen, schliessen,
    // SIGSEGV.
    if (m_panStack) { m_panStack->shutDownFloating(); }

    for (AppletFloatingWindow* w : m_floatingApplets) { delete w; }
    m_floatingApplets.clear();
    if (m_rotorWindow) { delete m_rotorWindow; m_rotorWindow = nullptr; }

    // ── UND ALLES ANDERE, WAS NOCH AM SCHREIBTISCH STEHT ────────────
    //
    // Der Betreiber am 2026-08-22: "test selbst, gar nichts
    // funktioniert." Nachgesehen: Longpath lief, das Menue stand da —
    // aber das HAUPTFENSTER war weg. Uebrig war ein abgeloestes
    // Container-Fenster ("RX1 Main Panel"), und genau das hielt die
    // App am Leben: Qt beendet erst, wenn das LETZTE Fenster faellt.
    // Der Bediener hatte damit eine laufende App ohne Panadapter, ohne
    // Wasserfall, ohne irgendetwas zu bedienen — und keinen Weg
    // zurueck.
    //
    // Die Liste oben kannte nur zwei Sorten (AppletFloatingWindow und
    // das Rotorfenster). Container-Fenster stehen in einer anderen
    // Verwaltung und wurden nie mitgeraeumt. Statt die dritte Liste
    // nachzupflegen — und beim naechsten Fenstertyp die vierte vergessen
    // — wird hier nach TATSACHEN aufgeraeumt: was noch als eigenes
    // Fenster dasteht und nicht dieses hier ist, geht mit.
    //
    // Dialoge sind oben schon geschlossen; was hier ankommt, sind
    // Werkzeugfenster.
    //
    // ── Popups sind KEINE Werkzeugfenster (Absturz 2026-08-30) ───────
    //
    // Absturzbericht 2026-08-30 13:58: SIGABRT, "pointer being freed
    // was not allocated". Der Bediener hatte per Rechtsklick auf ein
    // ProfileRail-Abzeichen ein Kontextmenue offen (showMenuFor() in
    // widgets/ProfileRail.cpp — ein STAPEL-lokales `QMenu menu(this)`,
    // gerade mitten in seinem eigenen menu.exec()) und schloss dabei
    // das Fenster. Der Klick auf die Ampel lief ueber genau die
    // verschachtelte Cocoa-Eventschleife, die menu.exec() fuer sich
    // selbst aufgemacht hatte — deshalb feuerte dieses closeEvent()
    // REENTRANT, mit dem exec()-Aufruf noch auf dem Stapel darunter.
    //
    // Ein QMenu ist trotz `parent = this` ein echtes Top-Level-Fenster
    // (Qt::Popup) und stand damit in QApplication::topLevelWidgets().
    // Die Schleife unten hat es also miteingesammelt: hide() +eigenes
    // deleteLater(), sofort im naechsten Sendpostedevents-Aufruf
    // zugestellt — "delete this" auf ein QMenu, dessen "this" eine
    // Stapeladresse ist, nie ein malloc()-Zeiger. Daher exakt dieses
    // Fehlerbild.
    //
    // Popups (Kontextmenues, DspParamPopup, SpectrumOverlayMenu, ...)
    // sind fluechtig und raeumen sich ueber ihre eigene exec()/hide()
    // selbst ab — sie gehoeren nicht zu den "Werkzeugfenstern", die
    // diese Schleife eigentlich sucht, und duerfen hier nicht
    // angefasst werden, gleich ob sie gerade laufen oder nicht.
    for (QWidget* w : QApplication::topLevelWidgets()) {
        if (w == this) { continue; }
        if (!w->isWindow()) { continue; }
        if (qobject_cast<QDialog*>(w)) { continue; }   // oben erledigt
        if (w->windowType() == Qt::Popup) { continue; }
        // ── Ein ANDERES Hauptfenster ist kein verwaistes Werkzeugfenster ──
        //
        // Im Betrieb gibt es genau eines (main.cpp); im Test-Harness aber
        // mehrere nacheinander im selben Prozess. Wer hier ein fremdes
        // MainWindow einsammelt, dessen closeEvent() nie lief, loescht es
        // im sendPostedEvents() gleich darunter SYNCHRON -- und dessen
        // ~QObject reisst m_fftThread noch laufend in ~QThread(): QFATAL
        // "Destroyed while thread 'SpectrumThread' is still running",
        // SIGABRT. Per lldb belegt am 2026-09-03 in
        // tst_settings_are_remembered (Stapel: closeEvent() -> sendPosted
        // Events -> ~MainWindow() eines ANDEREN Objekts -> ~QThread).
        if (qobject_cast<MainWindow*>(w)) { continue; }
        w->hide();
        w->deleteLater();
    }
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);

    event->accept();

    // Ask Qt for an orderly exit from the event loop. Previously called
    // std::exit(0) which runs C++ static destructors before Qt's thread
    // cleanup — that caused QThreadStoragePrivate::finish to fire a qWarning
    // against a destructed QRegularExpression in the PII-redaction message
    // handler, segfaulting every close (~100 diagnostic reports in one day).
    QCoreApplication::quit();
}

// =============================================================================
// Phase 3G-14: AI-Assisted Issue Reporter
// Ported from AetherSDR TitleBar::showFeatureRequestDialog() /
// showFeatureRequestDialogImpl()
// =============================================================================

void MainWindow::showFeatureRequestDialog()
{
    // Version check gate — warn if not on latest release before filing
    auto* nam = new QNetworkAccessManager(this);
    QNetworkRequest req(QUrl(QStringLiteral(
        "https://api.github.com/repos/boydsoftprez/NereusSDR/releases/latest")));
    req.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Longpath"));
    auto* reply = nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply, nam] {
        reply->deleteLater();
        nam->deleteLater();

        if (reply->error() == QNetworkReply::NoError) {
            QJsonDocument doc = QJsonDocument::fromJson(reply->readAll());
            QString latest = doc.object().value(QStringLiteral("tag_name")).toString();
            if (latest.startsWith(QLatin1Char('v'))) {
                latest = latest.mid(1);
            }
            QVersionNumber latestVer = QVersionNumber::fromString(latest);
            QVersionNumber currentVer = QVersionNumber::fromString(
                QCoreApplication::applicationVersion());
            if (!latestVer.isNull() && currentVer < latestVer) {
                auto answer = QMessageBox::warning(this,
                    QStringLiteral("Outdated Version"),
                    QStringLiteral(
                        "<p>You are running <b>v%1</b> but <b>v%2</b> is available.</p>"
                        "<p>Your issue may already be fixed in the latest release. "
                        "Please update before filing a bug report.</p>"
                        "<p>Continue anyway?</p>")
                        .arg(QCoreApplication::applicationVersion(), latest),
                    QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
                if (answer != QMessageBox::Yes) {
                    return;
                }
            }
        }
        // Proceed to show the issue dialog
        showFeatureRequestDialogImpl();
    });
}

void MainWindow::showFeatureRequestDialogImpl()
{
    static const QString kPrompt = QStringLiteral(
        "IMPORTANT — before doing anything else, fetch the complete list of open\n"
        "issues by reading pages sequentially until you get fewer than 100 results:\n"
        "  Page 1: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=1\n"
        "  Page 2: https://github.com/boydsoftprez/NereusSDR/issues?state=open&per_page=100&page=2\n"
        "  ... continue until a page returns fewer than 100 issues.\n"
        "Do NOT rely on cached or training data for the issue list.\n\n"
        "Also fetch CLAUDE.md fresh (do not use cached versions):\n"
        "  https://raw.githubusercontent.com/boydsoftprez/NereusSDR/main/CLAUDE.md\n\n"
        "I want to report an issue or request a feature for NereusSDR, a cross-platform\n"
        "Qt6/C++20 SDR console for OpenHPSDR radios (ANAN, Hermes Lite 2, etc.). It uses\n"
        "the OpenHPSDR Protocol 1 and Protocol 2 over UDP, with client-side DSP via WDSP.\n\n"
        "DUPLICATE CHECK — this is mandatory. Search the fetched issue list for keywords\n"
        "related to my description below. Check titles AND bodies. If you find an existing\n"
        "issue that covers the same thing, STOP and tell me:\n"
        "  > Duplicate found: #<number> — <title>\n"
        "  > I recommend adding a +1 reaction and a comment describing your use case.\n"
        "Do NOT write a new issue if a duplicate exists.\n\n"
        "If no duplicate exists, determine whether my description is a BUG REPORT or a\n"
        "FEATURE REQUEST, then write a GitHub issue using the appropriate format below.\n"
        "Use GitHub-flavored Markdown formatting (headers, code blocks, bullet points).\n\n"
        "FOR FEATURE REQUESTS include:\n"
        "1. A clear, concise title (imperative mood)\n"
        "2. ## What — what the feature does from the user's perspective\n"
        "3. ## Why — what problem it solves\n"
        "4. ## How Other Clients Do It — how Thetis, PowerSDR, SparkSDR, etc. handle this\n"
        "5. ## Suggested Behavior — specific UX: what the user clicks, sees, what happens.\n"
        "   Reference NereusSDR UI elements (AppletPanel, RxApplet, TxApplet, SetupDialog, etc.)\n"
        "6. ## Protocol Hints — relevant OpenHPSDR commands, or \"Unknown — needs research\"\n"
        "7. ## Acceptance Criteria — 3-5 bullet points defining done vs not-done\n\n"
        "FOR BUG REPORTS include:\n"
        "1. A clear title describing the broken behavior\n"
        "2. ## What happened — describe the incorrect behavior\n"
        "3. ## What I expected — describe the correct behavior\n"
        "4. ## Steps to reproduce — numbered steps to trigger the bug\n"
        "5. ## Environment — OS, radio model, protocol version, firmware version if relevant\n"
        "6. ## Suggested fix — if you have an idea what's wrong, describe it\n\n"
        "Suggest appropriate labels from: enhancement, bug, documentation,\n"
        "help wanted, good first issue, question\n\n"
        "Here is my idea or bug report:\n\n"
        "[Describe your feature or bug here in plain English]");

    // Reuse existing dialog if still open
    static QPointer<QDialog> sDlg;
    if (sDlg) {
        sDlg->raise();
        sDlg->activateWindow();
        return;
    }

    auto* dlg = new QDialog(this);
    sDlg = dlg;
    dlg->setWindowTitle(QStringLiteral("AI-Assisted Issue Reporter"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->setStyleSheet(Style::themed(QStringLiteral("QDialog { background: #0f0f1a; }")));
    dlg->setMinimumWidth(620);

    auto* vbox = new QVBoxLayout(dlg);
    vbox->setSpacing(8);
    vbox->setContentsMargins(16, 16, 16, 16);

    auto* header = new QLabel(QStringLiteral(
        "<h3 style='color:#c8d8e8;'>AI-Assisted Issue Reporter</h3>"
        "<p style='color:#8090a0;'>Use any AI assistant to write a detailed bug report or feature request.</p>"
        "<ol style='color:#c8d8e8;'>"
        "<li><b>Choose your AI</b> below — prompt is copied to your clipboard</li>"
        "<li><b>Paste the prompt</b> into the AI chat</li>"
        "<li><b>Describe your idea</b> — edit the [bracketed] section</li>"
        "<li><b>Copy the AI's output</b> and click <b>Submit Your Idea</b></li>"
        "</ol>"));
    header->setWordWrap(true);
    vbox->addWidget(header);

    // Status label — shows after provider selected
    auto* statusLabel = new QLabel;
    statusLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: #6fa384; font-size: 11px; font-weight: bold; }"));
    statusLabel->setAlignment(Qt::AlignCenter);
    statusLabel->hide();
    vbox->addWidget(statusLabel);

    // AI provider buttons
    const QString btnStyle = QStringLiteral(
        "QPushButton { background: #1a2a3a; border: 1px solid #304050; "
        "border-radius: 6px; color: #c8d8e8; font-size: 13px; font-weight: bold; "
        "padding: 6px 12px; }"
        "QPushButton:hover { background: #203040; }");

    auto* btnRow1 = new QHBoxLayout;
    struct Provider { const char* name; const char* url; };
    static constexpr Provider providers[] = {
        {"Claude",     "https://claude.ai/new"},
        {"ChatGPT",    "https://chat.openai.com/"},
        {"Gemini",     "https://gemini.google.com/"},
        {"Grok",       "https://grok.x.ai/"},
        {"Perplexity", "https://www.perplexity.ai/"},
    };
    for (const auto& p : providers) {
        auto* btn = new QPushButton(QString::fromUtf8(p.name), dlg);
        btn->setStyleSheet(btnStyle);
        btn->setAutoDefault(false);
        QString url = QString::fromUtf8(p.url);
        connect(btn, &QPushButton::clicked, dlg, [url, statusLabel] {
            QApplication::clipboard()->setText(kPrompt);
            QDesktopServices::openUrl(QUrl(url));
            statusLabel->setText(QStringLiteral(
                "Prompt copied to clipboard — paste into the AI, "
                "then come back and click Submit Your Idea"));
            statusLabel->show();
        });
        btnRow1->addWidget(btn);
    }
    vbox->addLayout(btnRow1);

    vbox->addSpacing(8);

    // Submit / Report / Close
    auto* btnRow2 = new QHBoxLayout;

    auto* submitBtn = new QPushButton(QStringLiteral("Submit Your Idea"), dlg);
    submitBtn->setAutoDefault(false);
    submitBtn->setStyleSheet(Style::themed(QStringLiteral(
        "QPushButton { background: #4a7ba8; color: #0f0f1a; font-weight: bold; "
        "border-radius: 6px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: #4a7ba8; }")));
    connect(submitBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=feature_request.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(submitBtn);

    auto* bugBtn = new QPushButton(QStringLiteral("Report a Bug"), dlg);
    bugBtn->setAutoDefault(false);
    // Ein gewoehnlicher Knopf, kein roter.
    //
    // War #cc4040 auf Weiss, dann kurz das Paletten-Rot. Beides falsch:
    // Rot markiert in diesem Programm, dass etwas NICHT GEHT -- die
    // Bandkante, ein SWR, bei dem man nicht senden sollte. Diesen Knopf
    // zu druecken geht, und es geht dabei auch nichts kaputt.
    //
    // Anders als "Forget" und "Disconnect" macht er nicht einmal etwas
    // rueckgaengig, deshalb auch keine warnende Schrift: die Toene von
    // buttonBaseStyle(), nur mit der groesseren Geometrie dieses
    // Dialogs (13 px statt 10, Polsterung 8/20 statt 2/4).
    bugBtn->setStyleSheet(QStringLiteral(
        "QPushButton { background: %1; border: 1px solid %2;"
        "  color: %3; font-weight: bold;"
        "  border-radius: 6px; padding: 8px 20px; font-size: 13px; }"
        "QPushButton:hover { background: %4; }")
        .arg(QLatin1String(Style::kButtonBg), QLatin1String(Style::kBorder),
             QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kButtonAltHover)));
    connect(bugBtn, &QPushButton::clicked, dlg, [dlg] {
        QDesktopServices::openUrl(QUrl(QStringLiteral(
            "https://github.com/boydsoftprez/NereusSDR/issues/new?template=bug_report.yml")));
        QTimer::singleShot(500, dlg, &QDialog::close);
    });
    btnRow2->addWidget(bugBtn);

    auto* closeBtn = new QPushButton(QStringLiteral("Close"), dlg);
    closeBtn->setAutoDefault(false);
    closeBtn->setStyleSheet(btnStyle);
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);
    btnRow2->addWidget(closeBtn);
    vbox->addLayout(btnRow2);

    // Copy prompt to clipboard on first open
    QApplication::clipboard()->setText(kPrompt);

    dlg->show();
}

} // namespace Longpath
