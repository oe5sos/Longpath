#pragma once

// =================================================================
// src/core/QrzLogbookUploader.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// IMPORTANT — this is NOT the XML lookup service. QRZ runs two separate
// interfaces with separate credentials:
//
//   xmldata.qrz.com/xml/current  — callsign lookup, username + password,
//                                  needs an XML subscription.  (QrzClient)
//   logbook.qrz.com/api          — logbook upload, one API KEY taken
//                                  from the logbook's own settings page.
//
// The key is per-logbook, so an operator with several logbooks has
// several keys. Mixing the two up is the commonest way to get an
// unhelpful "invalid" back, which is why they are separate classes with
// separate settings rather than one "QRZ credentials" blob.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include "QsoUploader.h"

#include <QNetworkAccessManager>

class QNetworkReply;

namespace Longpath {

class QrzLogbookUploader : public QsoUploader {
    Q_OBJECT
public:
    explicit QrzLogbookUploader(QObject* parent = nullptr);

    void setApiKey(const QString& key);
    QString apiKey() const { return m_apiKey; }

    QString serviceName() const override;
    bool isConfigured() const override { return !m_apiKey.isEmpty(); }
    void upload(const LogEntry& entry) override;

    // Response bodies are `KEY=VALUE&KEY=VALUE` — not XML, despite the
    // sibling service. Public so tests can pin the parsing without a
    // network round trip.
    struct Result {
        bool    ok{false};
        bool    duplicate{false};
        QString message;
        QString logId;
    };
    static Result parseResponse(const QByteArray& body);

private:
    QNetworkAccessManager m_nam;
    QString m_apiKey;
};

} // namespace Longpath
