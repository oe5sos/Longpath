#pragma once

// =================================================================
// src/core/QrzClient.h  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/aethersdr/AetherSDR (GPLv3; see
//       LICENSE and About dialog for the live contributor list)
//
//   This file is a port of AetherSDR `src/core/QrzClient.{h,cpp}`
//   [@3a1f59e]. AetherSDR is licensed under the GNU General Public
//   License v3. NereusSDR is also GPLv3. Attribution follows GPLv3 §5
//   requirements.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace change; the
//                 AetherSDR LogManager category becomes a local
//                 Q_LOGGING_CATEGORY; user agent renamed. Upstream
//                 bug-fix comments are preserved verbatim — they
//                 document failures that are silent until they bite.
// =================================================================

#include "CallsignInfo.h"

#include <QNetworkAccessManager>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>

class QNetworkReply;

namespace NereusSDR {

// Minimal QRZ.com XML API client (https://xmldata.qrz.com/xml/current/).
//
// Handles the session-key lifecycle the API requires: log in once with
// username/password, reuse the returned key, and transparently
// re-login and retry when the server invalidates it (keys have no
// guaranteed lifetime and die on IP change). The key is memory-only.
//
// One lookup in flight at a time; callers queue freely.
//
// Access note: full callsign detail from the XML interface requires a
// QRZ XML subscription. A non-subscriber login still returns a session
// key with limited fields, and the server says so in the session
// message — testLogin() hands that back so the UI can report it
// plainly rather than looking broken.
class QrzClient : public QObject {
    Q_OBJECT

public:
    enum class Error {
        Network,     // transport failure / timeout
        AuthFailed,  // bad username/password
        NotFound,    // callsign not in the QRZ database
        Provider,    // any other server-reported error
    };
    Q_ENUM(Error)

    explicit QrzClient(QObject* parent = nullptr);

    void setCredentials(const QString& username, const QString& password);
    bool hasCredentials() const
    {
        return !m_username.isEmpty() && !m_password.isEmpty();
    }

    // Queue a lookup. Results arrive via lookupSucceeded/lookupFailed
    // keyed on the normalised call.
    void lookup(const QString& call);

    // Credential check for the settings dialog's Test button — logs in
    // without touching the client's stored session.
    void testLogin(const QString& username, const QString& password);

    // Parsed subset of a QRZ XML response. Public (with parseXml) so
    // tests can pin the parser against real <QRZDatabase>-wrapped
    // shapes: the root-element bug fixed upstream in #4043 broke login
    // end-to-end and no test caught it.
    struct ParsedResponse {
        QString      sessionKey;
        QString      sessionError;
        QString      sessionMessage;
        CallsignInfo info;
    };
    static ParsedResponse parseXml(const QByteArray& xml);

signals:
    // `call` is the queried (normalised) form; `info.call` is QRZ's
    // canonical base call, which can differ for portable or prefixed
    // queries — consumers must key their state on `call`, not
    // info.call (upstream #3990).
    void lookupSucceeded(const QString& call,
                         const NereusSDR::CallsignInfo& info);
    void lookupFailed(const QString& call, Error error,
                      const QString& message);
    void loginTestFinished(bool ok, const QString& message);

private:
    QNetworkReply* getUrl(const QString& query);
    void startNextLookup();
    void startLogin();
    void handleLoginReply(QNetworkReply* reply);
    void handleLookupReply(QNetworkReply* reply, const QString& call,
                           bool retriedAfterRelogin);

    QNetworkAccessManager m_nam;
    QString         m_username;
    QString         m_password;
    QString         m_sessionKey;      // memory-only
    QQueue<QString> m_pending;
    QSet<QString>   m_retriedAfterRelogin;
    bool            m_lookupInFlight{false};
    bool            m_loginInFlight{false};
};

} // namespace NereusSDR
