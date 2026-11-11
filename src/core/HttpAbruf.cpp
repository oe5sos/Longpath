// no-port-check: Longpath-original, no Thetis logic.
//
// src/core/HttpAbruf.cpp  (Longpath) -- siehe HttpAbruf.h.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#include "core/HttpAbruf.h"

#include <QCoreApplication>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

namespace Longpath {

HttpAbruf standardHttpAbruf(QObject* besitzer, const QString& userAgent)
{
    auto* nam = new QNetworkAccessManager(besitzer);
    QPointer<QNetworkAccessManager> schwach(nam);
    return [schwach, userAgent](const QUrl& url,
                                std::function<void(const HttpAntwort&)> fertig) {
        if (QStandardPaths::isTestModeEnabled() || !schwach) {
            // Kein Netz in Pruefstaenden -- aber dieselbe Rueckmeldung
            // wie bei einem echten Ausfall, damit der Weg dahinter laeuft.
            QObject* kontext = schwach ? static_cast<QObject*>(schwach.data())
                                       : QCoreApplication::instance();
            QTimer::singleShot(0, kontext, [fertig]() {
                HttpAntwort a;
                a.netzFehler = QStringLiteral("offline (test mode)");
                fertig(a);
            });
            return;
        }
        QNetworkRequest req(url);
        req.setHeader(QNetworkRequest::UserAgentHeader, userAgent);
        req.setTransferTimeout(30000);
        QNetworkReply* reply = schwach->get(req);
        QObject::connect(reply, &QNetworkReply::finished, reply, [reply, fertig]() {
            reply->deleteLater();
            HttpAntwort a;
            a.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            a.body = reply->readAll();
            // Ein HTTP-Fehler (400 "Not found") ist eine Antwort, kein
            // Netzfehler: der Aufrufer entscheidet, was sie bedeutet.
            if (a.status == 0 && reply->error() != QNetworkReply::NoError) {
                a.netzFehler = reply->errorString();
            }
            fertig(a);
        });
    };
}

} // namespace Longpath
