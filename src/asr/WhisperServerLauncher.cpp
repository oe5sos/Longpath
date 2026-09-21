// =================================================================
// src/asr/WhisperServerLauncher.cpp  (Longpath)
// =================================================================
// Longpath-original. Begruendung im Header.
// =================================================================

#include "asr/WhisperServerLauncher.h"

#include "core/AppSettings.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTcpSocket>
#include <QUrl>
#include <QDebug>

namespace Longpath {

namespace {
constexpr int kMaxRecentLines = 40;
constexpr int kStopGraceMs    = 2000;
}

WhisperServerLauncher::WhisperServerLauncher(QObject* parent)
    : QObject(parent)
{
}

WhisperServerLauncher::~WhisperServerLauncher()
{
    stop();
}

WhisperServerLauncher& WhisperServerLauncher::instance()
{
    static WhisperServerLauncher* s_inst = new WhisperServerLauncher(QCoreApplication::instance());
    return *s_inst;
}

QString WhisperServerLauncher::defaultBinary()
{
    // Homebrew zuerst (Apple Silicon, dann Intel), dann der Suchpfad.
    for (const QString& p : {QStringLiteral("/opt/homebrew/bin/whisper-server"),
                             QStringLiteral("/usr/local/bin/whisper-server")}) {
        if (QFileInfo(p).isExecutable()) { return p; }
    }
    return QStandardPaths::findExecutable(QStringLiteral("whisper-server"));
}

QString WhisperServerLauncher::defaultModel()
{
    const QDir dir(QDir::homePath() + QStringLiteral("/whisper"));
    const QStringList models = dir.entryList({QStringLiteral("ggml-*.bin")},
                                             QDir::Files, QDir::Name);
    return models.isEmpty() ? QString() : dir.absoluteFilePath(models.first());
}

WhisperServerLauncher::Config WhisperServerLauncher::configFromSettings()
{
    auto& s = AppSettings::instance();
    Config c;
    c.binary   = s.value(QStringLiteral("AsrServerBinary"), QString()).toString().trimmed();
    c.model    = s.value(QStringLiteral("AsrModelPath"), QString()).toString().trimmed();
    c.endpoint = s.value(QStringLiteral("AsrEndpointUrl"),
                         QStringLiteral("http://127.0.0.1:8080/inference")).toString().trimmed();
    c.language = s.value(QStringLiteral("AsrLanguage"), QStringLiteral("de")).toString().trimmed();
    if (c.binary.isEmpty()) { c.binary = defaultBinary(); }
    if (c.model.isEmpty())  { c.model  = defaultModel(); }
    return c;
}

bool WhisperServerLauncher::endpointIsLocal(const QString& endpoint)
{
    const QString host = QUrl(endpoint).host().toLower();
    return host == QStringLiteral("127.0.0.1") || host == QStringLiteral("localhost")
        || host == QStringLiteral("::1") || host.isEmpty();
}

bool WhisperServerLauncher::portInUse(const QString& endpoint, int timeoutMs)
{
    const QUrl url(endpoint);
    QString host = url.host();
    if (host.isEmpty()) { host = QStringLiteral("127.0.0.1"); }
    int port = url.port();
    if (port <= 0) { port = 8080; }
    QTcpSocket probe;
    probe.connectToHost(host, static_cast<quint16>(port));
    const bool open = probe.waitForConnected(timeoutMs);
    probe.abort();
    return open;
}

QStringList WhisperServerLauncher::argumentsFor(const Config& cfg)
{
    // whisper.cpp's server: -m Modell, --host/--port, -l Sprache.
    // Port aus der Adresse; fehlt er, 8080 (die Vorgabe des Dienstes).
    int port = QUrl(cfg.endpoint).port();
    if (port <= 0) { port = 8080; }
    QStringList args{QStringLiteral("-m"), cfg.model,
                     QStringLiteral("--host"), QStringLiteral("127.0.0.1"),
                     QStringLiteral("--port"), QString::number(port)};
    if (!cfg.language.isEmpty() && cfg.language != QStringLiteral("auto")) {
        args << QStringLiteral("-l") << cfg.language;
    }
    return args;
}

void WhisperServerLauncher::setState(State s, const QString& reason)
{
    if (m_state == s && m_reason == reason) { return; }
    m_state  = s;
    m_reason = reason;
    emit stateChanged(s, reason);
}

void WhisperServerLauncher::start(const Config& cfg)
{
    if (m_proc && m_proc->state() != QProcess::NotRunning) { return; }   // laeuft schon

    // Jemand anders lauscht schon (Login-Dienst, Terminal): benutzen,
    // nicht verdoppeln. Siehe Header.
    if (portInUse(cfg.endpoint)) {
        int port = QUrl(cfg.endpoint).port();
        if (port <= 0) { port = 8080; }
        setState(State::External,
                 QStringLiteral("auf Port %1 antwortet schon ein Dienst, "
                                "der nicht von Longpath gestartet wurde").arg(port));
        return;
    }

    if (cfg.binary.isEmpty() || !QFileInfo(cfg.binary).isExecutable()) {
        setState(State::Failed,
                 cfg.binary.isEmpty()
                     ? QStringLiteral("whisper-server nicht gefunden (brew install whisper-cpp)")
                     : QStringLiteral("Programm nicht ausfuehrbar: %1").arg(cfg.binary));
        return;
    }
    if (cfg.model.isEmpty() || !QFileInfo::exists(cfg.model)) {
        setState(State::Failed,
                 cfg.model.isEmpty()
                     ? QStringLiteral("kein Modell (ggml-*.bin) in ~/whisper")
                     : QStringLiteral("Modell nicht gefunden: %1").arg(cfg.model));
        return;
    }

    if (!m_proc) {
        m_proc = new QProcess(this);
        m_proc->setProcessChannelMode(QProcess::MergedChannels);
        connect(m_proc, &QProcess::readyReadStandardOutput,
                this, &WhisperServerLauncher::onOutput);
        connect(m_proc, &QProcess::started, this, [this]() {
            setState(State::Running);
        });
        connect(m_proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) {
                setState(State::Failed,
                         QStringLiteral("Start fehlgeschlagen: %1").arg(m_proc->errorString()));
            }
        });
        connect(m_proc, &QProcess::finished, this,
                [this](int code, QProcess::ExitStatus st) {
            if (m_state == State::Stopped) { return; }   // von uns beendet
            const QString last = m_recent.isEmpty() ? QString() : m_recent.last();
            setState(State::Failed,
                     st == QProcess::CrashExit
                         ? QStringLiteral("Dienst abgestuerzt")
                         : QStringLiteral("Dienst beendet (Code %1)%2")
                               .arg(code).arg(last.isEmpty() ? QString()
                                                             : QStringLiteral(": ") + last));
        });
    }

    m_recent.clear();
    setState(State::Starting);
    m_proc->start(cfg.binary, argumentsFor(cfg));
}

void WhisperServerLauncher::stop()
{
    // Extern heisst: nicht unser Prozess — da gibt es nichts zu beenden.
    if (!m_proc || m_state == State::External) { setState(State::Stopped); return; }
    setState(State::Stopped);   // vor terminate(): finished() liest den Zustand
    if (m_proc->state() != QProcess::NotRunning) {
        m_proc->terminate();
        if (!m_proc->waitForFinished(kStopGraceMs)) {
            m_proc->kill();
            m_proc->waitForFinished(kStopGraceMs);
        }
    }
}

void WhisperServerLauncher::onOutput()
{
    const QString text = QString::fromUtf8(m_proc->readAllStandardOutput());
    for (const QString& line : text.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        m_recent << line.trimmed();
        while (m_recent.size() > kMaxRecentLines) { m_recent.removeFirst(); }
    }
}

} // namespace Longpath
