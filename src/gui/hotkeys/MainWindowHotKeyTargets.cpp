// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/hotkeys/MainWindowHotKeyTargets.cpp  (Longpath)
// =================================================================
//
// Siehe den Kopf in MainWindowHotKeyTargets.h.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Fuer Longpath neu geschrieben von Martin Fischer
//                (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original in der Gestalt, Inhalt Longpath-original.

#include "gui/hotkeys/MainWindowHotKeyTargets.h"

#include "core/MoxController.h"
#include "core/WdspTypes.h"
#include "core/audio/VoiceKeyerPlayer.h"
#include "core/hotkeys/HotKeyActionList.h"
#include "core/hotkeys/HotKeyBindings.h"
#include "gui/MainWindow.h"
#include "gui/widgets/StatusToast.h"
#include "models/Band.h"
#include "models/FilterPresetStore.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QKeySequence>
#include <QList>
#include <QMetaObject>
#include <QPair>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace Longpath {

namespace {

// Ein Druck, so weit wie die AF-Knoepfe eines Thetis-Behaelters
// schalten. Upstream NereusSDR nimmt denselben Wert und belegt ihn mit
// Thetis console.cs:51556-51557 [v2.10.3.15].
constexpr int kVolumeStep = 5;

// Wie die Lupe im Panorama (MainWindow, Anschluss von
// SpectrumOverlayPanel::zoomIn/zoomOut): 1,5 je Druck, AetherSDRs Wert.
constexpr double kZoomFactor = 1.5;

// Ein Filter gilt als Vorgabe, wenn beide Kanten innerhalb so vieler Hz
// liegen. Upstream NereusSDRs Wert (RxApplet::updateFilterButtons dort).
constexpr int kPresetMatchHz = 50;

// Die Liste des RX-Panels, in ihrer Reihenfolge (RxApplet, m_modeCombo).
const std::array<QLatin1String, 13> kModeWalk = {
    QLatin1String("USB"),  QLatin1String("LSB"),  QLatin1String("CWU"),    QLatin1String("CWL"),
    QLatin1String("AM"),   QLatin1String("SAM"),  QLatin1String("FM"),     QLatin1String("DSB"),
    QLatin1String("DIGU"), QLatin1String("DIGL"), QLatin1String("DRM"),    QLatin1String("RADE-U"),
    QLatin1String("RADE-L"),
};

// Die Amateurfunkbaender der Bandleiste, 160 m bis 6 m. 2 m nur mit
// Transverter und darum nicht im Kreis; GEN, WWV und Rundfunk auch nicht.
const std::array<Band, 11> kBandWalk = {
    Band::Band160m, Band::Band80m, Band::Band60m, Band::Band40m, Band::Band30m, Band::Band20m,
    Band::Band17m,  Band::Band15m, Band::Band12m, Band::Band10m, Band::Band6m,
};

int steppedIndex(int from, int direction, int size)
{
    if (from < 0) {
        return direction > 0 ? 0 : size - 1;
    }
    return (from + direction + size) % size;
}

} // namespace

MainWindowHotKeyTargets::MainWindowHotKeyTargets(MainWindow* window)
    : m_window(window)
{
    m_clock.start();
}

RadioModel* MainWindowHotKeyTargets::model() const
{
    return m_window != nullptr ? m_window->m_radioModel : nullptr;
}

MoxController* MainWindowHotKeyTargets::mox() const
{
    RadioModel* const m = model();
    return m != nullptr ? m->moxController() : nullptr;
}

SliceModel* MainWindowHotKeyTargets::windowSlice() const
{
    RadioModel* const m = model();
    return m != nullptr ? m->activeSlice() : nullptr;
}

SliceModel* MainWindowHotKeyTargets::sliceByLetter(QChar letter) const
{
    RadioModel* const m = model();
    if (m == nullptr) {
        return nullptr;
    }
    const QChar wanted = letter.toUpper();
    for (SliceModel* const slice : m->slices()) {
        if (slice != nullptr && slice->sliceLetter() == wanted) {
            return slice;
        }
    }
    return nullptr;
}

QChar MainWindowHotKeyTargets::letterOf(const SliceModel* slice) const
{
    return slice != nullptr ? slice->sliceLetter() : QChar();
}

bool MainWindowHotKeyTargets::tuneTo(SliceModel* slice, double hz)
{
    if (m_window->m_shuttingDown || slice == nullptr) {
        return false;
    }
    const double before = slice->frequency();
    m_window->stimmeVomBediener(slice, hz);
    return slice->frequency() != before;
}

void MainWindowHotKeyTargets::runSliceAction(const QString& id, SliceModel* slice, int steps)
{
    if (slice == nullptr || m_window->m_shuttingDown) {
        return;
    }
    if (id == QLatin1String("step.larger")) {
        stepTuneStep(slice, 1);
    } else if (id == QLatin1String("step.smaller")) {
        stepTuneStep(slice, -1);
    } else if (id == QLatin1String("slice.lock")) {
        slice->setLocked(!slice->locked());
        toast(slice->locked() ? QStringLiteral("Slice %1 locked.").arg(slice->sliceLetter())
                              : QStringLiteral("Slice %1 unlocked.").arg(slice->sliceLetter()));
    } else if (id == QLatin1String("volume.up")) {
        changeVolume(slice, kVolumeStep * steps);
    } else if (id == QLatin1String("volume.down")) {
        changeVolume(slice, -kVolumeStep * steps);
    } else if (id == QLatin1String("mute")) {
        slice->setMuted(!slice->muted());
    } else if (id == QLatin1String("filter.next")) {
        walkFilter(slice, 1);
    } else if (id == QLatin1String("filter.previous")) {
        walkFilter(slice, -1);
    } else if (id == QLatin1String("mode.next")) {
        walkMode(slice, 1);
    } else if (id == QLatin1String("mode.previous")) {
        walkMode(slice, -1);
    } else if (id == QLatin1String("band.next")) {
        walkBand(slice, 1);
    } else if (id == QLatin1String("band.previous")) {
        walkBand(slice, -1);
    } else if (id == QLatin1String("nr.toggle")) {
        toggleNr(slice);
    } else if (id == QLatin1String("nb.toggle")) {
        cycleNb(slice);
    } else if (id == QLatin1String("anf.toggle")) {
        slice->setAnfEnabled(!slice->anfEnabled());
    } else if (id == QLatin1String("rit.toggle")) {
        slice->setRitEnabled(!slice->ritEnabled());
    } else if (id == QLatin1String("xit.toggle")) {
        slice->setXitEnabled(!slice->xitEnabled());
    } else {
        qCWarning(lcHotKeys) << "No slice action for:" << id;
    }
}

void MainWindowHotKeyTargets::runWindowAction(const QString& id)
{
    if (m_window->m_shuttingDown) {
        return;
    }
    if (id == QLatin1String("slice.next")) {
        selectNextSlice();
    } else if (id == QLatin1String("zoom.in")) {
        m_window->panoramaZoomen(1.0 / kZoomFactor);
    } else if (id == QLatin1String("zoom.out")) {
        m_window->panoramaZoomen(kZoomFactor);
    } else {
        qCWarning(lcHotKeys) << "No window action for:" << id;
    }
}

QString MainWindowHotKeyTargets::unavailableReason(const QString& id) const
{
    const HotKeyAction* const action = HotKeyActionList::find(id);
    if (action == nullptr) {
        return {};
    }
    RadioModel* const m = model();
    const bool needsRadio = action->transmits || id.startsWith(QLatin1String("voice.rec"));
    if (needsRadio && (m == nullptr || !m->isConnected())) {
        return QStringLiteral("Not connected to a radio.");
    }
    return {};
}

bool MainWindowHotKeyTargets::takesKey(const QString& id) const
{
    // Esc stoppt nur, wenn etwas laeuft. Sonst bleibt Esc, was es war --
    // ein Dialog oder ein Ausklappfeld darf sich weiter damit schliessen.
    if (id == QLatin1String("voice.stop")) {
        RadioModel* const m = model();
        return m != nullptr && m->voiceKeyerPlayer() != nullptr && m->voiceKeyerPlayer()->busy();
    }
    return true;
}

bool MainWindowHotKeyTargets::transmitting() const
{
    MoxController* const c = mox();
    return c != nullptr && c->isMox();
}

bool MainWindowHotKeyTargets::tuneOn() const
{
    RadioModel* const m = model();
    return m != nullptr && m->isTune();
}

bool MainWindowHotKeyTargets::keyWithReason(bool on, bool space)
{
    MoxController* const c = mox();
    if (c == nullptr) {
        return false;
    }
    // Wie Ft8Applet: die Ablehnung kommt synchron als moxRejected und
    // sonst gar nicht -- setMox() gibt nichts zurueck.
    QString reason;
    const QMetaObject::Connection conn = QObject::connect(
        c, &MoxController::moxRejected, c, [&reason](const QString& r) { reason = r; });
    if (space) {
        c->onSpacePtt(on);
    } else {
        c->setMox(on);
    }
    QObject::disconnect(conn);
    if (!reason.isEmpty()) {
        toast(QStringLiteral("Not transmitting: %1").arg(reason));
        return false;
    }
    return true;
}

bool MainWindowHotKeyTargets::keyHold(bool down)
{
    if (m_window->m_shuttingDown && down) {
        return false;
    }
    return keyWithReason(down, true);
}

void MainWindowHotKeyTargets::runTransmitAction(const QString& id)
{
    if (m_window->m_shuttingDown) {
        return;
    }
    if (id == QLatin1String("tx.mox")) {
        // Wie der MOX-Knopf: umschalten.
        keyWithReason(!transmitting(), false);
    } else if (id == QLatin1String("tx.tune")) {
        // Wie der TUNE-Knopf im TX-Panel. Eine Ablehnung meldet
        // RadioModel::tuneRefused; das Hauptfenster zeigt sie ohnehin.
        if (RadioModel* const m = model()) {
            m->setTune(!m->isTune());
        }
    } else {
        qCWarning(lcHotKeys) << "No transmit action for:" << id;
    }
}

QString MainWindowHotKeyTargets::keyTextFor(const QString& id) const
{
    RadioModel* const m = model();
    if (m == nullptr || m->hotKeyBindings() == nullptr) {
        return {};
    }
    const QStringList keys = m->hotKeyBindings()->keysFor(id);
    if (keys.isEmpty()) {
        return {};
    }
    const QString native = QKeySequence(keys.first()).toString(QKeySequence::NativeText);
    return native.isEmpty() ? keys.first() : native;
}

void MainWindowHotKeyTargets::runVoiceAction(const QString& id)
{
    RadioModel* const m = model();
    VoiceKeyerPlayer* const player = m != nullptr ? m->voiceKeyerPlayer() : nullptr;
    if (player == nullptr || m_window->m_shuttingDown) {
        return;
    }
    if (id == QLatin1String("voice.stop")) {
        player->stop();
        return;
    }
    const int slot = HotKeyActionList::voiceSlotOf(id);
    if (slot < 0) {
        qCWarning(lcHotKeys) << "No voice action for:" << id;
        return;
    }
    if (id.startsWith(QLatin1String("voice.msg"))) {
        const QString refusal = player->play(slot);
        if (!refusal.isEmpty()) {
            toast(refusal);
        }
        return;
    }

    // voice.recN: dieselbe Taste beginnt und beendet die Aufnahme.
    const bool wasRecordingThis = player->state() == VoiceKeyerPlayer::State::Recording
                                  && player->activeSlot() == slot;
    const QString refusal = player->record(slot);
    if (!refusal.isEmpty()) {
        toast(refusal);
        return;
    }
    if (wasRecordingThis) {
        const VoiceKeyerSlot& s = m->voiceKeyer().slot(slot);
        if (!s.isEmpty()) {
            toast(QStringLiteral("Voice message %1 stored (%2 s).")
                      .arg(slot + 1).arg(s.seconds, 0, 'f', 1));
        }
        return;
    }
    if (player->state() == VoiceKeyerPlayer::State::Recording) {
        const QString key = keyTextFor(id);
        toast(key.isEmpty()
                  ? QStringLiteral("Recording voice message %1 — Esc stops.").arg(slot + 1)
                  : QStringLiteral("Recording voice message %1 — %2 or Esc stops.").arg(slot + 1).arg(key));
    }
}

void MainWindowHotKeyTargets::toast(const QString& text)
{
    if (m_window->m_shuttingDown) {
        return;
    }
    m_window->showToast(text, ToastSeverity::Info, 3000);
}

qint64 MainWindowHotKeyTargets::nowMs() const
{
    return m_clock.elapsed();
}

// ── Die einzelnen Wege ──────────────────────────────────────────────

void MainWindowHotKeyTargets::stepTuneStep(SliceModel* slice, int direction)
{
    // Wie die STEP-Pfeile im RX-Panel, aber ohne Umlauf: eine Taste fuer
    // „groesser" soll an der groessten Stufe stehen bleiben, nicht auf
    // 1 Hz springen.
    const int current = slice->stepHz();
    int idx = -1;
    for (int i = 0; i < kStageOneStepLadderSize; ++i) {
        if (kStageOneStepLadder[i] == current) {
            idx = i;
            break;
        }
    }
    if (idx < 0) {
        // Kein Wert der Leiter: zur naechsten Stufe in Druckrichtung.
        idx = direction > 0 ? kStageOneStepLadderSize - 1 : 0;
        for (int i = 0; i < kStageOneStepLadderSize; ++i) {
            if (direction > 0 && kStageOneStepLadder[i] > current) {
                idx = i;
                break;
            }
            if (direction < 0 && kStageOneStepLadder[i] < current) {
                idx = i;
            }
        }
        slice->setStepHz(kStageOneStepLadder[idx]);
    } else {
        const int next = std::clamp(idx + direction, 0, kStageOneStepLadderSize - 1);
        slice->setStepHz(kStageOneStepLadder[next]);
    }
    const int hz = slice->stepHz();
    toast(hz >= 1000 ? QStringLiteral("Tune step %1 kHz").arg(hz / 1000.0)
                     : QStringLiteral("Tune step %1 Hz").arg(hz));
}

void MainWindowHotKeyTargets::changeVolume(SliceModel* slice, int delta)
{
    slice->setAfGain(std::clamp(slice->afGain() + delta, 0, 100));
}

void MainWindowHotKeyTargets::walkFilter(SliceModel* slice, int direction)
{
    // Dieselbe Liste wie das Filtermenue der Befehlsleiste
    // (CommandBar::filterMenuEntries).
    const DSPMode mode = slice->dspMode();
    QList<QPair<int, int>> presets;
    RadioModel* const m = model();
    if (m != nullptr && m->filterPresetStore() != nullptr) {
        for (const FilterPreset& p : m->filterPresetStore()->presetsForMode(mode)) {
            presets.append({p.low, p.high});
        }
    } else {
        for (const auto& edges : SliceModel::presetsForMode(mode)) {
            presets.append({edges.first, edges.second});
        }
    }
    if (presets.isEmpty()) {
        return;
    }
    // Die naechstliegende Vorgabe innerhalb der Toleranz, damit zwei
    // dicht beieinanderliegende den Lauf nicht auf der ersten festhalten.
    int best = -1;
    int bestDistance = 0;
    for (int i = 0; i < presets.size(); ++i) {
        const int dl = std::abs(slice->filterLow() - presets[i].first);
        const int dh = std::abs(slice->filterHigh() - presets[i].second);
        if (dl > kPresetMatchHz || dh > kPresetMatchHz) {
            continue;
        }
        if (best < 0 || dl + dh < bestDistance) {
            best = i;
            bestDistance = dl + dh;
        }
    }
    const int next = steppedIndex(best, direction, static_cast<int>(presets.size()));
    slice->setFilter(presets[next].first, presets[next].second);
}

void MainWindowHotKeyTargets::walkMode(SliceModel* slice, int direction)
{
    const QString current = SliceModel::modeName(slice->dspMode());
    int idx = -1;
    for (int i = 0; i < static_cast<int>(kModeWalk.size()); ++i) {
        if (current == kModeWalk[static_cast<size_t>(i)]) {
            idx = i;
            break;
        }
    }
    const int next = steppedIndex(idx, direction, static_cast<int>(kModeWalk.size()));
    slice->setDspMode(SliceModel::modeFromName(QString(kModeWalk[static_cast<size_t>(next)])));
}

void MainWindowHotKeyTargets::walkBand(SliceModel* slice, int direction)
{
    // Naechstes Band oberhalb bzw. unterhalb der jetzigen Frequenz. Aus
    // einem Band heraus ist das das Nachbarband; ausserhalb aller Baender
    // (GEN) das naechstgelegene in Druckrichtung. Am Ende geht es rundum.
    const double f = slice->frequency();
    int target = -1;
    if (direction > 0) {
        for (int i = 0; i < static_cast<int>(kBandWalk.size()); ++i) {
            double lo = 0.0;
            double hi = 0.0;
            if (bandRangeHz(kBandWalk[static_cast<size_t>(i)], lo, hi) && lo > f) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            target = 0;
        }
    } else {
        for (int i = static_cast<int>(kBandWalk.size()) - 1; i >= 0; --i) {
            double lo = 0.0;
            double hi = 0.0;
            if (bandRangeHz(kBandWalk[static_cast<size_t>(i)], lo, hi) && hi < f) {
                target = i;
                break;
            }
        }
        if (target < 0) {
            target = static_cast<int>(kBandWalk.size()) - 1;
        }
    }
    // Der eine Weg aller Bandknoepfe -- er wirkt auf den aktiven
    // Empfaenger. Ist die Taste an einen anderen gebunden, erst den
    // aktiv machen.
    RadioModel* const m = model();
    if (m != nullptr && m->activeSlice() != slice) {
        // setActiveSlice nimmt die Stelle in der Liste, nicht die Nummer.
        m->setActiveSlice(static_cast<int>(m->slices().indexOf(slice)));
    }
    m_window->bandUmschalten(kBandWalk[static_cast<size_t>(target)]);
}

void MainWindowHotKeyTargets::toggleNr(SliceModel* slice)
{
    const NrSlot now = slice->activeNr();
    if (now != NrSlot::Off) {
        m_lastNr.insert(slice->sliceIndex(), static_cast<int>(now));
        slice->setActiveNr(NrSlot::Off);
        return;
    }
    // Ohne Gedaechtnis NR1: in Thetis die erste Stelle des NR-Knopfs.
    const int last = m_lastNr.value(slice->sliceIndex(), static_cast<int>(NrSlot::NR1));
    slice->setActiveNr(static_cast<NrSlot>(last));
}

void MainWindowHotKeyTargets::cycleNb(SliceModel* slice)
{
    // Wie der NB-Knopf im RX-Panel: Aus -> NB -> NB2 -> Aus.
    switch (slice->nbMode()) {
    case NbMode::Off:
        slice->setNbMode(NbMode::NB);
        break;
    case NbMode::NB:
        slice->setNbMode(NbMode::NB2);
        break;
    default:
        slice->setNbMode(NbMode::Off);
        break;
    }
}

void MainWindowHotKeyTargets::selectNextSlice()
{
    RadioModel* const m = model();
    if (m == nullptr) {
        return;
    }
    const QList<SliceModel*> slices = m->slices();
    if (slices.size() < 2) {
        return;
    }
    const int current = static_cast<int>(slices.indexOf(m->activeSlice()));
    // setActiveSlice nimmt die Stelle in der Liste, nicht die Nummer.
    m->setActiveSlice((current + 1) % static_cast<int>(slices.size()));
}

} // namespace Longpath
