// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/gui/applets/RoundtableApplet.h  (Longpath)
// =================================================================
//
// Roundtable in the right-hand column (design 11.2 Panel): the
// RoundtablePanel following the active slice, and its own window
// (RoundtableWindow) when the panel's "Open in its own window" asks for it.
// While the panel is in its window the applet says so and offers to put it
// back.
//
// The panel's own reading ("You", shown only here and marked NOT SENT while
// nothing about the station is sent) is the followed slice's S-meter as the
// hub reads it (RoundtableHub::sliceMeterRead). Tune A here tunes the
// followed slice: the room's frequency and, when the slice is in another
// family, the family's mode. Never while transmitting (overlay design D22).
//
// Uebernommen aus dem Upstream NereusSDR
// (`src/gui/applets/RoundtableApplet.h` @ e56b3b44c, J.J. Boyd KG4VCF).
// Longpath-Abweichungen:
//   - Kennung "Roundtable" (Panelkennung = appletId, check-applet-ids.py).
//   - Longpaths AppletWidget hat keine eigene Abloese-Schnittstelle
//     (canFloat/createFloatingWindow/AppletPanelWidget::floatApplet): das
//     eigene Fenster macht das Applet selbst auf und zu. Das Abloesen der
//     Zelle als Ganzes (AppletFloatingWindow) bleibt davon unberuehrt.
//   - Keine Aufnahme mit Zurueckspulen bei uns (RecordingsAccess):
//     notePastDocument, updatePastPause und contextHz entfallen.
//   - Der eigene Messwert kommt vom Hub, nicht aus SliceModel::
//     meterReadingNsChanged (das es bei uns nicht gibt).
//   - Settings oeffnet den Reiter "Roundtable" im Spot Hub (dort steht die
//     Erstfrage; eine zweite gibt es nicht).
//   - Namensraum Longpath.
//
// Modification history (NereusSDR):
//   2026-11-16: While the slice plays the past the panel stays on the room
//               where it was live, says it is paused, and shows no own
//               reading. J.J. Boyd (KG4VCF), AI-assisted via Anthropic
//               Claude Code.
//   2026-11-04: A tune leaves a read the open inspector holds. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-04: setSliceSource: a window gives the applet its own slice, so the
//               panel's label, stream and Tune agree. J.J. Boyd (KG4VCF),
//               AI-assisted via Anthropic Claude Code.
//   2026-11-03: Tune is refused and its button greyed while keyed (overlay design
//               D22). J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-30: Approved frequency-first Roundtable desktop surfaces.
//               J.J. Boyd (KG4VCF), AI-assisted via OpenAI Codex.
//   2026-10-24: original implementation for NereusSDR by J.J. Boyd
//               (KG4VCF), with AI-assisted implementation via Anthropic
//               Claude Code (Roundtable plan Task 13).
//   2026-10-24: Roundtable plan Task 15: the active slice's meter is the
//               panel's own reading (setOwnReading). J.J. Boyd (KG4VCF),
//               AI-assisted via Claude Code.
//   2026-10-24: host() falls back to the panel named on the applet, for a
//               managed workspace. J.J. Boyd (KG4VCF), AI-assisted via
//               Claude Code.
//   2026-10-24: tuneToRoom, for a panadapter tag (Roundtable plan Task 14).
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
//   2026-10-25: the popped-out note takes the workspace look's roles.
//               J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude Code.
// Modification history (Longpath):
//   2026-11-18 - Uebernommen fuer Longpath (Roundtable Stufe 2) von
//                Martin Fischer (OE5SOS), KI-gestuetzt ueber Anthropic
//                Claude.
// =================================================================
// no-port-check: NereusSDR-original, in Longpath uebernommen.

#include "AppletWidget.h"
#include "gui/roundtable/RoundtableWindow.h"
#include "models/RoundtableModel.h"

#include <QPointer>
#include <QUrl>

#include <functional>
#include <utility>

class QVBoxLayout;

namespace Longpath {

class RoundtableHub;
class RoundtablePanel;
class SliceModel;

class RoundtableApplet : public AppletWidget {
    Q_OBJECT

public:
    /// The Roundtable of `model` (RadioModel::roundtableHub()).
    explicit RoundtableApplet(RadioModel* model, QWidget* parent = nullptr);
    /// For tests: a hub of its own; `model` may be null.
    RoundtableApplet(RadioModel* model, RoundtableHub* hub, QWidget* parent);
    ~RoundtableApplet() override;

    QString appletId() const override { return QStringLiteral("Roundtable"); }
    QString appletTitle() const override { return QStringLiteral("Roundtable"); }
    void syncFromModel() override;

    /// The panel in its own window (the panel's "Open in its own window").
    QWidget* createFloatingWindow();
    /// The panel back here.
    void dockFloatingWindow();

    RoundtablePanel* panel() const { return m_panel; }
    /// The own window once made (it lives as long as the applet).
    RoundtableWindow* floatingWindow() const { return m_window; }
    bool isPoppedOut() const;
    RoundtableModel* roundtable() const { return m_roundtable; }

    /// Tunes the followed slice to room `roomId` from the band list, as
    /// Tune A here does: a tag in the panadapter's strip (design 11.2).
    void tuneToRoom(const QString& roomId);

    /// Which slice this applet follows, Tunes and reads for, asked again on
    /// every syncFromModel. Unset, the radio's active slice. Null means none.
    void setSliceSource(std::function<SliceModel*()> source);

    /// What opens the Archive's sign-in link (a seam for tests).
    void setUrlOpener(std::function<void(const QUrl&)> opener) { m_urlOpener = std::move(opener); }

signals:
    /// The panel's Settings: open Roundtable's settings (Spot Hub's tab).
    void settingsRequested();
    void floatingWindowDocked();

private:
    void build();
    void bindActiveSlice();
    void tuneHere(qint64 hz, const QString& family);
    void openArchive();
    void popOut();
    void putBack();

    QPointer<RoundtableHub> m_hub;
    QPointer<RoundtableModel> m_roundtable;
    RoundtablePanel* m_panel = nullptr;
    QVBoxLayout* m_layout = nullptr;
    QWidget* m_poppedNote = nullptr;
    QPointer<RoundtableWindow> m_window;
    QPointer<SliceModel> m_slice;
    std::function<SliceModel*()> m_sliceSource;
    int m_boundSliceId = -2;  // the slice last bound; -1 for none, -2 before the first bind
    QMetaObject::Connection m_sliceFrequency;
    QMetaObject::Connection m_sliceMode;
    std::function<void(const QUrl&)> m_urlOpener;
    bool m_webLinkPending = false;
};

} // namespace Longpath
