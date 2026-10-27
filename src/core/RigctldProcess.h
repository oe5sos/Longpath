#pragma once

// =================================================================
// src/core/RigctldProcess.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Startet Hamlibs rigctld, damit der Betreiber es nicht tun muss.
//
// ── Anlass ──────────────────────────────────────────────────────────
//
// 2026-10-27, die 2-m-Conteststation: ein klassisches Funkgeraet
// (TS-590S oder K3) arbeitet als ZF hinter einem Transverter,
// Longpath haengt mit einem SDR am ZF-Ausgang und zeigt das Band.
// Damit die Anzeige weiss, WO im Band das Geraet gerade steht, muss
// Longpath es fragen -- und das geht ueber rigctld.
//
// Longpath spricht rigctld bisher nur als SERVER (RigctldServer:
// fremde Logbuchprogramme steuern Longpath). Dies ist die andere
// Richtung.
//
// Der Rumpf steht in HamlibDaemonProcess; hier steht nur, welcher
// Daemon gemeint ist -- rigctld auf 4532, dem Geschwisterport zu
// rotctlds 4533.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "core/HamlibDaemonProcess.h"

namespace Longpath {

class RigctldProcess : public HamlibDaemonProcess {
    Q_OBJECT
public:
    /// rigctlds Vorgabeport -- das Geschwister zu rotctlds 4533.
    static constexpr quint16 kDefaultPort = 4532;

    explicit RigctldProcess(QObject* parent = nullptr);

    /// Wo rigctld liegt, oder leer. Siehe findBinaryNamed().
    static QString findBinary();
};

} // namespace Longpath
