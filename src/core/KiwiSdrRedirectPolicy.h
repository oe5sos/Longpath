// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/core/KiwiSdrRedirectPolicy.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (NereusSDR):
//   2026-08-23 — Portiert (Nachtschicht, KiwiSDR). Namensraum und
//                Kopfdatei-Pfade angepasst, sonst zeichengetreu.

#pragma once

#include <QString>

class QUrl;

namespace Longpath::KiwiSdrRedirectPolicy {

QString canonicalHost(QString host);
QString proxyReceiverAlias(const QString& host);
bool isAllowedStatusRedirect(const QUrl& from, const QUrl& to,
                             QString* detail = nullptr);

} // namespace Longpath::KiwiSdrRedirectPolicy
