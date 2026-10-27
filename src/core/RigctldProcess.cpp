// =================================================================
// src/core/RigctldProcess.cpp  (Longpath)
// =================================================================
//
// Longpath-original — siehe RigctldProcess.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
// =================================================================

#include "RigctldProcess.h"

namespace Longpath {

RigctldProcess::RigctldProcess(QObject* parent)
    : HamlibDaemonProcess(QStringLiteral("rigctld"), kDefaultPort, parent)
{
}

QString RigctldProcess::findBinary()
{
    return findBinaryNamed(QStringLiteral("rigctld"));
}

} // namespace Longpath
