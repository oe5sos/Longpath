// =================================================================
// src/gui/setup/SpePage.cpp  (Longpath-native)
// =================================================================
// Siehe SpePage.h fuer Zweck und Herkunft.
// =================================================================

#include "SpePage.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"
#include "models/RadioModel.h"
#include "core/SpeConnection.h"

#include <QCheckBox>
#include <QSignalBlocker>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

namespace Longpath {

SpePage::SpePage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(12);

    // Welcher Funkgeraete-Bereich gerade bearbeitet wird -- die Schluessel
    // liegen unter hardware/<mac>/peripherals/, ohne verbundenes Geraet
    // gibt es keinen Bereich zum Schreiben. Dieselbe Zeile wie bei
    // RfKitPage, aus demselben Grund.
    m_connectionBanner = new QLabel(this);
    m_connectionBanner->setWordWrap(true);
    m_connectionBanner->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px; font-weight: bold;")
            .arg(QLatin1String(Style::kAmberText))
            .arg(Style::kFontSmall)));
    root->addWidget(m_connectionBanner);

    m_master = new QCheckBox(tr("Enable SPE Expert amplifier integration"), this);
    m_master->setToolTip(tr(
        "Gates the SPE applet in the right-column panel and the connection "
        "settings below. Off by default; turn on only when an SPE Expert "
        "(1.3K-FA / 1.5K-FA / 2K-FA) is reachable. Setting is scoped to the "
        "currently connected radio."));
    connect(m_master, &QCheckBox::toggled, this, &SpePage::onMasterToggled);
    root->addWidget(m_master);

    auto* helper = new QLabel(tr(
        "The amplifier only answers when asked, so Longpath polls its status "
        "ten times a second and sends front-panel keystrokes. Band keys are "
        "deliberately not offered: the amplifier follows the radio's band on "
        "its own. Menu navigation and manual ATU stepping are left to SPE's "
        "own KTerm application."), this);
    helper->setWordWrap(true);
    helper->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextTertiary))
            .arg(Style::kFontSmall)));
    root->addWidget(helper);

    // ── Anschluss ────────────────────────────────────────────────────
    auto* connBox = new QGroupBox(tr("Connection"), this);
    m_connBox = connBox;
    auto* fm = new QFormLayout(connBox);

    m_mode = new QComboBox(connBox);
    // Die Reihenfolge der Einträge ist der Index, der gespeichert wird --
    // 0 = Netz, 1 = seriell. Geschrieben wird aber der KLARTEXT
    // ("Network"/"Serial"), damit ein Blick in die Einstellungsdatei
    // verständlich bleibt und ein eingeschobener Eintrag nichts verdreht.
    m_mode->addItem(tr("Network (ser2net proxy)"), QStringLiteral("Network"));
    m_mode->addItem(tr("Serial port"),             QStringLiteral("Serial"));
    m_mode->setToolTip(tr(
        "The same protocol bytes flow either way. Network mode expects a "
        "ser2net-style TCP proxy in raw or telnet mode."));
    connect(m_mode, &QComboBox::currentIndexChanged, this, &SpePage::onModeChanged);
    fm->addRow(tr("Transport:"), m_mode);

    m_host = new QLineEdit(connBox);
    m_host->setPlaceholderText(QStringLiteral("192.168.1.52"));
    m_hostLabel = new QLabel(tr("Host:"), connBox);
    fm->addRow(m_hostLabel, m_host);

    m_port = new QSpinBox(connBox);
    m_port->setRange(1, 65535);
    // 4001 ist ser2nets erster uebliche Anschluss. Eine Vorgabe, keine
    // Festlegung -- es gibt fuer den SPE keinen genormten Anschluss.
    m_port->setValue(4001);
    m_portLabel = new QLabel(tr("Port:"), connBox);
    fm->addRow(m_portLabel, m_port);

    m_serialPort = new QLineEdit(connBox);
#ifdef Q_OS_WIN
    m_serialPort->setPlaceholderText(QStringLiteral("COM4"));
#else
    m_serialPort->setPlaceholderText(QStringLiteral("/dev/cu.usbserial-0001"));
#endif
    m_serialPort->setToolTip(tr(
        "115200 8N1, no handshake. The amplifier auto-adapts to lower speeds, "
        "so there is nothing else to configure."));
    m_serialPortLabel = new QLabel(tr("Serial port:"), connBox);
    fm->addRow(m_serialPortLabel, m_serialPort);

    m_autoReconnect = new QCheckBox(tr("Auto-reconnect on disconnect"), connBox);
    fm->addRow(QString(), m_autoReconnect);

    auto* note = new QLabel(tr(
        "Remote power-ON pulses the proxy's control lines over RFC 2217, so "
        "that needs the ser2net port configured as "
        "\"accepter: telnet(rfc2217=true),<port>\". Polling, telemetry and "
        "every keystroke work over a raw port too."), connBox);
    note->setWordWrap(true);
    note->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextTertiary))
            .arg(Style::kFontCaption)));
    fm->addRow(note);

    m_applyBtn = new QPushButton(tr("Save and connect"), connBox);
    connect(m_applyBtn, &QPushButton::clicked, this, &SpePage::saveAndApply);
    auto* btnRow = new QHBoxLayout();
    btnRow->addWidget(m_applyBtn);
    btnRow->addStretch();
    fm->addRow(btnRow);

    root->addWidget(connBox);

    m_liveStatus = new QLabel(this);
    m_liveStatus->setTextFormat(Qt::RichText);
    root->addWidget(m_liveStatus);

    root->addStretch();

    reloadFromPeripherals();
    if (m_model) {
        connect(m_model, &RadioModel::connectionStateChanged,
                this, &SpePage::refreshConnectionBanner);
    }
    refreshConnectionBanner();

    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &SpePage::refreshLiveStatus);
    timer->start();
    refreshLiveStatus();
}

bool SpePage::serialMode() const
{
    return m_mode->currentData().toString() == QStringLiteral("Serial");
}

void SpePage::reloadFromPeripherals()
{
    if (!m_model) {
        return;
    }
    const QString modus = m_model->peripheralValue(QStringLiteral("Spe_Mode"),
                                                   QStringLiteral("Network"));
    const int idx = m_mode->findData(modus);
    m_mode->setCurrentIndex(idx >= 0 ? idx : 0);
    m_host->setText(m_model->peripheralValue(QStringLiteral("Spe_Host")));
    m_port->setValue(m_model->peripheralValue(QStringLiteral("Spe_Port"),
                                              QStringLiteral("4001")).toInt());
    m_serialPort->setText(m_model->peripheralValue(QStringLiteral("Spe_SerialPort")));
    m_autoReconnect->setChecked(
        m_model->peripheralValue(QStringLiteral("Spe_AutoReconnect"),
                                 QStringLiteral("True")) == QStringLiteral("True"));
    {
        // Der Umschalter darf hier nicht als Bedienung durchgehen:
        // setChecked loest toggled aus, und onMasterToggled schriebe den
        // gerade gelesenen Wert gleich wieder zurueck -- ohne verbundenes
        // Funkgeraet also eine Warnung je Seitenaufbau.
        QSignalBlocker sperre(m_master);
        m_master->setChecked(m_model->speEnabled());
    }
    applyMasterGate(m_model->speEnabled());
    onModeChanged();
}

void SpePage::onModeChanged()
{
    const bool seriell = serialMode();
    m_serialPort->setVisible(seriell);
    m_serialPortLabel->setVisible(seriell);
    m_host->setVisible(!seriell);
    m_hostLabel->setVisible(!seriell);
    m_port->setVisible(!seriell);
    m_portLabel->setVisible(!seriell);
}

void SpePage::applyMasterGate(bool on)
{
    if (m_connBox) {
        m_connBox->setEnabled(on);
    }
}

void SpePage::onMasterToggled(bool on)
{
    if (!m_model) {
        return;
    }
    applyMasterGate(on);
    if (on) {
        // Erst die Angaben sichern, dann einschalten -- sonst schaltet der
        // Umschalter ein und findet nichts, womit er verbinden koennte.
        saveAndApply();
    }
    m_model->setSpeEnabled(on);
}

void SpePage::saveAndApply()
{
    if (!m_model) {
        return;
    }
    m_model->setPeripheralValue(QStringLiteral("Spe_Mode"),
                                m_mode->currentData().toString());
    m_model->setPeripheralValue(QStringLiteral("Spe_Host"), m_host->text().trimmed());
    m_model->setPeripheralValue(QStringLiteral("Spe_Port"),
                                QString::number(m_port->value()));
    m_model->setPeripheralValue(QStringLiteral("Spe_SerialPort"),
                                m_serialPort->text().trimmed());
    m_model->setPeripheralValue(QStringLiteral("Spe_AutoReconnect"),
                                m_autoReconnect->isChecked()
                                    ? QStringLiteral("True")
                                    : QStringLiteral("False"));
    m_model->applySpeConnection();
}

void SpePage::refreshConnectionBanner()
{
    if (!m_model) {
        return;
    }
    const QString mac = m_model->currentRadioMac();
    if (mac.isEmpty()) {
        m_connectionBanner->setText(tr(
            "No radio connected — these settings are stored per radio, so "
            "there is nowhere to write them yet. Connect a radio first."));
        m_master->setEnabled(false);
        applyMasterGate(false);
        return;
    }
    m_connectionBanner->setText(tr("Editing SPE settings for %1").arg(mac));
    m_master->setEnabled(true);
    reloadFromPeripherals();
}

void SpePage::refreshLiveStatus()
{
    if (!m_model) {
        return;
    }
    SpeConnection* verb = m_model->speConnection();
    if (!verb || !verb->isConnected()) {
        m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
            .arg(QLatin1String(Style::kTextInactive), tr("Not connected")));
        return;
    }
    // Zwei verschiedene Dinge, und der Unterschied ist der ganze Grund,
    // warum SpeConnection respondingChanged kennt: die Leitung kann stehen,
    // waehrend der Verstaerker aus ist.
    if (!verb->isResponding()) {
        m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
            .arg(QLatin1String(Style::kAmberText),
                 tr("Link up to %1, but the amplifier is not answering — "
                    "switched off?").arg(verb->description())));
        return;
    }
    const QString kennung = verb->currentModelId().isEmpty()
        ? tr("unknown model")
        : Spe::modelSpec(verb->currentModelId()).displayName;
    m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
        .arg(QLatin1String(Style::kGreenText),
             tr("Connected to %1 via %2 (%3)")
                 .arg(kennung, verb->description(), verb->sourceLabel())));
}

}  // namespace Longpath
