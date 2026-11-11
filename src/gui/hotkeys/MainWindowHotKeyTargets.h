// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/gui/hotkeys/MainWindowHotKeyTargets.h  (Longpath)
// =================================================================
//
// Was die Tasten im echten Fenster tun: dieselben Wege wie die
// Bedienelemente auf dem Schirm.
//
//   Abstimmen       wie ein Klick ins Panorama (Sperre meldet sich,
//                   Begleitgeraet folgt) -- MainWindow::stimmeVomBediener
//   Schrittweite    die Leiter des RX-Panels (kStageOneStepLadder)
//   Filter          die Vorgaben der Befehlsleiste (FilterPresetStore)
//   Band            MainWindow::bandUmschalten, der eine Weg aller
//                   Bandknoepfe
//   Betriebsart     die Liste des RX-Panels
//   Zoom            die Lupe im Panorama (Faktor 1,5)
//   NR/NB/ANF/RIT   die Knoepfe des RX-Panels
//   MOX/TUNE/PTT    MoxController und RadioModel::setTune wie das
//                   TX-Panel; die Sprechtaste ueber onSpacePtt, das seit
//                   Phase 3M auf genau diesen Anschluss wartete
//   Sprachspeicher  VoiceKeyerPlayer
//
// Uebernommen in der Gestalt aus dem Upstream NereusSDR (`src/gui/
// hotkeys/MainWindowHotKeyTargets.h/.cpp` @ 9036ffc04, J.J. Boyd KG4VCF);
// der Inhalt ist fuer Longpath neu geschrieben, weil Longpaths Fenster
// und Bedienwege andere sind (keine Behaelterknoepfe, kein entfernter
// Kern, kein Bandstapel).
//
// Modification history (NereusSDR):
//   2026-11-05 - J.J. Boyd (KG4VCF), hot keys plan.
//                AI-assisted implementation via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-10 - Fuer Longpath neu geschrieben von Martin Fischer
//                (OE5SOS), KI-gestuetzt ueber Anthropic Claude.
// =================================================================
// no-port-check: NereusSDR-original in der Gestalt, Inhalt Longpath-original.

#include "gui/hotkeys/HotKeyTargets.h"

#include <QElapsedTimer>
#include <QHash>

namespace Longpath {

class MainWindow;
class MoxController;
class RadioModel;

class MainWindowHotKeyTargets final : public HotKeyTargets {
public:
    explicit MainWindowHotKeyTargets(MainWindow* window);

    SliceModel* windowSlice() const override;
    SliceModel* sliceByLetter(QChar letter) const override;
    QChar letterOf(const SliceModel* slice) const override;
    bool tuneTo(SliceModel* slice, double hz) override;
    void runSliceAction(const QString& id, SliceModel* slice, int steps) override;
    void runWindowAction(const QString& id) override;
    QString unavailableReason(const QString& id) const override;
    bool takesKey(const QString& id) const override;
    bool transmitting() const override;
    bool tuneOn() const override;
    bool keyHold(bool down) override;
    void runTransmitAction(const QString& id) override;
    void runVoiceAction(const QString& id) override;
    void toast(const QString& text) override;
    qint64 nowMs() const override;

    // Die Taste einer Handlung, wie der Betreiber sie liest (auf dem Mac
    // mit ⌘). Leer, wenn keine belegt ist.
    QString keyTextFor(const QString& id) const;

private:
    RadioModel* model() const;
    MoxController* mox() const;
    void stepTuneStep(SliceModel* slice, int direction);
    void walkFilter(SliceModel* slice, int direction);
    void walkMode(SliceModel* slice, int direction);
    void walkBand(SliceModel* slice, int direction);
    void changeVolume(SliceModel* slice, int delta);
    void toggleNr(SliceModel* slice);
    void cycleNb(SliceModel* slice);
    void selectNextSlice();
    // setMox/onSpacePtt rufen und eine Ablehnung als Hinweis zeigen.
    bool keyWithReason(bool on, bool space);

    MainWindow* m_window;
    QElapsedTimer m_clock;
    // Die zuletzt benutzte Rauschminderung je Empfaenger: R schaltet sie
    // aus und dieselbe wieder ein, nicht irgendeine.
    QHash<int, int> m_lastNr;
};

} // namespace Longpath
