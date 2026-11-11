// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/gui/hotkeys/HotKeyTargets.h  (Longpath)
// =================================================================
//
// Was der Verteiler ruft. Die echte Umsetzung (MainWindowHotKeyTargets)
// geht an dieselben Stellen wie die Bedienelemente auf dem Schirm; die
// Pruefstaende nehmen eine Attrappe.
//
// Uebernommen aus dem Upstream NereusSDR (`src/gui/hotkeys/
// HotKeyTargets.h` @ 9036ffc04, J.J. Boyd KG4VCF). Longpath hat keinen
// entfernten Kern und keine fremden Geraete am selben Funkgeraet; was
// Upstream dafuer braucht (sliceRefusal, listened, Kern-CW), entfaellt.
// Dazu kommt runVoiceAction fuer den Sprachspeicher, den Upstream noch
// nicht anschliesst, und takesKey, damit Esc nur dann geschluckt wird,
// wenn es etwas zu stoppen gibt.
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Uebernommen und gekuerzt fuer Longpath von Martin
//                Fischer (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include <QChar>
#include <QString>
#include <QtGlobal>

namespace Longpath {

class SliceModel;

enum class HotKeySourceKind { Keyboard, Midi, Knob, Deck };

// Ein Ding, das eine Taste halten kann. instance trennt zwei gleiche.
struct HotKeySource {
    HotKeySourceKind kind;
    QString instance;

    bool operator==(const HotKeySource& other) const
    {
        return kind == other.kind && instance == other.instance;
    }
};

class HotKeyTargets {
public:
    virtual ~HotKeyTargets() = default;

    virtual SliceModel* windowSlice() const = 0;
    virtual SliceModel* sliceByLetter(QChar letter) const = 0;
    virtual QChar letterOf(const SliceModel* slice) const = 0;
    virtual bool tuneTo(SliceModel* slice, double hz) = 0;          // derselbe Weg wie ein Klick ins Panorama
    virtual void runSliceAction(const QString& id, SliceModel* slice, int steps) = 0;
    virtual void runWindowAction(const QString& id) = 0;
    virtual QString unavailableReason(const QString& id) const = 0;  // leer, wenn verfuegbar
    // Ob die Taste gerade etwas zu tun hat. false: der Druck geht an das
    // Fenster weiter, als gaebe es die Belegung nicht (Esc ohne laufende
    // Ansage schliesst weiter, was Esc vorher schloss).
    virtual bool takesKey(const QString& id) const { Q_UNUSED(id); return true; }
    virtual bool transmitting() const = 0;                          // MOX oder TUNE an
    virtual bool tuneOn() const = 0;                                // TUNE bestaetigt an
    virtual bool keyHold(bool down) = 0;                            // true, wenn der Druck angenommen wurde
    virtual void runTransmitAction(const QString& id) = 0;          // tx.mox, tx.tune
    virtual void runVoiceAction(const QString& id) = 0;             // voice.msgN, voice.recN, voice.stop
    virtual void toast(const QString& text) = 0;
    virtual qint64 nowMs() const = 0;
};

} // namespace Longpath
