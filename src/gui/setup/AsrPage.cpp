// no-port-check: Longpath-original UI file. Kein Thetis-Code.
// =================================================================
// src/gui/setup/AsrPage.cpp  (Longpath)
// =================================================================
// Siehe AsrPage.h.
// =================================================================

#include "AsrPage.h"
#include "asr/WhisperServerLauncher.h"
#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QUrl>

namespace Longpath {

namespace {

static QString kEditStyle() { return Longpath::Style::formFieldStyle(); }   // seit 2026-09-18 das Glasfeld

constexpr int kProbeTimeoutMs = 2500;

const QString kDefaultUrl = QStringLiteral("http://127.0.0.1:8080/inference");

// Die Probe geht auf den WURZELPFAD des Dienstes, nicht auf /inference:
// /inference erwartet einen Datei-Anhang und antwortet sonst mit einem
// Fehler, den man nicht von "Dienst laeuft nicht" unterscheiden koennte.
// Der Wurzelpfad dagegen liefert bei whisper-server eine Seite -- und
// wenn niemand horcht, kommt ConnectionRefusedError. Genau diese beiden
// Faelle wollen wir auseinanderhalten.
QUrl probeUrlFor(const QString& endpoint)
{
    QUrl u(endpoint.trimmed());
    if (!u.isValid() || u.host().isEmpty()) { return {}; }
    u.setPath(QStringLiteral("/"));
    u.setQuery(QString());
    u.setFragment(QString());
    return u;
}

void storeTrimmed(const QString& key, const QString& value)
{
    AppSettings::instance().setValue(key, value.trimmed());
}

} // namespace

AsrPage::AsrPage(RadioModel* model, QWidget* parent)
    : SetupPage(QStringLiteral("Spracherkennung"), model, parent)
{
    buildUI();
}

void AsrPage::buildUI()
{
    Longpath::Style::applyDarkPageStyle(this);

    buildServerGroup();
    buildRecognitionGroup();
    buildHintGroup();

    contentLayout()->addStretch();
}

// ---------------------------------------------------------------------------
// Gruppe 1: der Dienst
// ---------------------------------------------------------------------------
void AsrPage::buildServerGroup()
{
    auto* group = new QGroupBox(tr("Erkennungsdienst"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    m_urlEdit = new QLineEdit(group);
    m_urlEdit->setStyleSheet(kEditStyle());
    m_urlEdit->setText(s.value(QStringLiteral("AsrEndpointUrl"), kDefaultUrl).toString());
    m_urlEdit->setToolTip(
        tr("Adresse des Whisper-Dienstes. Voreinstellung ist der eigene Rechner "
           "(127.0.0.1) -- damit verlaesst kein Ton die Maschine."));
    connect(m_urlEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrEndpointUrl"), m_urlEdit->text());
    });
    form->addRow(tr("Adresse:"), m_urlEdit);

    m_keyEdit = new QLineEdit(group);
    m_keyEdit->setStyleSheet(kEditStyle());
    m_keyEdit->setEchoMode(QLineEdit::Password);
    m_keyEdit->setText(s.value(QStringLiteral("AsrApiKey"), QString()).toString());
    m_keyEdit->setPlaceholderText(tr("leer lassen beim eigenen Dienst"));
    m_keyEdit->setToolTip(
        tr("Nur noetig, wenn der Dienst einen Schluessel verlangt. Ein lokaler "
           "whisper-server braucht keinen."));
    connect(m_keyEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrApiKey"), m_keyEdit->text());
    });
    form->addRow(tr("Schluessel:"), m_keyEdit);

    auto* row = new QWidget(group);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    rowLayout->setSpacing(8);

    m_probeButton = new QPushButton(tr("Erreichbarkeit pruefen"), row);
    m_probeButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(m_probeButton, &QPushButton::clicked, this, &AsrPage::probeEndpoint);
    rowLayout->addWidget(m_probeButton);

    m_probeLabel = new QLabel(QString(), row);
    m_probeLabel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    rowLayout->addWidget(m_probeLabel, 1);

    form->addRow(row);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Gruppe 2: was erkannt wird
// ---------------------------------------------------------------------------
void AsrPage::buildRecognitionGroup()
{
    auto* group = new QGroupBox(tr("Erkennung"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();

    m_languageEdit = new QLineEdit(group);
    m_languageEdit->setStyleSheet(kEditStyle());
    m_languageEdit->setText(
        s.value(QStringLiteral("AsrLanguage"), QStringLiteral("de")).toString());
    m_languageEdit->setToolTip(
        tr("Sprachkuerzel, etwa de, en, it. \"auto\" laesst den Dienst raten -- "
           "das kostet Genauigkeit, wenn man weiss, was kommt."));
    connect(m_languageEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrLanguage"), m_languageEdit->text());
    });
    form->addRow(tr("Sprache:"), m_languageEdit);

    m_modelEdit = new QLineEdit(group);
    m_modelEdit->setStyleSheet(kEditStyle());
    m_modelEdit->setText(
        s.value(QStringLiteral("AsrModel"), QStringLiteral("whisper-1")).toString());
    m_modelEdit->setToolTip(
        tr("Modellname. Der lokale whisper-server ignoriert dieses Feld -- er "
           "nimmt das Modell, mit dem er gestartet wurde."));
    connect(m_modelEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrModel"), m_modelEdit->text());
    });
    form->addRow(tr("Modell:"), m_modelEdit);

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Gruppe 3: der Hinweis, wie man den Dienst ueberhaupt startet
// ---------------------------------------------------------------------------
void AsrPage::buildHintGroup()
{
    // ── Der Dienst aus Longpath heraus (2026-09-17) ──────────────────
    //
    // Hier stand ein Hinweis mit einer Terminalzeile. Betreiber: "ein
    // Startknopf fuer den Dienst aus Longpath heraus, damit du das
    // Terminal nicht brauchst" — "ja bitte". Jetzt: Programm und
    // Modell (mit Vorgaben, wo Homebrew und ~/whisper sie hinlegen),
    // ein Haken fuer den Automatikstart beim Einschalten der Mitschrift,
    // Starten/Stoppen von Hand, und der Zustand mit Grund.
    auto* group = new QGroupBox(tr("Dienst auf diesem Rechner"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);
    form->setSpacing(6);

    auto& s = AppSettings::instance();
    const auto cfg = WhisperServerLauncher::configFromSettings();

    m_binaryEdit = new QLineEdit(group);
    m_binaryEdit->setStyleSheet(kEditStyle());
    m_binaryEdit->setText(s.value(QStringLiteral("AsrServerBinary"), QString()).toString());
    m_binaryEdit->setPlaceholderText(cfg.binary.isEmpty()
        ? tr("whisper-server nicht gefunden — brew install whisper-cpp")
        : cfg.binary);
    m_binaryEdit->setToolTip(tr("Pfad zu whisper-server. Leer: die Homebrew-Vorgabe."));
    connect(m_binaryEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrServerBinary"), m_binaryEdit->text());
    });
    form->addRow(tr("Programm:"), m_binaryEdit);

    m_modelPathEdit = new QLineEdit(group);
    m_modelPathEdit->setStyleSheet(kEditStyle());
    m_modelPathEdit->setText(s.value(QStringLiteral("AsrModelPath"), QString()).toString());
    m_modelPathEdit->setPlaceholderText(cfg.model.isEmpty()
        ? tr("kein ggml-*.bin in ~/whisper")
        : cfg.model);
    m_modelPathEdit->setToolTip(tr("Die Modelldatei (ggml-*.bin). Leer: die erste in ~/whisper. "
                                   "\"small\" ist schnell, \"medium\" versteht deutlich mehr."));
    connect(m_modelPathEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrModelPath"), m_modelPathEdit->text());
    });
    form->addRow(tr("Modelldatei:"), m_modelPathEdit);

    m_autoStart = new QCheckBox(tr("beim Einschalten der Mitschrift selbst starten"), group);
    m_autoStart->setStyleSheet(QString::fromLatin1(Style::kCheckBoxStyle));
    m_autoStart->setChecked(s.value(QStringLiteral("AsrAutoStartServer"),
                                    QStringLiteral("True")).toString()
                                .compare(QStringLiteral("True"), Qt::CaseInsensitive) == 0);
    connect(m_autoStart, &QCheckBox::toggled, this, [](bool on) {
        AppSettings::instance().setValue(QStringLiteral("AsrAutoStartServer"),
                                         on ? QStringLiteral("True") : QStringLiteral("False"));
    });
    form->addRow(m_autoStart);

    auto* row = new QWidget(group);
    auto* rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(0, 0, 0, 0);
    m_startButton = new QPushButton(tr("Starten"), row);
    m_startButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        storeTrimmed(QStringLiteral("AsrServerBinary"), m_binaryEdit->text());
        storeTrimmed(QStringLiteral("AsrModelPath"), m_modelPathEdit->text());
        WhisperServerLauncher::instance().start(WhisperServerLauncher::configFromSettings());
    });
    rowLayout->addWidget(m_startButton);
    m_stopButton = new QPushButton(tr("Stoppen"), row);
    m_stopButton->setStyleSheet(QString::fromLatin1(Style::kButtonStyle));
    connect(m_stopButton, &QPushButton::clicked, this, [] {
        WhisperServerLauncher::instance().stop();
    });
    rowLayout->addWidget(m_stopButton);
    m_serverState = new QLabel(QString(), row);
    m_serverState->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    m_serverState->setWordWrap(true);
    rowLayout->addWidget(m_serverState, 1);
    form->addRow(row);

    auto& launcher = WhisperServerLauncher::instance();
    auto showState = [this](WhisperServerLauncher::State st, const QString& reason) {
        using S = WhisperServerLauncher::State;
        QString text;
        QString colour = QString::fromLatin1(Style::kTextSecondary);
        switch (st) {
        case S::Stopped:  text = tr("aus"); break;
        case S::Starting: text = tr("startet … (das Modell laedt einige Sekunden)"); break;
        case S::Running:  text = tr("laeuft"); colour = QString::fromLatin1(Style::kGreenText); break;
        case S::External: text = tr("laeuft ausserhalb von Longpath (%1)").arg(reason);
                          colour = QString::fromLatin1(Style::kGreenText); break;
        case S::Failed:   text = tr("Fehler: %1").arg(reason);
                          colour = QString::fromLatin1(Style::kRedBorder); break;
        }
        m_serverState->setText(text);
        m_serverState->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(colour));
        m_startButton->setEnabled(st != S::Running && st != S::Starting);
        m_stopButton->setEnabled(st == S::Running || st == S::Starting);
    };
    connect(&launcher, &WhisperServerLauncher::stateChanged, this, showState);
    showState(launcher.state(), launcher.reason());

    contentLayout()->addWidget(group);
}

// ---------------------------------------------------------------------------
// Die Probe
// ---------------------------------------------------------------------------
void AsrPage::probeEndpoint()
{
    if (m_probe) { return; }   // eine Probe zur Zeit genuegt

    // Der Knopf speichert mit: wer die Adresse tippt und sofort prueft,
    // hat editingFinished noch nicht ausgeloest.
    storeTrimmed(QStringLiteral("AsrEndpointUrl"), m_urlEdit->text());

    const QUrl url = probeUrlFor(m_urlEdit->text());
    if (!url.isValid()) {
        showProbeResult(tr("Adresse unlesbar"), QString::fromLatin1(Style::kTxRed));
        return;
    }

    if (!m_net) { m_net = new QNetworkAccessManager(this); }

    showProbeResult(tr("frage nach ..."), QStringLiteral("#8090a0"));
    m_probeButton->setEnabled(false);

    QNetworkRequest req(url);
    req.setTransferTimeout(kProbeTimeoutMs);
    m_probe = m_net->get(req);

    connect(m_probe, &QNetworkReply::finished, this, [this] {
        QNetworkReply* reply = m_probe;
        m_probe = nullptr;
        m_probeButton->setEnabled(true);
        if (!reply) { return; }
        reply->deleteLater();

        const QNetworkReply::NetworkError err = reply->error();
        if (err == QNetworkReply::ConnectionRefusedError
            || err == QNetworkReply::HostNotFoundError
            || err == QNetworkReply::TimeoutError
            || err == QNetworkReply::OperationCanceledError) {
            showProbeResult(tr("niemand horcht dort"), QString::fromLatin1(Style::kTxRed));
            return;
        }
        // Jede HTTP-Antwort -- auch 404 -- beweist, dass ein Dienst da ist.
        showProbeResult(tr("Dienst antwortet"), QString::fromLatin1(Style::kGreenText));
    });
}

void AsrPage::showProbeResult(const QString& text, const QString& colour)
{
    if (!m_probeLabel) { return; }
    m_probeLabel->setText(text);
    m_probeLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(colour));
}

} // namespace Longpath
