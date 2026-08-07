#pragma once

// =================================================================
// src/core/QsoUploader.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. No Thetis or AetherSDR equivalent — Thetis hands
// QSOs to an external logger, and AetherSDR's QRZ code is lookup-only.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "models/LogEntry.h"

#include <QObject>
#include <QString>

namespace NereusSDR {

// One place a logged contact can be sent.
//
// The interface exists from the first implementation rather than after
// the second, because the operator asked for "QRZ and the usual
// loggers" — Club Log, eQSL and LoTW all take the same ADIF record and
// differ only in endpoint and credential. Adding one should mean a new
// subclass, not a second copy of the send path.
//
// Every uploader is expected to:
//   - accept the record from LogEntry::toAdifRecord(), not a private
//     serialisation, so a contact is spelled identically everywhere;
//   - report exactly once per submitted entry, success or failure;
//   - treat a duplicate as success — re-uploading a QSO the service
//     already has is the normal result of a retry, not an error the
//     operator needs to see.
class QsoUploader : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~QsoUploader() override = default;

    // Name for the UI ("QRZ Logbook").
    virtual QString serviceName() const = 0;

    // False when the service has no credentials yet; the UI greys the
    // upload rather than letting it fail on every contact.
    virtual bool isConfigured() const = 0;

    // Send one contact. Result arrives via uploadFinished.
    virtual void upload(const LogEntry& entry) = 0;

signals:
    // `call` identifies which contact this is about — uploads may
    // overlap. `duplicate` marks "the service already had it", which
    // callers should treat as done, not as an error.
    void uploadFinished(const QString& call, bool ok, bool duplicate,
                        const QString& message);
};

} // namespace NereusSDR
