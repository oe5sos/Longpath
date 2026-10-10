// =================================================================
// src/gui/applets/SpeApplet.cpp  (Longpath)
// =================================================================
// Ported from AetherSDR src/gui/SpeApplet.cpp at d58e2b8a -- see the full
// attribution block and the list of deviations at the head of
// gui/applets/SpeApplet.h.
//
// Modification history (Longpath):
//   2026-10-09 — Portiert fuer Longpath von Martin Fischer (OE5SOS),
//                KI-gestuetzt mit Claude Code.
// =================================================================

#include "SpeApplet.h"

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

// Fuenf gleichmaessige Teilstriche ueber [0, max] -- wie Rf2ksApplet und
// AmpApplet. Longpaths HGauge nimmt reine Beschriftungen, keine Paare aus
// Wert und Text, darum wird nur der Text gebaut.
QStringList gleichmaessigeStriche(float max)
{
    QStringList striche;
    for (float anteil : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f}) {
        striche << QString::number(static_cast<int>(max * anteil));
    }
    return striche;
}


// Der Knopf der Verstaerkerfelder, aus StyleConstants.h. Dieselben
// Farben wie bei Rf2ksApplet -- derselbe benannte Stil, kein eigener.
// Dazu kommt hier nur die graue Schrift fuer den abgeschalteten Zustand:
// die meisten Tasten dieses Feldes schalten ab, solange der Verstaerker
// stumm ist (updateCommandsEnabled), was der von sich aus sprechende
// RF2K-S nie braucht.
QString mitAusgrauung(const char* grundstil)
{
    return QLatin1String(grundstil)
        + QStringLiteral("QPushButton:disabled { color: %1; }")
              .arg(QLatin1String(Style::kTextInactive));
}

QString operateAktivStil()
{
    return mitAusgrauung(Style::kAmpOperateActiveBtnStyle);
}

QString neutralStil()
{
    return mitAusgrauung(Style::kAmpNeutralBtnStyle);
}

// Die Zustandspille. Drei Zustaende plus "noch nichts bekannt"; die
// Farbfamilien sind die benannten aus StyleConstants.h.
QString pillStil(bool gruen, bool sendet)
{
    if (!gruen) {
        return QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %3; "
            "border-radius: 7px; padding: 1px 6px; font-size: 9px; "
            "font-weight: bold; }")
            .arg(QLatin1String(Style::kBadgeOffBg),
                 QLatin1String(Style::kTextTertiary),
                 QLatin1String(Style::kTextInactive));
    }
    if (sendet) {
        return QStringLiteral(
            "QLabel { background: %1; color: %2; border: 1px solid %3; "
            "border-radius: 7px; padding: 1px 6px; font-size: 9px; "
            "font-weight: bold; }")
            .arg(QLatin1String(Style::kBadgeTxBg),
                 QLatin1String(Style::kRedText),
                 QLatin1String(Style::kRedBorder));
    }
    return QStringLiteral(
        "QLabel { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 7px; padding: 1px 6px; font-size: 9px; "
        "font-weight: bold; }")
        .arg(QLatin1String(Style::kGreenBg),
             QLatin1String(Style::kGreenText),
             QLatin1String(Style::kGreenBorder));
}

}  // namespace

SpeApplet::SpeApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(8, 6, 8, 6);
    vbox->setSpacing(4);

    // ── Kopfzeile: Quelle (Verbindung) + Geraet + Zustandspille ──────
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
    m_statusPill->setStyleSheet(Style::themed(pillStil(false, false)));
    m_statusPill->setAlignment(Qt::AlignCenter);

    auto* kopf = new QHBoxLayout;
    kopf->addWidget(new QLabel(QStringLiteral("SPE Expert"), this));
    kopf->addSpacing(8);
    kopf->addWidget(m_sourceLabel);
    kopf->addSpacing(8);
    kopf->addWidget(m_modelLabel);
    kopf->addStretch();
    kopf->addWidget(m_statusPill);
    vbox->addLayout(kopf);

    // ── Drei Messstreifen ────────────────────────────────────────────
    //
    // Alle drei Felder sind beim SPE eigenstaendig echt (Ausgangsleistung,
    // Stehwelle an der Antenne, Stehwelle VOR dem Anpassgeraet), darum
    // bekommt jedes einen eigenen Streifen. Spannung, Strom und
    // Temperaturen sind Zahlen: fuer sie gibt das Protokoll keine
    // Soll- oder Hoechstwerte her, an denen man eine Achse aufziehen
    // koennte.
    //
    // Der Messwert steht in der Mulde (setReadout) -- das kann Longpaths
    // HGauge selbst, einschliesslich des Gedankenstrichs solange der Wert
    // auf dem Skalenanfang steht. AetherSDR baut dafuer drei eigene
    // Beschriftungsfelder und eine 10-Hz-Bremse.
    m_pwrGauge = new HGauge(this);
    m_pwrGauge->setRange(0.0, 1600.0);
    m_pwrGauge->setYellowStart(1450.0);
    m_pwrGauge->setRedStart(1500.0);
    m_pwrGauge->setTitle(QStringLiteral("PWR"));
    m_pwrGauge->setTickLabels(gleichmaessigeStriche(1600.0f));
    m_pwrGauge->setReadout(true, 0, QStringLiteral("W"));
    m_pwrGauge->setAccessibleName(tr("Ausgangsleistung"));
    vbox->addWidget(m_pwrGauge);

    // Die Stehwellen-Achsen sind geraeteunabhaengig 1,0..3,0 -- ein
    // Verhaeltnis braucht keine Skalierung je Geraet.
    auto macheSwrStreifen = [this](const QString& titel, const QString& name) {
        auto* g = new HGauge(this);
        g->setRange(1.0, 3.0);
        g->setYellowStart(2.0);
        g->setRedStart(2.5);
        g->setTitle(titel);
        g->setTickLabels({QStringLiteral("1"), QStringLiteral("1.5"),
                          QStringLiteral("2"), QStringLiteral("2.5"),
                          QStringLiteral("3")});
        g->setReadout(true, 1, QString());
        g->setValue(1.0);
        g->setAccessibleName(name);
        return g;
    };
    m_swrAntGauge = macheSwrStreifen(QStringLiteral("SWR"),
                                     tr("Stehwelle an der Antenne"));
    vbox->addWidget(m_swrAntGauge);
    m_swrAtuGauge = macheSwrStreifen(QStringLiteral("ATU"),
                                     tr("Stehwelle vor dem Anpassgeraet"));
    vbox->addWidget(m_swrAtuGauge);

    // ── Zahlenfeld: Temp / V / I, dann Band / Antenne / Eingang·Stufe ─
    const QString zahlenStil = QStringLiteral("QLabel { color: %1; font-size: %2px; }")
                                   .arg(QLatin1String(Style::kTextSecondary))
                                   .arg(Style::kFontSmall);

    m_tempLabel = new QLabel(QStringLiteral("TEMP  —"), this);
    m_tempLabel->setStyleSheet(Style::themed(zahlenStil));
    // Der Verstaerker meldet Grad in der Einheit, auf die sein EIGENES
    // Display eingestellt ist, und sagt auf der Leitung nicht welche
    // (Anleitung §5) -- darum ein nacktes Gradzeichen und kein C/F-Schalter.
    m_tempLabel->setToolTip(tr("Kühlkörpertemperatur, in der Einheit, auf die "
                               "das Display des Verstärkers eingestellt ist"));
    m_voltLabel = new QLabel(QStringLiteral("V  — V"), this);
    m_voltLabel->setStyleSheet(Style::themed(zahlenStil));
    m_currLabel = new QLabel(QStringLiteral("I  — A"), this);
    m_currLabel->setStyleSheet(Style::themed(zahlenStil));
    m_bandLabel = new QLabel(this);
    m_bandLabel->setStyleSheet(Style::themed(zahlenStil));
    m_bandLabel->hide();
    m_antLabel = new QLabel(this);
    m_antLabel->setStyleSheet(Style::themed(zahlenStil));
    m_antLabel->hide();
    m_inputLabel = new QLabel(this);
    m_inputLabel->setStyleSheet(Style::themed(zahlenStil));
    m_inputLabel->hide();

    auto* gitter = new QGridLayout;
    gitter->setHorizontalSpacing(12);
    gitter->setVerticalSpacing(2);
    gitter->addWidget(m_tempLabel,  0, 0);
    gitter->addWidget(m_voltLabel,  0, 1);
    gitter->addWidget(m_currLabel,  0, 2);
    gitter->addWidget(m_bandLabel,  1, 0);
    gitter->addWidget(m_antLabel,   1, 1);
    gitter->addWidget(m_inputLabel, 1, 2);
    vbox->addLayout(gitter);

    // ── Warn-/Alarmband (eigene Zeile, nur wenn etwas anliegt) ───────
    m_faultLabel = new QLabel(this);
    m_faultLabel->setWordWrap(true);
    m_faultLabel->hide();
    setFaultText(QString());
    vbox->addWidget(m_faultLabel);

    // ── Knopfreihen. Jeder Knopf ist ein Tastendruck am Geraet. ──────
    auto macheTaste = [this](const QString& text) {
        auto* btn = new QPushButton(text, this);
        btn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
        btn->setStyleSheet(Style::themed(neutralStil()));
        return btn;
    };

    auto* reihe1 = new QHBoxLayout;
    reihe1->setSpacing(6);
    // ON ist kein Tastendruck, sondern ein Impuls auf der Hardware-Leitung
    // -- darum eigens gefaerbt und als EINZIGER Knopf aktiv, solange der
    // Verstaerker stumm ist (siehe updateCommandsEnabled). Das ist sein
    // ganzer Zweck.
    m_onBtn = new QPushButton(QStringLiteral("ON"), this);
    m_onBtn->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
    m_onBtn->setStyleSheet(Style::themed(operateAktivStil()));
    m_onBtn->setToolTip(tr("Verstärker einschalten — ein Impuls auf den "
                           "Steuerleitungen. Über Netz braucht das einen "
                           "ser2net-Anschluss mit rfc2217; siehe den Hinweis "
                           "in den Einstellungen."));
    connect(m_onBtn, &QPushButton::clicked, this, &SpeApplet::powerOnClicked);
    // "OPER"/"STBY" statt der ganzen Woerter -- in der Reihe stehen fuenf
    // Knoepfe, und die langen Beschriftungen schneiden bei der normalen
    // Feldbreite ab (bei AetherSDR am Geraet gemessen).
    m_operateBtn = macheTaste(QStringLiteral("OPER"));
    m_operateBtn->setToolTip(tr("Zwischen STANDBY und OPERATE umschalten "
                                "(OPERATE-Taste am Gerät)"));
    connect(m_operateBtn, &QPushButton::clicked, this, &SpeApplet::operateClicked);
    // Die Beschriftung IST die aktuelle Stufe (LOW/MID/HIGH), sobald sie
    // bekannt ist -- ein Druck schaltet zur naechsten, wie die
    // POWER-Taste am Geraet.
    m_pwrLevelBtn = macheTaste(QStringLiteral("PWR"));
    m_pwrLevelBtn->setToolTip(tr("Leistungsstufe — ein Druck schaltet "
                                 "LOW / MID / HIGH weiter"));
    connect(m_pwrLevelBtn, &QPushButton::clicked, this, &SpeApplet::powerLevelClicked);
    m_tuneBtn = macheTaste(QStringLiteral("TUNE"));
    // AetherSDR markiert diesen Knopf als sendetastend (TxKeyingMarker);
    // das Gegenstueck gibt es hier nicht, also steht es im Hinweistext.
    m_tuneBtn->setToolTip(tr("Anpassgerät abstimmen (TUNE-Taste am Gerät). "
                             "SENDET: die Abstimmung läuft mit Vorlauf."));
    connect(m_tuneBtn, &QPushButton::clicked, this, &SpeApplet::tuneClicked);
    m_offBtn = macheTaste(QStringLiteral("OFF"));
    m_offBtn->setToolTip(tr("Verstärker ausschalten. Mit ON wieder hoch "
                            "(über Netz braucht das einen ser2net-Anschluss "
                            "mit rfc2217)."));
    connect(m_offBtn, &QPushButton::clicked, this, &SpeApplet::offClicked);
    reihe1->addStretch();
    reihe1->addWidget(m_onBtn);
    reihe1->addWidget(m_operateBtn);
    reihe1->addWidget(m_pwrLevelBtn);
    reihe1->addWidget(m_tuneBtn);
    reihe1->addWidget(m_offBtn);
    vbox->addLayout(reihe1);

    auto* reihe2 = new QHBoxLayout;
    reihe2->setSpacing(6);
    m_inputBtn = macheTaste(QStringLiteral("INPUT"));
    m_inputBtn->setToolTip(tr("Eingang 1 / 2 umschalten"));
    connect(m_inputBtn, &QPushButton::clicked, this, &SpeApplet::inputClicked);
    m_antBtn = macheTaste(QStringLiteral("ANT"));
    m_antBtn->setToolTip(tr("Sendeantenne für das aktuelle Band weiterschalten"));
    connect(m_antBtn, &QPushButton::clicked, this, &SpeApplet::antennaClicked);
    // Die Pfeiltasten des Expert stellen die Vorlaufleistung ein, die der
    // Verstaerker vom Funkgeraet ueber CAT anfordert. Bandtasten gibt es
    // hier bewusst nicht: das Band findet der Verstaerker selbst.
    m_driveDownBtn = macheTaste(QStringLiteral("▼"));
    // Das Zeichen IST die ganze Beschriftung -- ohne eigenen Namen liest
    // eine Vorlesehilfe das Pfeilzeichen vor. Hinweistexte erreichen sie
    // nur auf Anfrage und ersetzen den Namen darum nicht.
    m_driveDownBtn->setAccessibleName(tr("Angeforderte Vorlaufleistung senken"));
    m_driveDownBtn->setToolTip(tr("Die Vorlaufleistung senken, die der "
                                  "Verstärker vom Funkgerät anfordert "
                                  "(Pfeiltaste am Gerät)"));
    connect(m_driveDownBtn, &QPushButton::clicked, this, &SpeApplet::driveDownClicked);
    m_driveUpBtn = macheTaste(QStringLiteral("▲"));
    m_driveUpBtn->setAccessibleName(tr("Angeforderte Vorlaufleistung erhöhen"));
    m_driveUpBtn->setToolTip(tr("Die Vorlaufleistung erhöhen, die der "
                                "Verstärker vom Funkgerät anfordert "
                                "(Pfeiltaste am Gerät)"));
    connect(m_driveUpBtn, &QPushButton::clicked, this, &SpeApplet::driveUpClicked);
    reihe2->addStretch();
    reihe2->addWidget(m_inputBtn);
    reihe2->addWidget(m_antBtn);
    reihe2->addWidget(m_driveDownBtn);
    reihe2->addWidget(m_driveUpBtn);
    vbox->addLayout(reihe2);

    // Bremse fuer die Zahlenfelder -- 10 Hz, wie ueberall im Haus.
    m_labelTimer.setInterval(100);
    connect(&m_labelTimer, &QTimer::timeout, this, &SpeApplet::updateValueLabels);
    m_labelTimer.start();

    m_peakTimer = new QTimer(this);
    m_peakTimer->setSingleShot(true);
    m_peakTimer->setInterval(2500);
    connect(m_peakTimer, &QTimer::timeout, this, [this]() {
        m_peakFwd = 0.0f;
        m_pwrGauge->setPeakValue(0.0);
    });

    setConnected(false);
}

void SpeApplet::setPowerRange(float nominalW, float warnW, float maxW)
{
    // Called on every status frame with the level-derived scale — no-op on
    // repeats so only an actual LOW/MID/HIGH (or model) change repaints.
    // All three thresholds are compared: warnW is derived from nominalW
    // today, but a guard that silently ignores one of its inputs is a trap
    // for whoever changes that derivation.
    if (nominalW == m_rangeNominal && warnW == m_rangeWarn && maxW == m_rangeMax) {
        return;
    }
    m_rangeNominal = nominalW;
    m_rangeWarn = warnW;
    m_rangeMax = maxW;
    m_pwrGauge->setRange(0.0, static_cast<double>(maxW));
    m_pwrGauge->setYellowStart(static_cast<double>(warnW));
    m_pwrGauge->setRedStart(static_cast<double>(nominalW));
    m_pwrGauge->setTickLabels(gleichmaessigeStriche(maxW));
}

void SpeApplet::setModelName(const QString& displayName)
{
    m_modelLabel->setText(displayName);
}

void SpeApplet::setForwardPower(float watts)
{
    m_fwdWatts = watts;
    m_pwrGauge->setValue(static_cast<double>(watts));
    if (watts > m_peakFwd) {
        m_peakFwd = watts;
        m_pwrGauge->setPeakValue(static_cast<double>(watts));
        m_peakTimer->start();
    }
}

void SpeApplet::setSwrAnt(float swr)
{
    m_swrAntVal = swr;
    // Without forward drive SWR is undefined (the amp reports 0.00 in RX,
    // which is below the gauge's 1.0 floor anyway) — hold the needle at 1.0.
    m_swrAntGauge->setValue(m_fwdWatts >= 1.0f ? static_cast<double>(swr) : 1.0);
}

void SpeApplet::setSwrAtu(float swr)
{
    m_swrAtuVal = swr;
    m_swrAtuGauge->setValue(m_fwdWatts >= 1.0f ? static_cast<double>(swr) : 1.0);
}

void SpeApplet::setSupplyVoltage(float volts)
{
    m_supplyVolts = volts;
    m_diagDirty = true;
}

void SpeApplet::setSupplyCurrent(float amps)
{
    m_supplyAmps = amps;
    m_diagDirty = true;
}

void SpeApplet::setTemps(int upper, int lower, int combiner, bool hasCombiner)
{
    m_pendingTempText = hasCombiner
        ? QStringLiteral("TEMP  %1° %2° %3°").arg(upper).arg(lower).arg(combiner)
        : QStringLiteral("TEMP  %1°").arg(upper);
    m_tempDirty = true;
}

void SpeApplet::setBand(const QString& band)
{
    m_pendingBand = band;
    m_bandDirty = true;
}

void SpeApplet::setAntenna(int antenna, QChar atuState)
{
    QString text = QStringLiteral("ANT  %1").arg(antenna);
    if (atuState == u'a') {
        text += QStringLiteral(" · ATU");
    } else if (atuState == u'b') {
        text += QStringLiteral(" · BYP");
    } else if (atuState == u't') {
        text += QStringLiteral(" · TUN");
    }
    m_pendingAntText = text;
    m_antDirty = true;
}

void SpeApplet::setInputPort(int input)
{
    // Composed with the power level into one grid cell by updateValueLabels().
    m_inputPort = input;
    m_inputDirty = true;
}

void SpeApplet::setPowerLevel(const QString& levelName)
{
    m_levelName = levelName;
    m_inputDirty = true;
    // Der Knopf ist gleichzeitig die Anzeige der Stufe.
    m_pwrLevelBtn->setText(levelName.isEmpty() ? QStringLiteral("PWR") : levelName);
}

void SpeApplet::setMode(bool operate, bool transmitting)
{
    m_operate = operate;
    m_transmitting = transmitting;
    applyModePill();
}

void SpeApplet::applyModePill()
{
    Pill zustand = Pill::Neutral;
    if (m_connected && m_responding) {
        zustand = !m_operate ? Pill::Standby
                             : (m_transmitting ? Pill::OperateTx : Pill::OperateRx);
    }
    // Laeuft bei jedem Zustandsrahmen, also zehnmal je Sekunde, und
    // setStyleSheet() loest unbedingt neu auf. Der Text der Pille ist 1:1
    // mit dem Zustand und darum ein ausreichender Waechter.
    QString text;
    switch (zustand) {
        case Pill::OperateTx: text = QStringLiteral("OPR · TX"); break;
        case Pill::OperateRx: text = QStringLiteral("OPR · RX"); break;
        case Pill::Standby:   text = QStringLiteral("STANDBY");  break;
        default:              text = QStringLiteral("—");        break;
    }
    if (m_statusPill->text() == text) {
        return;
    }
    m_statusPill->setText(text);

    const bool gruen = (zustand == Pill::OperateRx || zustand == Pill::OperateTx);
    m_statusPill->setStyleSheet(
        Style::themed(pillStil(gruen, zustand == Pill::OperateTx)));
    // Kurze Beschriftungen -- OPERATE/STANDBY schneiden bei der normalen
    // Feldbreite ab.
    m_operateBtn->setText(gruen ? QStringLiteral("STBY") : QStringLiteral("OPER"));
    m_operateBtn->setStyleSheet(
        Style::themed(gruen ? operateAktivStil() : neutralStil()));
}

void SpeApplet::setFaultText(const QString& text, bool alarm)
{
    if (text.isEmpty()) {
        m_faultIsAlarm = false;
        m_faultLabel->hide();
        m_faultLabel->clear();
        return;
    }
    m_faultIsAlarm = alarm;
    // Hausstil: das kraeftige Rot bleibt der Gefahr. Eine WARNUNG ("ATU
    // bypassed", "Tuning with no power") ist keine, ein ALARM ("SWR
    // exceeding limits", "Amplifier protection") schon.
    m_faultLabel->setStyleSheet(Style::themed(
        QStringLiteral("QLabel { color: %1; font-size: %2px; font-weight: bold; }")
            .arg(QLatin1String(alarm ? Style::kRedBorder : Style::kAmberText))
            .arg(Style::kFontSmall)));
    m_faultLabel->setText(text);
    m_faultLabel->show();
}

void SpeApplet::setSource(const QString& text)
{
    m_sourceLabel->setText(QStringLiteral("● %1").arg(text));
}

void SpeApplet::setResponding(bool responding)
{
    m_responding = responding;
    // Blank the readings, not just the buttons. Over ser2net — the topology
    // this integration is built around — the TCP link outlives the amplifier
    // being switched off, so this is the ONLY signal that arrives: without
    // the reset the panel keeps rendering the last poll's supply voltage,
    // heatsink temperature, band/antenna/level and alarm banner as though
    // they were live, which is worse than showing nothing. The pill going
    // neutral and the keys greying out are too quiet to carry that alone.
    if (!responding) {
        clearTelemetry();
    }
    updateCommandsEnabled();
    applyModePill();
}

void SpeApplet::updateCommandsEnabled()
{
    // Commands only make sense while the amplifier is actually answering —
    // a live ser2net socket with the amp switched off would otherwise offer
    // buttons that silently do nothing.
    const bool enabled = m_connected && m_responding;
    for (auto* btn : {m_operateBtn, m_pwrLevelBtn, m_tuneBtn, m_offBtn,
                      m_inputBtn, m_antBtn, m_driveDownBtn, m_driveUpBtn}) {
        btn->setEnabled(enabled);
    }
    // ON bleibt verfuegbar, solange die Leitung steht -- ein stummer
    // Verstaerker ist genau der Fall, fuer den es den Knopf gibt.
    m_onBtn->setEnabled(m_connected);
}

void SpeApplet::clearTelemetry()
{
    setFaultText(QString());
    m_bandLabel->hide();
    m_antLabel->hide();
    m_inputLabel->hide();
    m_bandDirty = false;
    m_antDirty = false;
    m_inputDirty = false;
    m_inputPort = 0;
    m_levelName.clear();
    m_pwrLevelBtn->setText(QStringLiteral("PWR"));
    m_supplyVolts = 0.0f;
    m_supplyAmps = 0.0f;
    m_diagDirty = false;
    m_voltLabel->setText(QStringLiteral("V  — V"));
    m_currLabel->setText(QStringLiteral("I  — A"));
    m_tempDirty = false;
    m_tempLabel->setText(QStringLiteral("TEMP  —"));
    m_operate = false;
    m_transmitting = false;
    m_fwdWatts = 0.0f;
    m_swrAntVal = 1.0f;
    m_swrAtuVal = 1.0f;
    // Den Spitzenwert mit -- ein alter Spitzenwert ueberlebte sonst in die
    // naechste Sitzung.
    m_peakFwd = 0.0f;
    if (m_peakTimer) {
        m_peakTimer->stop();
    }
    m_pwrGauge->setValue(0.0);
    m_pwrGauge->setPeakValue(0.0);
    m_swrAntGauge->setValue(1.0);
    m_swrAntGauge->setPeakValue(1.0);
    m_swrAtuGauge->setValue(1.0);
    m_swrAtuGauge->setPeakValue(1.0);
    updateValueLabels();
}

void SpeApplet::setConnected(bool connected)
{
    m_connected = connected;
    if (!connected) {
        m_responding = false;
        // Transport-level identity goes too, which the responding path keeps:
        // a silent amplifier is still reached over a known link, and is still
        // the model it identified as.
        m_sourceLabel->setText(QStringLiteral("● —"));
        m_modelLabel->clear();
        clearTelemetry();
    }
    updateCommandsEnabled();
    applyModePill();
}

void SpeApplet::updateValueLabels()
{
    if (m_tempDirty) {
        m_tempDirty = false;
        m_tempLabel->setText(m_pendingTempText);
    }
    if (m_diagDirty) {
        m_diagDirty = false;
        // Zahlen, keine Streifen -- das Protokoll gibt fuer Spannung und
        // Strom keinen Soll- oder Hoechstwert her.
        m_voltLabel->setText(QStringLiteral("V  %1V").arg(m_supplyVolts, 0, 'f', 1));
        m_currLabel->setText(QStringLiteral("I  %1A").arg(m_supplyAmps, 0, 'f', 1));
    }
    if (m_bandDirty) {
        m_bandDirty = false;
        m_bandLabel->setText(QStringLiteral("BAND  %1").arg(m_pendingBand));
        m_bandLabel->show();
    }
    if (m_antDirty) {
        m_antDirty = false;
        m_antLabel->setText(m_pendingAntText);
        m_antLabel->show();
    }
    if (m_inputDirty) {
        m_inputDirty = false;
        QString text = QStringLiteral("IN  %1").arg(
            m_inputPort > 0 ? QString::number(m_inputPort) : QStringLiteral("—"));
        if (!m_levelName.isEmpty()) {
            text += QStringLiteral(" · %1").arg(m_levelName);
        }
        m_inputLabel->setText(text);
        m_inputLabel->show();
    }
}

// ── Pruefstand-Nahtstellen ───────────────────────────────────────────

QString SpeApplet::statusPillTextForTesting() const
{
    return m_statusPill->text();
}

QString SpeApplet::infoTextForTesting() const
{
    QStringList teile;
    for (const QLabel* l : {m_tempLabel, m_voltLabel, m_currLabel,
                            m_bandLabel, m_antLabel, m_inputLabel}) {
        teile << (l->isHidden() ? QString() : l->text());
    }
    return teile.join(QStringLiteral(" | "));
}

QString SpeApplet::faultTextForTesting() const
{
    return m_faultLabel->isHidden() ? QString() : m_faultLabel->text();
}

bool SpeApplet::faultIsAlarmForTesting() const
{
    return m_faultIsAlarm;
}

QString SpeApplet::powerLevelButtonTextForTesting() const
{
    return m_pwrLevelBtn->text();
}

bool SpeApplet::commandsEnabledForTesting() const
{
    return m_operateBtn->isEnabled();
}

bool SpeApplet::powerOnEnabledForTesting() const
{
    return m_onBtn->isEnabled();
}

}  // namespace Longpath
