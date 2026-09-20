#include "LogCategories.h"
#include "AppSettings.h"

#include <QStandardPaths>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>

namespace Longpath {

// --- Category Definitions ---
Q_LOGGING_CATEGORY(lcDiscovery,  "longpath.discovery")
Q_LOGGING_CATEGORY(lcConnection, "longpath.connection")
Q_LOGGING_CATEGORY(lcKiwiSdr, "longpath.kiwisdr")
Q_LOGGING_CATEGORY(lcKiwiSdrAudio, "longpath.kiwi.sdr.audio")
Q_LOGGING_CATEGORY(lcProtocol,   "longpath.protocol")
Q_LOGGING_CATEGORY(lcReceiver,   "longpath.receiver")
Q_LOGGING_CATEGORY(lcAudio,      "longpath.audio",  QtInfoMsg)
Q_LOGGING_CATEGORY(lcDsp,        "longpath.dsp",    QtInfoMsg)
Q_LOGGING_CATEGORY(lcSpectrum,   "longpath.spectrum", QtInfoMsg)
Q_LOGGING_CATEGORY(lcContainer,  "longpath.container")
Q_LOGGING_CATEGORY(lcMeter,      "longpath.meter")
Q_LOGGING_CATEGORY(lcMmio,       "longpath.mmio")
Q_LOGGING_CATEGORY(lcTci,        "longpath.tci")
Q_LOGGING_CATEGORY(lcSpots,      "longpath.spots")
Q_LOGGING_CATEGORY(lcAutomation, "longpath.automation")
Q_LOGGING_CATEGORY(lcRxProfiles,  "longpath.rxprofiles", QtInfoMsg)

// --- LogManager ---

LogManager& LogManager::instance()
{
    static LogManager mgr;
    return mgr;
}

LogManager::LogManager()
{
    // Register all categories with metadata
    // Discovery and Connection enabled by default so first-time users see activity
    m_categories = {
        { QStringLiteral("longpath.discovery"),  QStringLiteral("Discovery"),
          QStringLiteral("UDP radio discovery broadcasts and responses"), true },
        { QStringLiteral("longpath.connection"), QStringLiteral("Connection"),
          QStringLiteral("Radio connection lifecycle, state changes, reconnect"), true },
        { QStringLiteral("longpath.protocol"),   QStringLiteral("Protocol"),
          QStringLiteral("P1/P2 packet framing, I/Q data parsing, sequence tracking"), false },
        { QStringLiteral("longpath.receiver"),   QStringLiteral("Receiver"),
          QStringLiteral("Receiver lifecycle, hardware DDC mapping, I/Q routing"), false },
        { QStringLiteral("longpath.audio"),      QStringLiteral("Audio"),
          QStringLiteral("Audio device negotiation, I/Q-to-audio pipeline"), false },
        { QStringLiteral("longpath.dsp"),        QStringLiteral("DSP"),
          QStringLiteral("WDSP channel processing, demodulation, FFT"), false },
        { QStringLiteral("longpath.spectrum"),   QStringLiteral("Spectrum"),
          QStringLiteral("FFT engine, spectrum display, waterfall rendering"), false },
        { QStringLiteral("longpath.container"), QStringLiteral("Container"),
          QStringLiteral("Container dock/float/resize, lifecycle, persistence"), false },
        { QStringLiteral("longpath.meter"),    QStringLiteral("Meter"),
          QStringLiteral("Meter widget rendering, polling, and item lifecycle"), false },
        { QStringLiteral("longpath.mmio"),     QStringLiteral("MMIO"),
          QStringLiteral("Multi-Meter I/O endpoints, transports, parsers"), false },
        { QStringLiteral("longpath.tci"),     QStringLiteral("TCI"),
          QStringLiteral("TCI WebSocket server, command dispatch, client lifecycle"), false },
        { QStringLiteral("longpath.spots"),    QStringLiteral("Spots"),
          QStringLiteral("DX spot ingest (cluster, RBN, SpotCollector, WSJT-X)"), false },
        { QStringLiteral("longpath.automation"), QStringLiteral("Automation"),
          QStringLiteral("Dev automation bridge (LONGPATH_AUTOMATION) -- widget-tree dump, screen capture"), false },
    };
}

bool LogManager::isEnabled(const QString& id) const
{
    for (const auto& cat : m_categories) {
        if (cat.id == id) {
            return cat.enabled;
        }
    }
    return false;
}

void LogManager::setEnabled(const QString& id, bool on)
{
    for (auto& cat : m_categories) {
        if (cat.id == id) {
            if (cat.enabled != on) {
                cat.enabled = on;
                applyFilterRules();
                saveSettings();
                emit categoryChanged(id, on);
            }
            return;
        }
    }
}

void LogManager::setAllEnabled(bool on)
{
    bool changed = false;
    for (auto& cat : m_categories) {
        if (cat.enabled != on) {
            cat.enabled = on;
            changed = true;
        }
    }
    if (changed) {
        applyFilterRules();
        saveSettings();
        for (const auto& cat : m_categories) {
            emit categoryChanged(cat.id, cat.enabled);
        }
    }
}

void LogManager::applyFilterRules()
{
    QStringList rules;
    // Default: disable all debug output for longpath.*
    rules << QStringLiteral("longpath.*.debug=false");

    // Enable specific categories
    for (const auto& cat : m_categories) {
        if (cat.enabled) {
            rules << QStringLiteral("%1.debug=true").arg(cat.id);
        }
    }

    QLoggingCategory::setFilterRules(rules.join(QLatin1Char('\n')));
}

QString LogManager::logDirPath() const
{
    // Issue #100 / PR #103: honor the --profile override so SupportDialog
    // and SupportBundle read from the same directory main.cpp writes logs
    // to. AppSettings::resolveConfigDir returns the legacy path when no
    // profile is set and .../NereusSDR/profiles/<name>/ when one is.
    return AppSettings::resolveConfigDir(AppSettings::profileOverride());
}

QString LogManager::logFilePath() const
{
    // Find the most recent log file
    QDir dir(logDirPath());
    QStringList logs = dir.entryList({QStringLiteral("longpath-*.log"),
                                      QStringLiteral("nereussdr-*.log")},   // vor 2026-09-17
                                     QDir::Files, QDir::Time);
    if (logs.isEmpty()) {
        return {};
    }
    return dir.absoluteFilePath(logs.first());
}

qint64 LogManager::logFileSize() const
{
    QString path = logFilePath();
    if (path.isEmpty()) {
        return 0;
    }
    return QFileInfo(path).size();
}

void LogManager::clearLog()
{
    QString path = logFilePath();
    if (!path.isEmpty()) {
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.close();
        }
    }
}

void LogManager::saveSettings()
{
    auto& s = AppSettings::instance();
    for (const auto& cat : m_categories) {
        s.setValue(QStringLiteral("LogCategory_%1").arg(cat.id),
                   cat.enabled ? QStringLiteral("True") : QStringLiteral("False"));
    }
}

void LogManager::loadSettings()
{
    auto& s = AppSettings::instance();
    for (auto& cat : m_categories) {
        QString key = QStringLiteral("LogCategory_%1").arg(cat.id);
        if (s.contains(key)) {
            cat.enabled = (s.value(key).toString() == QStringLiteral("True"));
        }
        // If key doesn't exist, keep the default from constructor
    }
    applyFilterRules();
}

} // namespace Longpath
