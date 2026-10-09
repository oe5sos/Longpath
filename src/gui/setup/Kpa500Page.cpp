// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/Kpa500Page.cpp  (Longpath-eigen)
// =================================================================
// Siehe Kpa500Page.h fuer Zweck und Herkunft.
// =================================================================

#include "Kpa500Page.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"
#include "models/RadioModel.h"
#include "core/Kpa500Connection.h"

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

Kpa500Page::Kpa500Page(RadioModel* model, QWidget* parent)
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

    m_master = new QCheckBox(tr("Enable Elecraft KPA500 amplifier integration"), this);
    m_master->setToolTip(tr(
        "Gates the KPA500 applet in the right-column panel and the connection "
        "settings below. Off by default; turn on only when an SPE Expert "
        "(1.3K-FA / 1.5K-FA / 2K-FA) is reachable. Setting is scoped to the "
        "currently connected radio."));
    connect(m_master, &QCheckBox::toggled, this, &Kpa500Page::onMasterToggled);
    root->addWidget(m_master);

    auto* helper = new QLabel(tr(
        "The amplifier only answers when asked, so Longpath polls its status "
        "four times a second. Unlike most accessories the KPA500 can tell "
        "Longpath that it is switched off but still reachable on the wire — "
        "then, and only then, the applet's ON button can power it up. "
        "Firmware upload is deliberately not implemented: Elecraft reserves "
        "that command for their own use and warns that issuing it by accident "
        "may need a rear-panel power cycle to recover."), this);
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
    connect(m_mode, &QComboBox::currentIndexChanged, this, &Kpa500Page::onModeChanged);
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
        "8N1, no handshake. The data rate is set separately — see below."));
    m_serialPortLabel = new QLabel(tr("Serial port:"), connBox);
    fm->addRow(m_serialPortLabel, m_serialPort);

    // Die vier Werte, die ^BRP kennt (Elecraft Rev A2, §^BRP) -- nicht
    // mehr und nicht weniger. Ein freies Zahlenfeld waere hier falsch:
    // der Verstaerker nimmt nur diese vier.
    m_baud = new QComboBox(connBox);
    for (int rate : {4800, 9600, 19200, 38400}) {
        m_baud->addItem(QString::number(rate), rate);
    }
    m_baud->setCurrentIndex(m_baud->findData(38400));
    m_baud->setToolTip(tr(
        "Must match the amplifier's own ^BRP setting. At the wrong rate "
        "nothing gets through — Elecraft's reference does not document which "
        "rate ships as default, so 38400 (the highest) is offered first."));
    m_baudLabel = new QLabel(tr("Data rate:"), connBox);
    fm->addRow(m_baudLabel, m_baud);

    m_autoReconnect = new QCheckBox(tr("Auto-reconnect on disconnect"), connBox);
    fm->addRow(QString(), m_autoReconnect);

    auto* note = new QLabel(tr(
        "Remote power-on is a documented command here ('P' in the "
        "amplifier's boot loader), so a plain raw ser2net port is enough — "
        "no RFC 2217 control lines needed. The data rate must match what is "
        "set in the amplifier's own ^BRP setting: at the wrong rate nothing "
        "gets through at all, and Elecraft's reference does not say which "
        "rate ships as default."), connBox);
    note->setWordWrap(true);
    note->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextTertiary))
            .arg(Style::kFontCaption)));
    fm->addRow(note);

    m_applyBtn = new QPushButton(tr("Save and connect"), connBox);
    connect(m_applyBtn, &QPushButton::clicked, this, &Kpa500Page::saveAndApply);
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
                this, &Kpa500Page::refreshConnectionBanner);
    }
    refreshConnectionBanner();

    auto* timer = new QTimer(this);
    timer->setInterval(1000);
    connect(timer, &QTimer::timeout, this, &Kpa500Page::refreshLiveStatus);
    timer->start();
    refreshLiveStatus();
}

bool Kpa500Page::serialMode() const
{
    return m_mode->currentData().toString() == QStringLiteral("Serial");
}

void Kpa500Page::reloadFromPeripherals()
{
    if (!m_model) {
        return;
    }
    const QString modus = m_model->peripheralValue(QStringLiteral("Kpa500_Mode"),
                                                   QStringLiteral("Network"));
    const int idx = m_mode->findData(modus);
    m_mode->setCurrentIndex(idx >= 0 ? idx : 0);
    m_host->setText(m_model->peripheralValue(QStringLiteral("Kpa500_Host")));
    m_port->setValue(m_model->peripheralValue(QStringLiteral("Kpa500_Port"),
                                              QStringLiteral("4001")).toInt());
    m_serialPort->setText(m_model->peripheralValue(QStringLiteral("Kpa500_SerialPort")));
    {
        const int baud = m_model->peripheralValue(QStringLiteral("Kpa500_Baud"),
                                                  QStringLiteral("38400")).toInt();
        const int idxB = m_baud->findData(baud);
        m_baud->setCurrentIndex(idxB >= 0 ? idxB : m_baud->findData(38400));
    }
    m_autoReconnect->setChecked(
        m_model->peripheralValue(QStringLiteral("Kpa500_AutoReconnect"),
                                 QStringLiteral("True")) == QStringLiteral("True"));
    {
        // Der Umschalter darf hier nicht als Bedienung durchgehen:
        // setChecked loest toggled aus, und onMasterToggled schriebe den
        // gerade gelesenen Wert gleich wieder zurueck -- ohne verbundenes
        // Funkgeraet also eine Warnung je Seitenaufbau.
        QSignalBlocker sperre(m_master);
        m_master->setChecked(m_model->kpa500Enabled());
    }
    applyMasterGate(m_model->kpa500Enabled());
    onModeChanged();
}

void Kpa500Page::onModeChanged()
{
    const bool seriell = serialMode();
    m_serialPort->setVisible(seriell);
    m_serialPortLabel->setVisible(seriell);
    m_baud->setVisible(seriell);
    m_baudLabel->setVisible(seriell);
    m_host->setVisible(!seriell);
    m_hostLabel->setVisible(!seriell);
    m_port->setVisible(!seriell);
    m_portLabel->setVisible(!seriell);
}

void Kpa500Page::applyMasterGate(bool on)
{
    if (m_connBox) {
        m_connBox->setEnabled(on);
    }
}

void Kpa500Page::onMasterToggled(bool on)
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
    m_model->setKpa500Enabled(on);
}

void Kpa500Page::saveAndApply()
{
    if (!m_model) {
        return;
    }
    m_model->setPeripheralValue(QStringLiteral("Kpa500_Mode"),
                                m_mode->currentData().toString());
    m_model->setPeripheralValue(QStringLiteral("Kpa500_Host"), m_host->text().trimmed());
    m_model->setPeripheralValue(QStringLiteral("Kpa500_Port"),
                                QString::number(m_port->value()));
    m_model->setPeripheralValue(QStringLiteral("Kpa500_SerialPort"),
                                m_serialPort->text().trimmed());
    m_model->setPeripheralValue(QStringLiteral("Kpa500_Baud"),
                                QString::number(m_baud->currentData().toInt()));
    m_model->setPeripheralValue(QStringLiteral("Kpa500_AutoReconnect"),
                                m_autoReconnect->isChecked()
                                    ? QStringLiteral("True")
                                    : QStringLiteral("False"));
    m_model->applyKpa500Connection();
}

void Kpa500Page::refreshConnectionBanner()
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

void Kpa500Page::refreshLiveStatus()
{
    if (!m_model) {
        return;
    }
    Kpa500Connection* verb = m_model->kpa500Connection();
    if (!verb || !verb->isConnected()) {
        m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
            .arg(QLatin1String(Style::kTextInactive), tr("Not connected")));
        return;
    }
    // DREI Zustaende, nicht zwei -- das ist der Unterschied zum SPE und
    // der Grund, warum diese Seite nicht einfach die SPE-Seite mit
    // anderen Namen ist.
    switch (verb->liveness()) {
        case Kpa500Connection::Liveness::Running:
            m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
                .arg(QLatin1String(Style::kGreenText),
                     tr("Connected and answering — %1, firmware %2, serial %3")
                         .arg(verb->description(), verb->firmware(),
                              verb->serialNumber())));
            break;
        case Kpa500Connection::Liveness::BootMode:
            m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
                .arg(QLatin1String(Style::kAmberText),
                     tr("Reachable on %1 but switched off (boot loader "
                        "answering) — the applet's ON button can power it up.")
                         .arg(verb->description())));
            break;
        case Kpa500Connection::Liveness::Silent:
            m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
                .arg(QLatin1String(Style::kAmberText),
                     tr("Link to %1 is up but nothing answers — wrong port, "
                        "wrong data rate, or nothing there.")
                         .arg(verb->description())));
            break;
        default:
            m_liveStatus->setText(QStringLiteral("<span style=\"color:%1\">%2</span>")
                .arg(QLatin1String(Style::kTextTertiary), tr("Waiting for a reply…")));
            break;
    }
}

}  // namespace Longpath
