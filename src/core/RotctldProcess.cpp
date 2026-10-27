// =================================================================
// src/core/RotctldProcess.cpp  (Longpath)
// =================================================================
//
// Longpath-original — see RotctldProcess.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
//   2026-10-27 — Rumpf nach HamlibDaemonProcess ausgelagert.
// =================================================================

#include "RotctldProcess.h"

namespace Longpath {

RotctldProcess::RotctldProcess(QObject* parent)
    : HamlibDaemonProcess(QStringLiteral("rotctld"), kDefaultPort, parent)
{
}

QString RotctldProcess::findBinary()
{
    return findBinaryNamed(QStringLiteral("rotctld"));
}

} // namespace Longpath
