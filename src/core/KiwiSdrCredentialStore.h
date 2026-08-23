// SPDX-License-Identifier: GPL-3.0-or-later
//
// Ported from AetherSDR src/core/KiwiSdrCredentialStore.h [@31b29583].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3)
//
// Modification history (NereusSDR):
//   2026-08-23 — Portiert (Nachtschicht, KiwiSDR). Namensraum und
//                Kopfdatei-Pfade angepasst, sonst zeichengetreu.

#pragma once

#include <QString>

#include <functional>
#include <memory>

class QObject;

namespace Longpath {

enum class KiwiSdrCredentialResultCode {
    Success,
    NotFound,
    Error,
};

struct KiwiSdrCredentialResult {
    KiwiSdrCredentialResultCode code{KiwiSdrCredentialResultCode::Success};
    QString value;
    QString error;
};

class IKiwiSdrCredentialStore {
public:
    using Callback = std::function<void(const KiwiSdrCredentialResult&)>;

    virtual ~IKiwiSdrCredentialStore() = default;
    // Implementations preserve submission order for operations sharing a key.
    // Persistent implementations also own queued work independently of the
    // callback context so a manager teardown cannot discard a later write or
    // delete that has already been handed off.
    virtual bool isPersistent() const = 0;
    virtual void read(const QString& key, QObject* context,
                      Callback callback) = 0;
    virtual void write(const QString& key, const QString& value,
                       QObject* context, Callback callback) = 0;
    virtual void remove(const QString& key, QObject* context,
                        Callback callback) = 0;
};

std::shared_ptr<IKiwiSdrCredentialStore>
createDefaultKiwiSdrCredentialStore();

} // namespace Longpath
