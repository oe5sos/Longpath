// =================================================================
// src/gui/applets/DvkApplet.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   NereusSDR is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-18 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Port of AetherSDR `src/gui/DvkPanel.{h,cpp}` (DVK F-key
//                 slot grid + record/play controls). Renamed to
//                 DvkApplet in NereusSDR. All controls NYI.
// =================================================================

#pragma once
#include "AppletWidget.h"

class QPushButton;
class QLabel;
class QSlider;

namespace Longpath {

class HGauge;

// Digital Voice Keyer — voice memory playback/record.
// NYI — Phase 3I-1 (Basic SSB TX, mic input, MOX state machine).
//
// Controls:
//   1. Voice keyer slots    — 4 rows: QLabel "Slot N" + "●Rec" + "▶Play" + "■Stop"
//   2. Record button+level  — QPushButton "Record" + HGauge (0-100)
//   3. Repeat toggle+interval — QPushButton green "Rpt" + QSlider (1..60s) + inset
//   4. Semi break-in toggle — QPushButton green "Semi BK"
//   5. WAV file import      — QPushButton "Import WAV..."
class DvkApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit DvkApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("dvk"); }
    QString appletTitle() const override { return QStringLiteral("Voice Keyer"); }
    void    syncFromModel() override;

private:
    void buildUI();

    // Zehn Plaetze, nicht vier (2026-08-19, auf Ansage des Betreibers:
    // „10 audio files aufnehmen … mit shortcuts verbinden … als CQ CALL
    // abspielen"). Die Zeile zeigt Taste, Beschriftung, Dauer und die
    // drei Knoepfe.
    static constexpr int kSlots = 10;

    // Die Zeile selbst, damit sie einen Grund tragen kann: Zebra, und
    // waehrend der Aufnahme die ganze Zeile rot. Ein roter Punkt allein
    // wird fuer „auf Sendung" gehalten.
    QWidget*     m_slotRow[kSlots]   = {};
    QLabel*      m_slotKey[kSlots]   = {};
    QLabel*      m_slotLabel[kSlots] = {};
    QLabel*      m_slotLen[kSlots]   = {};
    QPushButton* m_recBtn[kSlots]    = {};
    QPushButton* m_playBtn[kSlots]   = {};
    QPushButton* m_stopBtn[kSlots]   = {};
    QPushButton* m_loadBtn[kSlots]   = {};

    void refreshSlot(int index);

    // Zebra, oder rot wenn diese Zeile gerade aufnimmt.
    void applyRowStyle(int index);

    // Aufnahme vom Mikrofon in einen Platz. Nimmt denselben Weg wie der
    // Sprach-Selbsttest: TxWorkerThread::preStripAudioReady in einen
    // TxAudioRecorder, also VOR der Sprachbearbeitung. Eine Ansage soll
    // klingen wie die Stimme, nicht wie die Stimme durch den heutigen
    // Kompressor — die Bearbeitung kommt beim Senden ohnehin darueber.
    //
    // SENDET NICHT. Der Abgriff liest mit, was das Mikrofon liefert;
    // getastet wird nichts.
    void startRecording(int index);
    void finishRecording();

    class TxAudioRecorder* m_recorder = nullptr;
    QMetaObject::Connection m_micTap;
    int   m_recordingSlot = -1;

    // Control 2 — record level gauge (0-100)
    HGauge*      m_recLevel      = nullptr;

    // Control 3 — repeat toggle + interval slider
    QPushButton* m_repeatBtn     = nullptr;
    QSlider*     m_repeatSlider  = nullptr;
    QLabel*      m_repeatValue   = nullptr;

    // Control 4 — semi break-in toggle
    QPushButton* m_semiBkBtn     = nullptr;

    // Control 5 — WAV import button
    QPushButton* m_importBtn     = nullptr;
};

} // namespace Longpath
