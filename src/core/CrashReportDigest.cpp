// =================================================================
// src/core/CrashReportDigest.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Zweck und Grenzen: CrashReportDigest.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include "CrashReportDigest.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QStandardPaths>

namespace Longpath {

QString CrashReportDigest::reportDir()
{
#ifdef Q_OS_MAC
    const QString home = QDir::homePath();
    return home + QStringLiteral("/Library/Logs/DiagnosticReports");
#else
    // Linux und Windows legen Absturzberichte nicht an einem Ort ab,
    // den ein Programm ohne weiteres lesen darf (coredumpctl, WER).
    // Dort gibt es nichts nachzuholen.
    return {};
#endif
}

QString CrashReportDigest::summariseFile(const QString& path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) { return {}; }
    // Eine .ips besteht aus zwei JSON-Objekten: eine Kopfzeile mit
    // Zeitpunkt und Version, danach der eigentliche Bericht. Mehr als
    // die ersten 64 kB braucht es dafuer nicht -- ein Bericht mit
    // Stapeln aller Threads ist schnell ein Megabyte gross.
    const QByteArray roh = f.read(64 * 1024);
    const int umbruch = roh.indexOf('\n');
    if (umbruch <= 0) { return {}; }

    const QJsonObject kopf =
        QJsonDocument::fromJson(roh.left(umbruch)).object();
    const QJsonObject leib =
        QJsonDocument::fromJson(roh.mid(umbruch)).object();

    const QString zeit = kopf.value(QStringLiteral("timestamp")).toString();
    const QString version = kopf.value(QStringLiteral("app_version")).toString();

    QString signal = leib.value(QStringLiteral("exception"))
                         .toObject()
                         .value(QStringLiteral("signal"))
                         .toString();
    const QJsonObject ende = leib.value(QStringLiteral("termination")).toObject();
    QString grund = ende.value(QStringLiteral("indicator")).toString();
    // Der erste Grund ist der sprechende ("Library not loaded: ..."),
    // die weiteren zaehlen nur Pfade auf.
    const QJsonArray gruende = ende.value(QStringLiteral("reasons")).toArray();
    if (!gruende.isEmpty()) {
        const QString erster = gruende.first().toString();
        if (!erster.isEmpty()) {
            grund = grund.isEmpty() ? erster
                                    : grund + QStringLiteral(" — ") + erster;
        }
    }

    if (signal.isEmpty() && grund.isEmpty()) { return {}; }
    if (signal.isEmpty()) { signal = QStringLiteral("?"); }
    if (grund.isEmpty())  { grund  = QStringLiteral("ohne Angabe"); }
    // Lange Gruende (dyld zaehlt jeden gesuchten Pfad auf) kuerzen: die
    // Zeile soll im Protokoll lesbar bleiben, nicht vollstaendig sein.
    if (grund.size() > 160) { grund = grund.left(157) + QStringLiteral("…"); }

    return QStringLiteral("%1 · %2 · %3 · %4")
        .arg(zeit.isEmpty() ? QFileInfo(path).fileName() : zeit,
             version.isEmpty() ? QStringLiteral("?") : version,
             signal, grund);
}

QStringList CrashReportDigest::summarise(const QString& dir,
                                         const QDateTime& since,
                                         int maxReports)
{
    QStringList aus;
    if (dir.isEmpty() || maxReports <= 0) { return aus; }
    QDir d(dir);
    if (!d.exists()) { return aus; }

    // Nur die eigenen. Fremde Abstuerze gehen uns nichts an, und sie
    // zu lesen waere ein Blick in Daten anderer Programme.
    const QFileInfoList dateien =
        d.entryInfoList({QStringLiteral("Longpath-*.ips")},
                        QDir::Files, QDir::Time);

    QStringList neueste;
    for (const QFileInfo& fi : dateien) {
        if (since.isValid() && fi.lastModified().toUTC() <= since) { break; }
        const QString zeile = summariseFile(fi.absoluteFilePath());
        if (!zeile.isEmpty()) { neueste.append(zeile); }
        if (neueste.size() >= maxReports) { break; }
    }
    // entryInfoList gibt die neuesten zuerst; im Protokoll liest sich
    // die Zeitfolge besser andersherum.
    for (int i = neueste.size() - 1; i >= 0; --i) { aus.append(neueste.at(i)); }
    return aus;
}

} // namespace Longpath
