// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/AcomPage.cpp  (Longpath-eigen)
// =================================================================
// Siehe AcomPage.h fuer Zweck und Herkunft.
//
// Modification history (Longpath):
//   2026-10-09 — Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                Claude Code.
// =================================================================

#include "AcomPage.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"
#include "models/RadioModel.h"
#include "core/AcomConnection.h"

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

AcomPage::AcomPage(RadioModel* model, QWidget* parent)
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

    m_master = new QCheckBox(tr("Enable ACOM amplifier integration"), this);
    m_master->setToolTip(tr(
        "Gates the ACOM applet in the right-column panel and the connection "
        "settings below. Off by default; turn on only when an ACOM S-series "
        "amplifier is reachable. Setting is scoped to the currently connected "
        "radio."));
    connect(m_master, &QCheckBox::toggled, this, &AcomPage::onMasterToggled);
    root->addWidget(m_master);

    auto* helper = new QLabel(tr(
        "Unlike the other amplifiers, the ACOM pushes its telemetry about ten "
        "times a second once Longpath switches it on — there is no polling. "
        "There is also no model selector: the manufacturer's protocol "
        "documents exactly one type code, so the applet works out the power "
        "class from the output power it actually sees (a 500S cannot report "
        "900 W) and rescales its gauges accordingly. Factory reset, "
        "service-mode tests and bootloader access are deliberately not "
        "implemented."), this);
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
    connect(m_mode, &QComboBox::currentIndexChanged, this, &AcomPage::onModeChanged);
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
        "9600 8N1, no handshake — mandated by the amplifier's own protocol, "
        "not configurable."));
    m_serialPortLabel = new QLabel(tr("Serial port:"), connBox);
    fm->addRow(m_serialPortLabel, m_serialPort);

    m_autoReconnect = new QCheckBox(tr("Auto-reconnect on disconnect"), connBox);
    fm->addRow(QString(), m_autoReconnect);

    auto* note = new QLabel(tr(
        "Network mode needs a RAW TCP proxy — ser2net \"connection type: "
        "raw\". A telnet-mode port will escape byte 0xFF, which this protocol "
        "uses legitimately (0xFF means \"no fault\"), and the stream will be "
        "corrupted. Serial mode is fixed at 9600 8N1: the manufacturer's "
        "protocol mandates it, so there is nothing to configure."), connBox);
    note->setWordWrap(true);
    note->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextTertiary))
            .arg(Style::kFontCaption)));
    fm->addRow(note);

    m_applyBtn = new QPushButton(tr("Save and connect"), connBox);
    connect(m_applyBtn, &QPushButton::clicked, this, &AcomPage::saveAndApply);
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
                this, &AcomPage::refreshConnectionBanner);
    }
    refreshConnectionBanner();

    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &AcomPage::refreshLiveStatus);
    timer->start();
    refreshLiveStatus();
}

bool AcomPage::serialMode() const
{
    return m_mode->currentData().toString() == QStringLiteral("Serial");
}

void AcomPage::reloadFromPeripherals()
{
    if (!m_model) {
        return;
    }
    const QString modus = m_model->peripheralValue(QStringLiteral("Acom_Mode"),
                                                   QStringLiteral("Network"));
    const int idx = m_mode->findData(modus);
    m_mode->setCurrentIndex(idx >= 0 ? idx : 0);
    m_host->setText(m_model->peripheralValue(QStringLiteral("Acom_Host")));
    m_port->setValue(m_model->peripheralValue(QStringLiteral("Acom_Port"),
                                              QStringLiteral("4001")).toInt());
    m_serialPort->setText(m_model->peripheralValue(QStringLiteral("Acom_SerialPort")));
    m_autoReconnect->setChecked(
        m_model->peripheralValue(QStringLiteral("Acom_AutoReconnect"),
                                 QStringLiteral("True")) == QStringLiteral("True"));
    {
        // Der Umschalter darf hier nicht als Bedienung durchgehen:
        // setChecked loest toggled aus, und onMasterToggled schriebe den
        // gerade gelesenen Wert gleich wieder zurueck -- ohne verbundenes
        // Funkgeraet also eine Warnung je Seitenaufbau.
        QSignalBlocker sperre(m_master);
        m_master->setChecked(m_model->acomEnabled());
    }
    applyMasterGate(m_model->acomEnabled());
    onModeChanged();
}

void AcomPage::onModeChanged()
{
    const bool seriell = serialMode();
    m_serialPort->setVisible(seriell);
    m_serialPortLabel->setVisible(seriell);
    m_host->setVisible(!seriell);
    m_hostLabel->setVisible(!seriell);
    m_port->setVisible(!seriell);
    m_portLabel->setVisible(!seriell);
}

void AcomPage::applyMasterGate(bool on)
{
    if (m_connBox) {
        m_connBox->setEnabled(on);
    }
}

void AcomPage::onMasterToggled(bool on)
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
    m_model->setAcomEnabled(on);
}

void AcomPage::saveAndApply()
{
    if (!m_model) {
        return;
    }
    m_model->setPeripheralValue(QStringLiteral("Acom_Mode"),
                                m_mode->currentData().toString());
    m_model->setPeripheralValue(QStringLiteral("Acom_Host"), m_host->text().trimmed());
    m_model->setPeripheralValue(QStringLiteral("Acom_Port"),
                                QString::number(m_port->value()));
    m_model->setPeripheralValue(QStringLiteral("Acom_SerialPort"),
                                m_serialPort->text().trimmed());
    m_model->setPeripheralValue(QStringLiteral("Acom_AutoReconnect"),
                                m_autoReconnect->isChecked()
                                    ? QStringLiteral("True")
                                    : QStringLiteral("False"));
    m_model->applyAcomConnection();
}

void AcomPage::refreshConnectionBanner()
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

void AcomPage::refreshLiveStatus()
{
    if (!m_model) {
        return;
    }
    AcomConnection* verb = m_model->acomConnection();
    if (!verb || !verb->isConnected()) {
        m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
            .arg(QLatin1String(Style::kTextInactive), tr("Not connected")));
        return;
    }
    // Statt eines Modellwaehlers: zeigen, WAS erkannt wurde. Die Stufe
    // kann sich mitten in der Sitzung nach oben bewegen, darum im
    // Sekundentakt nachgelesen.
    m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
        .arg(QLatin1String(Style::kGreenText),
             tr("Connected to %1 — power class %2")
                 .arg(verb->description(), verb->currentModel())));
}

}  // namespace Longpath
