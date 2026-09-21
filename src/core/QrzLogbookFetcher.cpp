// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/QrzLogbookFetcher.cpp  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis port. See QrzLogbookFetcher.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "QrzLogbookFetcher.h"

#include <QCoreApplication>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QRegularExpression>
#include <QUrlQuery>

#include <algorithm>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcQrzFetch, "nereus.qrz.fetch")

constexpr int kTimeoutMs = 60'000;   // eine Seite mit 250 Datensaetzen

// `(^|&)NAME=WERT` — der Wert endet am naechsten `&` oder am Ende.
QString field(const QString& body, const char* name)
{
    const QRegularExpression re(QStringLiteral("(?:^|&)%1=([^&]*)").arg(QLatin1String(name)),
                                QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch m = re.match(body);
    return m.hasMatch() ? m.captured(1) : QString();
}
} // namespace

QrzLogbookFetcher::Page QrzLogbookFetcher::parsePage(const QByteArray& body)
{
    Page out;
    const QString text = QString::fromUtf8(body);

    out.ok = field(text, "RESULT").trimmed().compare(QLatin1String("OK"),
                                                       Qt::CaseInsensitive) == 0;
    out.reason = QUrl::fromPercentEncoding(field(text, "REASON").toUtf8()).trimmed();
    out.count  = field(text, "COUNT").trimmed().toInt();

    // LOGIDS: Komma-Liste. Die hoechste ist der Anker der naechsten Seite.
    const QString ids = field(text, "LOGIDS");
    for (const QString& s : ids.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        bool okNum = false;
        const qint64 v = s.trimmed().toLongLong(&okNum);
        if (okNum) { out.maxLogId = std::max(out.maxLogId, v); }
    }

    // ADIF: ab `ADIF=` der Rest — nicht bis zum naechsten `&`, denn ein
    // Kommentar darf ein `&` enthalten, und QRZ kodiert nicht.
    const QRegularExpression adifRe(QStringLiteral("(?:^|&)ADIF="),
                                    QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch am = adifRe.match(text);
    if (am.hasMatch()) {
        QString adif = text.mid(am.capturedEnd());
        // Doch kodiert? Dann kein `<`, aber `%3C`.
        if (!adif.contains(QLatin1Char('<')) && adif.contains(QLatin1String("%3C"), Qt::CaseInsensitive)) {
            adif = QUrl::fromPercentEncoding(adif.toUtf8());
        }
        out.adif = adif;
    }

    // Ohne LOGIDS: die Kennungen stehen auch in jedem Datensatz.
    if (out.maxLogId < 0 && !out.adif.isEmpty()) {
        const QRegularExpression idRe(QStringLiteral("<APP_QRZLOG_LOGID:\\d+>(\\d+)"),
                                      QRegularExpression::CaseInsensitiveOption);
        auto it = idRe.globalMatch(out.adif);
        while (it.hasNext()) {
            out.maxLogId = std::max(out.maxLogId, it.next().captured(1).toLongLong());
        }
    }
    // COUNT fehlt manchmal; dann zaehlen wir die Datensaetze selbst.
    if (out.count == 0 && !out.adif.isEmpty()) {
        out.count = out.adif.count(QLatin1String("<EOR>"), Qt::CaseInsensitive);
    }
    return out;
}

QrzLogbookFetcher::QrzLogbookFetcher(QObject* parent) : QObject(parent) {}

void QrzLogbookFetcher::fetchAll()
{
    if (m_busy) { return; }
    if (!isConfigured()) {
        emit finished(false, QString(), 0, QStringLiteral("No QRZ logbook key"));
        return;
    }
    m_busy = true;
    m_adif.clear();
    m_records = 0;
    m_pages = 0;
    m_lastAfter = -1;
    requestPage(0);
}

void QrzLogbookFetcher::abort()
{
    if (m_inFlight) { m_inFlight->abort(); }
}

void QrzLogbookFetcher::requestPage(qint64 afterLogId)
{
    QUrlQuery form;
    form.addQueryItem(QStringLiteral("KEY"), m_apiKey);
    form.addQueryItem(QStringLiteral("ACTION"), QStringLiteral("FETCH"));
    QString option = QStringLiteral("MAX:%1").arg(kPageSize);
    if (afterLogId > 0) {
        option += QStringLiteral(",AFTERLOGID:%1").arg(afterLogId);
    }
    form.addQueryItem(QStringLiteral("OPTION"), option);

    QNetworkRequest req(m_endpoint);
    req.setHeader(QNetworkRequest::ContentTypeHeader,
                  QStringLiteral("application/x-www-form-urlencoded"));
    // QRZ verlangt einen erkennbaren User-Agent.
    req.setHeader(QNetworkRequest::UserAgentHeader,
                  QStringLiteral("Longpath/%1 (+https://www.longpath.at)")
                      .arg(QCoreApplication::applicationVersion()));
    req.setTransferTimeout(kTimeoutMs);

    m_lastAfter = afterLogId;
    QNetworkReply* reply = m_nam.post(req, form.toString(QUrl::FullyEncoded).toUtf8());
    m_inFlight = reply;
    connect(reply, &QNetworkReply::finished, this,
            [this, reply, afterLogId]() { onReply(reply, afterLogId); });
}

void QrzLogbookFetcher::onReply(QNetworkReply* reply, qint64 afterLogId)
{
    reply->deleteLater();
    m_inFlight = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        done(false, reply->errorString());
        return;
    }
    const Page page = parsePage(reply->readAll());
    ++m_pages;

    if (!page.ok) {
        // Auf der ersten Seite ist FAIL ein Fehler (Schluessel, Abo).
        // Auf einer spaeteren bedeutet „nichts mehr" nur: fertig.
        const bool nothingMore = m_pages > 1
            && (page.count == 0 || page.reason.contains(QLatin1String("no "), Qt::CaseInsensitive)
                || page.reason.contains(QLatin1String("not found"), Qt::CaseInsensitive));
        if (nothingMore) { done(true, QString()); return; }
        done(false, page.reason.isEmpty() ? QStringLiteral("QRZ answered FAIL") : page.reason);
        return;
    }

    if (!page.adif.isEmpty()) {
        m_adif += page.adif;
        if (!m_adif.endsWith(QLatin1Char('\n'))) { m_adif += QLatin1Char('\n'); }
    }
    m_records += page.count;
    emit progress(m_records, m_pages);
    qCDebug(lcQrzFetch) << "page" << m_pages << "after" << afterLogId
                        << "count" << page.count << "maxLogId" << page.maxLogId;

    // Weiter, solange eine Seite voll war und QRZ eine Kennung zum
    // Anknuepfen nannte, die vorwaerts fuehrt.
    const bool full = page.count >= kPageSize;
    const qint64 next = page.maxLogId + 1;
    if (full && page.maxLogId >= 0 && next > afterLogId && m_pages < kMaxPages) {
        requestPage(next);
        return;
    }
    done(true, QString());
}

void QrzLogbookFetcher::done(bool ok, const QString& error)
{
    m_busy = false;
    const QString adif = m_adif;
    const int records = m_records;
    m_adif.clear();
    emit finished(ok, ok ? adif : QString(), ok ? records : 0, error);
}

} // namespace Longpath
