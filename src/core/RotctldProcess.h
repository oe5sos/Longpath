#pragma once

// =================================================================
// src/core/RotctldProcess.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Starts Hamlib's rotctld so the operator does not have to.
//
// Der Rumpf dieser Klasse steht seit dem 2026-10-27 in
// HamlibDaemonProcess: Suche nach dem Programm, Portwahl, Start,
// Abbruch, Neustart, der Grund aus Hamlibs stderr. Hier bleibt nur
// noch, WELCHER Daemon gemeint ist -- rotctld auf 4533. Der Anlass
// war der CAT-Weg (RigctldProcess), der denselben Rumpf braucht; eine
// Abschrift waere die zweite Stelle gewesen, an der jede kuenftige
// Behebung haette landen muessen.
//
// Die statischen Funktionen bleiben unter diesem Namen erreichbar,
// damit kein Aufrufer und kein Pruefstand angefasst werden muss.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-09-16 — Takes a free port when the preferred one is held (a
//                 rotctld orphaned by an earlier crash was sitting on
//                 4533 with the wrong device, live); restart() for a
//                 daemon whose controller link has died; exited() no
//                 longer fires for a stop this object asked for.
//                 AI-assisted via Anthropic Claude (Claude Code),
//                 operator Martin Fischer.
//   2026-10-27 — Rumpf nach HamlibDaemonProcess ausgelagert; diese
//                 Klasse ist jetzt die rotctld-Auspraegung davon.
//                 Verhalten unveraendert. KI-gestuetzt ueber
//                 Anthropic Claude Code.
// =================================================================

#include "core/HamlibDaemonProcess.h"

namespace Longpath {

class RotctldProcess : public HamlibDaemonProcess {
    Q_OBJECT
public:
    /// rotctlds Vorgabeport.
    static constexpr quint16 kDefaultPort = 4533;

    explicit RotctldProcess(QObject* parent = nullptr);

    /// Wo rotctld liegt, oder leer. Siehe findBinaryNamed().
    static QString findBinary();
};

} // namespace Longpath
