// =================================================================
// src/gui/widgets/DspQuickPopups.cpp  (NereusSDR)
// =================================================================
// Siehe DspQuickPopups.h — der Schnellregler-Rechtsklick.
//
// Ported from Thetis source:
//   Project Files/Source/Console/setup.cs, original licence from Thetis
//   source is included below — Bereiche und Vorgaben der NR-Regler
//   (udDSPNR1Taps / udDSPNR1Delay / udDSPNR1Gain / udDSPNR1Leak,
//   grpDSPGainMethod / grpDSPNR2NPEMethod / chkDSPNR2AE,
//   chkNR2PostProc_enable_rx1 und die Entsprechungen fuer NR3/NR4).
//
// Aufbau und Anordnung folgen AetherSDR MainWindow.cpp:7980-8324
// [@0cd4559] (ten9876/AetherSDR, GPLv3; NereusSDR ist ebenfalls GPLv3).
//
//
// Inhalt am 2026-08-18 unveraendert aus VfoWidget.cpp:3252-3533
// herausgeloest, damit er die Loeschung der VFO-Flagge ueberlebt.
// Geaendert wurde nur, was der Ortswechsel erzwingt: aus Methoden auf
// VfoWidget wurden freie Funktionen ueber einem SliceModel*, und aus
// dem `emit openNrSetupRequested(slot)` im Verweis „More Settings…"
// wurde ein hereingereichter Rueckruf.
//
// Die Zitate je Regler stehen unveraendert an ihrer Zeile — sie sind
// die Herkunft der Bereiche und Vorgaben und gehoeren zum Port.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Als sieben private Methoden in VfoWidget.cpp
//                 entstanden (Sub-epic C-1 Task 15), J.J. Boyd
//                 (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-08-18 — Unveraendert nach hier herausgeloest, damit sie die
//                 Loeschung der VFO-Flagge ueberleben. Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

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


#include "gui/widgets/DspQuickPopups.h"

#include "gui/widgets/DspParamPopup.h"
#include "models/SliceModel.h"

#include <QCoreApplication>
#include <QWidget>

namespace NereusSDR {
namespace DspQuickPopup {
namespace {

// tr() ohne QObject: die Beschriftungen sind uebersetzbar wie zuvor,
// nur haengen sie jetzt an keiner Klasse mehr.
inline QString tr(const char* s) {
    return QCoreApplication::translate("DspQuickPopup", s);
}

// ---- Sub-epic C-1: NR bank DspParamPopup builders (Task 15) ----
// Each popup shows the 3-5 most-adjusted knobs for the given NR slot.
// "More Settings…" fires openNrSetupRequested(slot) routed by MainWindow in Task 18.
// Ranges and defaults from Thetis setup.cs [v2.10.3.13] + AetherSDR MainWindow.cpp
// [@0cd4559] lines 7980-8324.

void showNr1Popup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // NR1 (ANR — Adaptive LMS).
    // From Thetis setup.cs udDSPNR1Taps/udDSPNR1Delay/udDSPNR1Gain/udDSPNR1Leak ranges
    // [v2.10.3.13].  Gain/Leakage stored as WDSP-domain values; sliders use UI units.
    p->addSlider(QStringLiteral("Taps"), 16, 128, m_slice->nr1Taps(),
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr1Taps(v); });
    p->addSlider(QStringLiteral("Delay"), 1, 256, m_slice->nr1Delay(),
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr1Delay(v); });
    // Gain: UI units = WDSP value / 1e-6. Slider range 0-999 = 0.0-0.000999 WDSP.
    const int uiGain = static_cast<int>(m_slice->nr1Gain() / 1e-6);
    p->addSlider(QStringLiteral("Gain"), 0, 999, uiGain,
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr1Gain(v * 1e-6); });
    // Leakage: UI units = WDSP value / 1e-3. Slider range 0-999 = 0.0-0.999e-3 WDSP.
    const int uiLeak = static_cast<int>(m_slice->nr1Leakage() / 1e-3);
    p->addSlider(QStringLiteral("Leak"), 0, 999, uiLeak,
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr1Leakage(v * 1e-3); });
    p->addRadioGroup(QStringLiteral("Position"),
                     {QStringLiteral("Pre-AGC"), QStringLiteral("Post-AGC")},
                     static_cast<int>(m_slice->nr1Position()),
                     [m_slice](int v) {
                         if (m_slice) m_slice->setNr1Position(static_cast<NereusSDR::NrPosition>(v));
                     });
    p->finalize(onMore, nullptr);
    p->showAt(globalPos);
}

void showNr2Popup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // NR2 (EMNR — Enhanced Multiband Noise Reduction).
    // From Thetis setup.designer.cs grpDSPGainMethod / grpDSPNR2NPEMethod /
    // chkDSPNR2AE / chkNR2PostProc_enable_rx1 labels [v2.10.3.13].
    p->addRadioGroup(QStringLiteral("Gain Method"),
                     {QStringLiteral("Linear"), QStringLiteral("Log"),
                      QStringLiteral("Gamma"), QStringLiteral("Trained")},
                     static_cast<int>(m_slice->nr2GainMethod()),
                     [m_slice](int v) {
                         if (m_slice) m_slice->setNr2GainMethod(static_cast<NereusSDR::EmnrGainMethod>(v));
                     });
    // From Thetis setup.designer.cs grpDSPNR2NPEMethod / radDSPNR2OSMS/MMSE/NSTAT [v2.10.3.13].
    p->addRadioGroup(QStringLiteral("NPE Method"),
                     {QStringLiteral("OSMS"), QStringLiteral("MMSE"), QStringLiteral("NSTAT")},
                     static_cast<int>(m_slice->nr2NpeMethod()),
                     [m_slice](int v) {
                         if (m_slice) m_slice->setNr2NpeMethod(static_cast<NereusSDR::EmnrNpeMethod>(v));
                     });
    // From Thetis setup.designer.cs chkDSPNR2AE.Text = "AE Filter" [v2.10.3.13].
    p->addCheckbox(QStringLiteral("AE Filter"), m_slice->nr2AeFilter(),
                   [m_slice](bool v) { if (m_slice) m_slice->setNr2AeFilter(v); });
    // From Thetis setup.designer.cs chkNR2PostProc_enable_rx1.Text = "Noise post proc" [v2.10.3.13].
    p->addCheckbox(QStringLiteral("Noise post proc"), m_slice->nr2Post2Run(),
                   [m_slice](bool v) { if (m_slice) m_slice->setNr2Post2Run(v); });
    // From Thetis setup.designer.cs labelTS476.Text = "Factor:" / labelTS475.Text = "Rate:" [v2.10.3.13].
    const int post2Factor = static_cast<int>(m_slice->nr2Post2Factor());
    p->addSlider(QStringLiteral("Factor"), 0, 30, post2Factor,
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr2Post2Factor(static_cast<double>(v)); });
    const int post2Rate = static_cast<int>(m_slice->nr2Post2Rate());
    p->addSlider(QStringLiteral("Rate"), 0, 30, post2Rate,
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr2Post2Rate(static_cast<double>(v)); });
    p->finalize(onMore, nullptr);
    p->showAt(globalPos);
}

void showNr3Popup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // NR3 (RNNR — Recurrent Neural Net NR).
    // From Thetis setup.cs udRNNR position + RXANR3FixedGain [v2.10.3.13]
    // and AetherSDR MainWindow.cpp:8200-8260 [@0cd4559].
    p->addRadioGroup(QStringLiteral("Position"),
                     {QStringLiteral("Pre-AGC"), QStringLiteral("Post-AGC")},
                     static_cast<int>(m_slice->nr3Position()),
                     [m_slice](int v) {
                         if (m_slice) m_slice->setNr3Position(static_cast<NereusSDR::NrPosition>(v));
                     });
    // From Thetis setup.designer.cs chkNR3_RNNoiseFixedGain.Text =
    // "Use fixed gain for input samples" [v2.10.3.13].
    p->addCheckbox(QStringLiteral("Use fixed gain for input samples"), m_slice->nr3UseDefaultGain(),
                   [m_slice](bool v) { if (m_slice) m_slice->setNr3UseDefaultGain(v); });
    // "Load Model…" opens Setup NR3 page where file dialog lives (Task 17).
    p->finalize(onMore, nullptr);
    p->showAt(globalPos);
}

void showNr4Popup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // NR4 (SBNR — Spectral Baseline NR).
    // From Thetis setup.designer.cs labelTS446/473 "Reduction", labelTS449/471 "Smoothing",
    // labelTS451/468 "Whitening", labelTS453/466 "Rescale", labelTS455/459 "SNRthresh",
    // radNR4_algo1/2/3 "Algo 1/2/3" [v2.10.3.13].
    const int reduction = static_cast<int>(m_slice->nr4Reduction());
    p->addSlider(QStringLiteral("Reduction"), 0, 20, reduction,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr4Reduction(static_cast<double>(v)); });
    const int smoothing = static_cast<int>(m_slice->nr4Smoothing());
    p->addSlider(QStringLiteral("Smoothing"), 0, 100, smoothing,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr4Smoothing(static_cast<double>(v)); });
    const int whitening = static_cast<int>(m_slice->nr4Whitening());
    p->addSlider(QStringLiteral("Whitening"), 0, 100, whitening,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr4Whitening(static_cast<double>(v)); });
    const int rescale = static_cast<int>(m_slice->nr4Rescale());
    p->addSlider(QStringLiteral("Rescale"), 0, 20, rescale,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr4Rescale(static_cast<double>(v)); });
    const int snrThresh = static_cast<int>(m_slice->nr4PostThresh());
    p->addSlider(QStringLiteral("SNRthresh"), -30, 0, snrThresh,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [m_slice](int v) { if (m_slice) m_slice->setNr4PostThresh(static_cast<double>(v)); });
    p->addRadioGroup(QStringLiteral("Algo"),
                     {QStringLiteral("Algo 1"), QStringLiteral("Algo 2"), QStringLiteral("Algo 3")},
                     static_cast<int>(m_slice->nr4Algo()),
                     [m_slice](int v) {
                         if (m_slice) m_slice->setNr4Algo(static_cast<NereusSDR::SbnrAlgo>(v));
                     });
    p->finalize(onMore, nullptr);
    p->showAt(globalPos);
}

void showDfnrPopup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // DFNR (DeepFilterNet3) — AetherSDR post-WDSP filter, not in Thetis.
    // Factory defaults per user directive 2026-04-23: AttenLimit 100 dB,
    // Post-Filter Beta 0.05 (UI 5).
    const int attenLimit = static_cast<int>(m_slice->dfnrAttenLimit());
    p->addSlider(QStringLiteral("Attenuation Limit"), 0, 100, attenLimit,
                 [](int v) { return QString::number(v) + QStringLiteral(" dB"); },
                 [m_slice](int v) { if (m_slice) m_slice->setDfnrAttenLimit(static_cast<double>(v)); },
                 tr("Maximum noise attenuation in dB (0 = bypass, 100 = maximum). "
                    "Default 100. Higher values suppress more noise but may clip speech peaks."),
                 /*factory=*/100);

    const int beta = static_cast<int>(m_slice->dfnrPostFilterBeta() * 100.0);
    p->addSlider(QStringLiteral("Post-Filter Beta"), 0, 100, beta,
                 [](int v) { return QString::number(v / 100.0, 'f', 2); },
                 [m_slice](int v) { if (m_slice) m_slice->setDfnrPostFilterBeta(v / 100.0); },
                 tr("Post-filter aggressiveness (0 = disabled, 0.30+ = aggressive). "
                    "Default 0 (off) — matches AetherSDR. Higher values reduce "
                    "residual musical-noise artifacts but may over-attenuate "
                    "consonants. Typical tuning: start at 0.05-0.10 and nudge up."),
                 /*factory=*/0);

    p->finalize(onMore,
                /*onReset=*/[]() { /* per-slider resetters push via valueChanged */ });
    p->showAt(globalPos);
}

void showBnrPopup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // BNR (NVIDIA Noise Removal) — button hidden unless HAVE_BNR; popup
    // included for completeness in case BNR is enabled in a future build.
    // AetherSDR MainWindow.cpp:8080-8100 [@0cd4559].
    const int strength = static_cast<int>(m_slice->bnrStrength() * 100.0);
    p->addSlider(QStringLiteral("Strength"), 0, 100, strength,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [m_slice](int v) { if (m_slice) m_slice->setBnrStrength(v / 100.0); });
    p->finalize(onMore, nullptr);
    p->showAt(globalPos);
}

void showMnrPopup(QWidget* parent, SliceModel* m_slice,
                  const QPoint& globalPos,
                  const std::function<void()>& onMore)
{
    if (!m_slice) { return; }
    auto* p = new DspParamPopup(parent);

    // MNR (macOS Accelerate MMSE-Wiener NR). 6 runtime-tunable knobs with
    // factory defaults tuned for balanced noticeable-but-not-underwater NR.
    // Right-click → Reset button restores these defaults.
    const int strength = static_cast<int>(m_slice->mnrStrength() * 100.0);
    p->addSlider(QStringLiteral("Strength"), 0, 200, strength,
                 [](int v) { return QString::number(v) + QStringLiteral("%"); },
                 [m_slice](int v) { if (m_slice) { m_slice->setMnrStrength(v / 100.0); } },
                 tr("Dry/wet blend.\n"
                    "  0%   = bypass (filter runs but output = input)\n"
                    "  100% = full NR (output = filter result)\n"
                    "  200% = over-drive (phase-flip, destructive)\n"
                    "Default 100."),
                 /*factory=*/100);

    const int oversubUi = static_cast<int>(m_slice->mnrOversub());
    p->addSlider(QStringLiteral("Aggressiveness"), 1, 1000, oversubUi,
                 [](int v) { return QString::number(v); },
                 [m_slice](int v) { if (m_slice) { m_slice->setMnrOversub(static_cast<double>(v)); } },
                 tr("MMSE-Wiener oversubtraction factor. Higher values attenuate "
                    "low-SNR bins more aggressively while leaving high-SNR (voice) "
                    "bins closer to unity.\n"
                    "  1    = very gentle\n"
                    "  6    = noticeable NR (default)\n"
                    "  20+  = underwater/robotic\n"
                    "  200+ = diminishing returns"),
                 /*factory=*/6);

    const int floorUi = static_cast<int>(m_slice->mnrFloor() * 1000.0);
    p->addSlider(QStringLiteral("Floor"), 0, 2000, floorUi,
                 [](int v) { return QString::number(v) + QStringLiteral("m"); },
                 [m_slice](int v) { if (m_slice) { m_slice->setMnrFloor(v * 0.001); } },
                 tr("Minimum Wiener gain per bin (×0.001).\n"
                    "  0    = total silence (filter can zero a bin)\n"
                    "  50   = -26 dB max attenuation (default)\n"
                    "  1000 = 0 dB (bin never attenuated)\n"
                    "  2000 = amplify (destructive)\n"
                    "Lower floor = more aggressive noise subtraction but more "
                    "musical-noise artifacts."),
                 /*factory=*/50);

    const int alphaUi = static_cast<int>(m_slice->mnrAlpha() * 100.0);
    p->addSlider(QStringLiteral("Alpha"), 0, 100, alphaUi,
                 [](int v) { return QString::number(v / 100.0, 'f', 2); },
                 [m_slice](int v) { if (m_slice) { m_slice->setMnrAlpha(v * 0.01); } },
                 tr("Decision-directed smoothing coefficient.\n"
                    "  0.00 = no smoothing (fast/chattery tracking)\n"
                    "  0.92 = Ephraim-Malah classic (default)\n"
                    "  1.00 = frozen (prior SNR never updates)\n"
                    "Balances NR speed vs. musical-noise artifacts."),
                 /*factory=*/92);

    const int biasUi = static_cast<int>(m_slice->mnrBias() * 10.0);
    p->addSlider(QStringLiteral("Bias"), 0, 100, biasUi,
                 [](int v) { return QString::number(v / 10.0, 'f', 1); },
                 [m_slice](int v) { if (m_slice) { m_slice->setMnrBias(v * 0.1); } },
                 tr("Min-statistics noise-floor bias correction.\n"
                    "  <1.0 = underestimate noise floor (less NR, more signal)\n"
                    "  1.5  = balanced (default)\n"
                    "  >3.0 = overestimate noise floor (more NR, may erode signal)\n"
                    "If NR is too weak, nudge Bias up. If it's eating speech, nudge down."),
                 /*factory=*/15);

    const int gsmoothUi = static_cast<int>(m_slice->mnrGsmooth() * 100.0);
    p->addSlider(QStringLiteral("Gsmooth"), 0, 100, gsmoothUi,
                 [](int v) { return QString::number(v / 100.0, 'f', 2); },
                 [m_slice](int v) { if (m_slice) { m_slice->setMnrGsmooth(v * 0.01); } },
                 tr("Temporal (per-bin) gain smoothing.\n"
                    "  0.00 = instant (more musical noise, fast transients)\n"
                    "  0.70 = balanced (default)\n"
                    "  1.00 = frozen (gain never updates — filter stuck)\n"
                    "Higher = smoother but slower to react to changing noise."),
                 /*factory=*/70);

    // Wire Reset button (finalize's second callback) to restore the
    // factory defaults on every slider. DspParamPopup::finalize runs the
    // per-slider resetters registered by addSlider's /*factory=*/ arg.
    p->finalize(onMore,
                /*onReset=*/[]() {
                    // Per-slider resetters registered via addSlider's
                    // factoryDefault arg already push slider → onChange →
                    // SliceModel. This empty callback exists only so the
                    // Reset button renders in the popup footer (finalize
                    // hides it when onReset is null).
                });
    p->showAt(globalPos);
}

} // namespace

void showFor(QWidget* parent, SliceModel* slice, NrSlot slot,
             const QPoint& globalPos,
             const std::function<void()>& onMore)
{
    if (!slice) { return; }
    switch (slot) {
    case NrSlot::NR1:  showNr1Popup(parent, slice, globalPos, onMore);  break;
    case NrSlot::NR2:  showNr2Popup(parent, slice, globalPos, onMore);  break;
    case NrSlot::NR3:  showNr3Popup(parent, slice, globalPos, onMore);  break;
    case NrSlot::NR4:  showNr4Popup(parent, slice, globalPos, onMore);  break;
    case NrSlot::DFNR: showDfnrPopup(parent, slice, globalPos, onMore); break;
    case NrSlot::BNR:  showBnrPopup(parent, slice, globalPos, onMore);  break;
    case NrSlot::MNR:  showMnrPopup(parent, slice, globalPos, onMore);  break;
    case NrSlot::Off:  break;   // „keine" hat nichts einzustellen
    }
}

} // namespace DspQuickPopup
} // namespace NereusSDR
