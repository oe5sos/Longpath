// no-port-check: Longpath-original UI file. Kein Thetis-Code.
// =================================================================
// src/gui/setup/AsrPage.cpp  (Longpath)
// =================================================================
// Siehe AsrPage.h.
// =================================================================

#include "AsrPage.h"
#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

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

constexpr auto kEditStyle =
    "QLineEdit { background: #1a2a3a; border: 1px solid #304050;"
    " border-radius: 6px; color: #c8d8e8; font-size: 13px; padding: 3px 6px; }";

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
    m_urlEdit->setStyleSheet(QString::fromLatin1(kEditStyle));
    m_urlEdit->setText(s.value(QStringLiteral("AsrEndpointUrl"), kDefaultUrl).toString());
    m_urlEdit->setToolTip(
        tr("Adresse des Whisper-Dienstes. Voreinstellung ist der eigene Rechner "
           "(127.0.0.1) -- damit verlaesst kein Ton die Maschine."));
    connect(m_urlEdit, &QLineEdit::editingFinished, this, [this] {
        storeTrimmed(QStringLiteral("AsrEndpointUrl"), m_urlEdit->text());
    });
    form->addRow(tr("Adresse:"), m_urlEdit);

    m_keyEdit = new QLineEdit(group);
    m_keyEdit->setStyleSheet(QString::fromLatin1(kEditStyle));
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
    m_languageEdit->setStyleSheet(QString::fromLatin1(kEditStyle));
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
    m_modelEdit->setStyleSheet(QString::fromLatin1(kEditStyle));
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
    auto* group = new QGroupBox(tr("Hinweis"), this);
    group->setStyleSheet(QString::fromLatin1(Style::kGroupBoxStyle));
    auto* form = new QFormLayout(group);

    auto* hint = new QLabel(
        tr("Die Mitschrift braucht einen laufenden Dienst. Lokal etwa:\n"
           "    whisper-server -m ggml-large-v3-turbo.bin --port 8080\n"
           "Solange dort nichts horcht, meldet die Mitschrift \"Fehler\"."),
        group);
    hint->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    hint->setWordWrap(true);
    hint->setTextInteractionFlags(Qt::TextSelectableByMouse);
    form->addRow(hint);

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
        showProbeResult(tr("Adresse unlesbar"), QStringLiteral("#c85050"));
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
            showProbeResult(tr("niemand horcht dort"), QStringLiteral("#c85050"));
            return;
        }
        // Jede HTTP-Antwort -- auch 404 -- beweist, dass ein Dienst da ist.
        showProbeResult(tr("Dienst antwortet"), QStringLiteral("#50c878"));
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
