// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/sat/TleStore.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Bahndaten (TLE) fuer die Amateurfunksatelliten: holt die Gruppe
// „amateur" von CelesTrak, legt sie als Datei ab und liest sie beim
// naechsten Start wieder — ohne Netz gibt es die letzte Fassung, mit
// Netz einmal am Tag eine frische. CelesTrak bittet ausdruecklich, nicht
// oefter als noetig zu laden; ein TLE ist tagelang brauchbar.
//
// Der Store kennt nur Text und Zeitpunkte; rechnen tut SatelliteTracker.
// =================================================================
// Modification history (Longpath):
//   2026-09-21 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include <QDateTime>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkReply;

namespace Longpath {

class TleStore : public QObject {
    Q_OBJECT
public:
    explicit TleStore(QObject* parent = nullptr);

    // Standard: <AppData>/tle/amateur.tle
    void    setCachePath(const QString& path) { m_cachePath = path; }
    QString cachePath() const { return m_cachePath; }

    // Standard: CelesTrak, Gruppe „amateur", Format TLE.
    void setSourceUrl(const QUrl& url) { m_source = url; }
    QUrl sourceUrl() const { return m_source; }

    // Liest die Datei; true, wenn etwas drinstand.
    bool loadCached();

    QString   text() const { return m_text; }
    QDateTime fetchedAt() const { return m_fetchedAt; }   // Dateizeit, UTC
    bool      hasData() const { return !m_text.isEmpty(); }
    bool      isStale(int maxAgeHours = 24) const;

    // Laedt vom Netz; bei Erfolg Datei schreiben + updated(). Ein zweiter
    // Aufruf waehrend einer laufenden Anfrage tut nichts.
    void refresh();
    void refreshIfStale(int maxAgeHours = 24);
    bool isBusy() const { return m_reply != nullptr; }

signals:
    void updated(const QString& text);
    void failed(const QString& why);

private:
    void onFinished();
    static bool looksLikeTle(const QString& text);

    QString               m_cachePath;
    QUrl                  m_source;
    QString               m_text;
    QDateTime             m_fetchedAt;
    QNetworkAccessManager m_nam;
    QNetworkReply*        m_reply{nullptr};
};

} // namespace Longpath
