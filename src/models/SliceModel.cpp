// =================================================================
// src/models/SliceModel.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/display.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-09-27 — FM filter from the FM deviation: rx_fm_highcut from
//                 radio.cs:1571 [@852bf0e]. Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude Code.
//   2026-09-27 -- CW pitch follows live, by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e: centreCwFilterOnPitch (loop body
//                 of the console.cs CWPitch setter, image-limit slide),
//                 followCwPitch, recentreStoredCwFilters, clampCwPitch;
//                 CW pitch range 200..2250 (udCWPitch/udDSPCWPitch), one
//                 read site instead of four.
//   2026-09-28 -- Filter preset table Thetis-faithful, by Martin Fischer
//                 (OE5SOS), AI-assisted via Anthropic Claude, from Thetis
//                 v2.10.3.15-5-g852bf0e console.cs InitFilterPresets:
//                 values and names (presetNamesForMode) for LSB/USB/DIGL/
//                 DIGU/CWL/CWU/AM/SAM/DSB replace a table whose values no
//                 Thetis release ever carried; DIGU/DIGL default F5 +-500
//                 (was +-600 "1.2k", likewise never upstream); FM/SPEC/DRM
//                 marked Longpath-own; unused commonPresetsForMode removed.
// =================================================================

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
// display.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley (W5WC)
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
// Waterfall AGC Modifications Copyright (C) 2013 Phil Harman (VK6APH)
// Transitions to directX and continual modifications Copyright (C) 2020-2025 Richard Samphire (MW0LGE)
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

#include "SliceModel.h"

#include "Band.h"
#include "core/AppSettings.h"
#include "core/LogCategories.h"
#include "core/RadeChannel.h"
#include "core/WdspEngine.h"
#include "core/accessories/AlexController.h"
#include "core/SkuUiProfile.h"  // issue #257 — rxOnlyLabels lookup in refreshAntennasFromAlex
#include "models/RadioModel.h"

#include <QFile>
#include <QRegularExpression>
#include <QStandardPaths>

#include <algorithm>

namespace Longpath {

SliceModel::SliceModel(QObject* parent)
    : QObject(parent)
{
    setupRadeIdleClearTimer();
}

SliceModel::SliceModel(int sliceId, QObject* parent)
    : QObject(parent)
    , m_sliceIndex(sliceId)
{
    setupRadeIdleClearTimer();
}

// 2026-05-12 bench: RADE idle-clear timer setup.
//
// One-time construction during SliceModel ctor.  Reads
// RadeIdleClearMs from AppSettings (default 20 s, clamp 5..120 s).
// Single-shot QTimer; expiry lambda clears callsign + SNR back to
// the "no decode yet" sentinels.  Timer is parented to `this` so
// destruction is automatic; safe to leave running across mode swaps
// because the expiry only clears state that's already cleared in
// those paths.
void SliceModel::setupRadeIdleClearTimer()
{
    auto& s = AppSettings::instance();
    int ms = s.value(QStringLiteral("RadeIdleClearMs"), 20000).toInt();
    if (ms < 5000)   { ms = 5000;   }
    if (ms > 120000) { ms = 120000; }
    m_radeIdleClearMs = ms;

    m_radeIdleClearTimer = new QTimer(this);
    m_radeIdleClearTimer->setSingleShot(true);
    m_radeIdleClearTimer->setInterval(m_radeIdleClearMs);
    connect(m_radeIdleClearTimer, &QTimer::timeout, this, [this]() {
        // Clear callsign back to empty.  Renderer (VfoWidget) falls
        // back to the "RADE" literal prefix when callsign is empty,
        // so no "NaN" or empty cell on screen.
        if (!m_lastRadeRxCallsign.isEmpty()) {
            m_lastRadeRxCallsign.clear();
            emit lastRadeRxCallsignChanged(m_lastRadeRxCallsign);
        }
        // Clear SNR to NaN.  VfoWidget renders NaN as "---" (not
        // the literal string "NaN") in the SNR column.
        if (!qIsNaN(m_snrDb)) {
            m_snrDb = std::numeric_limits<double>::quiet_NaN();
            emit snrDbChanged(m_snrDb);
        }
    });
}

// Restart the idle timer when fresh activity arrives.  Only runs the
// timer while in a RADE sideband -- SSB / WSJT-X paths don't care
// about RADE idle and would burn cycles on an irrelevant timer.
void SliceModel::restartRadeIdleClearTimer()
{
    if (!m_radeIdleClearTimer) { return; }
    if (m_dspMode != DSPMode::RADE_U && m_dspMode != DSPMode::RADE_L) {
        return;
    }
    m_radeIdleClearTimer->start();  // restarts even if already running
}

SliceModel::~SliceModel() = default;

// ---------------------------------------------------------------------------
// Frequency
// ---------------------------------------------------------------------------

void SliceModel::setFrequency(double freq)
{
    // 3G-10 S2.9: client-side lock guard. When locked, setFrequency is a
    // no-op — prevents accidental tuning. The hardware VFO is not changed.
    if (m_locked) { return; }
    if (!qFuzzyCompare(m_frequency, freq)) {
        m_frequency = freq;
        emit frequencyChanged(freq);

        // Phase 3P-II Task 64: emit bandChanged on band boundary cross.
        // Uses Band::bandFromFrequency (IARU Region 2, GEN fallback).
        // Guarded so the signal fires at most once per distinct Band change.
        Band newBand = bandFromFrequency(freq);
        if (newBand != m_currentBand) {
            m_currentBand = newBand;
            emit bandChanged(newBand);
        }
    }
}

// ---------------------------------------------------------------------------
// Demodulation mode
// ---------------------------------------------------------------------------

void SliceModel::setDspMode(DSPMode mode)
{
    const bool modeChanged = (m_dspMode != mode);
    const DSPMode oldMode = m_dspMode;
    m_dspMode = mode;

    // ── Phase 3R J3 + K-bench: RADE channel-additive lifecycle ────────────
    //
    // RADE_U / RADE_L are Longpath-native DSPModes (J1).  Original J3
    // design destroyed the WDSP RxChannel and replaced it with a
    // RadeChannel on entry into RADE.  K-bench reframed the RX pipeline
    // (RxDspWorker.cpp:160-191) so RADE is now ADDITIVE rather than
    // replacement:
    //   - WDSP RxChannel stays alive in EVERY mode.  WDSP serves as
    //     the SSB demod front-end and produces decoded audio that
    //     feeds the S-meter / spectrum / AGC every tick.
    //   - RadeChannel is created ALONGSIDE RxChannel in RADE_U /
    //     RADE_L, consumes WDSP's decoded audio (downsampled to
    //     24 kHz), and owns the speaker path while active.
    //   - The WDSP-facing mode is mapped at the RxChannel boundary
    //     (RxChannel::wdspModeFor): RADE_U -> USB, RADE_L -> LSB.
    //     Without that mapping, raw enum 12/13 would land in WDSP's
    //     mode enum (review finding 2026-05-12, PR #238).
    //
    // So the swap logic below is now only about RadeChannel
    // create/destroy.  RxChannel is created once at connect time
    // (RadioModel) and stays alive.
    //
    // RADE_U <-> RADE_L is still a destroy-and-recreate of RadeChannel
    // because the sideband flag is set on construction; the RxChannel
    // is untouched (RxChannel::setMode below will retune USB <-> LSB
    // via the wdspModeFor mapping when it fires from the
    // dspModeChanged signal).
    //
    // Reach the WdspEngine via the parent RadioModel rather than holding
    // a direct pointer on SliceModel; this keeps the construction graph
    // unchanged (slices are parented to RadioModel; see RadioModel.cpp:
    // 1374 [Phase 3R J3] new SliceModel(this)).
    if (modeChanged) {
        const auto isRade = [](DSPMode m) {
            return m == DSPMode::RADE_U || m == DSPMode::RADE_L;
        };

        // 2026-05-12 bench: clear last RADE-decoded speaker callsign
        // when leaving the *current* RADE sideband.  Two cases now
        // covered (refined from 2026-05-11 design which kept the
        // callsign sticky on U <-> L swap):
        //   1. RADE -> non-RADE: leaving RADE entirely.
        //   2. RADE_U <-> RADE_L: still in RADE, but the channel is
        //      destroyed and recreated below so the decoder state is
        //      no longer associated with the old caller's transmission.
        // Trigger: oldMode was a RADE sideband AND mode actually changed
        // (we're already inside the modeChanged guard).
        if (isRade(oldMode) && !m_lastRadeRxCallsign.isEmpty()) {
            m_lastRadeRxCallsign.clear();
            emit lastRadeRxCallsignChanged(m_lastRadeRxCallsign);
        }

        // 2026-05-12 bench: stop the idle-clear timer when leaving
        // RADE.  The clear above already happened; letting the timer
        // fire would just re-emit lastRadeRxCallsignChanged("") and
        // snrDbChanged(NaN) needlessly.  Also stop on RADE_U <-> RADE_L
        // swaps for the same reason.
        if (isRade(oldMode) && m_radeIdleClearTimer) {
            m_radeIdleClearTimer->stop();
        }

        auto* radio = qobject_cast<RadioModel*>(parent());
        if (radio != nullptr) {
            WdspEngine* engine = radio->wdspEngine();
            if (engine != nullptr) {
                const int channelId = m_sliceIndex;
                const bool oldIsRade = isRade(oldMode);
                const bool newIsRade = isRade(mode);

                auto wireAndStartRade = [&](RadeChannel* radeCh,
                                            const char* context) {
                    if (radeCh == nullptr) return;
                    radeCh->setSideband(mode == DSPMode::RADE_U);
                    radio->wireRadeChannel(channelId, radeCh, this);
                    const QString modelPath = radeModelPath();
                    if (!radeCh->start(modelPath)) {
                        qCWarning(lcDsp)
                            << "SliceModel" << m_sliceIndex
                            << context
                            << ": RadeChannel.start() failed for"
                            << modelPath
                            << "- channel-swap proceeds but RADE will"
                               " not decode";
                    }
                };

                if (oldIsRade && !newIsRade) {
                    // RADE -> any WDSP mode: tear down the RadeChannel
                    // only.  K-bench: WDSP RxChannel was running the
                    // whole time as the demod front-end; leave it
                    // alone.  WDSP-facing mode will retune from
                    // USB/LSB (the wdspModeFor mapping) to the new
                    // mode via the dspModeChanged -> rxCh->setMode
                    // path in RadioModel.cpp:5202-5206.
                    engine->destroyRadeChannel(channelId);
                } else if (!oldIsRade && newIsRade) {
                    // Any WDSP mode -> RADE: create RadeChannel
                    // alongside the still-running RxChannel.  Wire
                    // its signals into RadioModel's per-slice slot
                    // graph and start it with the configured model
                    // path.  WDSP-facing mode will map to USB/LSB
                    // via the dspModeChanged path.
                    wireAndStartRade(engine->createRadeChannel(channelId),
                                     "setDspMode(RADE)");
                } else if (oldIsRade && newIsRade) {
                    // RADE_U <-> RADE_L: destroy + recreate the
                    // RadeChannel so the sideband flag is set fresh
                    // on a clean instance.  RxChannel is untouched;
                    // the dspModeChanged path retunes it USB <-> LSB
                    // through wdspModeFor.
                    engine->destroyRadeChannel(channelId);
                    wireAndStartRade(engine->createRadeChannel(channelId),
                                     "setDspMode(RADE U<->L)");
                }
            }
        }
    }

    // Phase 3J-1 closeout Item 4 (2026-05-12): per-(band, mode) LastFilter.
    //
    // Before the mode swap, save the CURRENT filter under (currentBand,
    // OLD mode) so coming back to that mode in this band restores the
    // operator's last-set cutoffs.  Then look up the saved filter for the
    // (currentBand, NEW mode) tuple; if persisted, use it; if absent, fall
    // back to defaultFilterForMode (Thetis F5 presets).  Mirrors
    // Thetis preset[m].LastFilter (console.cs:14653-14671 [v2.10.3.13]).
    //
    // Band is computed from the slice's current frequency rather than read
    // off PanadapterModel, so SliceModel stays decoupled from the
    // panadapter (no signal subscription needed).  bandFromFrequency
    // returns the same Band PanadapterModel uses, so the keyspace is
    // shared between the panadapter band-crossing save path (RadioModel
    // signal handler) and this mode-change save path.
    // bandModePrefix() lives in the anonymous namespace later in this
    // file; reach it via a forward-declared helper to keep the source
    // order stable.  Same with AppSettings::save -- we don't call it
    // here because writes are flushed on shutdown / band-change /
    // explicit caller save; mode-change writes are best-effort and
    // shouldn't block on disk I/O.
    auto& s = AppSettings::instance();
    int low = 0, high = 0;
    if (modeChanged) {
        const Band currentBand = bandFromFrequency(m_frequency);
        // 1. Save current filter under (currentBand, OLD mode)
        const QString oldPrefix =
            QStringLiteral("Slice%1/Band%2/Mode%3/")
                .arg(m_sliceIndex)
                .arg(bandKeyName(currentBand))
                .arg(SliceModel::modeName(oldMode));
        s.setValue(oldPrefix + QStringLiteral("FilterLow"),  m_filterLow);
        s.setValue(oldPrefix + QStringLiteral("FilterHigh"), m_filterHigh);

        // 2. Restore filter for (currentBand, NEW mode); fall back to default.
        const QString newPrefix =
            QStringLiteral("Slice%1/Band%2/Mode%3/")
                .arg(m_sliceIndex)
                .arg(bandKeyName(currentBand))
                .arg(SliceModel::modeName(mode));
        if (s.contains(newPrefix + QStringLiteral("FilterLow")) &&
            s.contains(newPrefix + QStringLiteral("FilterHigh"))) {
            low  = s.value(newPrefix + QStringLiteral("FilterLow")).toInt();
            high = s.value(newPrefix + QStringLiteral("FilterHigh")).toInt();
            // Dieselbe Wache wie in restoreBandState(): ein gespeicherter
            // Durchlass, der bei LSB/USB ueber den Traeger reicht, ist
            // kein Wunsch des Bedienenden, sondern ein Rest des
            // Bandfilter-Fehlers vom 2026-09-17 -- Vorgabe statt Muell.
            if (filterCrossesCarrier(low, high, mode)) {
                qCWarning(lcDsp) << "Persisted filter" << low << high
                                 << "crosses the carrier for"
                                 << SliceModel::modeName(mode)
                                 << "-- using the mode default instead";
                auto pair = defaultFilterForMode(mode);
                low  = pair.first;
                high = pair.second;
            }
        } else {
            // From Thetis console.cs:5180-5575 — InitFilterPresets, F5 per mode
            auto pair = defaultFilterForMode(mode);
            low  = pair.first;
            high = pair.second;
        }
    } else {
        // Mode didn't actually change -- preserve current cutoffs.
        low  = m_filterLow;
        high = m_filterHigh;
    }
    bool filterChanged = (m_filterLow != low || m_filterHigh != high);
    m_filterLow = low;
    m_filterHigh = high;

    if (modeChanged) {
        emit dspModeChanged(mode);
    }
    if (filterChanged) {
        emit this->filterChanged(m_filterLow, m_filterHigh);
    }
}

// Phase 3R Task J3 - see SliceModel.h declaration for the design rationale.
QString SliceModel::radeModelPath() const
{
    auto& s = AppSettings::instance();
    const QString configured =
        s.value(QStringLiteral("Rade/ModelPath"), QString()).toString();
    if (!configured.isEmpty() && QFile::exists(configured)) {
        return configured;
    }
    // From AetherSDR RADEEngine.cpp:34 [@0cd4559] - librade convention
    // "use the built-in weights, ignore the model_file argument".
    return QStringLiteral("dummy");
}

// ---------------------------------------------------------------------------
// Bandpass filter
// ---------------------------------------------------------------------------

// Beide gehen ueber setFilter, damit die Begrenzung nicht zu umgehen
// ist. Vorher setzten sie direkt — ein zweiter Weg an der Pruefung
// vorbei ist keine Pruefung.
void SliceModel::setFilterLow(int low)
{
    setFilter(low, m_filterHigh);
}

void SliceModel::setFilterHigh(int high)
{
    setFilter(m_filterLow, high);
}

// ── Die Kanten begrenzen ─────────────────────────────────────────────
//
// From Thetis console.cs:34974-35062 [@852bf0e] —
// ConstrainFilter.
//
// Der Stempel nennt den COMMIT, nicht den describe-Text.
//
// Hier stand [v2.10.3.15-5-g852bf0e]. Das ist die Ausgabe von
// `git describe`, und der Pruefer liest daraus den TAG v2.10.3.15 —
// also einen Baum fuenf Commits vor dem, gegen den portiert wurde.
// Dort stehen an denselben Zeilennummern voellig andere Stellen
// (34994 ist im Tag `// G8NJJ update popup`), und
// verify-inline-tag-preservation meldete folgerichtig fuenf fehlende
// Autorenkuerzel, die es nie gab.
//
// Die Zeilennummern stimmen fuer 852bf0e: dort beginnt ConstrainFilter
// auf 34974. Der einzige Autorenvermerk im zitierten Bereich ist
// //MW0LGE_21k9 am SPEC-Zweig, und der steht in diesem Port weiter
// unten wortgleich.
//
// Grammatik: docs/attribution/HOW-TO-PORT.md §Inline cite versioning —
// [v<version>] fuer eine Freigabe, [@<shortsha>] sonst. Begruendung und die beiden Regeln stehen am
// Kopf der Deklaration.
bool SliceModel::constrainFilter(int& low, int& high, DSPMode mode,
                                 bool filterShift, bool limitToSidebands)
{
    const int originalLow  = low;
    const int originalHigh = high;

    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::DIGL:
    case DSPMode::CWL:
    case DSPMode::RADE_L:
        if (high > 0 && limitToSidebands) {
            if (filterShift) { low -= high; }
            high = 0;
        }
        if (low < -kMaxFilterShiftHz) {
            const int n = -kMaxFilterShiftHz - low;
            low += n;
            if (filterShift) { high += n; }
        }
        if (high > kMaxFilterShiftHz) {
            const int n = high - kMaxFilterShiftHz;
            high -= n;
            if (filterShift) { low -= n; }
        }
        break;

    case DSPMode::USB:
    case DSPMode::DIGU:
    case DSPMode::CWU:
    case DSPMode::RADE_U:
        if (low < 0 && limitToSidebands) {
            if (filterShift) { high += -low; }
            low = 0;
        }
        if (low < -kMaxFilterShiftHz) {
            const int n = -kMaxFilterShiftHz - low;
            low += n;
            if (filterShift) { high += n; }
        }
        if (high > kMaxFilterShiftHz) {
            const int n = high - kMaxFilterShiftHz;
            high -= n;
            if (filterShift) { low -= n; }
        }
        break;

    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
    case DSPMode::SPEC:   //MW0LGE_21k9
        if (low > 0 && limitToSidebands) {
            if (filterShift) { high -= low; }
            low = 0;
        }
        if (high < 0 && limitToSidebands) {
            if (filterShift) { low += -high; }
            high = 0;
        }
        if (low < -kMaxFilterShiftHz) {
            const int n = -kMaxFilterShiftHz - low;
            low += n;
            if (filterShift) { high += n; }
        }
        if (high > kMaxFilterShiftHz) {
            const int n = high - kMaxFilterShiftHz;
            high -= n;
            if (filterShift) { low -= n; }
        }
        break;

    case DSPMode::FM:
        // Bei FM gar nichts. Die Bandbreite kommt aus Hub und
        // Hoehenschnitt, nicht von Hand.
        break;

    default:
        // DRM und alles Kuenftige: nur der Deckel unten, kein
        // Seitenband-Zwang. Lieber nichts tun als etwas Falsches.
        break;
    }

    if (mode != DSPMode::FM) {
        if (low  < -kMaxFilterWidthHz) { low  = -kMaxFilterWidthHz; }
        if (low  >  kMaxFilterWidthHz) { low  =  kMaxFilterWidthHz; }
        if (high >  kMaxFilterWidthHz) { high =  kMaxFilterWidthHz; }
        if (high < -kMaxFilterWidthHz) { high = -kMaxFilterWidthHz; }
    }

    return (low != originalLow) || (high != originalHigh);
}

bool SliceModel::filterCrossesCarrier(int low, int high, DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::CWL:
    case DSPMode::DIGL:
    case DSPMode::RADE_L:
        return high > 0;
    case DSPMode::USB:
    case DSPMode::CWU:
    case DSPMode::DIGU:
    case DSPMode::RADE_U:
        return low < 0;
    default:
        return false;
    }
}

// ── Breite und Lage rechnen mit ──────────────────────────────────────
//
// Begruendung und die Regeln stehen am Kopf der Deklaration.

namespace {

// From Thetis display.cs:1023 [@852bf0e] — cw_pitch
// default 600. Seit 2026-09-27 die EINE Lesestelle: defaultFilterForMode,
// presetsForMode und commonPresetsForMode (2026-09-28 entfernt, ohne
// Aufrufer) lasen den Schluessel vorher je selbst und klemmten auf
// 100..2000 — viermal dieselbe, falsche Spanne.
// Die richtige steht bei kCwPitchMinHz/kCwPitchMaxHz im Kopf.
int currentCwPitch()
{
    auto& s = AppSettings::instance();
    return SliceModel::clampCwPitch(
        s.value(QStringLiteral("CWPitch"),
                SliceModel::kCwPitchDefaultHz).toInt());
}

// From Thetis console.cs:14636 / :14671 [@852bf0e].
// Upstream inline attribution preserved verbatim:
//   :14669  //reset preset filter's center frequency - W4TME
constexpr int kDiguClickTuneOffset = 1500;
constexpr int kDiglClickTuneOffset = 2210;

} // namespace

int SliceModel::defaultLowCut()
{
    // From Thetis console.cs:12718 [@852bf0e] —
    // default_low_cut = 150.
    auto& s = AppSettings::instance();
    int v = s.value(QStringLiteral("DefaultLowCut"), 150).toInt();
    if (v < 0)    { v = 0; }
    if (v > 1000) { v = 1000; }
    return v;
}

void SliceModel::setDefaultLowCut(int hz)
{
    AppSettings::instance().setValue(QStringLiteral("DefaultLowCut"),
                                     QString::number(qBound(0, hz, 1000)));
}

// From Thetis console.cs:35318-35348 [@852bf0e] —
// der switch am Ende von ptbFilterWidth_Scroll.
void SliceModel::widthToEdges(int widthHz, DSPMode mode, int currentCenter,
                              int& low, int& high)
{
    // From Thetis console.cs:35293 — „10 step minimum".
    if (widthHz < 10) { widthHz = 10; }

    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::RADE_L:
        high = -defaultLowCut();
        low  = high - widthHz;
        break;

    case DSPMode::USB:
    case DSPMode::RADE_U:
        low  = defaultLowCut();
        high = low + widthHz;
        break;

    case DSPMode::CWL:
    case DSPMode::CWU:
    case DSPMode::DIGL:
    case DSPMode::DIGU:
        // Mitte bleibt stehen, nur die Breite aendert sich.
        low  = currentCenter - widthHz / 2;
        high = currentCenter + widthHz / 2;
        break;

    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::FM:
    case DSPMode::DSB:
        // ±Breite, NICHT ±Breite/2: bei diesen vier ist die angezeigte
        // Breite die halbe (console.cs:35225 rechnet vorher `bw /= 2`).
        low  = currentCenter - widthHz;
        high = currentCenter + widthHz;
        break;

    default:
        // SPEC, DRM und alles Kuenftige: symmetrisch, das ist die
        // harmloseste Annahme.
        low  = currentCenter - widthHz / 2;
        high = currentCenter + widthHz / 2;
        break;
    }
}

// From Thetis console.cs:35076-35097 [@852bf0e] —
// die default_center-Berechnung aus ptbFilterShift_Scroll.
int SliceModel::cwPitchHz()
{
    return currentCwPitch();
}

int SliceModel::fmDeviationHz()
{
    const int hz = AppSettings::instance()
        .value(QStringLiteral("FmDeviationHz"), 5000).toInt();
    return hz > 0 ? hz : 5000;
}

int SliceModel::fmHalfBandwidthHz()
{
    // From Thetis Console/radio.cs:1571 [@852bf0e] -- rx_fm_highcut = 3000.0
    constexpr int kRxFmHighCutHz = 3000;
    return fmDeviationHz() + kRxFmHighCutHz;
}

int SliceModel::clampCwPitch(int hz)
{
    // Die Spanne der beiden Thetis-Felder (kCwPitchMinHz..kCwPitchMaxHz,
    // Herkunft im Kopf). Der Setter selbst klemmt nur nach unten
    // (console.cs:18150, `if (cw_pitch <= 0) cw_pitch = 0;  //-W2PA`);
    // was ueber die Felder oder CAT kommt, ist schon in der Spanne —
    // RadioModel::setCwPitch klemmt beim Setzen wie CATCWPitch.
    return std::clamp(hz, kCwPitchMinHz, kCwPitchMaxHz);
}

// ── Ein CW-Durchlass folgt der Tonhoehe ──────────────────────────────
//
// Porting from Thetis console.cs:18142-18242 [@852bf0e] — public int
// CWPitch, set. Der Setter macht der Reihe nach:
//   1. cw_pitch setzen, nach unten auf 0 klemmen (//-W2PA),
//      udCWPitch.Value, Display.CWPitch und
//      NetworkIO.SetCWSidetoneFreq(cw_pitch) nachziehen;
//   2. fuer JEDEN Platz F1..NONE (also auch VAR1/VAR2) die CWL- und
//      CWU-Vorgabe von rx1_filters UND rx2_filters auf die Tonhoehe
//      setzen, Breite behalten, an der Spiegelgrenze rutschen — das
//      hier;
//   3. steht RX1 in CWL/CWU: bei MOX VFO A (und B bei Split) um die
//      Differenz schieben, sonst die VFO-Texte neu auswerten
//      (txtVFOAFreq_LostFocus, also neu abstimmen), dann RX1Filter und
//      RX2Filter neu anwenden — die Vorgabe, die gerade in 2. verschoben
//      wurde;
//   4. APF-Mitte fuer RX1/RX1sub/RX2 neu setzen (SetupForm.RX1APFFreq …,
//      das landet in tbRX1APFTune_Scroll: RXAPFFreq = CWPitch + Tune);
//   5. CWPitchChangedHandlers, nur wenn sich der Wert geaendert hat.
// Wo was bei uns liegt: RadioModel::setCwPitch.
//
// Upstream-Kommentar vor der Schleife, wortgetreu (console.cs:18155-18159):
//
//   //-W2PA June 2017
//   //      This centers the passband of the CW filters on the pitch frequency, but if CWPitch setter is called by mode buttons,
//   //      it prevents filter setting from persisting when the mode changes or band changes, since band changes trigger mode changes.
//   //      This happened because of a line:  CWPitch = cw_pitch;  in SetRX1Mode and SetRX2Mode.
//   //      Those are now commented out. This should only be called by the CW Pitch control in the UI and Setup, or by a CAT command.
//
// Bei uns genauso: nur RadioModel::setCwPitch ruft das auf (aus dem
// Setup-Feld), nie ein Betriebsartwechsel.
//
// `bw / 2` ist Ganzzahlteilung: eine ungerade Breite verliert dabei 1 Hz.
// Das ist Thetis, und so bleibt es.
void SliceModel::centreCwFilterOnPitch(int& low, int& high, DSPMode mode,
                                       int pitchHz)
{
    // From Thetis console.cs:18162-18195 [@852bf0e] — Schleifenkoerper
    // des CWPitch-Setters (W2PA, siehe oben), je Betriebsart eine Haelfte.
    const int bw = high - low;
    switch (mode) {
    case DSPMode::CWL:
        // Adjust CWL filters
        low  = -pitchHz - bw / 2;
        high = -pitchHz + bw / 2;
        if (high > 0) { // stop shifting the passband when it hits the image limit, while allowing pitch to continue to decrease
            low -= high;  // slide the passband down to put its edge at zero
            high = 0;
        }
        // n6vl  [original inline comment from console.cs:18178, auf
        // rx2_filters[CWL].SetFilter — die zweite Tabelle bekommt
        // dieselben Werte; bei uns teilen sich alle Scheiben eine]
        break;
    case DSPMode::CWU:
        // Adjust CWU filters
        low  = pitchHz - bw / 2;
        high = pitchHz + bw / 2;
        if (low < 0) { // stop adjusting the passband when it hits the image limit, while allowing pitch to continue to decrease
            high -= low;  // slide the passband up to put its edge at zero
            low = 0;
        }
        break;
    default:
        break;
    }
}

// Schritt 2 (VAR1/VAR2) und Schritt 3 (RX1Filter = rx1_filter) des
// Setters fuer eine Scheibe.
//
// Schritt 3 wendet in Thetis den gewaehlten PLATZ neu an, dessen Werte
// Schritt 2 gerade verschoben hat. Wir fuehren keinen gewaehlten Platz
// (RxApplet leitet die Hervorhebung aus den Werten ab), also wird der
// laufende Durchlass selbst verschoben — mit derselben Rechnung. Das
// Ergebnis ist dasselbe: auch ein von Hand gezogener Durchlass liegt in
// Thetis auf VAR1 (SelectRX1VarFilter) und wird von der Schleife
// mitgenommen.
//
// Abweichung: Thetis wendet RX2Filter nur an, wenn RX1 in CW steht
// (der switch fragt _rx1_dsp_mode). Hier folgt jede Scheibe, die selbst
// in CW steht — die Scheiben sind unabhaengig, und eine CW-Scheibe mit
// einem Durchlass neben dem Ton waere der Fehler, den der Setter
// verhindern soll.
void SliceModel::followCwPitch(int pitchHz)
{
    for (int slot = 0; slot < kVarSlots; ++slot) {
        for (DSPMode m : {DSPMode::CWL, DSPMode::CWU}) {
            auto it = m_varFilters[slot].find(static_cast<int>(m));
            if (it == m_varFilters[slot].end()) { continue; }
            int low  = it->first;
            int high = it->second;
            centreCwFilterOnPitch(low, high, m, pitchHz);
            *it = qMakePair(low, high);
        }
    }

    if (m_dspMode == DSPMode::CWL || m_dspMode == DSPMode::CWU) {
        int low  = m_filterLow;
        int high = m_filterHigh;
        centreCwFilterOnPitch(low, high, m_dspMode, pitchHz);
        setFilter(low, high);   // geht durch die Begrenzung
    }
}

// Die gespeicherten CW-Durchlaesse anderer Baender. Thetis braucht das
// nicht: ein Band merkt sich dort den Platz (preset[m].LastFilter), und
// die Werte des Platzes hat die Schleife schon verschoben. Wir merken
// uns je (Scheibe, Band, Betriebsart) die Kanten selbst — ohne diesen
// Schritt kaeme nach dem naechsten Bandwechsel der alte Durchlass neben
// dem neuen Ton zurueck.
//
// Zwei Schluesselformen, beide aus saveToSettings:
//   Slice<n>/Band<b>/ModeCWL/FilterLow|High   (seit Phase 3J-1 Item 4)
//   Slice<n>/Band<b>/FilterLow|High           (alt; gehoert zur Betriebsart
//                                              in Slice<n>/Band<b>/DspMode)
void SliceModel::recentreStoredCwFilters(int pitchHz)
{
    static const QRegularExpression kModeKey(
        QStringLiteral("^(Slice\\d+/Band[^/]+/Mode(CWL|CWU)/)FilterLow$"));
    static const QRegularExpression kBandKey(
        QStringLiteral("^(Slice\\d+/Band[^/]+/)FilterLow$"));

    auto& s = AppSettings::instance();
    const QStringList keys = s.allKeys();
    for (const QString& lowKey : keys) {
        QString prefix;
        DSPMode mode = DSPMode::CWU;
        if (const QRegularExpressionMatch m = kModeKey.match(lowKey); m.hasMatch()) {
            prefix = m.captured(1);
            mode   = modeFromName(m.captured(2));
        } else if (const QRegularExpressionMatch b = kBandKey.match(lowKey); b.hasMatch()) {
            prefix = b.captured(1);
            const QString modeKey = prefix + QStringLiteral("DspMode");
            if (!s.contains(modeKey)) { continue; }
            const int raw = s.value(modeKey).toInt();
            if (raw != static_cast<int>(DSPMode::CWL)
                && raw != static_cast<int>(DSPMode::CWU)) {
                continue;
            }
            mode = static_cast<DSPMode>(raw);
        } else {
            continue;
        }

        const QString highKey = prefix + QStringLiteral("FilterHigh");
        if (!s.contains(highKey)) { continue; }
        int low  = s.value(lowKey).toInt();
        int high = s.value(highKey).toInt();
        centreCwFilterOnPitch(low, high, mode, pitchHz);
        s.setValue(lowKey,  low);
        s.setValue(highKey, high);
    }
}

int SliceModel::defaultFilterCenter(DSPMode mode, int widthHz)
{
    switch (mode) {
    case DSPMode::USB:
    case DSPMode::RADE_U:
        return defaultLowCut() + widthHz / 2;
    case DSPMode::LSB:
    case DSPMode::RADE_L:
        return -defaultLowCut() - widthHz / 2;
    case DSPMode::CWU:
        return currentCwPitch();
    case DSPMode::CWL:
        return -currentCwPitch();
    case DSPMode::DIGU:
        return kDiguClickTuneOffset;
    case DSPMode::DIGL:
        return -kDiglClickTuneOffset;
    default:
        // AM, SAM, FM, DSB und der Rest sitzen um null.
        return 0;
    }
}

// ── Der ganze Durchlass zurueck auf die Vorgabe ──────────────────────
//
// Der Betreiber am 2026-08-23, nachdem auf 40 m nichts zu verstehen
// war: "es funktioniert, die bandweite war komplett falsch, start bei
// 2000."
//
// Wie er dort hingekommen ist, laesst sich nicht mehr feststellen —
// wahrscheinlich durch ein unabsichtliches Ziehen an der Filterkante
// im Panadapter, die seit dieser Woche ziehbar ist. Das geschieht
// lautlos: es gibt keine Meldung, und ein Durchlass von 2000 bis 2800
// sieht auf dem Bild nicht falsch aus, er klingt nur so.
//
// Der Rueckstellknopf half nicht, denn er rief resetFilterCenter() —
// der ZENTRIERT und behaelt die Breite. Wer eine kaputte BREITE hat,
// kommt damit nicht heraus.
//
// resetFilter() stellt beides her: die uebliche Breite der
// Betriebsart und deren Vorgabelage. Die Breiten stammen aus Thetis'
// Voreinstellungen (console.cs setupFilters) und sind genau die, die
// ein Funker erwartet, wenn er "zurueck auf Anfang" drueckt.
void SliceModel::resetFilter()
{
    int width = 2800;   // SSB
    switch (m_dspMode) {
    case DSPMode::CWL:
    case DSPMode::CWU:
        width = 500;
        break;
    case DSPMode::DIGL:
    case DSPMode::DIGU:
        width = 3000;
        break;
    case DSPMode::AM:
    case DSPMode::SAM:
    case DSPMode::DSB:
        width = 6000;
        break;
    case DSPMode::FM:
        width = 12000;
        break;
    default:
        break;   // LSB, USB, RADE_*, SPEC, DRM: 2800
    }

    int low = 0;
    int high = 0;
    widthToEdges(width, m_dspMode, defaultFilterCenter(m_dspMode, width),
                 low, high);
    setFilter(low, high);
}

// ── VAR1 und VAR2 ────────────────────────────────────────────────────
//
// From Thetis console.cs:7237-7249 [@852bf0e]. Begruendung
// steht am Kopf der Deklaration.

void SliceModel::storeVarFilter(int slot)
{
    if (slot < 0 || slot >= kVarSlots) { return; }
    // Je Betriebsart getrennt: ein Handdurchlass fuer CW hat in SSB
    // nichts verloren — dort waere er nicht einmal erlaubt.
    m_varFilters[slot].insert(static_cast<int>(m_dspMode),
                              qMakePair(m_filterLow, m_filterHigh));
}

void SliceModel::recallVarFilter(int slot)
{
    if (!hasVarFilter(slot)) { return; }
    const QPair<int,int> v = m_varFilters[slot]
        .value(static_cast<int>(m_dspMode));
    setFilter(v.first, v.second);   // geht durch die Begrenzung
}

bool SliceModel::hasVarFilter(int slot) const
{
    if (slot < 0 || slot >= kVarSlots) { return false; }
    const QPair<int,int> v = m_varFilters[slot]
        .value(static_cast<int>(m_dspMode), qMakePair(0, 0));
    // Ein Durchlass ohne Breite ist kein Durchlass — so unterscheidet
    // sich „leer" von „zufaellig 0/0 gespeichert".
    return v.first != v.second;
}

QPair<int,int> SliceModel::varFilter(int slot) const
{
    if (slot < 0 || slot >= kVarSlots) { return qMakePair(0, 0); }
    return m_varFilters[slot].value(static_cast<int>(m_dspMode),
                                    qMakePair(0, 0));
}

void SliceModel::setFilterWidth(int widthHz)
{
    int low = 0, high = 0;
    widthToEdges(widthHz, m_dspMode, filterCenter(), low, high);
    setFilter(low, high);   // geht durch die Begrenzung
}

void SliceModel::setFilterCenter(int centerHz)
{
    // Die Breite bleibt — das ist der Unterschied zum Ziehen einer
    // Kante. Deshalb hier constrainFilter mit filterShift == true:
    // stoesst eine Kante an, wandert die andere mit.
    const int bw = filterWidth();
    int low  = centerHz - bw / 2;
    int high = low + bw;

    constrainFilter(low, high, m_dspMode, /*filterShift=*/true,
                    m_limitFiltersToSidebands);
    if (low == high) { return; }

    if (m_filterLow != low || m_filterHigh != high) {
        m_filterLow  = low;
        m_filterHigh = high;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

void SliceModel::resetFilterCenter()
{
    setFilterCenter(defaultFilterCenter(m_dspMode, filterWidth()));
}

void SliceModel::setLimitFiltersToSidebands(bool on)
{
    if (m_limitFiltersToSidebands == on) { return; }
    m_limitFiltersToSidebands = on;

    // Sofort anwenden, nicht erst beim naechsten Verstellen. Ein
    // Schalter, der erst wirkt, wenn man etwas anderes anfasst, wirkt
    // fuer den Bedienenden gar nicht.
    setFilter(m_filterLow, m_filterHigh);
}

// ── Von Hand verstellt? Dann nach VAR1 ───────────────────────────────
//
// From Thetis console.cs:7237 — SelectRX1VarFilter. Dort springt die
// Auswahl beim ersten Verstellen auf VAR1, und der benannte Filter
// bleibt unberuehrt.
//
// Bei uns gibt es keine Auswahl-Marke (die Knoepfe leiten ihre
// Hervorhebung aus den Werten ab, RxApplet::updateFilterButtons), also
// bleibt der nuetzliche Teil: die Handeinstellung AUFHEBEN, damit sie
// nach einem Klick auf „2.4k" nicht verloren ist.
//
// Warum ein eigener Weg und nicht in setFilter: setFilter laeuft auch,
// wenn ein GESPEICHERTER Filter angewandt wird. Wuerde jeder Aufruf
// nach VAR1 schreiben, waere VAR1 nach dem ersten Knopfdruck genau der
// Knopf — und damit wertlos.
void SliceModel::setFilterByHand(int low, int high)
{
    setFilter(low, high);
    storeVarFilter(0);
}

void SliceModel::setFilter(int low, int high)
{
    // DER EINE TRICHTER. Thetis begrenzt in UpdateRX1Filters
    // (console.cs:7510), also an der Stelle, durch die jede
    // Filteraenderung muss — Knopf, Ziehen, CAT, gespeicherter Filter.
    // Hier ist unsere.
    constrainFilter(low, high, m_dspMode, /*filterShift=*/false,
                    m_limitFiltersToSidebands);

    // From Thetis console.cs:7512 [@852bf0e]:
    //   if (low == high) return; // not a good idea to have a 0hz width filter
    // Ein Filter ohne Breite ist Stille, und Stille sieht aus wie ein
    // kaputter Empfaenger.
    if (low == high) { return; }

    if (m_filterLow != low || m_filterHigh != high) {
        m_filterLow = low;
        m_filterHigh = high;
        emit filterChanged(m_filterLow, m_filterHigh);
    }
}

// ---------------------------------------------------------------------------
// AGC
// ---------------------------------------------------------------------------

void SliceModel::setAgcMode(AGCMode mode)
{
    if (m_agcMode != mode) {
        m_agcMode = mode;
        emit agcModeChanged(mode);
    }
}

// ---------------------------------------------------------------------------
// Tuning step
// ---------------------------------------------------------------------------

void SliceModel::setStepHz(int hz)
{
    if (m_stepHz != hz && hz > 0) {
        m_stepHz = hz;
        emit stepHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Gains
// ---------------------------------------------------------------------------

void SliceModel::setAfGain(int gain)
{
    gain = std::clamp(gain, 0, 100);
    if (m_afGain != gain) {
        m_afGain = gain;
        emit afGainChanged(gain);
    }
}

void SliceModel::setRfGain(int gain)
{
    // This is the WDSP AGC top / max gain (RadioModel feeds it to
    // RxChannel::setAgcTop -> SetRXAAGCTop). From Thetis
    // console.designer.cs:3708-3709 [v2.10.3.15]: ptbRF.Minimum = -20,
    // ptbRF.Maximum = 120 -- the same bounds TCIServer.cs handleAgcGain
    // clamps to and RxChannel::readBackAgcTop already applies. The earlier
    // 0..100 here was a Longpath-original guess that silently narrowed
    // both the TCI agc_gain path and the AGC-threshold readback mirror.
    gain = std::clamp(gain, -20, 120);
    if (m_rfGain != gain) {
        m_rfGain = gain;
        emit rfGainChanged(gain);
    }
}

// ---------------------------------------------------------------------------
// Antenna selection
// ---------------------------------------------------------------------------

void SliceModel::setRxAntenna(const QString& ant)
{
    if (m_rxAntenna != ant) {
        m_rxAntenna = ant;
        emit rxAntennaChanged(ant);
    }
}

void SliceModel::setTxAntenna(const QString& ant)
{
    if (m_txAntenna != ant) {
        m_txAntenna = ant;
        emit txAntennaChanged(ant);
    }
}

// Phase 3P-I-a T13 — reverse sync: AlexController write → slice cache refresh.
// Reads the current per-band RX and TX antenna values from AlexController
// and refreshes the slice's cached m_rxAntenna / m_txAntenna via the public
// setters so rxAntennaChanged / txAntennaChanged signals fire to VFO Flag
// and RxApplet. The write-back to AlexController via T12's RadioModel handler
// is idempotent — AlexController::setRxAnt/setTxAnt returns early (without
// emitting antennaChanged) when the stored value equals the new value
// (AlexController.cpp:95,107), so no signal loop occurs.
//
// Issue #257: when AlexController::rxOnlyAnt(band) != 0 the radio is
// actually routing through the rx-only mux (EXT1 / EXT2 / BYPS / XVTR
// depending on SKU). The cached m_rxAntenna label must reflect that or
// the user sees "ANT1" while the radio is on EXT1 — and the next pick of
// "ANT1" in the popup gets no-op'd by setRxAntenna's equality guard,
// trapping the user on the bypass path. With the SkuUiProfile in hand
// we resolve rxOnlyAnt (1..3) to the per-SKU label trio (BYPS/EXT1/XVTR
// for ANAN-7000D, EXT2/EXT1/XVTR for ANAN-100D, etc).
void SliceModel::refreshAntennasFromAlex(const AlexController& alex,
                                         Band band,
                                         const SkuUiProfile* sku)
{
    const int rxOnly = alex.rxOnlyAnt(band);  // 0=none, 1/2/3 indexed
    const int rx     = alex.rxAnt(band);      // 1..3
    const int tx     = alex.txAnt(band);      // 1..3
    auto name = [](int n) {
        switch (n) {
            case 2:  return QStringLiteral("ANT2");
            case 3:  return QStringLiteral("ANT3");
            default: return QStringLiteral("ANT1");
        }
    };

    // Issue #257: prefer the SKU-specific RX-only label when the bypass mux
    // is engaged. Fall back to the ANT* label when sku is null, when the
    // mux is disengaged (rxOnly == 0), or when the indexed slot in the SKU
    // is empty (defensive — should never happen because rxOnlyLabels is
    // always-3 in SkuUiProfile.h).
    QString rxLabel;
    if (sku && rxOnly >= 1 && rxOnly <= 3) {
        const QString& slot = sku->rxOnlyLabels[static_cast<size_t>(rxOnly - 1)];
        rxLabel = slot.isEmpty() ? name(rx) : slot;
    } else {
        rxLabel = name(rx);
    }

    // Use the public setters so rxAntennaChanged / txAntennaChanged
    // signals fire — VFO Flag and RxApplet listen. The loop back to
    // AlexController via T12's handler is idempotent: AlexController's
    // setRxAnt/setTxAnt returns early on equal value, so no signal is
    // emitted.
    setRxAntenna(rxLabel);
    setTxAntenna(name(tx));
}

// ---------------------------------------------------------------------------
// Slice state
// ---------------------------------------------------------------------------

void SliceModel::setActive(bool active)
{
    if (m_active != active) {
        m_active = active;
        emit activeChanged(active);
    }
}

void SliceModel::setTxSlice(bool tx)
{
    if (m_txSlice != tx) {
        m_txSlice = tx;
        emit txSliceChanged(tx);
    }
}

// ── Phase 3F Sub-Epic A: multi-panadapter / multi-slice identity ────────────

void SliceModel::setChainIndex(int idx)
{
    if (m_chainIndex != idx) {
        m_chainIndex = idx;
        emit chainIndexChanged(idx);
    }
}

void SliceModel::setDdcIndex(int ddc)
{
    if (m_ddcIndex != ddc) {
        m_ddcIndex = ddc;
        emit ddcIndexChanged(ddc);
    }
}

void SliceModel::setStreamIndex(int idx)
{
    if (m_streamIndex != idx) {
        m_streamIndex = idx;
        emit streamIndexChanged(idx);
    }
}

void SliceModel::setShiftOffsetHz(double hz)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_shiftOffsetHz - hz)) {
        return;
    }
    m_shiftOffsetHz = hz;
    emit shiftOffsetHzChanged(hz);
}

void SliceModel::setPanKey(const QString& key)
{
    if (m_panKey != key) {
        m_panKey = key;
        emit panKeyChanged(key);
    }
}

void SliceModel::setSampleRateHz(int hz)
{
    if (m_sampleRateHz != hz) {
        m_sampleRateHz = hz;
        emit sampleRateHzChanged(hz);
    }
}

void SliceModel::setDiversityEnabled(bool on)
{
    if (m_diversityEnabled != on) {
        m_diversityEnabled = on;
        emit diversityEnabledChanged(on);
    }
}

// ── Phase 3F Sub-Epic G Task 2: per-band diversity tuning setters ────────────
//
// Behaviour mirrors the rest of the Sub-Epic A setters: emit-on-change so the
// future DiversityDialog (T8-T10) and the RadioModel signal wire (T13) only
// fire downstream work when the value actually moves. Domain clamping is the
// caller's responsibility for now; the spinner ranges in DiversityDialog
// will enforce 0..360 / -20..+20 at the UI edge.

void SliceModel::setDiversityPhaseDeg(double deg)
{
    if (m_diversityPhaseDeg != deg) {
        m_diversityPhaseDeg = deg;
        emit diversityPhaseDegChanged(deg);
    }
}

void SliceModel::setDiversityGainDb(double db)
{
    if (m_diversityGainDb != db) {
        m_diversityGainDb = db;
        emit diversityGainDbChanged(db);
    }
}

void SliceModel::setDiversityFineNullEnabled(bool on)
{
    if (m_diversityFineNullEnabled != on) {
        m_diversityFineNullEnabled = on;
        emit diversityFineNullEnabledChanged(on);
    }
}

void SliceModel::setWidebandExtensionRequested(bool on)
{
    if (m_widebandExtensionRequested != on) {
        m_widebandExtensionRequested = on;
        emit widebandExtensionRequestedChanged(on);
    }
}

void SliceModel::setPsPaused(bool paused)
{
    if (m_psPaused != paused) {
        m_psPaused = paused;
        emit psPausedChanged(paused);
    }
}

// ── Phase 3G-10 Stage 1 stubs (DSP state, Stage 2 wires to RxChannel) ──

void SliceModel::setLocked(bool v)
{
    if (m_locked != v) {
        m_locked = v;
        emit lockedChanged(v);
    }
}

void SliceModel::setMuted(bool v)
{
    if (m_muted != v) {
        m_muted = v;
        emit mutedChanged(v);
    }
}

void SliceModel::setAudioPan(double pan)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_audioPan - pan)) {
        return;
    }
    m_audioPan = pan;
    emit audioPanChanged(pan);
}

void SliceModel::setSsqlEnabled(bool v)
{
    if (m_ssqlEnabled != v) {
        m_ssqlEnabled = v;
        emit ssqlEnabledChanged(v);
    }
}

void SliceModel::setSsqlThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_ssqlThresh - dB)) {
        return;
    }
    m_ssqlThresh = dB;
    emit ssqlThreshChanged(dB);
}

void SliceModel::setAmsqEnabled(bool v)
{
    if (m_amsqEnabled != v) {
        m_amsqEnabled = v;
        emit amsqEnabledChanged(v);
    }
}

void SliceModel::setAmsqThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_amsqThresh - dB)) {
        return;
    }
    m_amsqThresh = dB;
    emit amsqThreshChanged(dB);
}

void SliceModel::setFmsqEnabled(bool v)
{
    if (m_fmsqEnabled != v) {
        m_fmsqEnabled = v;
        emit fmsqEnabledChanged(v);
    }
}

void SliceModel::setFmsqThresh(double dB)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmsqThresh - dB)) {
        return;
    }
    m_fmsqThresh = dB;
    emit fmsqThreshChanged(dB);
}

void SliceModel::setAgcThreshold(int dBu)
{
    // RxChannel::setAgcThreshold feeds this straight into WDSP's
    // SetRXAAGCThresh, which computes max_gain via
    // out_target / (var_gain * pow(10, (thresh+noise_offset)/20))
    // (wcpAGC.c:504-515) -- an extreme thresh drives pow(10, x/20)
    // toward overflow/underflow, producing an Inf/0/NaN max_gain that
    // propagates through the whole AGC chain.
    //
    // From Thetis console.cs:45969-45970 [v2.10.3.13] -- clamp [-160, +2],
    // already cited/ported at RadioModel.cpp's auto-AGC noise-floor calc
    // and matching RxChannel::readBackAgcThresh's own -160 floor
    // (console.cs:50345, "[2.10.3.6]MW0LGE changed from -143") and the
    // RxApplet AGC-T slider range (-160..0, console.cs:45977). An
    // earlier version of this clamp used an invented +-100 defensive
    // bound that was narrower than this real range on the low end --
    // caught live because it silently reclamped legitimate threshold
    // values the auto-AGC path and RF-Gain sync routinely compute below
    // -100, desyncing the model/UI from what WDSP was actually applying.
    static constexpr int kAgcThresholdMin = -160;
    static constexpr int kAgcThresholdMax = 2;
    const int clamped = qBound(kAgcThresholdMin, dBu, kAgcThresholdMax);
    if (m_agcThreshold != clamped) {
        m_agcThreshold = clamped;
        emit agcThresholdChanged(clamped);
    }
}

void SliceModel::setAgcHang(int ms)
{
    // From Thetis setup.designer.cs udDSPAGCHangTime.Minimum/Maximum
    // [v2.10.3.15]: 10..5000 ms.
    static constexpr int kAgcHangMin = 10;
    static constexpr int kAgcHangMax = 5000;
    const int clamped = qBound(kAgcHangMin, ms, kAgcHangMax);
    if (m_agcHang != clamped) {
        m_agcHang = clamped;
        emit agcHangChanged(clamped);
    }
}

void SliceModel::setAgcSlope(int dB)
{
    // From Thetis setup.designer.cs udDSPAGCSlope.Minimum/Maximum
    // [v2.10.3.15]: 0..20 dB.
    static constexpr int kAgcSlopeMin = 0;
    static constexpr int kAgcSlopeMax = 20;
    const int clamped = qBound(kAgcSlopeMin, dB, kAgcSlopeMax);
    if (m_agcSlope != clamped) {
        m_agcSlope = clamped;
        emit agcSlopeChanged(clamped);
    }
}

void SliceModel::setAgcAttack(int ms)
{
    // RxChannel::setAgcAttack's own port comment notes Thetis declares
    // SetRXAAGCAttack (dsp.cs:116-117) but has "no explicit radio.cs
    // call site (disabled in UI)" -- Thetis never exposes this as a
    // bounded control in practice. WDSP's SetRXAAGCAttack (wcpAGC.c:418)
    // does the same ms/1000.0 -> tau_attack conversion as the
    // Thetis-bounded Decay setter below, so this mirrors Decay's cited
    // 1..5000 ms range rather than inventing an unrelated number.
    static constexpr int kAgcAttackMin = 1;
    static constexpr int kAgcAttackMax = 5000;
    const int clamped = qBound(kAgcAttackMin, ms, kAgcAttackMax);
    if (m_agcAttack != clamped) {
        m_agcAttack = clamped;
        emit agcAttackChanged(clamped);
    }
}

void SliceModel::setAgcDecay(int ms)
{
    // From Thetis setup.designer.cs udDSPAGCDecay.Minimum/Maximum
    // [v2.10.3.15]: 1..5000 ms.
    static constexpr int kAgcDecayMin = 1;
    static constexpr int kAgcDecayMax = 5000;
    const int clamped = qBound(kAgcDecayMin, ms, kAgcDecayMax);
    if (m_agcDecay != clamped) {
        m_agcDecay = clamped;
        emit agcDecayChanged(clamped);
    }
}

void SliceModel::setAutoAgcEnabled(bool on)
{
    if (m_autoAgcEnabled != on) {
        m_autoAgcEnabled = on;
        emit autoAgcEnabledChanged(on);
    }
}

void SliceModel::setAutoAgcOffset(double dB)
{
    if (!qFuzzyCompare(m_autoAgcOffset, dB)) {
        m_autoAgcOffset = dB;
        emit autoAgcOffsetChanged(dB);
    }
}

void SliceModel::setAgcFixedGain(int dB)
{
    // From Thetis setup.designer.cs udDSPAGCFixedGaindB.Minimum/Maximum
    // [v2.10.3.15]: -20..120 dB (Minimum decimal encodes the sign via
    // its 4th int component, 0x80000000).
    static constexpr int kAgcFixedGainMin = -20;
    static constexpr int kAgcFixedGainMax = 120;
    const int clamped = qBound(kAgcFixedGainMin, dB, kAgcFixedGainMax);
    if (m_agcFixedGain != clamped) {
        m_agcFixedGain = clamped;
        emit agcFixedGainChanged(clamped);
    }
}

void SliceModel::setAgcHangThreshold(int val)
{
    // From Thetis setup.designer.cs tbDSPAGCHangThreshold.Maximum
    // [v2.10.3.15]: 100 (TrackBar; Minimum left at the WinForms
    // TrackBar default of 0 -- no explicit override in the designer
    // file).
    static constexpr int kAgcHangThresholdMin = 0;
    static constexpr int kAgcHangThresholdMax = 100;
    const int clamped = qBound(kAgcHangThresholdMin, val, kAgcHangThresholdMax);
    if (m_agcHangThreshold != clamped) {
        m_agcHangThreshold = clamped;
        emit agcHangThresholdChanged(clamped);
    }
}

void SliceModel::setAgcMaxGain(int dB)
{
    // From Thetis setup.designer.cs udDSPAGCMaxGaindB.Minimum/Maximum
    // [v2.10.3.15]: -20..120 dB (same encoding note as FixedGain above).
    static constexpr int kAgcMaxGainMin = -20;
    static constexpr int kAgcMaxGainMax = 120;
    const int clamped = qBound(kAgcMaxGainMin, dB, kAgcMaxGainMax);
    if (m_agcMaxGain != clamped) {
        m_agcMaxGain = clamped;
        emit agcMaxGainChanged(clamped);
    }
}

void SliceModel::setRitEnabled(bool v)
{
    if (m_ritEnabled != v) {
        m_ritEnabled = v;
        emit ritEnabledChanged(v);
    }
}

void SliceModel::setRitHz(int hz)
{
    if (m_ritHz != hz) {
        m_ritHz = hz;
        emit ritHzChanged(hz);
    }
}

void SliceModel::setXitEnabled(bool v)
{
    if (m_xitEnabled != v) {
        m_xitEnabled = v;
        emit xitEnabledChanged(v);
    }
}

void SliceModel::setXitHz(int hz)
{
    if (m_xitHz != hz) {
        m_xitHz = hz;
        emit xitHzChanged(hz);
    }
}

void SliceModel::setNbMode(Longpath::NbMode v)
{
    if (v == m_nbMode) { return; }
    m_nbMode = v;
    emit nbModeChanged(v);
}

// setNbTuning / nbTuningChanged removed 2026-04-22 — per-slice NB tuning is
// not a Thetis concept. All NB tuning is global per DSPRX and lives inside
// NbFamily, seeded from Setup → DSP → NB/SNB. See SliceModel.h.

// --- NR setters (Sub-epic C-1) ---
// See Thetis console.cs:43297-43450 SelectNR() [v2.10.3.13].

void SliceModel::setActiveNr(Longpath::NrSlot slot)
{
    if (m_activeNr == slot) { return; }
    m_activeNr = slot;
    emit activeNrChanged(slot);
}

// NR1
void SliceModel::setNr4Position(Longpath::NrPosition p)
{
    if (m_nr4Position == p) { return; }
    m_nr4Position = p;
    emit nr4PositionChanged(p);
}

// ANF — same shape as the NR1 setters below. Kept adjacent to them on
// purpose: they are one algorithm in two roles, and a change to one is
// almost always a change to both.
void SliceModel::setAnfTaps(int v)
{
    if (m_anfTaps == v) { return; }
    m_anfTaps = v;
    emit anfTapsChanged(v);
}

void SliceModel::setAnfDelay(int v)
{
    if (m_anfDelay == v) { return; }
    m_anfDelay = v;
    emit anfDelayChanged(v);
}

void SliceModel::setAnfGain(double v)
{
    if (qFuzzyCompare(m_anfGain, v)) { return; }
    m_anfGain = v;
    emit anfGainChanged(v);
}

void SliceModel::setAnfLeakage(double v)
{
    if (qFuzzyCompare(m_anfLeakage, v)) { return; }
    m_anfLeakage = v;
    emit anfLeakageChanged(v);
}

void SliceModel::setAnfPosition(Longpath::NrPosition p)
{
    if (m_anfPosition == p) { return; }
    m_anfPosition = p;
    emit anfPositionChanged(p);
}

void SliceModel::setNr1Taps(int v)
{
    if (m_nr1Taps == v) { return; }
    m_nr1Taps = v;
    emit nr1TapsChanged(v);
}
void SliceModel::setNr1Delay(int v)
{
    if (m_nr1Delay == v) { return; }
    m_nr1Delay = v;
    emit nr1DelayChanged(v);
}
void SliceModel::setNr1Gain(double v)
{
    if (qFuzzyCompare(m_nr1Gain, v)) { return; }
    m_nr1Gain = v;
    emit nr1GainChanged(v);
}
void SliceModel::setNr1Leakage(double v)
{
    if (qFuzzyCompare(m_nr1Leakage, v)) { return; }
    m_nr1Leakage = v;
    emit nr1LeakageChanged(v);
}
void SliceModel::setNr1Position(Longpath::NrPosition p)
{
    if (m_nr1Position == p) { return; }
    m_nr1Position = p;
    emit nr1PositionChanged(p);
}

// NR2
void SliceModel::setNr2GainMethod(Longpath::EmnrGainMethod v)
{
    if (m_nr2GainMethod == v) { return; }
    m_nr2GainMethod = v;
    emit nr2GainMethodChanged(v);
}
void SliceModel::setNr2NpeMethod(Longpath::EmnrNpeMethod v)
{
    if (m_nr2NpeMethod == v) { return; }
    m_nr2NpeMethod = v;
    emit nr2NpeMethodChanged(v);
}
void SliceModel::setNr2TrainT1(double v)
{
    if (qFuzzyCompare(m_nr2TrainT1, v)) { return; }
    m_nr2TrainT1 = v;
    emit nr2TrainT1Changed(v);
}
void SliceModel::setNr2TrainT2(double v)
{
    if (qFuzzyCompare(m_nr2TrainT2, v)) { return; }
    m_nr2TrainT2 = v;
    emit nr2TrainT2Changed(v);
}
void SliceModel::setNr2AeFilter(bool v)
{
    if (m_nr2AeFilter == v) { return; }
    m_nr2AeFilter = v;
    emit nr2AeFilterChanged(v);
}
void SliceModel::setNr2Position(Longpath::NrPosition p)
{
    if (m_nr2Position == p) { return; }
    m_nr2Position = p;
    emit nr2PositionChanged(p);
}
void SliceModel::setNr2Post2Run(bool v)
{
    if (m_nr2Post2Run == v) { return; }
    m_nr2Post2Run = v;
    emit nr2Post2RunChanged(v);
}
void SliceModel::setNr2Post2Level(double v)
{
    if (qFuzzyCompare(m_nr2Post2Level, v)) { return; }
    m_nr2Post2Level = v;
    emit nr2Post2LevelChanged(v);
}
void SliceModel::setNr2Post2Factor(double v)
{
    if (qFuzzyCompare(m_nr2Post2Factor, v)) { return; }
    m_nr2Post2Factor = v;
    emit nr2Post2FactorChanged(v);
}
void SliceModel::setNr2Post2Rate(double v)
{
    if (qFuzzyCompare(m_nr2Post2Rate, v)) { return; }
    m_nr2Post2Rate = v;
    emit nr2Post2RateChanged(v);
}
void SliceModel::setNr2Post2Taper(int v)
{
    if (m_nr2Post2Taper == v) { return; }
    m_nr2Post2Taper = v;
    emit nr2Post2TaperChanged(v);
}

// NR3
void SliceModel::setNr3Position(Longpath::NrPosition p)
{
    if (m_nr3Position == p) { return; }
    m_nr3Position = p;
    emit nr3PositionChanged(p);
}
void SliceModel::setNr3UseDefaultGain(bool v)
{
    if (m_nr3UseDefaultGain == v) { return; }
    m_nr3UseDefaultGain = v;
    emit nr3UseDefaultGainChanged(v);
}

// NR4
void SliceModel::setNr4Reduction(double v)
{
    if (qFuzzyCompare(m_nr4Reduction, v)) { return; }
    m_nr4Reduction = v;
    emit nr4ReductionChanged(v);
}
void SliceModel::setNr4Smoothing(double v)
{
    if (qFuzzyCompare(m_nr4Smoothing, v)) { return; }
    m_nr4Smoothing = v;
    emit nr4SmoothingChanged(v);
}
void SliceModel::setNr4Whitening(double v)
{
    if (qFuzzyCompare(m_nr4Whitening, v)) { return; }
    m_nr4Whitening = v;
    emit nr4WhiteningChanged(v);
}
void SliceModel::setNr4Rescale(double v)
{
    if (qFuzzyCompare(m_nr4Rescale, v)) { return; }
    m_nr4Rescale = v;
    emit nr4RescaleChanged(v);
}
void SliceModel::setNr4PostThresh(double v)
{
    if (qFuzzyCompare(m_nr4PostThresh, v)) { return; }
    m_nr4PostThresh = v;
    emit nr4PostThreshChanged(v);
}
void SliceModel::setNr4Algo(Longpath::SbnrAlgo v)
{
    if (m_nr4Algo == v) { return; }
    m_nr4Algo = v;
    emit nr4AlgoChanged(v);
}

// NNR
void SliceModel::setNnrPosition(Longpath::NrPosition p)
{
    if (m_nnrPosition == p) { return; }
    m_nnrPosition = p;
    emit nnrPositionChanged(p);
}
void SliceModel::setNnrModel(int v)
{
    if (m_nnrModel == v) { return; }
    m_nnrModel = v;
    emit nnrModelChanged(v);
}
void SliceModel::setNnrMaskFloor(double v)
{
    if (m_nnrMaskFloor == v) { return; }
    m_nnrMaskFloor = v;
    emit nnrMaskFloorChanged(v);
}
void SliceModel::setNnrAlpha(double v)
{
    if (m_nnrAlpha == v) { return; }
    m_nnrAlpha = v;
    emit nnrAlphaChanged(v);
}
void SliceModel::setNnrAlphaKnee(double v)
{
    if (m_nnrAlphaKnee == v) { return; }
    m_nnrAlphaKnee = v;
    emit nnrAlphaKneeChanged(v);
}
void SliceModel::setNnrTau(double v)
{
    if (m_nnrTau == v) { return; }
    m_nnrTau = v;
    emit nnrTauChanged(v);
}
void SliceModel::setNnrMaxGain(double v)
{
    if (m_nnrMaxGain == v) { return; }
    m_nnrMaxGain = v;
    emit nnrMaxGainChanged(v);
}
void SliceModel::setNnrAttackMs(double v)
{
    if (m_nnrAttackMs == v) { return; }
    m_nnrAttackMs = v;
    emit nnrAttackMsChanged(v);
}
void SliceModel::setNnrReleaseMs(double v)
{
    if (m_nnrReleaseMs == v) { return; }
    m_nnrReleaseMs = v;
    emit nnrReleaseMsChanged(v);
}

// DFNR
void SliceModel::setDfnrAttenLimit(double v)
{
    if (qFuzzyCompare(m_dfnrAttenLimit, v)) { return; }
    m_dfnrAttenLimit = v;
    emit dfnrAttenLimitChanged(v);
}
void SliceModel::setDfnrPostFilterBeta(double v)
{
    if (qFuzzyCompare(m_dfnrPostFilterBeta, v)) { return; }
    m_dfnrPostFilterBeta = v;
    emit dfnrPostFilterBetaChanged(v);
}

// BNR + MNR
void SliceModel::setBnrStrength(double v)
{
    if (qFuzzyCompare(m_bnrStrength, v)) { return; }
    m_bnrStrength = v;
    emit bnrStrengthChanged(v);
}
void SliceModel::setMnrStrength(double v)
{
    if (qFuzzyCompare(m_mnrStrength, v)) { return; }
    m_mnrStrength = v;
    emit mnrStrengthChanged(v);
}
void SliceModel::setMnrOversub(double v)
{
    if (qFuzzyCompare(m_mnrOversub, v)) { return; }
    m_mnrOversub = v;
    emit mnrOversubChanged(v);
}
void SliceModel::setMnrFloor(double v)
{
    if (qFuzzyCompare(m_mnrFloor, v)) { return; }
    m_mnrFloor = v;
    emit mnrFloorChanged(v);
}
void SliceModel::setMnrAlpha(double v)
{
    if (qFuzzyCompare(m_mnrAlpha, v)) { return; }
    m_mnrAlpha = v;
    emit mnrAlphaChanged(v);
}
void SliceModel::setMnrBias(double v)
{
    if (qFuzzyCompare(m_mnrBias, v)) { return; }
    m_mnrBias = v;
    emit mnrBiasChanged(v);
}
void SliceModel::setMnrGsmooth(double v)
{
    if (qFuzzyCompare(m_mnrGsmooth, v)) { return; }
    m_mnrGsmooth = v;
    emit mnrGsmoothChanged(v);
}

void SliceModel::setSnbEnabled(bool v)
{
    if (m_snbEnabled != v) {
        m_snbEnabled = v;
        emit snbEnabledChanged(v);
    }
}

void SliceModel::setAnfEnabled(bool v)
{
    if (m_anfEnabled != v) {
        m_anfEnabled = v;
        emit anfEnabledChanged(v);
    }
}

// ── NB1 / NB2 / SNB detailed tuning ─────────────────────────────────────────
// Ranges mirror Thetis's NumericUpDown limits byte-for-byte (grpDSPNB
// setup.designer.cs:44399-44604, grpDSPSNB :44280-44398 [v2.10.3.13]).
// Clamping here rather than at the UI edge keeps a TCI client or a corrupt
// settings file from handing WDSP a value the upstream widget could never
// produce; SetEXTANBTau and friends take the number without validating it.
//
// The idempotency guard is load-bearing beyond the usual signal hygiene: the
// NB1 / NB2 setters feed RadioModel's cross-slice mirror, so a setter that
// re-emitted on an unchanged value would bounce between co-hosted slices.
void SliceModel::setNb1Threshold(int v)
{
    const int clamped = qBound(1, v, 1000);
    if (m_nb1Threshold != clamped) {
        m_nb1Threshold = clamped;
        emit nb1ThresholdChanged(clamped);
    }
}

void SliceModel::setNb1TransitionMs(double v)
{
    const double clamped = qBound(0.01, v, 2.00);
    if (!qFuzzyCompare(m_nb1TransitionMs, clamped)) {
        m_nb1TransitionMs = clamped;
        emit nb1TransitionMsChanged(clamped);
    }
}

void SliceModel::setNb1LeadMs(double v)
{
    const double clamped = qBound(0.01, v, 2.00);
    if (!qFuzzyCompare(m_nb1LeadMs, clamped)) {
        m_nb1LeadMs = clamped;
        emit nb1LeadMsChanged(clamped);
    }
}

void SliceModel::setNb1LagMs(double v)
{
    const double clamped = qBound(0.01, v, 2.00);
    if (!qFuzzyCompare(m_nb1LagMs, clamped)) {
        m_nb1LagMs = clamped;
        emit nb1LagMsChanged(clamped);
    }
}

// comboDSPNOBmode has five entries: Zero / Sample and Hold / Mean-Hold /
// Hold and Sample / Linear Interpolate (setup.designer.cs:44434 [v2.10.3.13]).
void SliceModel::setNb2Mode(int v)
{
    const int clamped = qBound(0, v, 4);
    if (m_nb2Mode != clamped) {
        m_nb2Mode = clamped;
        emit nb2ModeChanged(clamped);
    }
}

void SliceModel::setSnbK1(double v)
{
    const double clamped = qBound(2.0, v, 20.0);
    if (!qFuzzyCompare(m_snbK1, clamped)) {
        m_snbK1 = clamped;
        emit snbK1Changed(clamped);
    }
}

void SliceModel::setSnbK2(double v)
{
    const double clamped = qBound(4.0, v, 60.0);
    if (!qFuzzyCompare(m_snbK2, clamped)) {
        m_snbK2 = clamped;
        emit snbK2Changed(clamped);
    }
}

// No Thetis Setup control for this one: Thetis picks SNB output bandwidth per
// mode at rxa.cs:112-124. The range is Longpath's own native override,
// unchanged from the slider it replaces.
void SliceModel::setSnbOutputBandwidthHz(int v)
{
    const int clamped = qBound(100, v, 96000);
    if (m_snbOutputBandwidthHz != clamped) {
        m_snbOutputBandwidthHz = clamped;
        emit snbOutputBandwidthHzChanged(clamped);
    }
}

void SliceModel::setApfEnabled(bool v)
{
    if (m_apfEnabled != v) {
        m_apfEnabled = v;
        emit apfEnabledChanged(v);
    }
}

void SliceModel::setApfTuneHz(int hz)
{
    if (m_apfTuneHz != hz) {
        m_apfTuneHz = hz;
        emit apfTuneHzChanged(hz);
    }
}

void SliceModel::setBinauralEnabled(bool v)
{
    if (m_binauralEnabled != v) {
        m_binauralEnabled = v;
        emit binauralEnabledChanged(v);
    }
}

void SliceModel::setFmCtcssMode(int mode)
{
    if (m_fmCtcssMode != mode) {
        // Decode (2), Enc+Dec (3) und DCS Decode (4) hoeren alle auf
        // etwas -- in allen dreien ist der Kanal stumm, bis das Erwartete
        // anliegt.
        const bool hoertZu = (mode == 2 || mode == 3 || mode == 4);
        m_fmCtcssMode = mode;
        // Die Tonmeldung mitziehen, und zwar in dieselbe Richtung, die
        // `RxChannel::setCtcssSquelch` einschlaegt: beim Einschalten gilt
        // der Ton als FEHLEND (der Detektor braucht sein erstes Fenster),
        // beim Abschalten als ANLIEGEND (dann sperrt nichts).
        //
        // Ohne das zeigte die Tonleuchte im Moment des Einschaltens gruen,
        // weil die Vorgabe true ist -- waehrend die Sperre in Wahrheit zu
        // war und der Kanal stumm. Eine Anzeige, die im Einschaltmoment
        // das Gegenteil behauptet, ist schlimmer als keine.
        // (Vom Pruefstand gefangen: sieFolgtDerMeldungOhneUmweg.)
        setFmCtcssToneDetected(!hoertZu);
        emit fmCtcssModeChanged(mode);
    }
}

void SliceModel::setFmCtcssValueHz(double hz)
{
    // qFuzzyCompare is undefined when either arg is 0.0; use the subtraction-to-zero pattern.
    if (qFuzzyIsNull(m_fmCtcssValueHz - hz)) {
        return;
    }
    m_fmCtcssValueHz = hz;
    emit fmCtcssValueHzChanged(hz);
}

void SliceModel::setFmCtcssToneDetected(bool detected)
{
    // Meldung aus dem Empfangsweg, keine Einstellung -- darum kein
    // scheduleSettingsSave beim Aufrufer und nichts Gespeichertes hier.
    if (m_fmCtcssToneDetected == detected) {
        return;
    }
    m_fmCtcssToneDetected = detected;
    emit fmCtcssToneDetectedChanged(detected);
}

void SliceModel::setFmDcsCode(int oktal)
{
    if (m_fmDcsCode == oktal) {
        return;
    }
    m_fmDcsCode = oktal;
    // Wie beim Tonwechsel in setFmCtcssMode: der Detektor faengt von
    // vorne an, also gilt der Code bis auf Weiteres als nicht erkannt.
    if (m_fmCtcssMode == 4) {
        setFmCtcssToneDetected(false);
    }
    emit fmDcsCodeChanged(oktal);
}

void SliceModel::setFmDcsPolarity(int polarity)
{
    const int p = (polarity != 0) ? 1 : 0;
    if (m_fmDcsPolarity == p) {
        return;
    }
    m_fmDcsPolarity = p;
    if (m_fmCtcssMode == 4) {
        setFmCtcssToneDetected(false);
    }
    emit fmDcsPolarityChanged(p);
}

void SliceModel::setFmOffsetHz(int hz)
{
    if (m_fmOffsetHz != hz) {
        m_fmOffsetHz = hz;
        emit fmOffsetHzChanged(hz);
    }
}

void SliceModel::setFmTxMode(FmTxMode mode)
{
    if (m_fmTxMode == mode) { return; }
    // From Thetis console.cs:40400-40440 [@852bf0e] -- chkFMTXHigh /
    // chkFMTXSimplex / chkFMTXLow_CheckedChanged: every new TX mode turns
    // Reverse off FIRST (chkFMTXRev.Checked = false, while CurrentFMTXMode
    // still holds the old mode, so the receive frequency moves back by the
    // right offset), then sets the mode. Before 2026-09-27 Reverse stayed
    // on and the receiver stayed on the input frequency.
    setFmReverse(false);
    m_fmTxMode = mode;
    emit fmTxModeChanged(mode);
}

void SliceModel::setFmReverse(bool v)
{
    if (m_fmReverse != v) {
        m_fmReverse = v;
        emit fmReverseChanged(v);
    }
}

void SliceModel::setDiglOffsetHz(int hz)
{
    if (m_diglOffsetHz == hz) { return; }
    m_diglOffsetHz = hz;
    emit diglOffsetHzChanged(hz);
}

void SliceModel::setDiguOffsetHz(int hz)
{
    if (m_diguOffsetHz == hz) { return; }
    m_diguOffsetHz = hz;
    emit diguOffsetHzChanged(hz);
}

void SliceModel::setRttyMarkHz(int hz)
{
    if (m_rttyMarkHz != hz) {
        m_rttyMarkHz = hz;
        emit rttyMarkHzChanged(hz);
    }
}

void SliceModel::setRttyShiftHz(int hz)
{
    if (m_rttyShiftHz != hz) {
        m_rttyShiftHz = hz;
        emit rttyShiftHzChanged(hz);
    }
}

// ---------------------------------------------------------------------------
// Per-mode default filter presets
// ---------------------------------------------------------------------------

// Porting from Thetis console.cs:5180-5575 [v2.10.3.13] — InitFilterPresets, F5 per mode.
//
// Filter low/high are in Hz relative to the carrier frequency.
// LSB: negative offsets (passband below carrier)
// USB: positive offsets (passband above carrier)
// AM/SAM/DSB: symmetric around carrier
// CW: centered on cw_pitch (600 Hz from Thetis display.cs:1023)
// DIGU: centered on digu_click_tune_offset (1500 Hz from Thetis console.cs:14636)
// DIGL: centered on -digl_click_tune_offset (-2210 Hz from Thetis console.cs:14671)
std::pair<int, int> SliceModel::defaultFilterForMode(DSPMode mode)
{
    // Phase 3J-1 closeout Item 6 (2026-05-12): read CW pitch from
    // AppSettings instead of hardcoding 600.  Operator-configurable in
    // Thetis (Setup → DSP → CW → CW Pitch; default 600 Hz).  The setter
    // arrived 2026-09-27 as RadioModel::setCwPitch (Setup → DSP → CW),
    // independent of Phase 3M-2 CW TX.  Range 200..2250 from Thetis
    // udCWPitch / udDSPCWPitch — see kCwPitchMinHz in SliceModel.h (the
    // 100..2000 once quoted here was never Thetis's).
    //
    // From Thetis display.cs:1023 [v2.10.3.13] — cw_pitch default 600.
    const int kCwPitch = currentCwPitch();
    // From Thetis console.cs:14636 [v2.10.3.13]
    static constexpr int kDiguOffset = 1500;
    // From Thetis console.cs:14671 [v2.10.3.13]
    // Upstream inline attribution preserved verbatim:
    //   :14669  //reset preset filter's center frequency - W4TME
    static constexpr int kDiglOffset = 2210;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5207 [v2.10.3.13] — F5: -3000 to -100
        return {-3000, -100};
    case DSPMode::USB:
        // From Thetis console.cs:5249 [v2.10.3.13] — F5: 100 to 3000
        return {100, 3000};
    case DSPMode::DSB:
        // From Thetis console.cs:5543 [v2.10.3.13] — F5: -3300 to 3300
        return {-3300, 3300};
    case DSPMode::CWL:
        // From Thetis console.cs:5375 [v2.10.3.13] — F5: -(cw_pitch+200) to -(cw_pitch-200)
        return {-(kCwPitch + 200), -(kCwPitch - 200)};
    case DSPMode::CWU:
        // From Thetis console.cs:5417 [v2.10.3.13] — F5: (cw_pitch-200) to (cw_pitch+200)
        return {kCwPitch - 200, kCwPitch + 200};
    case DSPMode::FM: {
        // FM filters are dynamic in Thetis (from deviation + high cut):
        // +/-8000 at 5 kHz deviation, +/-5500 at 2.5 kHz (2026-09-27; was
        // a fixed +/-8000).
        // From Thetis console.cs:7498-7503 [@852bf0e]
        // Upstream inline attribution preserved verbatim (console.cs:7499):
        //   int halfBw = (int)(radio.GetDSPRX(0, 0).RXFMDeviation + radio.GetDSPRX(0, 0).RXFMHighCut);  //[2.10.3.4]MW0LGE
        const int halfBw = fmHalfBandwidthHz();
        return {-halfBw, halfBw};
    }
    case DSPMode::AM:
        // From Thetis console.cs:5459 [v2.10.3.13] — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DIGU:
        // Phase 3J-1 closeout Item 4 (2026-05-12): reverted to Thetis F5
        // default (kDiguOffset ± 500 = 1000..2000 Hz).  The Phase 3J-1
        // bench fix (commit 624b51c6) widened this to F1 (3 kHz) because
        // setDspMode slammed the default on EVERY mode change, which
        // chopped FT8/FT4 audio when WSJT-X drove band switches via
        // TCI.  With Item 4's per-(band, mode) LastFilter persistence
        // in place, the operator's first widening sticks -- F5 (1 kHz)
        // is now the right Thetis-faithful first-touch default, matching
        // upstream behavior.
        //
        // 2026-09-28: bis dahin ±600 mit dem angeblich woertlichen Zitat
        // `digu_click_tune_offset - 600, … "1.2k"` — das stand in keiner
        // Thetis-Fassung (alle 567 Staende von console.cs durchsucht).
        //
        // From Thetis console.cs:5271 [@852bf0e] — DIGU F5 preset:
        //   preset[m].SetFilter(f, digu_click_tune_offset - 500, digu_click_tune_offset + 500, "1.0k");
        return {kDiguOffset - 500, kDiguOffset + 500};
    case DSPMode::SPEC:
        // SPEC mode: passthrough, wide filter
        return {-5000, 5000};
    case DSPMode::DIGL:
        // Phase 3J-1 closeout Item 4 (2026-05-12): reverted to Thetis F5
        // default -- see DIGU case above for the full rationale.
        //
        // From Thetis console.cs:5229 [@852bf0e] — DIGL F5 preset:
        //   preset[m].SetFilter(f, -digl_click_tune_offset - 500, -digl_click_tune_offset + 500, "1.0k");
        return {-kDiglOffset - 500, -kDiglOffset + 500};
    case DSPMode::SAM:
        // From Thetis console.cs:5501 [v2.10.3.13] — F5: -5000 to 5000
        return {-5000, 5000};
    case DSPMode::DRM:
        // DRM: wide filter similar to AM
        return {-5000, 5000};
    case DSPMode::RADE_U:
        // Phase 3R Task J1.  RADE Upper sideband: the modem occupies
        // ~650..2350 Hz (1700 Hz wide, centered at +1500 Hz).  This is
        // the SSB-style passband that the panadapter filter window
        // displays and that the TX I/Q routing confines the RADE
        // baseband energy to.  The earlier +/-5000 Hz AM-class window
        // was a placeholder; the filter IS visible on the panadapter
        // AND defines the IF/baseband passband for the modem energy.
        return {650, 2350};
    case DSPMode::RADE_L:
        // Phase 3R Task J1.  RADE Lower sideband: mirror of RADE-U.
        return {-2350, -650};
    }
    // Fallback
    return {100, 3000};
}

// ---------------------------------------------------------------------------
// Full per-mode filter preset table
// ---------------------------------------------------------------------------
//
// Porting from Thetis console.cs:5118-5515 [@852bf0e] — InitFilterPresets.
// Wortgleich bei v2.10.3.13 (501e3f5, dort 5180-5577) und bei mi0bot
// (0cef1c9). Je Betriebsart F1..F10 mit Kanten UND Namen; der Name ist
// das, was Thetis auf den Knopf schreibt („1.0k", „800" …). VAR1/VAR2
// derselben Tabelle fuehrt bei uns SliceModel selbst (m_varFilters).
//
//   // used to initialize all the filter variables
//   [original inline comment from console.cs:5120]
//
// Bis 2026-09-28 stand hier eine andere Tabelle (seit 67f5079e,
// 2026-05-02). Sie zitierte diese Stelle mit den richtigen Zeilen, ihre
// Werte standen aber in keiner Thetis-Fassung: alle 567 Staende von
// console.cs seit 2017 durchsucht, keiner kennt CW ±750…±6, SSB
// 100..3300/2700/600, DIGU ±3000 oder AM ±1000/±500. Die Namen hiessen
// schlicht „F1"…„F10".

namespace {

struct PresetSlot {
    int         low;
    int         high;
    const char* name;
};

QList<PresetSlot> presetTable(DSPMode mode)
{
    // Die Thetis-Namen, damit die Zeilen unten wie das Original lesen.
    const int cw_pitch = currentCwPitch();
    const int digu_click_tune_offset = kDiguClickTuneOffset;
    const int digl_click_tune_offset = kDiglClickTuneOffset;

    switch (mode) {
    case DSPMode::LSB:
        // From Thetis console.cs:5129-5169 [@852bf0e]
        return { {-5100, -100, "5.0k"}, {-4500, -100, "4.4k"},
                 {-3900, -100, "3.8k"}, {-3400, -100, "3.3k"},
                 {-3000, -100, "2.9k"}, {-2800, -100, "2.7k"},
                 {-2500, -100, "2.4k"}, {-2200, -100, "2.1k"},
                 {-1900, -100, "1.8k"}, {-1100, -100, "1.0k"} };
    case DSPMode::USB:
        // From Thetis console.cs:5171-5211 [@852bf0e]
        return { {100, 5100, "5.0k"}, {100, 4500, "4.4k"},
                 {100, 3900, "3.8k"}, {100, 3400, "3.3k"},
                 {100, 3000, "2.9k"}, {100, 2800, "2.7k"},
                 {100, 2500, "2.4k"}, {100, 2200, "2.1k"},
                 {100, 1900, "1.8k"}, {100, 1100, "1.0k"} };
    case DSPMode::DIGL:
        // From Thetis console.cs:5213-5253 [@852bf0e]
        return { {-digl_click_tune_offset - 1500, -digl_click_tune_offset + 1500, "3.0k"},
                 {-digl_click_tune_offset - 1250, -digl_click_tune_offset + 1250, "2.5k"},
                 {-digl_click_tune_offset - 1000, -digl_click_tune_offset + 1000, "2.0k"},
                 {-digl_click_tune_offset - 750,  -digl_click_tune_offset + 750,  "1.5k"},
                 {-digl_click_tune_offset - 500,  -digl_click_tune_offset + 500,  "1.0k"},
                 {-digl_click_tune_offset - 400,  -digl_click_tune_offset + 400,  "800"},
                 {-digl_click_tune_offset - 300,  -digl_click_tune_offset + 300,  "600"},
                 {-digl_click_tune_offset - 150,  -digl_click_tune_offset + 150,  "300"},
                 {-digl_click_tune_offset - 75,   -digl_click_tune_offset + 75,   "150"},
                 {-digl_click_tune_offset - 38,   -digl_click_tune_offset + 38,   "75"} };
    case DSPMode::DIGU:
        // From Thetis console.cs:5255-5295 [@852bf0e]
        return { {digu_click_tune_offset - 1500, digu_click_tune_offset + 1500, "3.0k"},
                 {digu_click_tune_offset - 1250, digu_click_tune_offset + 1250, "2.5k"},
                 {digu_click_tune_offset - 1000, digu_click_tune_offset + 1000, "2.0k"},
                 {digu_click_tune_offset - 750,  digu_click_tune_offset + 750,  "1.5k"},
                 {digu_click_tune_offset - 500,  digu_click_tune_offset + 500,  "1.0k"},
                 {digu_click_tune_offset - 400,  digu_click_tune_offset + 400,  "800"},
                 {digu_click_tune_offset - 300,  digu_click_tune_offset + 300,  "600"},
                 {digu_click_tune_offset - 150,  digu_click_tune_offset + 150,  "300"},
                 {digu_click_tune_offset - 75,   digu_click_tune_offset + 75,   "150"},
                 {digu_click_tune_offset - 38,   digu_click_tune_offset + 38,   "75"} };
    case DSPMode::CWL:
        // From Thetis console.cs:5297-5337 [@852bf0e]
        return { {-cw_pitch - 500, -cw_pitch + 500, "1.0k"},
                 {-cw_pitch - 400, -cw_pitch + 400, "800"},
                 {-cw_pitch - 300, -cw_pitch + 300, "600"},
                 {-cw_pitch - 250, -cw_pitch + 250, "500"},
                 {-cw_pitch - 200, -cw_pitch + 200, "400"},
                 {-cw_pitch - 125, -cw_pitch + 125, "250"},
                 {-cw_pitch - 75,  -cw_pitch + 75,  "150"},
                 {-cw_pitch - 50,  -cw_pitch + 50,  "100"},
                 {-cw_pitch - 25,  -cw_pitch + 25,  "50"},
                 {-cw_pitch - 13,  -cw_pitch + 13,  "25"} };
    case DSPMode::CWU:
        // From Thetis console.cs:5339-5379 [@852bf0e]
        return { {cw_pitch - 500, cw_pitch + 500, "1.0k"},
                 {cw_pitch - 400, cw_pitch + 400, "800"},
                 {cw_pitch - 300, cw_pitch + 300, "600"},
                 {cw_pitch - 250, cw_pitch + 250, "500"},
                 {cw_pitch - 200, cw_pitch + 200, "400"},
                 {cw_pitch - 125, cw_pitch + 125, "250"},
                 {cw_pitch - 75,  cw_pitch + 75,  "150"},
                 {cw_pitch - 50,  cw_pitch + 50,  "100"},
                 {cw_pitch - 25,  cw_pitch + 25,  "50"},
                 {cw_pitch - 13,  cw_pitch + 13,  "25"} };
    case DSPMode::AM:
        // From Thetis console.cs:5381-5421 [@852bf0e]
        return { {-10000, 10000, "20k"},  {-9000, 9000, "18k"},
                 {-8000,  8000,  "16k"},  {-6000, 6000, "12k"},
                 {-5000,  5000,  "10k"},  {-4500, 4500, "9.0k"},
                 {-4000,  4000,  "8.0k"}, {-3500, 3500, "7.0k"},
                 {-3000,  3000,  "6.0k"}, {-2500, 2500, "5.0k"} };
    case DSPMode::SAM:
        // From Thetis console.cs:5423-5463 [@852bf0e]
        return { {-10000, 10000, "20k"},  {-9000, 9000, "18k"},
                 {-8000,  8000,  "16k"},  {-6000, 6000, "12k"},
                 {-5000,  5000,  "10k"},  {-4500, 4500, "9.0k"},
                 {-4000,  4000,  "8.0k"}, {-3500, 3500, "7.0k"},
                 {-3000,  3000,  "6.0k"}, {-2500, 2500, "5.0k"} };
    case DSPMode::DSB:
        // From Thetis console.cs:5465-5505 [@852bf0e]
        return { {-8000, 8000, "16k"},  {-6000, 6000, "12k"},
                 {-5000, 5000, "10k"},  {-4000, 4000, "8.0k"},
                 {-3300, 3300, "6.6k"}, {-2600, 2600, "5.2k"},
                 {-2000, 2000, "4.0k"}, {-1550, 1550, "3.1k"},
                 {-1450, 1450, "2.9k"}, {-1200, 1200, "2.4k"} };

    // ── Longpath-eigen: Thetis hat hier keine Vorgaben ───────────────
    //
    // FM, SPEC und DRM fallen in InitFilterPresets in den `default:`-Zweig
    // (LastFilter = NONE, console.cs:5507-5508 [@852bf0e]), und
    // SetRX1Mode sperrt dort die Filterknoepfe (DisableAllFilters,
    // console.cs:34285 FM / 34339 SPEC / 34403 DRM [@852bf0e]). Die
    // Werte unten sind unsere; bis 2026-09-28 gab der FM-Zweig
    // „console.cs:5527 region" als Quelle an, das ist der DSB-Block. Der
    // Betreiber hat am 2026-09-28 entschieden, sie zu behalten. Namen in
    // Thetis' Schreibweise (volle Breite, „8.0k", „16k").
    case DSPMode::FM:
        return { {-8000, 8000, "16k"}, {-6000, 6000, "12k"}, {-4000, 4000, "8.0k"} };
    case DSPMode::SPEC:
        return { {-5000, 5000, "10k"} };
    case DSPMode::DRM:
        return { {-10000, 10000, "20k"}, {-5000, 5000, "10k"} };
    case DSPMode::RADE_U:
        // Phase 3R Task J1.  RADE Upper sideband: single fixed-bandwidth
        // preset matching the 1700 Hz modem passband.  No F1-F10
        // variants; RADE has a fixed bandwidth per sideband.
        return { {650, 2350, "1.7k"} };
    case DSPMode::RADE_L:
        // Phase 3R Task J1.  RADE Lower sideband: mirror of RADE-U.
        return { {-2350, -650, "1.7k"} };
    }
    // Fallback
    return { {100, 3000, "2.9k"} };
}

} // namespace

// Returns (low_hz, high_hz) pairs in Thetis F1→F10 order. Drives the „…"
// of the command bar (via FilterPresetStore), the Setup page and the
// memory Filter name (RadioModel::captureMemory).
QList<std::pair<int, int>> SliceModel::presetsForMode(DSPMode mode)
{
    // Die CW-Tabelle rutscht an der Spiegelgrenze, wie in Thetis jede
    // Vorgabe, sobald der CWPitch-Setter gelaufen ist (console.cs:
    // 18162-18195, centreCwFilterOnPitch): bei tiefer Tonhoehe reicht
    // ein breiter Platz sonst ueber den Traeger (CWU F1 bei 300 Hz:
    // -200..800 statt 0..1000). Die Plaetze sitzen schon auf der
    // Tonhoehe, das Nachzentrieren aendert nur das Rutschen.
    const QList<PresetSlot> table = presetTable(mode);
    const int pitch = currentCwPitch();
    QList<std::pair<int, int>> out;
    out.reserve(table.size());
    for (const PresetSlot& s : table) {
        int low  = s.low;
        int high = s.high;
        centreCwFilterOnPitch(low, high, mode, pitch);   // nur CWL/CWU
        out.append({low, high});
    }
    return out;
}

// Die Namen derselben Plaetze, in derselben Reihenfolge. Thetis behaelt
// den Namen, wenn der CWPitch-Setter einen Platz verschiebt
// (console.cs:18165 `string name = …GetName(f)` [@852bf0e]) — die Namen
// haengen also nicht an der Tonhoehe.
QStringList SliceModel::presetNamesForMode(DSPMode mode)
{
    QStringList out;
    for (const PresetSlot& s : presetTable(mode)) {
        out.append(QString::fromLatin1(s.name));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Mode name utilities
// ---------------------------------------------------------------------------

QString SliceModel::modeName(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:  return QStringLiteral("LSB");
    case DSPMode::USB:  return QStringLiteral("USB");
    case DSPMode::DSB:  return QStringLiteral("DSB");
    case DSPMode::CWL:  return QStringLiteral("CWL");
    case DSPMode::CWU:  return QStringLiteral("CWU");
    case DSPMode::FM:   return QStringLiteral("FM");
    case DSPMode::AM:   return QStringLiteral("AM");
    case DSPMode::DIGU: return QStringLiteral("DIGU");
    case DSPMode::SPEC: return QStringLiteral("SPEC");
    case DSPMode::DIGL: return QStringLiteral("DIGL");
    case DSPMode::SAM:  return QStringLiteral("SAM");
    case DSPMode::DRM:  return QStringLiteral("DRM");
    // Phase 3R Task J1.  Longpath-native; not WDSP modes.  Split
    // into upper/lower sidebands like USB/LSB.
    case DSPMode::RADE_U: return QStringLiteral("RADE-U");
    case DSPMode::RADE_L: return QStringLiteral("RADE-L");
    }
    return QStringLiteral("USB");
}

DSPMode SliceModel::modeFromName(const QString& name)
{
    if (name == QLatin1String("LSB"))  return DSPMode::LSB;
    if (name == QLatin1String("USB"))  return DSPMode::USB;
    if (name == QLatin1String("DSB"))  return DSPMode::DSB;
    if (name == QLatin1String("CWL"))  return DSPMode::CWL;
    if (name == QLatin1String("CWU"))  return DSPMode::CWU;
    if (name == QLatin1String("FM"))   return DSPMode::FM;
    if (name == QLatin1String("AM"))   return DSPMode::AM;
    if (name == QLatin1String("DIGU")) return DSPMode::DIGU;
    if (name == QLatin1String("SPEC")) return DSPMode::SPEC;
    if (name == QLatin1String("DIGL")) return DSPMode::DIGL;
    if (name == QLatin1String("SAM"))  return DSPMode::SAM;
    if (name == QLatin1String("DRM"))  return DSPMode::DRM;
    // Phase 3R Task J1.  Longpath-native; not WDSP modes.
    if (name == QLatin1String("RADE-U")) return DSPMode::RADE_U;
    if (name == QLatin1String("RADE-L")) return DSPMode::RADE_L;
    // Legacy migration: pre-fix builds persisted the singular "RADE"
    // string before the sideband split landed.  Map it to RADE_U so
    // existing per-MAC persisted slice modes keep working on upgrade.
    if (name == QLatin1String("RADE")) return DSPMode::RADE_U;
    return DSPMode::USB;
}

// ---------------------------------------------------------------------------
// Per-slice-per-band persistence (Phase 3G-10 Stage 2 — S2.P)
// ---------------------------------------------------------------------------
//
// Key layout:
//   Per-band DSP: Slice<N>/Band<key>/<Field>   (varies by band)
//   Session state: Slice<N>/<Field>             (band-agnostic)
//
// <N> comes from m_sliceIndex. <key> comes from bandKeyName(band).

namespace {

// Build the per-band prefix string, e.g. "Slice0/Band20m/".
QString bandPrefix(int sliceIndex, Band band)
{
    return QStringLiteral("Slice%1/Band%2/")
               .arg(sliceIndex)
               .arg(bandKeyName(band));
}

// Phase 3J-1 closeout Item 4 (2026-05-12): build the per-(band, mode)
// prefix, e.g. "Slice0/Band20m/ModeUSB/".  Used by setDspMode + saveTo
// Settings + restoreFromSettings to persist the filter cutoffs under
// (slice, band, mode) instead of the legacy (slice, band) tuple, so a
// mode change inside a band restores the operator's previously-set
// filter for THAT mode rather than slamming to defaultFilterForMode.
//
// Mirrors Thetis's preset[m].LastFilter machinery (console.cs:14653-
// 14671 [v2.10.3.13]) where each (band, mode) pair has its own remembered
// filter slot.
QString bandModePrefix(int sliceIndex, Band band, DSPMode mode)
{
    return QStringLiteral("Slice%1/Band%2/Mode%3/")
               .arg(sliceIndex)
               .arg(bandKeyName(band))
               .arg(SliceModel::modeName(mode));
}

// Build the session-state prefix string, e.g. "Slice0/".
QString slicePrefix(int sliceIndex)
{
    return QStringLiteral("Slice%1/").arg(sliceIndex);
}

// Boolean → AppSettings canonical string.
QString boolStr(bool v) { return v ? QStringLiteral("True") : QStringLiteral("False"); }

} // namespace

// DspMode is the sentinel: if present under the per-band namespace,
// the band is treated as visited. Alternatives (Frequency, FilterLow)
// are written by migrateLegacyKeys() even when the upstream VfoDspMode
// key was absent, so they'd report "visited" for a band the user has
// never intentionally configured on the mode side. Using DspMode matches
// the semantic we want for the #118 band-click handler: "has this band
// been configured, not just visited."
bool SliceModel::hasSettingsFor(Band band) const
{
    auto& s = AppSettings::instance();
    return s.contains(bandPrefix(m_sliceIndex, band) + QStringLiteral("DspMode"));
}

void SliceModel::saveToSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    s.setValue(bp + QStringLiteral("Frequency"),    m_frequency);
    s.setValue(bp + QStringLiteral("AgcThreshold"), m_agcThreshold);
    s.setValue(bp + QStringLiteral("AgcHang"),      m_agcHang);
    s.setValue(bp + QStringLiteral("AgcSlope"),     m_agcSlope);
    s.setValue(bp + QStringLiteral("AgcAttack"),    m_agcAttack);
    s.setValue(bp + QStringLiteral("AgcDecay"),     m_agcDecay);
    s.setValue(bp + QStringLiteral("AgcAutoEnabled"), m_autoAgcEnabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(bp + QStringLiteral("AgcAutoOffset"), m_autoAgcOffset);
    s.setValue(bp + QStringLiteral("AgcFixedGain"), m_agcFixedGain);
    s.setValue(bp + QStringLiteral("AgcHangThreshold"), m_agcHangThreshold);
    s.setValue(bp + QStringLiteral("AgcMaxGain"),   m_agcMaxGain);
    s.setValue(bp + QStringLiteral("FilterLow"),    m_filterLow);
    s.setValue(bp + QStringLiteral("FilterHigh"),   m_filterHigh);
    // Phase 3J-1 closeout Item 4 (2026-05-12): ALSO persist filter under
    // (band, currentMode) so a future mode change can restore it.  Legacy
    // (band)/FilterLow stays for backward compat with code that reads it
    // directly without going through restoreFromSettings.
    {
        const QString bmp = bandModePrefix(m_sliceIndex, band, m_dspMode);
        s.setValue(bmp + QStringLiteral("FilterLow"),  m_filterLow);
        s.setValue(bmp + QStringLiteral("FilterHigh"), m_filterHigh);
    }
    s.setValue(bp + QStringLiteral("DspMode"),      static_cast<int>(m_dspMode));
    s.setValue(bp + QStringLiteral("AgcMode"),      static_cast<int>(m_agcMode));
    s.setValue(bp + QStringLiteral("StepHz"),       m_stepHz);

    // Noise-blanker mode (per-band). Tri-state Off/NB/NB2 mirrors Thetis
    // chkNB state per-receiver. NB TUNING (threshold / tau / lag / lead) is
    // NOT per-band in Thetis and lives globally inside NbFamily — see
    // SliceModel.h for the 2026-04-22 removal note.
    s.setValue(bp + QStringLiteral("NbMode"), static_cast<int>(m_nbMode));

    // Phase 3F: per-slice DDC sample rate, persisted per-band so each band
    // can independently remember its preferred rate (e.g. 192 kHz on 40m,
    // 1536 kHz on 10m for a wider pan). Longpath-original (no Thetis cite).
    s.setValue(bp + QStringLiteral("SampleRate"), m_sampleRateHz);

    // Phase 3F Sub-Epic G Task 2: per-band diversity tuning. The 8-memory
    // slots (T3) + direction-finding fields (T11) join this block when they
    // ship. Longpath-original schema (Thetis persists diversity globally
    // in DSP.console.dsp / Diversity.cs; we scope per-band per-slice so
    // operators can keep distinct DF setups across bands).
    s.setValue(bp + QStringLiteral("DiversityPhaseDeg"), m_diversityPhaseDeg);
    s.setValue(bp + QStringLiteral("DiversityGainDb"), m_diversityGainDb);
    s.setValue(bp + QStringLiteral("DiversityFineNullEnabled"),
               boolStr(m_diversityFineNullEnabled));

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    // NR active slot + tuning — session-level only, no per-band suffix.
    // Per user directive Q10: no band suffix on NR keys.
    s.setValue(sp + QStringLiteral("NrActive"),        static_cast<int>(m_activeNr));
    // NR1
    s.setValue(sp + QStringLiteral("Nr1Taps"),         m_nr1Taps);
    s.setValue(sp + QStringLiteral("Nr1Delay"),        m_nr1Delay);
    s.setValue(sp + QStringLiteral("Nr1Gain"),         m_nr1Gain);
    s.setValue(sp + QStringLiteral("Nr1Leakage"),      m_nr1Leakage);
    s.setValue(sp + QStringLiteral("Nr1Position"),     static_cast<int>(m_nr1Position));
    // NR2
    s.setValue(sp + QStringLiteral("Nr2GainMethod"),   static_cast<int>(m_nr2GainMethod));
    s.setValue(sp + QStringLiteral("Nr2NpeMethod"),    static_cast<int>(m_nr2NpeMethod));
    s.setValue(sp + QStringLiteral("Nr2TrainT1"),      m_nr2TrainT1);
    s.setValue(sp + QStringLiteral("Nr2TrainT2"),      m_nr2TrainT2);
    s.setValue(sp + QStringLiteral("Nr2AeFilter"),     boolStr(m_nr2AeFilter));
    s.setValue(sp + QStringLiteral("Nr2Position"),     static_cast<int>(m_nr2Position));
    s.setValue(sp + QStringLiteral("Nr2Post2Run"),     boolStr(m_nr2Post2Run));
    s.setValue(sp + QStringLiteral("Nr2Post2Level"),   m_nr2Post2Level);
    s.setValue(sp + QStringLiteral("Nr2Post2Factor"),  m_nr2Post2Factor);
    s.setValue(sp + QStringLiteral("Nr2Post2Rate"),    m_nr2Post2Rate);
    s.setValue(sp + QStringLiteral("Nr2Post2Taper"),   m_nr2Post2Taper);
    // NR3
    s.setValue(sp + QStringLiteral("Nr3Position"),     static_cast<int>(m_nr3Position));
    s.setValue(sp + QStringLiteral("Nr3UseDefaultGain"), boolStr(m_nr3UseDefaultGain));
    // NR4
    s.setValue(sp + QStringLiteral("Nr4Reduction"),    m_nr4Reduction);
    s.setValue(sp + QStringLiteral("Nr4Smoothing"),    m_nr4Smoothing);
    s.setValue(sp + QStringLiteral("Nr4Whitening"),    m_nr4Whitening);
    s.setValue(sp + QStringLiteral("Nr4Rescale"),      m_nr4Rescale);
    s.setValue(sp + QStringLiteral("Nr4PostThresh"),   m_nr4PostThresh);
    s.setValue(sp + QStringLiteral("Nr4Algo"),         static_cast<int>(m_nr4Algo));
    // NNR
    s.setValue(sp + QStringLiteral("NnrPosition"),  static_cast<int>(m_nnrPosition));
    s.setValue(sp + QStringLiteral("NnrModel"),     m_nnrModel);
    s.setValue(sp + QStringLiteral("NnrMaskFloor"), m_nnrMaskFloor);
    s.setValue(sp + QStringLiteral("NnrAlpha"),     m_nnrAlpha);
    s.setValue(sp + QStringLiteral("NnrAlphaKnee"), m_nnrAlphaKnee);
    s.setValue(sp + QStringLiteral("NnrTau"),       m_nnrTau);
    s.setValue(sp + QStringLiteral("NnrMaxGain"),   m_nnrMaxGain);
    s.setValue(sp + QStringLiteral("NnrAttackMs"),  m_nnrAttackMs);
    s.setValue(sp + QStringLiteral("NnrReleaseMs"), m_nnrReleaseMs);
    // DFNR
    s.setValue(sp + QStringLiteral("DfnrAttenLimit"),     m_dfnrAttenLimit);
    s.setValue(sp + QStringLiteral("DfnrPostFilterBeta"), m_dfnrPostFilterBeta);
    // BNR + MNR
    s.setValue(sp + QStringLiteral("BnrStrength"),     m_bnrStrength);
    s.setValue(sp + QStringLiteral("MnrStrength"),     m_mnrStrength);
    s.setValue(sp + QStringLiteral("MnrOversub"),      m_mnrOversub);
    s.setValue(sp + QStringLiteral("MnrFloor"),        m_mnrFloor);
    s.setValue(sp + QStringLiteral("MnrAlpha"),        m_mnrAlpha);
    s.setValue(sp + QStringLiteral("MnrBias"),         m_mnrBias);
    s.setValue(sp + QStringLiteral("MnrGsmooth"),      m_mnrGsmooth);

    s.setValue(sp + QStringLiteral("SnbEnabled"), boolStr(m_snbEnabled));
    s.setValue(sp + QStringLiteral("AnfEnabled"), boolStr(m_anfEnabled));
    // NB1 / NB2 / SNB detailed tuning. Per slice per band like everything
    // else here, even though NB1 and NB2 behave as stream-shared while two
    // slices are co-hosted: the mirror lives in RadioModel and only applies
    // while they share a DDC, so slices that later separate must still have
    // their own stored values to go back to.
    s.setValue(sp + QStringLiteral("Nb1Threshold"),    m_nb1Threshold);
    s.setValue(sp + QStringLiteral("Nb1TransitionMs"), m_nb1TransitionMs);
    s.setValue(sp + QStringLiteral("Nb1LeadMs"),       m_nb1LeadMs);
    s.setValue(sp + QStringLiteral("Nb1LagMs"),        m_nb1LagMs);
    s.setValue(sp + QStringLiteral("Nb2Mode"),         m_nb2Mode);
    s.setValue(sp + QStringLiteral("SnbK1"),           m_snbK1);
    s.setValue(sp + QStringLiteral("SnbK2"),           m_snbK2);
    s.setValue(sp + QStringLiteral("SnbOutputBandwidthHz"), m_snbOutputBandwidthHz);
    s.setValue(sp + QStringLiteral("Locked"),     boolStr(m_locked));
    s.setValue(sp + QStringLiteral("Muted"),      boolStr(m_muted));
    s.setValue(sp + QStringLiteral("RitEnabled"), boolStr(m_ritEnabled));
    s.setValue(sp + QStringLiteral("RitHz"),      m_ritHz);
    s.setValue(sp + QStringLiteral("XitEnabled"), boolStr(m_xitEnabled));
    s.setValue(sp + QStringLiteral("XitHz"),      m_xitHz);
    // FM-Relais (2026-09-27): Ton, Ablage, Richtung. Reverse nicht --
    // es hat die Empfangsfrequenz schon verschoben, ein Wiederherstellen
    // verschoebe sie ein zweites Mal.
    s.setValue(sp + QStringLiteral("FmCtcssMode"),    m_fmCtcssMode);
    s.setValue(sp + QStringLiteral("FmCtcssValueHz"), m_fmCtcssValueHz);
    s.setValue(sp + QStringLiteral("FmOffsetHz"),     m_fmOffsetHz);
    s.setValue(sp + QStringLiteral("FmTxMode"),       static_cast<int>(m_fmTxMode));
    s.setValue(sp + QStringLiteral("AfGain"),     m_afGain);
    s.setValue(sp + QStringLiteral("RfGain"),     m_rfGain);
    s.setValue(sp + QStringLiteral("RxAntenna"),  m_rxAntenna);
    s.setValue(sp + QStringLiteral("TxAntenna"),  m_txAntenna);

    // Track the most recently saved band so RadioModel::loadSliceState() on
    // the next launch can land on the user's actual last-used frequency,
    // not the panadapter's 14.225 MHz default. Per-band Frequency keys
    // already store the per-band freq; this just records "which band was
    // active last." Read via SliceModel::loadLastBandFromSettings().
    s.setValue(sp + QStringLiteral("LastBand"), bandKeyName(band));
}

void SliceModel::restoreFromSettings(Band band)
{
    auto& s = AppSettings::instance();
    const QString bp = bandPrefix(m_sliceIndex, band);
    const QString sp = slicePrefix(m_sliceIndex);

    // ── Per-band DSP state ────────────────────────────────────────────────────
    // Each key: if absent, leave the current SliceModel default unchanged.

    if (s.contains(bp + QStringLiteral("Frequency"))) {
        setFrequency(s.value(bp + QStringLiteral("Frequency")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcThreshold"))) {
        setAgcThreshold(s.value(bp + QStringLiteral("AgcThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHang"))) {
        setAgcHang(s.value(bp + QStringLiteral("AgcHang")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcSlope"))) {
        setAgcSlope(s.value(bp + QStringLiteral("AgcSlope")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAttack"))) {
        setAgcAttack(s.value(bp + QStringLiteral("AgcAttack")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcDecay"))) {
        setAgcDecay(s.value(bp + QStringLiteral("AgcDecay")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcAutoEnabled"))) {
        setAutoAgcEnabled(s.value(bp + QStringLiteral("AgcAutoEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(bp + QStringLiteral("AgcAutoOffset"))) {
        setAutoAgcOffset(s.value(bp + QStringLiteral("AgcAutoOffset")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("AgcFixedGain"))) {
        setAgcFixedGain(s.value(bp + QStringLiteral("AgcFixedGain")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcHangThreshold"))) {
        setAgcHangThreshold(s.value(bp + QStringLiteral("AgcHangThreshold")).toInt());
    }
    if (s.contains(bp + QStringLiteral("AgcMaxGain"))) {
        setAgcMaxGain(s.value(bp + QStringLiteral("AgcMaxGain")).toInt());
    }
    if (s.contains(bp + QStringLiteral("DspMode"))) {
        // Set mode WITHOUT applying the default filter — filter follows below.
        // We must update m_dspMode before reading FilterLow/FilterHigh so
        // the final setFilter call is not superseded by setDspMode's default.
        const int rawMode = s.value(bp + QStringLiteral("DspMode")).toInt();
        // Guard against a corrupted/out-of-range persisted value (e.g. a
        // settings file edited or hand-crafted outside the app) reaching
        // static_cast<DSPMode> as undefined enum territory and flowing
        // unchecked into WDSP mode dispatch. RADE_L=13 is the highest
        // defined value (WdspTypes.h).
        if (rawMode >= static_cast<int>(DSPMode::LSB) &&
            rawMode <= static_cast<int>(DSPMode::RADE_L)) {
            const DSPMode mode = static_cast<DSPMode>(rawMode);
            // Directly assign mode without calling setDspMode() (which also
            // resets the filter). Emit the signal manually to keep observers in sync.
            if (m_dspMode != mode) {
                m_dspMode = mode;
                emit dspModeChanged(mode);
            }
        } else {
            qCWarning(lcDsp) << "Ignoring out-of-range persisted DspMode"
                              << rawMode << "for" << bp;
        }
    }
    // Phase 3J-1 closeout Item 4 (2026-05-12): prefer (band, currentMode)
    // filter when persisted; fall back to legacy (band)/FilterLow/High
    // for pre-Item-4 settings files.  m_dspMode was set above (line ~1491)
    // before reaching this restore block, so it reflects the destination
    // mode for the band restore.
    {
        // ── Wache gegen einen Durchlass auf dem falschen Seitenband ──
        //
        // Der Betreiber am 2026-09-17: "hört sich auf 40 meter
        // katastrophal an" -- in den Einstellungen stand fuer 40 m LSB
        // ein Durchlass von -100 … +2900 Hz, entstanden aus dem LOW/
        // WIDTH-Fehler des Bandfilters (BandwidthFilterApplet, sidebandOf).
        // Der Fehler ist behoben, aber der gespeicherte Wert kaeme bei
        // jedem Bandwechsel und jedem Start wieder: "wieder das gleiche".
        // Ein Durchlass, der bei einer einseitigen Betriebsart ueber den
        // Traeger reicht, ist kein Wunsch, den jemand gespeichert haben
        // wollte -- dieselbe Art Wache wie fuer DspMode/AgcMode hier
        // daneben: Vorgabe der Betriebsart statt des kaputten Werts.
        auto restoreFilter = [this](int low, int high) {
            if (filterCrossesCarrier(low, high, m_dspMode)) {
                qCWarning(lcDsp) << "Persisted filter" << low << high
                                 << "crosses the carrier for"
                                 << SliceModel::modeName(m_dspMode)
                                 << "-- using the mode default instead";
                const auto pair = defaultFilterForMode(m_dspMode);
                low  = pair.first;
                high = pair.second;
            }
            setFilter(low, high);
        };
        const QString bmp = bandModePrefix(m_sliceIndex, band, m_dspMode);
        if (s.contains(bmp + QStringLiteral("FilterLow")) &&
            s.contains(bmp + QStringLiteral("FilterHigh"))) {
            restoreFilter(s.value(bmp + QStringLiteral("FilterLow")).toInt(),
                          s.value(bmp + QStringLiteral("FilterHigh")).toInt());
        } else if (s.contains(bp + QStringLiteral("FilterLow")) &&
                   s.contains(bp + QStringLiteral("FilterHigh"))) {
            restoreFilter(s.value(bp + QStringLiteral("FilterLow")).toInt(),
                          s.value(bp + QStringLiteral("FilterHigh")).toInt());
        }
    }
    if (s.contains(bp + QStringLiteral("AgcMode"))) {
        // Same corrupted-settings guard as DspMode above -- AGCMode's
        // valid range is Off=0..Custom=5 (WdspTypes.h). An out-of-range
        // value would otherwise flow straight into
        // RxChannel::setAgcMode() -> SetRXAAGCMode() unchecked.
        const int rawAgcMode = s.value(bp + QStringLiteral("AgcMode")).toInt();
        if (rawAgcMode >= static_cast<int>(AGCMode::Off) &&
            rawAgcMode <= static_cast<int>(AGCMode::Custom)) {
            setAgcMode(static_cast<AGCMode>(rawAgcMode));
        } else {
            qCWarning(lcDsp) << "Ignoring out-of-range persisted AgcMode"
                              << rawAgcMode << "for" << bp;
        }
    }
    if (s.contains(bp + QStringLiteral("StepHz"))) {
        setStepHz(s.value(bp + QStringLiteral("StepHz")).toInt());
    }

    // Noise blanker mode (per-band). Tuning keys (NbThreshold/NbTauMs/
    // NbLeadMs/NbLagMs) from an earlier pre-2026-04-22 schema are ignored
    // if present — they'll be overwritten on next save. Per-band NB tuning
    // is not a Thetis concept.
    if (s.contains(bp + QStringLiteral("NbMode"))) {
        setNbMode(static_cast<Longpath::NbMode>(
            s.value(bp + QStringLiteral("NbMode")).toInt()));
    }

    // Phase 3F: per-slice DDC sample rate (per-band). Default 192000 when key
    // absent (new install or pre-3F settings file).
    if (s.contains(bp + QStringLiteral("SampleRate"))) {
        setSampleRateHz(s.value(bp + QStringLiteral("SampleRate")).toInt());
    }

    // Phase 3F Sub-Epic G Task 2: per-band diversity tuning. Defaults match
    // the SliceModel member-init values (0.0 deg, 0.0 dB, fine-null off) so
    // absent keys leave the slice in the passive reference-receiver state.
    if (s.contains(bp + QStringLiteral("DiversityPhaseDeg"))) {
        setDiversityPhaseDeg(
            s.value(bp + QStringLiteral("DiversityPhaseDeg")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("DiversityGainDb"))) {
        setDiversityGainDb(
            s.value(bp + QStringLiteral("DiversityGainDb")).toDouble());
    }
    if (s.contains(bp + QStringLiteral("DiversityFineNullEnabled"))) {
        setDiversityFineNullEnabled(
            s.value(bp + QStringLiteral("DiversityFineNullEnabled")).toString()
            == QLatin1String("True"));
    }

    // ── Session state (band-agnostic) ─────────────────────────────────────────
    // NR active slot + tuning (no per-band suffix, per user directive Q10).
    if (s.contains(sp + QStringLiteral("NrActive"))) {
        setActiveNr(static_cast<Longpath::NrSlot>(s.value(sp + QStringLiteral("NrActive")).toInt()));
    }
    // NR1
    if (s.contains(sp + QStringLiteral("Nr1Taps"))) {
        setNr1Taps(s.value(sp + QStringLiteral("Nr1Taps")).toInt());
    }
    if (s.contains(sp + QStringLiteral("Nr1Delay"))) {
        setNr1Delay(s.value(sp + QStringLiteral("Nr1Delay")).toInt());
    }
    if (s.contains(sp + QStringLiteral("Nr1Gain"))) {
        setNr1Gain(s.value(sp + QStringLiteral("Nr1Gain")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr1Leakage"))) {
        setNr1Leakage(s.value(sp + QStringLiteral("Nr1Leakage")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr1Position"))) {
        setNr1Position(static_cast<Longpath::NrPosition>(s.value(sp + QStringLiteral("Nr1Position")).toInt()));
    }
    // NR2
    if (s.contains(sp + QStringLiteral("Nr2GainMethod"))) {
        setNr2GainMethod(static_cast<Longpath::EmnrGainMethod>(s.value(sp + QStringLiteral("Nr2GainMethod")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2NpeMethod"))) {
        setNr2NpeMethod(static_cast<Longpath::EmnrNpeMethod>(s.value(sp + QStringLiteral("Nr2NpeMethod")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2TrainT1"))) {
        setNr2TrainT1(s.value(sp + QStringLiteral("Nr2TrainT1")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2TrainT2"))) {
        setNr2TrainT2(s.value(sp + QStringLiteral("Nr2TrainT2")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2AeFilter"))) {
        setNr2AeFilter(s.value(sp + QStringLiteral("Nr2AeFilter")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Nr2Position"))) {
        setNr2Position(static_cast<Longpath::NrPosition>(s.value(sp + QStringLiteral("Nr2Position")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Run"))) {
        setNr2Post2Run(s.value(sp + QStringLiteral("Nr2Post2Run")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Level"))) {
        setNr2Post2Level(s.value(sp + QStringLiteral("Nr2Post2Level")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Factor"))) {
        setNr2Post2Factor(s.value(sp + QStringLiteral("Nr2Post2Factor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Rate"))) {
        setNr2Post2Rate(s.value(sp + QStringLiteral("Nr2Post2Rate")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr2Post2Taper"))) {
        setNr2Post2Taper(s.value(sp + QStringLiteral("Nr2Post2Taper")).toInt());
    }
    // NR3
    if (s.contains(sp + QStringLiteral("Nr3Position"))) {
        setNr3Position(static_cast<Longpath::NrPosition>(s.value(sp + QStringLiteral("Nr3Position")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("Nr3UseDefaultGain"))) {
        setNr3UseDefaultGain(s.value(sp + QStringLiteral("Nr3UseDefaultGain")).toString() == QLatin1String("True"));
    }
    // NR4
    if (s.contains(sp + QStringLiteral("Nr4Reduction"))) {
        setNr4Reduction(s.value(sp + QStringLiteral("Nr4Reduction")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Smoothing"))) {
        setNr4Smoothing(s.value(sp + QStringLiteral("Nr4Smoothing")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Whitening"))) {
        setNr4Whitening(s.value(sp + QStringLiteral("Nr4Whitening")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Rescale"))) {
        setNr4Rescale(s.value(sp + QStringLiteral("Nr4Rescale")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4PostThresh"))) {
        setNr4PostThresh(s.value(sp + QStringLiteral("Nr4PostThresh")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("Nr4Algo"))) {
        setNr4Algo(static_cast<Longpath::SbnrAlgo>(s.value(sp + QStringLiteral("Nr4Algo")).toInt()));
    }
    // NNR
    if (s.contains(sp + QStringLiteral("NnrPosition"))) {
        setNnrPosition(static_cast<Longpath::NrPosition>(s.value(sp + QStringLiteral("NnrPosition")).toInt()));
    }
    if (s.contains(sp + QStringLiteral("NnrModel"))) {
        setNnrModel(s.value(sp + QStringLiteral("NnrModel")).toInt());
    }
    if (s.contains(sp + QStringLiteral("NnrMaskFloor"))) {
        setNnrMaskFloor(s.value(sp + QStringLiteral("NnrMaskFloor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("NnrAlpha"))) {
        setNnrAlpha(s.value(sp + QStringLiteral("NnrAlpha")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("NnrAlphaKnee"))) {
        setNnrAlphaKnee(s.value(sp + QStringLiteral("NnrAlphaKnee")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("NnrTau"))) {
        setNnrTau(s.value(sp + QStringLiteral("NnrTau")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("NnrMaxGain"))) {
        setNnrMaxGain(s.value(sp + QStringLiteral("NnrMaxGain")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("NnrAttackMs"))) {
        setNnrAttackMs(s.value(sp + QStringLiteral("NnrAttackMs")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("NnrReleaseMs"))) {
        setNnrReleaseMs(s.value(sp + QStringLiteral("NnrReleaseMs")).toDouble());
    }
    // DFNR
    if (s.contains(sp + QStringLiteral("DfnrAttenLimit"))) {
        setDfnrAttenLimit(s.value(sp + QStringLiteral("DfnrAttenLimit")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("DfnrPostFilterBeta"))) {
        setDfnrPostFilterBeta(s.value(sp + QStringLiteral("DfnrPostFilterBeta")).toDouble());
    }
    // BNR + MNR
    if (s.contains(sp + QStringLiteral("BnrStrength"))) {
        setBnrStrength(s.value(sp + QStringLiteral("BnrStrength")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrStrength"))) {
        setMnrStrength(s.value(sp + QStringLiteral("MnrStrength")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrOversub"))) {
        setMnrOversub(s.value(sp + QStringLiteral("MnrOversub")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrFloor"))) {
        setMnrFloor(s.value(sp + QStringLiteral("MnrFloor")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrAlpha"))) {
        setMnrAlpha(s.value(sp + QStringLiteral("MnrAlpha")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrBias"))) {
        setMnrBias(s.value(sp + QStringLiteral("MnrBias")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("MnrGsmooth"))) {
        setMnrGsmooth(s.value(sp + QStringLiteral("MnrGsmooth")).toDouble());
    }

    if (s.contains(sp + QStringLiteral("SnbEnabled"))) {
        setSnbEnabled(s.value(sp + QStringLiteral("SnbEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("AnfEnabled"))) {
        setAnfEnabled(s.value(sp + QStringLiteral("AnfEnabled")).toString() == QLatin1String("True"));
    }

    // NB1 / NB2 / SNB detailed tuning, with a one-way migration off the old
    // radio-global keys. Before these became per-slice properties the NB/SNB
    // setup page wrote one global value per knob and pushed it to channel 0;
    // an operator who had tuned the blanker would otherwise silently lose
    // that tuning on upgrade. So when a slice has no stored value of its own,
    // fall back to the legacy global before falling back to the default.
    // Nothing writes the legacy keys any more, so this decays naturally: once
    // a slice has been saved, its own key wins for good.
    auto restoreInt = [&](const char* perSlice, const char* legacyGlobal,
                          int fallback, auto&& setter) {
        const QString key = sp + QLatin1String(perSlice);
        if (s.contains(key)) {
            setter(s.value(key).toInt());
        } else if (s.contains(QLatin1String(legacyGlobal))) {
            setter(s.value(QLatin1String(legacyGlobal)).toInt());
        } else {
            setter(fallback);
        }
    };
    auto restoreDouble = [&](const char* perSlice, const char* legacyGlobal,
                             double legacyScale, double fallback, auto&& setter) {
        const QString key = sp + QLatin1String(perSlice);
        if (s.contains(key)) {
            setter(s.value(key).toDouble());
        } else if (s.contains(QLatin1String(legacyGlobal))) {
            setter(s.value(QLatin1String(legacyGlobal)).toDouble() * legacyScale);
        } else {
            setter(fallback);
        }
    };

    restoreInt("Nb1Threshold", "NbDefaultThresholdSlider", 30,
               [this](int v) { setNb1Threshold(v); });
    // The legacy transition / lead / lag keys stored the SLIDER integer at
    // x100 scale (1 == 0.01 ms), so migrating them needs the divide the
    // setup page used to apply on the way to WDSP.
    restoreDouble("Nb1TransitionMs", "NbDefaultTransition", 0.01, 0.01,
                  [this](double v) { setNb1TransitionMs(v); });
    restoreDouble("Nb1LeadMs", "NbDefaultLead", 0.01, 0.01,
                  [this](double v) { setNb1LeadMs(v); });
    restoreDouble("Nb1LagMs", "NbDefaultLag", 0.01, 0.01,
                  [this](double v) { setNb1LagMs(v); });
    restoreInt("Nb2Mode", "Nb2DefaultMode", 0,
               [this](int v) { setNb2Mode(v); });
    // The legacy SNB keys already stored real units, so scale is 1.0.
    restoreDouble("SnbK1", "SnbDefaultK1", 1.0, 8.0,
                  [this](double v) { setSnbK1(v); });
    restoreDouble("SnbK2", "SnbDefaultK2", 1.0, 20.0,
                  [this](double v) { setSnbK2(v); });
    restoreInt("SnbOutputBandwidthHz", "SnbDefaultOutputBW", 6000,
               [this](int v) { setSnbOutputBandwidthHz(v); });
    if (s.contains(sp + QStringLiteral("Locked"))) {
        setLocked(s.value(sp + QStringLiteral("Locked")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("Muted"))) {
        setMuted(s.value(sp + QStringLiteral("Muted")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitEnabled"))) {
        setRitEnabled(s.value(sp + QStringLiteral("RitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("RitHz"))) {
        setRitHz(s.value(sp + QStringLiteral("RitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("XitEnabled"))) {
        setXitEnabled(s.value(sp + QStringLiteral("XitEnabled")).toString() == QLatin1String("True"));
    }
    if (s.contains(sp + QStringLiteral("FmCtcssMode"))) {
        setFmCtcssMode(s.value(sp + QStringLiteral("FmCtcssMode")).toInt());
    }
    if (s.contains(sp + QStringLiteral("FmCtcssValueHz"))) {
        setFmCtcssValueHz(s.value(sp + QStringLiteral("FmCtcssValueHz")).toDouble());
    }
    if (s.contains(sp + QStringLiteral("FmOffsetHz"))) {
        setFmOffsetHz(s.value(sp + QStringLiteral("FmOffsetHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("FmTxMode"))) {
        const int m = s.value(sp + QStringLiteral("FmTxMode")).toInt();
        if (m >= 0 && m <= 2) { setFmTxMode(static_cast<FmTxMode>(m)); }
    }
    if (s.contains(sp + QStringLiteral("XitHz"))) {
        setXitHz(s.value(sp + QStringLiteral("XitHz")).toInt());
    }
    if (s.contains(sp + QStringLiteral("AfGain"))) {
        setAfGain(s.value(sp + QStringLiteral("AfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RfGain"))) {
        setRfGain(s.value(sp + QStringLiteral("RfGain")).toInt());
    }
    if (s.contains(sp + QStringLiteral("RxAntenna"))) {
        setRxAntenna(s.value(sp + QStringLiteral("RxAntenna")).toString());
    }
    if (s.contains(sp + QStringLiteral("TxAntenna"))) {
        setTxAntenna(s.value(sp + QStringLiteral("TxAntenna")).toString());
    }
}

// One-shot migration of the legacy flat key format (VfoFrequency, VfoDspMode,
// etc.) to the new per-slice-per-band namespace. Called once at startup before
// restoreFromSettings(). If the legacy key is absent the function is a no-op.
void SliceModel::migrateLegacyKeys()
{
    auto& s = AppSettings::instance();

    if (!s.contains(QStringLiteral("VfoFrequency"))) {
        return; // Nothing to migrate.
    }

    // Derive the band from the persisted frequency.
    double freq = s.value(QStringLiteral("VfoFrequency"), 14225000.0).toDouble();
    Band band = bandFromFrequency(freq);

    // Slice 0 — the only slice that can have legacy data.
    const QString bp = bandPrefix(0, band);
    const QString sp = slicePrefix(0);

    // Per-band DSP — migrate each key that exists.
    s.setValue(bp + QStringLiteral("Frequency"), freq);

    if (s.contains(QStringLiteral("VfoDspMode"))) {
        s.setValue(bp + QStringLiteral("DspMode"),
                   s.value(QStringLiteral("VfoDspMode")));
    }
    if (s.contains(QStringLiteral("VfoFilterLow"))) {
        s.setValue(bp + QStringLiteral("FilterLow"),
                   s.value(QStringLiteral("VfoFilterLow")));
    }
    if (s.contains(QStringLiteral("VfoFilterHigh"))) {
        s.setValue(bp + QStringLiteral("FilterHigh"),
                   s.value(QStringLiteral("VfoFilterHigh")));
    }
    if (s.contains(QStringLiteral("VfoAgcMode"))) {
        s.setValue(bp + QStringLiteral("AgcMode"),
                   s.value(QStringLiteral("VfoAgcMode")));
    }
    if (s.contains(QStringLiteral("VfoStepHz"))) {
        s.setValue(bp + QStringLiteral("StepHz"),
                   s.value(QStringLiteral("VfoStepHz")));
    }

    // Session state — migrate each key that exists.
    if (s.contains(QStringLiteral("VfoAfGain"))) {
        s.setValue(sp + QStringLiteral("AfGain"),
                   s.value(QStringLiteral("VfoAfGain")));
    }
    if (s.contains(QStringLiteral("VfoRfGain"))) {
        s.setValue(sp + QStringLiteral("RfGain"),
                   s.value(QStringLiteral("VfoRfGain")));
    }
    if (s.contains(QStringLiteral("VfoRxAntenna"))) {
        s.setValue(sp + QStringLiteral("RxAntenna"),
                   s.value(QStringLiteral("VfoRxAntenna")));
    }
    if (s.contains(QStringLiteral("VfoTxAntenna"))) {
        s.setValue(sp + QStringLiteral("TxAntenna"),
                   s.value(QStringLiteral("VfoTxAntenna")));
    }

    // Remove all legacy flat keys.
    s.remove(QStringLiteral("VfoFrequency"));
    s.remove(QStringLiteral("VfoDspMode"));
    s.remove(QStringLiteral("VfoFilterLow"));
    s.remove(QStringLiteral("VfoFilterHigh"));
    s.remove(QStringLiteral("VfoAgcMode"));
    s.remove(QStringLiteral("VfoStepHz"));
    s.remove(QStringLiteral("VfoAfGain"));
    s.remove(QStringLiteral("VfoRfGain"));
    s.remove(QStringLiteral("VfoRxAntenna"));
    s.remove(QStringLiteral("VfoTxAntenna"));
}

// Reads Slice<N>/LastBand and parses it back to a Band. Returns
// std::nullopt on missing key (fresh install / pre-LastBand settings)
// or unparseable values. Static so RadioModel can call this before
// constructing any slice.
std::optional<Band> SliceModel::loadLastBandFromSettings(int sliceIndex)
{
    auto& s = AppSettings::instance();
    const QString key = slicePrefix(sliceIndex) + QStringLiteral("LastBand");
    if (!s.contains(key)) {
        return std::nullopt;
    }
    const QString name = s.value(key).toString();
    if (name.isEmpty()) {
        return std::nullopt;
    }
    // bandFromName handles both label form ("20m") and short form ("20"),
    // plus GEN/WWV/XVTR. Falls back to Band::GEN on unknown input — guard
    // against that so a corrupted key doesn't silently land on GEN.
    const Band parsed = bandFromName(name);
    if (parsed == Band::GEN && name != QLatin1String("GEN")) {
        return std::nullopt;
    }
    return parsed;
}

// ---------------------------------------------------------------------------
// Phase 3O — VAX routing
// ---------------------------------------------------------------------------

void SliceModel::setVaxChannel(int ch)
{
    // Clamp to valid range.
    if (ch < 0 || ch > 4) { ch = 0; }

    const int prev = m_vaxChannel.exchange(ch, std::memory_order_acq_rel);
    if (prev == ch) { return; }

    AppSettings::instance().setValue(
        slicePrefix(m_sliceIndex) + QStringLiteral("VaxChannel"),
        QString::number(ch));

    emit vaxChannelChanged(ch);
}

// ── Phase 3J-2 Task D5: per-slice live SNR (Longpath-native) ──
//
// Emits snrDbChanged only on actual value change:
//   NaN    -> NaN              : no emission (signal stays absent)
//   x      -> identical x      : no emission (no change)
//   NaN    -> numeric          : emission (signal-acquired event)
//   numeric -> NaN             : emission (signal-lost event)
//   x      -> y (x != y)       : emission (normal update)
//
// NaN-aware comparison is required because IEEE NaN != NaN at the
// hardware level, so a naive equality check would treat NaN -> NaN as
// a change and spam emissions on every block when no signal is present.
void SliceModel::setSnrDb(double db)
{
    const bool dbNan   = qIsNaN(db);
    const bool prevNan = qIsNaN(m_snrDb);

    if (dbNan && prevNan) { return; }                                // both NaN: no change
    if (!dbNan && !prevNan && qFuzzyCompare(db, m_snrDb)) { return; }// both numeric and equal

    m_snrDb = db;
    emit snrDbChanged(db);

    // 2026-05-12 bench: heard fresh activity -> restart the idle
    // timer.  Only counts when the new SNR is numeric (NaN -> NaN was
    // already filtered above; numeric -> NaN means "we just cleared",
    // which shouldn't extend the activity window).
    if (!dbNan) {
        restartRadeIdleClearTimer();
    }
}

// ── 2026-05-11 bench: last RADE-decoded speaker callsign ────────────────────
//
// Sticky-while-in-RADE / clears-on-mode-off-RADE semantics per the
// bench design discussion (option A + D).  setDspMode in this file
// also clears the field when transitioning out of RADE_U/RADE_L; this
// setter is the write side for incoming decodes.
void SliceModel::setLastRadeRxCallsign(const QString& callsign)
{
    if (m_lastRadeRxCallsign == callsign) {
        return;
    }
    m_lastRadeRxCallsign = callsign;
    emit lastRadeRxCallsignChanged(callsign);

    // 2026-05-12 bench: heard fresh EOO callsign decode -> restart
    // the idle timer.  Empty -> non-empty is real activity; clears
    // back to empty (from timer expiry or mode-off-RADE) don't
    // restart -- letting the timer keep counting from the last
    // genuine activity.
    if (!callsign.isEmpty()) {
        restartRadeIdleClearTimer();
    }
}

void SliceModel::loadFromSettings()
{
    auto& s = AppSettings::instance();

    // ── VAX channel (Phase 3O) ────────────────────────────────────────────────
    int vaxCh = s.value(
        slicePrefix(m_sliceIndex) + QStringLiteral("VaxChannel"), "0")
        .toString().toInt();
    if (vaxCh < 0 || vaxCh > 4) { vaxCh = 0; }  // spec §5.1: invalid values clamp to 0
    if (vaxCh != m_vaxChannel.load(std::memory_order_acquire)) {
        m_vaxChannel.store(vaxCh, std::memory_order_release);
        emit vaxChannelChanged(vaxCh);
    }

}

} // namespace Longpath
