// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/hotkeys/HotKeyActionList.cpp  (Longpath)
// =================================================================
//
// Uebernommen aus dem Upstream NereusSDR (`src/core/hotkeys/
// HotKeyActionList.cpp` @ 9036ffc04, J.J. Boyd KG4VCF). Die Vorgabetasten
// fuer Abstimmen, Filter, Ton, Band/Betriebsart, Anzeige, DSP und Senden
// sind die des Upstream-Entwurfs (`docs/architecture/2026-11-05-hot-keys-
// design.md` §5 dort).
//
// ── Was Longpath anders hat ─────────────────────────────────────────
//
// * Sprachspeicher: ZEHN Plaetze statt vier (core/audio/VoiceKeyer.h,
//   Ansage des Betreibers vom 2026-08-19: „10 audio files aufnehmen …
//   mit shortcuts verbinden und als CQ CALL usw. abspielen"). Die Tasten
//   sind die aus N1MM Logger+, mit dem der Betreiber seit Jahren
//   Conteste faehrt: F1…F10 senden die Ansage, Strg+Umschalt+F1…F10
//   nimmt sie auf, Esc bricht ab. Upstream legt die Ansagen auf
//   Strg+Umschalt+F1…F4 — dort ist F1…F8 schon mit Strg den
//   CW-Speichern vergeben, die es bei uns nicht gibt.
// * Aufnehmen per Taste (voice.recN) ist neu. Upstream hat dafuer keine
//   Handlung.
// * Keine CW-Speicher, kein Punkt/Strich: CW-Senden ist zurueckgestellt
//   (Betreiberentscheidung 2026-08-27).
// * Kein Bandstapel: der ist in Longpath noch nicht gebaut.
// * Kein Zweiton, kein VOX: die Sendetasten beschraenken sich auf das,
//   was der Betreiber im Betrieb wirklich greift.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan Task 1.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen und auf Longpath zugeschnitten von Martin
//                Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "core/hotkeys/HotKeyActionList.h"

#include "core/audio/VoiceKeyer.h"

#include <QHash>

namespace Longpath {

Q_LOGGING_CATEGORY(lcHotKeys, "longpath.hotkeys")

namespace {

QList<HotKeyAction> buildActions()
{
    using G = HotKeyGesture;
    using S = HotKeyScope;

    const QString tuning = QStringLiteral("Tuning");
    const QString filter = QStringLiteral("Filter");
    const QString audio = QStringLiteral("Audio");
    const QString bandMode = QStringLiteral("Band and mode");
    const QString display = QStringLiteral("Display");
    const QString dsp = QStringLiteral("DSP");
    const QString transmit = QStringLiteral("Transmit");
    const QString voice = QStringLiteral("Voice keyer");

    QList<HotKeyAction> a;
    const auto add = [&a](const QString& id, const QString& name, const QString& group,
                          HotKeyGesture gesture, bool transmits, HotKeyScope scope,
                          const QStringList& keys) {
        a.append(HotKeyAction{id, name, group, gesture, transmits, scope, keys});
    };

    add(QStringLiteral("tune.up"), QStringLiteral("Tune up one step"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Right")});
    add(QStringLiteral("tune.down"), QStringLiteral("Tune down one step"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Left")});
    add(QStringLiteral("tune.up10"), QStringLiteral("Tune up ten steps"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Shift+Right")});
    add(QStringLiteral("tune.down10"), QStringLiteral("Tune down ten steps"), tuning, G::Turn, false, S::Slice, {QStringLiteral("Shift+Left")});
    add(QStringLiteral("step.larger"), QStringLiteral("Tune step larger"), tuning, G::Press, false, S::Slice, {QStringLiteral("]")});
    add(QStringLiteral("step.smaller"), QStringLiteral("Tune step smaller"), tuning, G::Press, false, S::Slice, {QStringLiteral("[")});
    add(QStringLiteral("slice.lock"), QStringLiteral("Lock"), tuning, G::Press, false, S::Slice, {QStringLiteral("L")});
    add(QStringLiteral("slice.next"), QStringLiteral("Next slice"), tuning, G::Press, false, S::Window, {QStringLiteral("`")});

    add(QStringLiteral("filter.next"), QStringLiteral("Filter next"), filter, G::Press, false, S::Slice, {QStringLiteral("B")});
    add(QStringLiteral("filter.previous"), QStringLiteral("Filter previous"), filter, G::Press, false, S::Slice, {QStringLiteral("V")});

    add(QStringLiteral("volume.up"), QStringLiteral("Volume up"), audio, G::Turn, false, S::Slice, {QStringLiteral("Up"), QStringLiteral("Num++")});
    add(QStringLiteral("volume.down"), QStringLiteral("Volume down"), audio, G::Turn, false, S::Slice, {QStringLiteral("Down"), QStringLiteral("Num+-")});
    add(QStringLiteral("mute"), QStringLiteral("Mute"), audio, G::Press, false, S::Slice, {QStringLiteral("U"), QStringLiteral("Num+*")});

    add(QStringLiteral("band.next"), QStringLiteral("Next band"), bandMode, G::Press, false, S::Slice, {QStringLiteral("J")});
    add(QStringLiteral("band.previous"), QStringLiteral("Previous band"), bandMode, G::Press, false, S::Slice, {QStringLiteral("H")});
    add(QStringLiteral("mode.next"), QStringLiteral("Mode next"), bandMode, G::Press, false, S::Slice, {QStringLiteral("X")});
    add(QStringLiteral("mode.previous"), QStringLiteral("Mode previous"), bandMode, G::Press, false, S::Slice, {QStringLiteral("Z")});

    add(QStringLiteral("zoom.in"), QStringLiteral("Zoom in"), display, G::Press, false, S::Pan, {QStringLiteral("=")});
    add(QStringLiteral("zoom.out"), QStringLiteral("Zoom out"), display, G::Press, false, S::Pan, {QStringLiteral("-")});

    add(QStringLiteral("nr.toggle"), QStringLiteral("Noise reduction"), dsp, G::Press, false, S::Slice, {QStringLiteral("R")});
    add(QStringLiteral("nb.toggle"), QStringLiteral("Noise blanker"), dsp, G::Press, false, S::Slice, {QStringLiteral("E")});
    add(QStringLiteral("anf.toggle"), QStringLiteral("Auto notch"), dsp, G::Press, false, S::Slice, {QStringLiteral("A")});
    add(QStringLiteral("rit.toggle"), QStringLiteral("RIT on or off"), dsp, G::Press, false, S::Slice, {QStringLiteral("I")});
    add(QStringLiteral("xit.toggle"), QStringLiteral("XIT on or off"), dsp, G::Press, false, S::Slice, {QStringLiteral("O")});

    add(QStringLiteral("tx.hold"), QStringLiteral("Hold to talk"), transmit, G::Hold, true, S::TransmitSlice, {QStringLiteral("Space")});
    add(QStringLiteral("tx.mox"), QStringLiteral("MOX"), transmit, G::Press, true, S::TransmitSlice, {QStringLiteral("Ctrl+Shift+M")});
    add(QStringLiteral("tx.tune"), QStringLiteral("TUNE"), transmit, G::Press, true, S::TransmitSlice, {QStringLiteral("Ctrl+T")});

    for (int n = 1; n <= kVoiceKeyerSlots; ++n) {
        add(QStringLiteral("voice.msg%1").arg(n), QStringLiteral("Send voice message %1").arg(n), voice,
            G::Press, true, S::TransmitSlice, {QStringLiteral("F%1").arg(n)});
    }
    for (int n = 1; n <= kVoiceKeyerSlots; ++n) {
        // Aufnehmen sendet nicht: der Mikrofonabgriff liest nur mit.
        add(QStringLiteral("voice.rec%1").arg(n), QStringLiteral("Record voice message %1").arg(n), voice,
            G::Press, false, S::Window, {QStringLiteral("Ctrl+Shift+F%1").arg(n)});
    }
    add(QStringLiteral("voice.stop"), QStringLiteral("Stop voice message or recording"), voice, G::Press, false,
        S::TransmitSlice, {QStringLiteral("Esc")});

    return a;
}

} // namespace

const QList<HotKeyAction>& HotKeyActionList::all()
{
    static const QList<HotKeyAction> kActions = buildActions();
    return kActions;
}

const HotKeyAction* HotKeyActionList::find(const QString& id)
{
    static const QHash<QString, const HotKeyAction*> kById = [] {
        QHash<QString, const HotKeyAction*> byId;
        for (const HotKeyAction& action : all()) {
            byId.insert(action.id, &action);
        }
        return byId;
    }();
    return kById.value(id, nullptr);
}

int HotKeyActionList::voiceSlotOf(const QString& id)
{
    QString number;
    if (id.startsWith(QLatin1String("voice.msg"))) {
        number = id.mid(9);
    } else if (id.startsWith(QLatin1String("voice.rec"))) {
        number = id.mid(9);
    } else {
        return -1;
    }
    bool ok = false;
    const int n = number.toInt(&ok);
    if (!ok || n < 1 || n > kVoiceKeyerSlots) {
        return -1;
    }
    return n - 1;
}

} // namespace Longpath
