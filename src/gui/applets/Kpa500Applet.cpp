// no-port-check: Longpath-eigene Datei, kein Port. Siehe den Kopf von
// gui/applets/Kpa500Applet.h.
// =================================================================
// src/gui/applets/Kpa500Applet.cpp  (Longpath-eigen)
// =================================================================

#include "Kpa500Applet.h"

#include "gui/HGauge.h"
#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"
#include "models/RadioModel.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

namespace Longpath {

namespace {

QString mitAusgrauung(const char* grundstil)
{
    return QLatin1String(grundstil)
        + QStringLiteral("QPushButton:disabled { color: %1; }")
              .arg(QLatin1String(Style::kTextInactive));
}

// Die Zustandspille. Vier Faelle, weil der KPA500 vier unterscheiden
// kann -- siehe den Kopf der Kopfdatei.
QString pillStil(const QString& hintergrund, const QString& schrift,
                 const QString& rand)
{
    return QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 7px; padding: 1px 6px; font-size: %4px; "
        "font-weight: bold; }")
        .arg(hintergrund, schrift, rand)
        .arg(Style::kFontCaption);
}

}  // namespace

Kpa500Applet::Kpa500Applet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 6, 8, 6);
    vbox->setSpacing(4);

    const QString kleinerText = QStringLiteral("QLabel { color: %1; font-size: %2px; }")
                                    .arg(QLatin1String(Style::kTextTertiary))
                                    .arg(Style::kFontCaption);

    m_sourceLabel = new QLabel(QStringLiteral("● —"), this);
    m_sourceLabel->setStyleSheet(Style::themed(kleinerText));
    m_identityLabel = new QLabel(this);
    m_identityLabel->setStyleSheet(Style::themed(kleinerText));
    m_statusPill = new QLabel(QStringLiteral("—"), this);
    m_statusPill->setAlignment(Qt::AlignCenter);

    auto* kopf = new QHBoxLayout;
    kopf->addWidget(new QLabel(QStringLiteral("Elecraft KPA500"), this));
    kopf->addSpacing(8);
    kopf->addWidget(m_sourceLabel);
    kopf->addSpacing(8);
    kopf->addWidget(m_identityLabel);
    kopf->addStretch();
    kopf->addWidget(m_statusPill);
    vbox->addLayout(kopf);

    // ── Drei Streifen ────────────────────────────────────────────────
    //
    // Leistung: Nennleistung 500 W, Skalenende 600. Die Schwellen sind
    // ABGELEITET (siehe core/Kpa500Protocol.h), nicht gemessen.
    m_pwrGauge = new HGauge(this);
    m_pwrGauge->setRange(0.0, static_cast<double>(Kpa500::kMaxPowerW));
    m_pwrGauge->setYellowStart(static_cast<double>(Kpa500::kWarnPowerW));
    m_pwrGauge->setRedStart(static_cast<double>(Kpa500::kNominalPowerW));
    m_pwrGauge->setTitle(QStringLiteral("PWR"));
    m_pwrGauge->setTickLabels({QStringLiteral("0"),   QStringLiteral("150"),
                               QStringLiteral("300"), QStringLiteral("450"),
                               QStringLiteral("600")});
    m_pwrGauge->setReadout(true, 0, QStringLiteral("W"));
    m_pwrGauge->setAccessibleName(tr("Ausgangsleistung"));
    vbox->addWidget(m_pwrGauge);

    m_swrGauge = new HGauge(this);
    m_swrGauge->setRange(1.0, 3.0);
    m_swrGauge->setYellowStart(2.0);
    m_swrGauge->setRedStart(2.5);
    m_swrGauge->setTitle(QStringLiteral("SWR"));
    m_swrGauge->setTickLabels({QStringLiteral("1"),   QStringLiteral("1.5"),
                               QStringLiteral("2"),   QStringLiteral("2.5"),
                               QStringLiteral("3")});
    m_swrGauge->setReadout(true, 1, QString());
    m_swrGauge->setValue(1.0);
    m_swrGauge->setAccessibleName(tr("Stehwellenverhältnis"));
    vbox->addWidget(m_swrGauge);

    // Temperatur als STREIFEN -- hier gibt das Dokument einen Bereich
    // her (0..150 °C, §^TM) und die Einheit ist bekannt. Beim SPE geht
    // beides nicht, darum steht sie dort als Zahl.
    m_tempGauge = new HGauge(this);
    m_tempGauge->setRange(0.0, 150.0);
    m_tempGauge->setYellowStart(static_cast<double>(Kpa500::kTempWarnC));
    m_tempGauge->setRedStart(static_cast<double>(Kpa500::kTempRedC));
    m_tempGauge->setTitle(QStringLiteral("TEMP"));
    m_tempGauge->setTickLabels({QStringLiteral("0"),  QStringLiteral("40"),
                                QStringLiteral("75"), QStringLiteral("110"),
                                QStringLiteral("150")});
    m_tempGauge->setReadout(true, 0, QStringLiteral("°C"));
    m_tempGauge->setAccessibleName(tr("Kühlkörpertemperatur"));
    vbox->addWidget(m_tempGauge);

    // ── Zahlenfeld ───────────────────────────────────────────────────
    const QString zahlenStil = QStringLiteral("QLabel { color: %1; font-size: %2px; }")
                                   .arg(QLatin1String(Style::kTextSecondary))
                                   .arg(Style::kFontSmall);
    m_voltLabel = new QLabel(QStringLiteral("V  — V"), this);
    m_voltLabel->setStyleSheet(Style::themed(zahlenStil));
    m_currLabel = new QLabel(QStringLiteral("I  — A"), this);
    m_currLabel->setStyleSheet(Style::themed(zahlenStil));
    m_bandLabel = new QLabel(QStringLiteral("BAND  —"), this);
    m_bandLabel->setStyleSheet(Style::themed(zahlenStil));

    auto* gitter = new QGridLayout;
    gitter->setHorizontalSpacing(12);
    gitter->setVerticalSpacing(2);
    gitter->addWidget(m_voltLabel, 0, 0);
    gitter->addWidget(m_currLabel, 0, 1);
    gitter->addWidget(m_bandLabel, 0, 2);
    vbox->addLayout(gitter);

    // ── Fehlerband ───────────────────────────────────────────────────
    m_faultLabel = new QLabel(this);
    m_faultLabel->setWordWrap(true);
    m_faultLabel->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; font-size: %2px; font-weight: bold; }")
            .arg(QLatin1String(Style::kRedBorder))
            .arg(Style::kFontSmall)));
    m_faultLabel->hide();
    vbox->addWidget(m_faultLabel);

    // ── Knoepfe ──────────────────────────────────────────────────────
    auto macheTaste = [this](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        btn->setStyleSheet(Style::themed(mitAusgrauung(Style::kAmpNeutralBtnStyle)));
        return btn;
    };

    // ON ist hier ein dokumentierter Befehl ('P' im Boot-Zustand), keine
    // Bastelei an Steuerleitungen wie beim SPE. Und er ist genau dann
    // bedienbar, wenn der Verstaerker im Boot-Zustand steht -- nicht
    // „immer, solange die Leitung steht": steht die Leitung und die
    // Hauptsoftware laeuft, ist er schon an.
    m_onBtn = new QPushButton(QStringLiteral("ON"), this);
    m_onBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_onBtn->setStyleSheet(
        Style::themed(mitAusgrauung(Style::kAmpOperateActiveBtnStyle)));
    m_onBtn->setToolTip(tr("Verstärker einschalten. Elecrafts Befehl 'P' des "
                           "Boot-Laders — nur sinnvoll, solange der Verstärker "
                           "aus, aber am Draht erreichbar ist."));
    connect(m_onBtn, &QPushButton::clicked, this, &Kpa500Applet::powerOnClicked);

    m_operateBtn = macheTaste(QStringLiteral("OPER"));
    m_operateBtn->setToolTip(tr("Zwischen STANDBY und OPERATE umschalten"));
    connect(m_operateBtn, &QPushButton::clicked, this, &Kpa500Applet::operateClicked);

    m_offBtn = macheTaste(QStringLiteral("OFF"));
    m_offBtn->setToolTip(tr("Verstärker ausschalten. Danach bleibt er am Draht "
                            "erreichbar und lässt sich mit ON wieder "
                            "einschalten."));
    connect(m_offBtn, &QPushButton::clicked, this, &Kpa500Applet::offClicked);

    // Den gibt es beim SPE nicht: der KPA500 kann einen anliegenden
    // Fehler auf Befehl loeschen (^FLC;). Sichtbar nur, wenn einer
    // anliegt -- ein Knopf ohne Anlass ist eine Einladung zum Druecken.
    m_clearFaultBtn = macheTaste(QStringLiteral("FEHLER LÖSCHEN"));
    m_clearFaultBtn->setToolTip(tr("Den anliegenden Fehler löschen (^FLC;)"));
    m_clearFaultBtn->hide();
    connect(m_clearFaultBtn, &QPushButton::clicked,
            this, &Kpa500Applet::clearFaultClicked);

    auto* reihe = new QHBoxLayout;
    reihe->setSpacing(6);
    reihe->addWidget(m_clearFaultBtn);
    reihe->addStretch();
    reihe->addWidget(m_onBtn);
    reihe->addWidget(m_operateBtn);
    reihe->addWidget(m_offBtn);
    vbox->addLayout(reihe);

    setConnected(false);
}

void Kpa500Applet::setSource(const QString& text)
{
    m_sourceLabel->setText(QStringLiteral("● %1").arg(text));
}

void Kpa500Applet::setIdentity(const QString& firmware, const QString& serial)
{
    if (firmware.isEmpty() && serial.isEmpty()) {
        m_identityLabel->clear();
        return;
    }
    m_identityLabel->setText(QStringLiteral("v%1 · #%2").arg(firmware, serial));
}

void Kpa500Applet::setPowerSwr(const Kpa500::PowerSwr& ps)
{
    m_watts = ps.watts;
    m_pwrGauge->setValue(ps.watts);
    // §^WS: im Empfang meldet das Geraet 000, nicht 1,0. Der Zeiger
    // bleibt dann am Skalenanfang stehen statt eine Stehwelle von 0
    // anzuzeigen, die es nicht gibt.
    m_swrGauge->setValue(ps.swr >= 1.0f ? static_cast<double>(ps.swr) : 1.0);
}

void Kpa500Applet::setVoltsAmps(const Kpa500::VoltsAmps& va)
{
    m_voltLabel->setText(QStringLiteral("V  %1V").arg(va.volts, 0, 'f', 1));
    m_currLabel->setText(QStringLiteral("I  %1A").arg(va.amps, 0, 'f', 1));
}

void Kpa500Applet::setTemperature(int degC)
{
    m_tempGauge->setValue(degC);
}

void Kpa500Applet::setOperate(bool operate)
{
    m_operate = operate;
    m_operateBtn->setText(operate ? QStringLiteral("STBY") : QStringLiteral("OPER"));
    m_operateBtn->setStyleSheet(Style::themed(mitAusgrauung(
        operate ? Style::kAmpOperateActiveBtnStyle : Style::kAmpNeutralBtnStyle)));
    applyPill();
}

void Kpa500Applet::setBand(const QString& band)
{
    m_bandLabel->setText(QStringLiteral("BAND  %1").arg(band));
}

void Kpa500Applet::setFault(int code)
{
    m_faultCode = code;
    if (code == 0) {
        m_faultLabel->hide();
        m_faultLabel->clear();
        m_clearFaultBtn->hide();
        return;
    }
    // KEIN Name. Das Dokument gibt die Zuordnung nicht her -- siehe
    // core/Kpa500Protocol.h, parseFault().
    m_faultLabel->setText(tr("FEHLER %1 — die Bedeutung steht nicht in "
                             "Elecrafts Beschreibung; das Gerät zeigt sie "
                             "an der Front.")
                              .arg(code, 2, 10, QLatin1Char('0')));
    m_faultLabel->show();
    m_clearFaultBtn->show();
}

void Kpa500Applet::setLiveness(Kpa500Connection::Liveness state)
{
    m_liveness = state;
    if (state != Kpa500Connection::Liveness::Running) {
        // Nicht nur die Knoepfe grau: die Messwerte sind dann keine
        // Messwerte mehr. Dieselbe Entscheidung wie beim SPE, aus
        // demselben Grund -- ein eingefrorenes Feld liest sich wie ein
        // lebendes.
        clearTelemetry();
    }
    updateCommandsEnabled();
    applyPill();
}

void Kpa500Applet::applyPill()
{
    QString text;
    QString hg;
    QString schrift;
    QString rand;

    if (!m_connected) {
        text = QStringLiteral("—");
        hg = QLatin1String(Style::kBadgeOffBg);
        schrift = QLatin1String(Style::kTextTertiary);
        rand = QLatin1String(Style::kTextInactive);
    } else {
        switch (m_liveness) {
            case Kpa500Connection::Liveness::Running:
                text = m_operate ? QStringLiteral("OPERATE")
                                 : QStringLiteral("STANDBY");
                hg = QLatin1String(m_operate ? Style::kGreenBg : Style::kBadgeInfoBg);
                schrift = QLatin1String(m_operate ? Style::kGreenText
                                                  : Style::kTextSecondary);
                rand = QLatin1String(m_operate ? Style::kGreenBorder
                                               : Style::kTextInactive);
                break;
            case Kpa500Connection::Liveness::BootMode:
                // Die Auskunft, die der SPE nicht geben kann: aus, aber
                // erreichbar. Bernstein, nicht Rot -- das ist kein
                // Fehler, sondern ein Zustand.
                text = QStringLiteral("AUS · am Draht");
                hg = QLatin1String(Style::kBadgeWarnBg);
                schrift = QLatin1String(Style::kAmberText);
                rand = QLatin1String(Style::kAmberText);
                break;
            case Kpa500Connection::Liveness::Silent:
                text = QStringLiteral("keine Antwort");
                hg = QLatin1String(Style::kBadgeOffBg);
                schrift = QLatin1String(Style::kTextTertiary);
                rand = QLatin1String(Style::kTextInactive);
                break;
            default:
                text = QStringLiteral("…");
                hg = QLatin1String(Style::kBadgeOffBg);
                schrift = QLatin1String(Style::kTextTertiary);
                rand = QLatin1String(Style::kTextInactive);
                break;
        }
    }

    if (m_statusPill->text() == text) {
        return;
    }
    m_statusPill->setText(text);
    m_statusPill->setStyleSheet(Style::themed(pillStil(hg, schrift, rand)));
}

void Kpa500Applet::updateCommandsEnabled()
{
    const bool laeuft = m_connected
        && m_liveness == Kpa500Connection::Liveness::Running;
    m_operateBtn->setEnabled(laeuft);
    m_offBtn->setEnabled(laeuft);
    m_clearFaultBtn->setEnabled(laeuft);
    // ON genau im Boot-Zustand -- nicht „immer, solange die Leitung
    // steht". Laeuft die Hauptsoftware, ist er schon an; antwortet auch
    // 'I' nicht, erreicht ihn auch das 'P' nicht.
    m_onBtn->setEnabled(m_connected
                        && m_liveness == Kpa500Connection::Liveness::BootMode);
}

void Kpa500Applet::clearTelemetry()
{
    m_watts = 0;
    m_pwrGauge->setValue(0.0);
    m_swrGauge->setValue(1.0);
    m_tempGauge->setValue(0.0);
    m_voltLabel->setText(QStringLiteral("V  — V"));
    m_currLabel->setText(QStringLiteral("I  — A"));
    m_bandLabel->setText(QStringLiteral("BAND  —"));
    m_operate = false;
    setFault(0);
}

void Kpa500Applet::setConnected(bool connected)
{
    m_connected = connected;
    if (!connected) {
        m_liveness = Kpa500Connection::Liveness::Unknown;
        m_sourceLabel->setText(QStringLiteral("● —"));
        m_identityLabel->clear();
        clearTelemetry();
    }
    updateCommandsEnabled();
    applyPill();
}

// ── Pruefstand-Nahtstellen ───────────────────────────────────────────

QString Kpa500Applet::statusPillTextForTesting() const
{
    return m_statusPill->text();
}

QString Kpa500Applet::infoTextForTesting() const
{
    return QStringLiteral("%1 | %2 | %3")
        .arg(m_voltLabel->text(), m_currLabel->text(), m_bandLabel->text());
}

QString Kpa500Applet::faultTextForTesting() const
{
    return m_faultLabel->isHidden() ? QString() : m_faultLabel->text();
}

bool Kpa500Applet::commandsEnabledForTesting() const
{
    return m_operateBtn->isEnabled();
}

bool Kpa500Applet::powerOnEnabledForTesting() const
{
    return m_onBtn->isEnabled();
}

bool Kpa500Applet::faultClearVisibleForTesting() const
{
    return !m_clearFaultBtn->isHidden();
}

}  // namespace Longpath
