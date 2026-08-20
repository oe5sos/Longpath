#pragma once

// =================================================================
// src/gui/meters/MeterPoller.h  (NereusSDR)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/MeterManager.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//   2026-04-26 — Phase 3M-1a H.2: TX meter bindings on MOX engage/release.
//                 setTxChannel() + setInTx(bool) slot added.  TX poll
//                 reads TXA_OUT_PK / TXA_ALC_PK / TXA_ALC_AV / TXA_ALC_GAIN
//                 via GetTXAMeter() when in TX mode.
//                 Cite: Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
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

#include "core/WdspTypes.h"

#include <functional>  // std::function for setRxOffsetSource (RXOffset port)

#include <QObject>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <QVector>

namespace Longpath {

class RxChannel;
class TxChannel;
class MeterWidget;
class RadioStatus;
class WdspEngine;

// Binding IDs map to WDSP meter types (RxMeterType enum values)
namespace MeterBinding {
    // RX meters (0-49)
    constexpr int SignalPeak   = 0;    // RxMeterType::SignalPeak
    constexpr int SignalAvg    = 1;    // RxMeterType::SignalAvg
    constexpr int AdcPeak      = 2;    // RxMeterType::AdcPeak
    constexpr int AdcAvg       = 3;    // RxMeterType::AdcAvg
    constexpr int AgcGain      = 4;    // RxMeterType::AgcGain
    constexpr int AgcPeak      = 5;    // RxMeterType::AgcPeak
    constexpr int AgcAvg       = 6;    // RxMeterType::AgcAvg

    // RX meters — new (Phase 3G-4)
    constexpr int SignalMaxBin = 7;    // Spectral peak bin
    constexpr int PbSnr        = 8;    // Peak-to-baseline SNR

    // ── Zwei Groessen, die NICHT aus WDSP kommen (2026-08-18) ────────
    //
    // Sie standen bis dahin nur als Beschriftung an der analogen
    // S-Meter-Anzeige im Panelkopf. Mit deren Wegfall brauchen sie eine
    // Kennung wie jede andere Messgroesse — dann kann jedes Instrument
    // sie waehlen, statt dass eine Anzeige sie als Sonderfall traegt.
    // OE5SOS, 2026-08-18: „ebenfalls ein Messwert mit Skala — als
    // Quelle ins Instrument, nicht als eigene Zeile irgendwo."
    //
    // Beide werden nicht gepollt, sondern EINGESPEIST: der Rauschflur
    // von ClarityController::noiseFloorChanged, das RADE-SNR von
    // RadioModel::radeSnrChanged. MeterPoller::feedReading nimmt sie
    // entgegen und schickt sie denselben Weg wie einen Pollwert.
    constexpr int NoiseFloor   = 9;    // ClarityController, dBm
    constexpr int RadeSnr      = 10;   // RADE-Decoder, dB in 3 kHz

    // TX meters (100+). From Thetis MeterManager.cs Reading enum.
    // Stub values until TxChannel exists (Phase 3I-1).
    // PWR/SWR are hardware PA measurements, not WDSP meters.
    constexpr int TxPower        = 100;  // Forward power (hardware PA)
    constexpr int TxReversePower = 101;  // Reverse power (hardware PA)
    constexpr int TxSwr          = 102;  // SWR (computed fwd/rev ratio)
    constexpr int TxMic          = 103;  // TXA_MIC_AV
    constexpr int TxComp         = 104;  // TXA_COMP_AV
    constexpr int TxAlc          = 105;  // TXA_ALC_AV

    // TX meters — new (Phase 3G-4)
    constexpr int TxEq           = 106;  // From Thetis MeterManager.cs EQ reading
    constexpr int TxLeveler      = 107;  // TXA_LEVELER_AV
    constexpr int TxLevelerGain  = 108;  // TXA_LEVELER_GAIN
    constexpr int TxAlcGain      = 109;  // TXA_ALC_GAIN
    constexpr int TxAlcGroup     = 110;  // TXA_ALC_GROUP
    constexpr int TxCfc          = 111;  // TXA_CFC_AV
    constexpr int TxCfcGain      = 112;  // TXA_CFC_GAIN

    // Hardware readings (200+)
    constexpr int HwVolts        = 200;  // PA supply voltage
    constexpr int HwAmps         = 201;  // PA supply current
    constexpr int HwTemperature  = 202;  // PA temperature

    // Rotator readings (300+)
    constexpr int RotatorAz      = 300;  // Azimuth (0-360)
    constexpr int RotatorEle     = 301;  // Elevation (0-90)
}

class MeterPoller : public QObject {
    Q_OBJECT

public:
    explicit MeterPoller(QObject* parent = nullptr);
    ~MeterPoller() override;

    void setRxChannel(RxChannel* channel);

    void setWdspEngine(WdspEngine* engine);

    // ── TX meter bindings (H.2, Phase 3M-1a) ─────────────────────────────
    //
    // setTxChannel: register the TX channel for TX-meter polling.
    // Call with nullptr to detach (e.g. on radio disconnect).
    // Non-owning; RadioModel/WdspEngine own the object.
    //
    // setInTx: switch the poll set between RX and TX meters.
    // Ported from Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter:
    //   case MeterType.TXA_OUT_PK:   val = GetTXAMeter(channel, TXA_OUT_PK);
    //   case MeterType.TXA_ALC_PK:   val = GetTXAMeter(channel, TXA_ALC_PK);
    //   case MeterType.TXA_ALC_AV:   val = GetTXAMeter(channel, TXA_ALC_AV);
    //   case MeterType.TXA_ALC_GAIN: val = GetTXAMeter(channel, TXA_ALC_GAIN);
    // When isTx=true the poll() path skips the RX meter loop and calls
    // GetTXAMeter for the four TX binding IDs (TxAlc, TxAlcGain, TxPower stub).
    // When isTx=false the poll() path resumes normal RX meter polling.
    void setTxChannel(TxChannel* channel);

    void addTarget(MeterWidget* widget);
    void removeTarget(MeterWidget* widget);

    // ── Polling interval (Task 3.1, MultimeterPage wire-up) ──────────────────
    // setIntervalMs / intervalMs: new preferred interface used by MultimeterPage.
    // Corresponds to Thetis udDisplayMeterDelay (display.cs) which sets the
    // UpdateInterval property that drives the meter polling timer.
    // Default 100ms (10 fps) from Thetis MeterManager.cs [v2.10.3.13].
    // setInterval / interval kept for internal callers (backward-compat).
    void setIntervalMs(int ms);
    int  intervalMs() const;

    // ── Averaging window (Task 3.1, forward-looking for Task 3.2) ────────────
    // setAverageWindow: controls how many poll samples are averaged before
    // dispatch.  Currently stored; full averaging dispatch lands in Task 3.2.
    // Corresponds to Thetis udDisplayMeterAvg (display.cs) [v2.10.3.13].
    // Default 1 (no averaging).
    void setAverageWindow(int n);
    int  averageWindow() const;

    void setInterval(int ms);
    int interval() const;

    void start();
    void stop();

    /// Wire PA telemetry signals from RadioStatus into the meter targets
    /// for MeterBinding::TxPower / TxReversePower / TxSwr.
    /// Call once during integration; call with nullptr to detach cleanly.
    /// Safe to call again with a new pointer — the previous connection is
    /// disconnected automatically before re-connecting.
    /// Cite: Thetis console.cs PollPAPWR loop [v2.10.3.13] (RadioStatus
    /// aggregates forward/reflected/swr from that loop via powerChanged).
    void setRadioStatus(RadioStatus* status);

    // ── RX meter calibration offset (Thetis-faithful port) ───────────────
    //
    // Source for the per-poll cumulative offset (preamp + cal) applied to
    // SignalPeak / SignalAvg / MaxBin readings before display.
    //
    // The callable is invoked once per poll tick and must be lightweight
    // (RadioModel::rxMeterOffsetDb() is a const lookup over
    // m_hardwareProfile + StepAttenuatorController + AppSettings, no
    // mutex, no I/O).  Returning 0.0 disables the offset cleanly.
    //
    // Thetis call sites:
    //   console.cs:46821  float offset = RXOffset(1);
    //   console.cs:46824  ... = CalculateRXMeter(...) + offset;       // S_PK
    //   console.cs:46828  ... = CalculateRXMeter(...) + offset;       // S_AV
    //   console.cs:46881  ... = GetDetectMaxBin(0)    + offset;       // MaxBin
    //
    // Pass nullptr to detach (e.g. on RadioModel teardown).
    void setRxOffsetSource(std::function<double()> source);

public slots:
    // Switch between RX and TX meter polling.
    // Connected to MoxController::moxStateChanged(bool) by MainWindow (H.2).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] CalculateTXMeter (TX branch)
    // vs CalculateRXMeter (RX branch). Switches happen at MOX engage/release,
    // not mid-poll, matching Thetis's integer-tick dispatch via UpdateTimer.
    void setInTx(bool isTx);

    /// Which slices to emit sliceSmeterUpdated for, by slice id.
    ///
    /// Slice id doubles as the WDSP RX channel id (the invariant Sub-Epic I
    /// establishes), so this is also the list of channels polled. Pushed by
    /// MainWindow on every slice add / remove; empty disables the per-slice
    /// pass entirely and costs nothing.
    void setSliceChannels(const QList<int>& sliceIds) { m_sliceChannels = sliceIds; }

signals:
    // Emitted on each poll tick with the current S-meter (SignalAvg) dBm value.
    // Connect to VfoWidget::setSmeter to drive the VFO level bar.
    //
    // This is the ACTIVE slice's reading only -- it carries no slice id, and
    // this poller owns a single m_rxChannel. Use sliceSmeterUpdated for a
    // specific slice's flag.
    void smeterUpdated(double dbm);

    /// Per-slice S-meter, so every flag can show its own signal.
    ///
    /// Slices B+ had no S-meter at all: the poller reads one channel and the
    /// unqualified signal above was connected to Slice A's flag, so every
    /// other flag's bar sat dead. Emitted once per slice per tick for the
    /// slices given to setSliceChannels().
    void sliceSmeterUpdated(int sliceIndex, double dbm);

    /// Jeder verteilte Messwert, mit seiner MeterBinding-Kennung.
    ///
    /// Für Anzeigen, die keine MeterWidget sind — die Zeiger- und
    /// Balkeninstrumente (2026-08-17). Sie hängen sich hier an, statt
    /// eine zweite Abfrage aufzumachen: derselbe Umlauf, derselbe
    /// Zeitpunkt, dieselben Zahlen wie die Meter-Items.
    ///
    /// Gesendet aus dispatch(), also aus allen vier Verteilstellen
    /// (RX-Schleife, TX-Schleife, PA-Telemetrie, MMIO).
    void readingUpdated(int bindingId, double value);

public slots:
    /// Einen Messwert von aussen einspeisen.
    ///
    /// Fuer Groessen, die nicht aus WDSP gepollt werden koennen, weil
    /// sie irgendwo im Baum ENTSTEHEN: der Rauschflur im
    /// ClarityController, das RADE-SNR im Decoder. Sie gehen von hier
    /// denselben Weg wie ein Pollwert — dispatch() an alle Ziele UND
    /// readingUpdated.
    ///
    /// Warum ueber den Poller und nicht direkt ans Instrument: sonst
    /// gaebe es zwei Wege, an einen Messwert zu kommen, und das
    /// Instrument muesste wissen, welcher fuer welche Groesse gilt.
    /// Genau diese Doppelung war am 2026-08-18 der Grund, warum „Max
    /// Bin" nur ueber ein zweites Menue erreichbar war.
    void feedReading(int bindingId, double value);

private slots:
    void poll();

private:
    /// Emit sliceSmeterUpdated for each slice in m_sliceChannels. Independent
    /// of pollSMeter's analog-widget and m_rxChannel guards.
    void pollSliceSMeters();

private slots:

private:
    /// Einen Messwert an alle Ziele geben UND readingUpdated senden.
    ///
    /// Bis 2026-08-17 stand die Zielschleife viermal wörtlich im
    /// Umlauf (RX, TX, PA-Telemetrie, MMIO). Sie steht jetzt einmal,
    /// und damit gibt es auch nur EINE Stelle, an der ein neuer
    /// Abnehmer hinzukommt — statt vier, von denen man eine vergisst.
    void dispatch(int bindingId, double value);

    // ── Prüfmodus: Messwerte ohne Funkgerät ──────────────────────────
    //
    // OE5SOS, 2026-08-17: „sag mir, wie ich beide zu einem Messwert
    // bekomme, ohne Funkgerät — sonst kann ich Verlauf und Glut nicht
    // beurteilen."
    //
    // Ohne Funkgerät läuft der normale Umlauf gar nicht: start() wird
    // erst nach der WDSP-Einrichtung beim Verbinden gerufen
    // (MainWindow.cpp:4696). Der Prüfmodus hat deshalb seinen EIGENEN
    // Zeitgeber und hängt nicht an m_timer.
    //
    // Aus, sofern nicht ausdrücklich verlangt: NEREUS_METER_DEMO=1.
    // Dasselbe Muster wie NEREUS_WF_DEBUG in SpectrumWidget — „ein
    // Diagnosemittel, das im Normalbetrieb etwas kostet, ist ein
    // Diagnosemittel, das gelöscht wird".
    //
    // Er läuft NUR, solange kein RX-Kanal gesetzt ist. setRxChannel mit
    // einem echten Kanal hält ihn an, und zwar endgültig: erfundene
    // Zahlen über einem laufenden Funkgerät wären genau die Lüge, die
    // wir gerade beim leeren Instrument abgestellt haben.
    void startDemoFeedIfRequested();
    void tickDemo();

    QTimer m_demoTimer;
    int    m_demoTick{0};
    bool   m_demoWanted{false};

    // ── TX poll helper ────────────────────────────────────────────────────────
    // Reads the 4 WDSP TX meters gated for 3M-1a and pushes them to all
    // registered MeterWidget targets.
    // Porting from Thetis dsp.cs:999-1050 [v2.10.3.13] CalculateTXMeter.
    void pollTxMeters();

    /// Sendet smeterUpdated mit dem gemittelten Empfangswert.
    ///
    /// Hiess einmal so, weil es die analoge S-Meter-Anzeige bediente.
    /// Die ist am 2026-08-18 weggefallen; der Name bleibt, weil das
    /// Signal so heisst.
    void pollSMeter();

    // m_avgWindow: averaging window size set by MultimeterPage (Task 3.1).
    // Task 3.2 will use this value in dispatch; stored here for round-trip.
    // From Thetis udDisplayMeterAvg (display.cs) [v2.10.3.13].
    int    m_avgWindow{1};

    QTimer m_timer;
    QPointer<RxChannel> m_rxChannel;

    /// Slice ids to poll for the per-slice S-meter pass; see setSliceChannels.
    /// Slice id == WDSP RX channel id, so these index rxChannel() directly.
    QList<int> m_sliceChannels;
    // Non-owning TX channel pointer (H.2).  Valid only while WdspEngine has
    // opened the TX channel (after createTxChannel()).  Guarded in poll().
    // QPointer auto-clears when TxChannel is destroyed — matches m_rxChannel
    // pattern (3M-1a review fixup: prevents dangling-pointer dereference if
    // WdspEngine destroys the channel without calling setTxChannel(nullptr)).
    QPointer<TxChannel> m_txChannel;
    // m_inTx: true while MOX is active; flipped by setInTx(bool).
    // From Thetis dsp.cs:995-1050 [v2.10.3.13] TX/RX meter dispatch.
    bool m_inTx{false};
    // QPointer auto-clears to nullptr when the MeterWidget is destroyed.
    // ContainerManager's float/dock swap replaces MeterWidgets mid-session;
    // without QPointer, poll() would dereference the deleted old widgets.
    QVector<QPointer<MeterWidget>> m_targets;

    // PA telemetry wiring (Part B — Phase 3M-0 Task 7).
    // m_radioStatus is a non-owning raw pointer (RadioModel owns the object).
    // m_powerConn holds the single connection to RadioStatus::powerChanged;
    // disconnected on re-set or when status is nullptr.
    RadioStatus*            m_radioStatus{nullptr};
    QMetaObject::Connection m_powerConn;
    /// Wie m_powerConn, für RadioStatus::paTemperatureChanged →
    /// MeterBinding::HwTemperature (2026-08-17). Getrennt gehalten,
    /// damit setRadioStatus beide sauber löst, statt eine zu vergessen.
    QMetaObject::Connection m_tempConn;

    WdspEngine*             m_wdspEngine{nullptr};

    // RX meter cal offset (Thetis-faithful port).  Set via
    // setRxOffsetSource(); empty callable yields 0.0 dB (no offset).
    // Polled once per pollSMeter() invocation, then reused for the
    // poll() SignalPeak/SignalAvg loop and the smeterUpdated emit.
    // See setRxOffsetSource() doc for Thetis console.cs:46821 cite.
    std::function<double()> m_rxOffsetSource;
};

} // namespace Longpath
