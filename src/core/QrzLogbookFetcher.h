// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/QrzLogbookFetcher.h  (Longpath)
// =================================================================
//
// Longpath-original. No Thetis port.
//
// Das Gegenstück zu QrzLogbookUploader: holt das QRZ-Logbuch AB. Gleiche
// Schnittstelle (logbook.qrz.com/api, ein Logbuch-Schlüssel, nicht das
// XML-Login), Aktion FETCH statt INSERT.
//
// Warum seitenweise: QRZ liefert ohne MAX das ganze Logbuch in einer
// Antwort — bei sechstausend Verbindungen ein Megabyte, und wenn die
// Leitung mittendrin reisst, ist nichts davon da. Die Anleitung von QRZ
// empfiehlt MAX:250 und AFTERLOGID auf die hoechste gelieferte Kennung
// plus eins, bis weniger als 250 kommen. Genau das tut fetchAll().
//
// Warum ein eigener Antwort-Parser: die Antwort SIEHT aus wie
// `RESULT=OK&COUNT=25&LOGIDS=…&ADIF=…`, aber die ADIF-Daten darin sind
// nicht kodiert — rohe `<`, `>` und `:` mitten im Body. QUrlQuery
// zerlegt das falsch. parsePage() zieht die bekannten Felder mit
// Mustern heraus und nimmt ab `ADIF=` den Rest; kommt es doch einmal
// prozentkodiert (kein `<`, aber `%3C`), wird dekodiert.
//
// =================================================================
// Modification history (Longpath):
//   2026-09-21 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QNetworkReply;

namespace Longpath {

class QrzLogbookFetcher : public QObject {
    Q_OBJECT
public:
    explicit QrzLogbookFetcher(QObject* parent = nullptr);

    void setApiKey(const QString& key) { m_apiKey = key.trimmed(); }
    QString apiKey() const { return m_apiKey; }
    bool isConfigured() const { return !m_apiKey.isEmpty(); }

    /// Das ganze Logbuch, seitenweise. Asynchron; am Ende finished().
    /// Ein zweiter Aufruf, solange einer laeuft, tut nichts.
    void fetchAll();
    bool isBusy() const { return m_busy; }
    void abort();

    static constexpr int kPageSize = 250;
    static constexpr int kMaxPages = 400;   // 100 000 Verbindungen — genug

    /// Eine Seite der Antwort, zerlegt.
    struct Page {
        bool        ok{false};
        QString     reason;          // bei FAIL: der Grund von QRZ
        int         count{0};        // COUNT
        qint64      maxLogId{-1};    // hoechste app_qrzlog_logid der Seite
        QString     adif;            // die ADIF-Datensaetze, roh
    };
    static Page parsePage(const QByteArray& body);

    /// Fuer Tests: ein anderer Endpunkt (lokaler Mock-Server).
    void setEndpointForTest(const QUrl& url) { m_endpoint = url; }

signals:
    /// Nach jeder Seite: wie viele Datensaetze bisher, welche Seite.
    void progress(int recordsSoFar, int page);
    /// Am Ende: ok mit allen ADIF-Datensaetzen (aneinandergehaengt) und
    /// ihrer Zahl — oder nicht ok mit dem Grund.
    void finished(bool ok, const QString& adif, int records, const QString& error);

private:
    void requestPage(qint64 afterLogId);
    void onReply(QNetworkReply* reply, qint64 afterLogId);
    void done(bool ok, const QString& error);

    QNetworkAccessManager m_nam;
    QString m_apiKey;
    QUrl    m_endpoint{QStringLiteral("https://logbook.qrz.com/api")};
    bool    m_busy{false};
    QNetworkReply* m_inFlight{nullptr};
    QString m_adif;
    int     m_records{0};
    int     m_pages{0};
    qint64  m_lastAfter{-1};
};

} // namespace Longpath
