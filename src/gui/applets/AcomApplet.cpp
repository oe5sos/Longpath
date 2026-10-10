// =================================================================
// src/gui/applets/AcomApplet.cpp  (Longpath)
// =================================================================
// Ported from AetherSDR src/gui/AcomApplet.cpp at d58e2b8a -- see the
// full attribution block and the list of deviations at the head of
// gui/applets/AcomApplet.h.
//
// Modification history (Longpath):
//   2026-10-09 — Portiert fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt mit Claude Code.
// =================================================================

#include "AcomApplet.h"

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

// Betriebszeit lesbar machen. Tage und Stunden reichen -- es sind
// GESAMTbetriebsstunden, da zaehlen Minuten nicht.
QString betriebszeit(quint32 sekunden)
{
    const quint32 stunden = sekunden / 3600;
    if (stunden < 24) {
        return QStringLiteral("%1 h").arg(stunden);
    }
    return QStringLiteral("%1 d %2 h").arg(stunden / 24).arg(stunden % 24);
}

}  // namespace

AcomApplet::AcomApplet(RadioModel* model, QWidget* parent)
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
    m_modelLabel = new QLabel(this);
    m_modelLabel->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; font-size: %2px; font-weight: bold; }")
            .arg(QLatin1String(Style::kTextSecondary))
            .arg(Style::kFontCaption)));
    m_statusPill = new QLabel(QStringLiteral("—"), this);
    m_statusPill->setAlignment(Qt::AlignCenter);

    auto* kopf = new QHBoxLayout;
    kopf->addWidget(new QLabel(QStringLiteral("ACOM"), this));
    kopf->addSpacing(8);
    kopf->addWidget(m_sourceLabel);
    kopf->addSpacing(8);
    kopf->addWidget(m_modelLabel);
    kopf->addStretch();
    kopf->addWidget(m_statusPill);
    vbox->addLayout(kopf);

    // ── Drei Balken ──────────────────────────────────────────────────
    //
    // Vorgabe ist die 600S-Stufe -- das einzige am Geraet bestaetigte
    // Modell. Die Achsen werden bei jeder Modellmeldung neu gesetzt
    // (setPowerRange/setReflectedRange), nicht nur einmal: die
    // Selbstskalierung kann die Stufe mitten in der Sitzung anheben.
    const auto& s600 = Acom::modelSpec(QStringLiteral("600S"));

    m_pwrGauge = new HGauge(this);
    m_pwrGauge->setTitle(QStringLiteral("PWR"));
    m_pwrGauge->setReadout(true, 0, QStringLiteral("W"));
    m_pwrGauge->setAccessibleName(tr("Ausgangsleistung"));
    setPowerRange(s600.nominalForwardW, s600.maxForwardW);
    vbox->addWidget(m_pwrGauge);

    m_reflGauge = new HGauge(this);
    m_reflGauge->setTitle(QStringLiteral("REFL"));
    m_reflGauge->setReadout(true, 0, QStringLiteral("W"));
    m_reflGauge->setAccessibleName(tr("Rücklaufleistung"));
    setReflectedRange(s600.nominalReflectedW, s600.maxReflectedW);
    vbox->addWidget(m_reflGauge);

    // Die Stehwelle ist geraeteunabhaengig 1,0..3,0 -- ein Verhaeltnis
    // braucht keine Skalierung je Modell.
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

    // ── Zahlenfeld: Temp / HV / Id, dann Band / Betrieb ──────────────
    const QString zahlenStil = QStringLiteral("QLabel { color: %1; font-size: %2px; }")
                                   .arg(QLatin1String(Style::kTextSecondary))
                                   .arg(Style::kFontSmall);
    auto macheZahl = [this, &zahlenStil](const QString& text) {
        auto* l = new QLabel(text, this);
        l->setStyleSheet(Style::themed(zahlenStil));
        return l;
    };
    m_tempLabel   = macheZahl(QStringLiteral("TEMP  — °C"));
    m_hvLabel     = macheZahl(QStringLiteral("HV  — V"));
    m_idLabel     = macheZahl(QStringLiteral("Id  — A"));
    m_bandLabel   = macheZahl(QStringLiteral("BAND  —"));
    m_uptimeLabel = macheZahl(QStringLiteral("BETRIEB  —"));
    // „BETRIEB", nicht „an seit": das Feld zaehlt die GESAMTE
    // Betriebszeit und wird beim Ausschalten nicht zurueckgesetzt (am
    // echten 600S bestaetigt).
    m_uptimeLabel->setToolTip(tr("Gesamte Betriebszeit des Verstärkers — "
                                 "wird beim Ausschalten nicht zurückgesetzt"));

    auto* gitter = new QGridLayout;
    gitter->setHorizontalSpacing(12);
    gitter->setVerticalSpacing(2);
    gitter->addWidget(m_tempLabel,   0, 0);
    gitter->addWidget(m_hvLabel,     0, 1);
    gitter->addWidget(m_idLabel,     0, 2);
    gitter->addWidget(m_bandLabel,   1, 0);
    gitter->addWidget(m_uptimeLabel, 1, 1);
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

    // ── Vier Knoepfe ─────────────────────────────────────────────────
    //
    // STANDBY, OPERATE und AUS sind drei eigene, kein Umschalter: das
    // Protokoll kennt drei Zielzustaende, und „Aus" ist keine Gegenseite
    // der anderen zwei. Dazu der Loeschknopf, der sichtbar BLEIBT und
    // nur bedienbar wird -- eine Reihe, die ihre Breite aendert, sprang
    // sonst bei jedem Fehler.
    auto macheTaste = [this](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        btn->setStyleSheet(Style::themed(mitAusgrauung(Style::kAmpNeutralBtnStyle)));
        return btn;
    };
    m_clearBtn = macheTaste(QStringLiteral("FEHLER LÖSCHEN"));
    m_clearBtn->setToolTip(tr("Anliegende weiche Fehler löschen"));
    connect(m_clearBtn, &QPushButton::clicked, this, &AcomApplet::clearFaultClicked);

    m_standbyBtn = macheTaste(QStringLiteral("STANDBY"));
    connect(m_standbyBtn, &QPushButton::clicked, this, &AcomApplet::standbyClicked);
    m_operateBtn = macheTaste(QStringLiteral("OPERATE"));
    connect(m_operateBtn, &QPushButton::clicked, this, &AcomApplet::operateClicked);
    m_offBtn = macheTaste(QStringLiteral("AUS"));
    m_offBtn->setToolTip(tr("Verstärker ausschalten"));
    connect(m_offBtn, &QPushButton::clicked, this, &AcomApplet::offClicked);

    auto* reihe = new QHBoxLayout;
    reihe->setSpacing(6);
    reihe->addWidget(m_clearBtn);
    reihe->addStretch();
    reihe->addWidget(m_standbyBtn);
    reihe->addWidget(m_operateBtn);
    reihe->addWidget(m_offBtn);
    vbox->addLayout(reihe);

    setConnected(false);
}

void AcomApplet::setPowerRange(float nominalW, float maxW)
{
    m_pwrNominal = nominalW;
    m_pwrMax = maxW;
    m_pwrGauge->setRange(0.0, static_cast<double>(maxW));
    // Zwei Zonen, kein Bernstein: die Beschreibung gibt fuer die
    // Leistung einen Nennwert und eine Decke her, keinen Warnbereich
    // dazwischen. Rot ab dem Nennwert.
    m_pwrGauge->setYellowStart(static_cast<double>(nominalW));
    m_pwrGauge->setRedStart(static_cast<double>(nominalW));
    m_pwrGauge->setTickLabels({
        QStringLiteral("0"),
        QString::number(static_cast<int>(maxW * 0.25f)),
        QString::number(static_cast<int>(maxW * 0.5f)),
        QString::number(static_cast<int>(maxW * 0.75f)),
        QString::number(static_cast<int>(maxW)),
    });
}

void AcomApplet::setReflectedRange(float nominalW, float maxW)
{
    m_reflNominal = nominalW;
    m_reflMax = maxW;
    m_reflGauge->setRange(0.0, static_cast<double>(maxW));
    m_reflGauge->setYellowStart(static_cast<double>(nominalW));
    m_reflGauge->setRedStart(static_cast<double>(nominalW));
    m_reflGauge->setTickLabels({
        QStringLiteral("0"),
        QString::number(static_cast<int>(maxW * 0.5f)),
        QString::number(static_cast<int>(maxW)),
    });
}

void AcomApplet::setForwardPower(float watts)
{
    m_fwdWatts = watts;
    m_pwrGauge->setValue(static_cast<double>(watts));
}

void AcomApplet::setReflectedPower(float watts)
{
    m_reflGauge->setValue(static_cast<double>(watts));
}

void AcomApplet::setSwr(float swr)
{
    // Ohne Vorlauf ist die Stehwelle keine Messung -- der Zeiger bleibt
    // am Skalenanfang statt eine Zahl zu zeigen, die es nicht gibt.
    m_swrGauge->setValue(m_fwdWatts >= 1.0f && swr >= 1.0f
                             ? static_cast<double>(swr)
                             : 1.0);
}

void AcomApplet::setDrainCurrent(float amps)
{
    m_idLabel->setText(QStringLiteral("Id  %1A").arg(amps, 0, 'f', 1));
}

void AcomApplet::setDrainVoltage(float volts)
{
    m_hvLabel->setText(QStringLiteral("HV  %1V").arg(volts, 0, 'f', 1));
}

void AcomApplet::setTemp(float degC)
{
    m_tempLabel->setText(QStringLiteral("TEMP  %1°C").arg(degC, 0, 'f', 0));
}

void AcomApplet::setBand(const QString& band)
{
    m_bandLabel->setText(QStringLiteral("BAND  %1").arg(band));
}

void AcomApplet::setUptime(quint32 totalSeconds)
{
    m_uptimeLabel->setText(QStringLiteral("BETRIEB  %1").arg(betriebszeit(totalSeconds)));
}

void AcomApplet::setSource(const QString& text)
{
    m_sourceLabel->setText(QStringLiteral("● %1").arg(text));
}

void AcomApplet::setModelName(const QString& name)
{
    m_modelLabel->setText(name);
}

void AcomApplet::setMode(Acom::Mode mode)
{
    m_mode = mode;
    updateCommandsEnabled();
    applyPill();
}

void AcomApplet::applyPill()
{
    QString text = QStringLiteral("—");
    QString hg = QLatin1String(Style::kBadgeOffBg);
    QString schrift = QLatin1String(Style::kTextTertiary);
    QString rand = QLatin1String(Style::kTextInactive);

    if (m_connected) {
        text = Acom::modeName(m_mode);
        switch (m_mode) {
            case Acom::Mode::OperateTx:
                // Sendet. Hausstil: das kraeftige Rot ist „Sendet /
                // Gefahr" -- hier ist es das Erste.
                hg = QLatin1String(Style::kBadgeTxBg);
                schrift = QLatin1String(Style::kRedText);
                rand = QLatin1String(Style::kRedBorder);
                break;
            case Acom::Mode::OperateRx:
                hg = QLatin1String(Style::kGreenBg);
                schrift = QLatin1String(Style::kGreenText);
                rand = QLatin1String(Style::kGreenBorder);
                break;
            case Acom::Mode::Standby:
                hg = QLatin1String(Style::kBadgeInfoBg);
                schrift = QLatin1String(Style::kTextSecondary);
                rand = QLatin1String(Style::kTextInactive);
                break;
            case Acom::Mode::Atac:
            case Acom::Mode::Init:
            case Acom::Mode::Reset:
            case Acom::Mode::SetParams:
                // Zwischenzustaende -- kein Fehler, aber auch nicht
                // betriebsbereit. Bernstein.
                hg = QLatin1String(Style::kBadgeWarnBg);
                schrift = QLatin1String(Style::kAmberText);
                rand = QLatin1String(Style::kAmberText);
                break;
            default:
                break;
        }
    }

    if (m_statusPill->text() == text) {
        return;
    }
    m_statusPill->setText(text);
    m_statusPill->setStyleSheet(Style::themed(pillStil(hg, schrift, rand)));
}

void AcomApplet::setFaultText(const QString& text)
{
    if (text.isEmpty()) {
        m_faultLabel->hide();
        m_faultLabel->clear();
        return;
    }
    m_faultLabel->setText(text);
    m_faultLabel->show();
}

void AcomApplet::setClearFaultEnabled(bool enabled)
{
    m_clearBtn->setEnabled(enabled && m_connected);
}

void AcomApplet::setDiagnosticTooltip(const QString& text)
{
    m_statusPill->setToolTip(text);
}

void AcomApplet::updateCommandsEnabled()
{
    for (auto* btn : {m_standbyBtn, m_operateBtn, m_offBtn}) {
        btn->setEnabled(m_connected);
    }
    // Der aktuelle Zustand ist kein Ziel mehr: wer auf STANDBY steht,
    // braucht den STANDBY-Knopf nicht.
    if (m_connected) {
        m_standbyBtn->setEnabled(m_mode != Acom::Mode::Standby);
        m_operateBtn->setEnabled(m_mode != Acom::Mode::OperateRx
                                 && m_mode != Acom::Mode::OperateTx);
    }
    const bool operateAktiv = m_connected
        && (m_mode == Acom::Mode::OperateRx || m_mode == Acom::Mode::OperateTx);
    m_operateBtn->setStyleSheet(Style::themed(mitAusgrauung(
        operateAktiv ? Style::kAmpOperateActiveBtnStyle : Style::kAmpNeutralBtnStyle)));
}

void AcomApplet::clearTelemetry()
{
    m_fwdWatts = 0.0f;
    m_pwrGauge->setValue(0.0);
    m_reflGauge->setValue(0.0);
    m_swrGauge->setValue(1.0);
    m_tempLabel->setText(QStringLiteral("TEMP  — °C"));
    m_hvLabel->setText(QStringLiteral("HV  — V"));
    m_idLabel->setText(QStringLiteral("Id  — A"));
    m_bandLabel->setText(QStringLiteral("BAND  —"));
    m_uptimeLabel->setText(QStringLiteral("BETRIEB  —"));
    m_mode = Acom::Mode::Unknown;
    setFaultText(QString());
    m_clearBtn->setEnabled(false);
}

void AcomApplet::setConnected(bool connected)
{
    m_connected = connected;
    if (!connected) {
        m_sourceLabel->setText(QStringLiteral("● —"));
        m_modelLabel->clear();
        m_statusPill->setToolTip(QString());
        clearTelemetry();
    }
    updateCommandsEnabled();
    applyPill();
}

// ── Pruefstand-Nahtstellen ───────────────────────────────────────────

QString AcomApplet::statusPillTextForTesting() const
{
    return m_statusPill->text();
}

QString AcomApplet::infoTextForTesting() const
{
    return QStringLiteral("%1 | %2 | %3 | %4 | %5")
        .arg(m_tempLabel->text(), m_hvLabel->text(), m_idLabel->text(),
             m_bandLabel->text(), m_uptimeLabel->text());
}

QString AcomApplet::faultTextForTesting() const
{
    return m_faultLabel->isHidden() ? QString() : m_faultLabel->text();
}

QString AcomApplet::diagnosticTooltipForTesting() const
{
    return m_statusPill->toolTip();
}

bool AcomApplet::commandsEnabledForTesting() const
{
    return m_offBtn->isEnabled();
}

bool AcomApplet::clearFaultEnabledForTesting() const
{
    return m_clearBtn->isEnabled();
}

}  // namespace Longpath
