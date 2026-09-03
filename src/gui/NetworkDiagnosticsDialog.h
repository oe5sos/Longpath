// src/gui/NetworkDiagnosticsDialog.h
#pragma once

// =================================================================
// src/gui/NetworkDiagnosticsDialog.h  (NereusSDR)
// =================================================================
//
//  Copyright (C) 2026 J.J. Boyd (KG4VCF)
//
//  This program is free software; you can redistribute it and/or
//  modify it under the terms of the GNU General Public License
//  as published by the Free Software Foundation; either version 2
//  of the License, or (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
//  GNU General Public License for more details.
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
// =================================================================
// Modification history (NereusSDR):
//   2026-04-30 — Created for Phase 3Q sub-PR-3 (shell-chrome redesign).
//                NereusSDR-original code (no Thetis port; no upstream
//                attribution required). J.J. Boyd (KG4VCF), with
//                AI-assisted implementation via Anthropic Claude Code.
// =================================================================

#include <QDialog>
#include <QPointer>
#include <QTimer>

class QGridLayout;
class QLabel;

namespace Longpath {

class RadioModel;
class AudioEngine;

// NetworkDiagnosticsDialog — modal diagnostic grid.
//
// Sections: Connection / Network / Audio / Radio Telemetry / TX (SunSDR).
// TX (SunSDR) added Step 4 of the SunSDR2 QRP TX-chain plan — a
// read-only window onto SunSdrRadioConnection's own bench-only TX gate
// state (armed / mox / pace-repeat count / trace tail); populated only
// when the active connection is a SunSdrRadioConnection, "— (not
// reported)" placeholders otherwise, same convention as the PA voltage
// row above. This dialog never arms or transmits anything itself.
// Refreshes at 250 ms from m_refreshTimer.
//
// Session-scoped accumulators (maxRtt, underruns) persist for the
// lifetime of the dialog instance and are zeroed by "Reset session stats".
//
// Wired up to the sub-PR-2 signals:
//   RadioConnection::pingRttMeasured(int)
//   RadioConnection::userAdc0Changed(float)   — PA volt; PSU/supplyVolts
//                                               wiring dropped 2026-04-30
//                                               (Thetis never displays AIN6).
//   AudioEngine::flowStateChanged(FlowState)
//
// Wire-up to ConnectionSegment left-click happens in sub-PR-4 / D.2.
class NetworkDiagnosticsDialog : public QDialog {
    Q_OBJECT

public:
    NetworkDiagnosticsDialog(RadioModel* model, AudioEngine* audio,
                             QWidget* parent = nullptr);

    // Test-only accessors — read the exact text shown in the new
    // TX (SunSDR) section (Step 4 of the SunSDR2 QRP TX-chain plan), so a
    // test can assert against what a user would actually see rather than
    // re-reading SunSdrRadioConnection's own state a second, independent
    // way. Same *ForTest() naming and QLabel-text-read shape as this
    // project's other GUI test accessors (e.g. PaValuesPage::
    // fwdCalibratedTextForTest()). Defined in the .cpp, not inline here —
    // QLabel is only forward-declared in this header.
    QString sunSdrArmedTextForTest() const;
    QString sunSdrMoxTextForTest() const;
    QString sunSdrPaceRepeatsTextForTest() const;
    QString sunSdrTraceTailTextForTest() const;

private slots:
    void refresh();
    void onResetSessionStats();

private:
    void buildUi();
    void buildConnectionSection(QGridLayout* grid, int& row);
    void buildNetworkSection(QGridLayout* grid, int& row);
    void buildAudioSection(QGridLayout* grid, int& row);
    void buildTelemetrySection(QGridLayout* grid, int& row);
    void buildSunSdrTxSection(QGridLayout* grid, int& row);
    QLabel* makeValueLabel();
    QLabel* makeFieldLabel(const QString& text);

    QPointer<RadioModel>  m_model;
    QPointer<AudioEngine> m_audio;
    QTimer                m_refreshTimer;

    // Connection section labels
    QLabel* m_statusLabel{nullptr};
    QLabel* m_uptimeLabel{nullptr};
    QLabel* m_radioLabel{nullptr};
    QLabel* m_protocolLabel{nullptr};
    QLabel* m_ipLabel{nullptr};
    QLabel* m_macLabel{nullptr};
    // Network section labels
    QLabel* m_rttLabel{nullptr};
    QLabel* m_maxRttLabel{nullptr};
    QLabel* m_jitterLabel{nullptr};
    QLabel* m_lossLabel{nullptr};
    QLabel* m_txRateLabel{nullptr};
    QLabel* m_rxRateLabel{nullptr};
    QLabel* m_sampleRateLabel{nullptr};
    QLabel* m_udpSeenLabel{nullptr};
    // Audio section labels
    QLabel* m_audioBackendLabel{nullptr};
    QLabel* m_audioBufferLabel{nullptr};
    QLabel* m_underrunLabel{nullptr};
    QLabel* m_packetGapLabel{nullptr};
    // Radio telemetry section labels (PSU dropped — see .cpp ctor note;
    // Thetis source-first audit 2026-04-30 confirmed supply_volts is
    // dead data Thetis itself never displays).
    QLabel* m_paVoltLabel{nullptr};
    QLabel* m_adcOvlLabel{nullptr};
    // TX (SunSDR) section labels — Step 4 of the SunSDR2 QRP TX-chain
    // plan (design synthesis: "Network Diagnostics wiring ... 'TX
    // (SunSDR)' row group: armed state, mox state, pace-repeat count,
    // trace tail"). Read-only display only; refresh() populates these
    // from SunSdrRadioConnection's own read-only accessors only when the
    // active connection is actually a SunSdrRadioConnection — see
    // buildSunSdrTxSection()'s own comment for the placeholder shown
    // otherwise, matching this dialog's existing "— (not reported)"
    // convention (m_paVoltLabel above).
    QLabel* m_sunSdrArmedLabel{nullptr};
    QLabel* m_sunSdrMoxLabel{nullptr};
    QLabel* m_sunSdrPaceRepeatsLabel{nullptr};
    QLabel* m_sunSdrTraceTailLabel{nullptr};

    // Session-scoped accumulators — zeroed by onResetSessionStats()
    int   m_maxRttMs{0};
    int   m_underrunCount{0};

    // Live values updated by signal lambdas
    int   m_lastRttMs{-1};
    float m_lastUserAdc0Volts{-1.0f};
};

} // namespace Longpath
