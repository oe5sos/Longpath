// =================================================================
// src/gui/applets/CatRigApplet.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original, siehe CatRigApplet.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "CatRigApplet.h"

#include "core/CompanionRigController.h"
#include "core/Ts590Cat.h"
#include "core/TransverterSetup.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSlider>
#include <QVBoxLayout>

namespace Longpath {

CatRigApplet::CatRigApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    if (auto* rig = model ? model->companionRig() : nullptr) {
        connect(rig, &CompanionRigController::catZustandGeaendert,
                this, &CatRigApplet::syncFromModel);
        connect(rig, &CompanionRigController::verbindungGeaendert,
                this, [this](bool) { syncFromModel(); });
        connect(rig, &CompanionRigController::sendetGeaendert,
                this, [this](bool) { syncFromModel(); });

        const auto schicke = [rig](const QByteArray& zeile) { rig->stelle(zeile); };
        connect(m_vorverstaerker, &QPushButton::clicked, this,
                [schicke](bool an) { schicke(Ts590Cat::stelleVorverstaerker(an)); });
        connect(m_daempfung, &QPushButton::clicked, this,
                [schicke](bool an) { schicke(Ts590Cat::stelleDaempfung(an)); });
        connect(m_nb, &QPushButton::clicked, this,
                [schicke](bool an) { schicke(Ts590Cat::stelleNb(an)); });
        connect(m_nr, &QPushButton::clicked, this,
                [schicke](bool an) { schicke(Ts590Cat::stelleNr(an)); });
        connect(m_agc, &QComboBox::activated, this, [schicke](int i) {
            schicke(Ts590Cat::stelleAgc(static_cast<Ts590Cat::Agc>(i)));
        });
        connect(m_rit, &QPushButton::clicked, this,
                [schicke](bool an) { schicke(Ts590Cat::stelleRit(an)); });
        connect(m_ritRunter, &QPushButton::clicked, this,
                [schicke]() { schicke(Ts590Cat::stelleRitSchritt(-10)); });
        connect(m_ritHoch, &QPushButton::clicked, this,
                [schicke]() { schicke(Ts590Cat::stelleRitSchritt(10)); });
        connect(m_ritNull, &QPushButton::clicked, this,
                [schicke]() { schicke(Ts590Cat::stelleRitNull()); });
        connect(m_notch, &QPushButton::clicked, this,
                [schicke](bool an) { schicke(Ts590Cat::stelleNotch(an)); });
        // Pegel: beim Loslassen und bei Rad/Tasten, wie RF Gain.
        const auto pegel = [this, schicke](QSlider* regler, QLabel* wert,
                                           QByteArray (*befehl)(int)) {
            connect(regler, &QSlider::sliderReleased, this,
                    [regler, schicke, befehl]() { schicke(befehl(regler->value())); });
            connect(regler, &QSlider::valueChanged, this,
                    [regler, wert, schicke, befehl](int w) {
                wert->setText(QString::number(w));
                if (!regler->isSliderDown()) { schicke(befehl(w)); }
            });
        };
        pegel(m_nbPegel, m_nbPegelWert, &Ts590Cat::stelleNbPegel);
        pegel(m_nrPegel, m_nrPegelWert, &Ts590Cat::stelleNrPegel);
        connect(m_filter, &QPushButton::clicked, this, [rig, schicke]() {
            schicke(Ts590Cat::stelleFilter(rig->catZustand().filter == 1 ? 2 : 1));
        });
        connect(m_rfGain, &QSlider::sliderReleased, this, [this, schicke]() {
            schicke(Ts590Cat::stelleRfGain(m_rfGain->value()));
        });
        connect(m_rfGain, &QSlider::valueChanged, this, [this, schicke](int wert) {
            m_rfWert->setText(QString::number(wert));
            // Mausrad, Tasten, Klick neben den Griff: kein sliderReleased.
            // Der Abgleich vom Geraet setzt den Wert unter einer Signalsperre
            // und landet darum nicht hier.
            if (!m_rfGain->isSliderDown()) { schicke(Ts590Cat::stelleRfGain(wert)); }
        });
    }
    syncFromModel();
}

void CatRigApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 4);
    root->setSpacing(4);

    m_status = new QLabel(this);
    m_status->setObjectName(QStringLiteral("catRigStatus"));
    root->addWidget(m_status);

    m_wege = new QLabel(this);
    m_wege->setObjectName(QStringLiteral("catRigWege"));
    root->addWidget(m_wege);

    auto* tasten = new QHBoxLayout;
    tasten->setSpacing(4);
    m_vorverstaerker = greenToggle(QStringLiteral("Preamp"));
    m_vorverstaerker->setObjectName(QStringLiteral("catRigPreamp"));
    m_daempfung = greenToggle(QStringLiteral("ATT"));
    m_daempfung->setObjectName(QStringLiteral("catRigAtt"));
    m_nb = greenToggle(QStringLiteral("NB"));
    m_nb->setObjectName(QStringLiteral("catRigNb"));
    m_nr = greenToggle(QStringLiteral("NR"));
    m_nr->setObjectName(QStringLiteral("catRigNr"));
    tasten->addWidget(m_vorverstaerker);
    tasten->addWidget(m_daempfung);
    tasten->addWidget(m_nb);
    tasten->addWidget(m_nr);
    root->addLayout(tasten);

    // RIT und Notch: fuer den Gegenpartner, der daneben ruft, und den
    // Traeger im Durchlass. Auf 2 m im SSB-Contest das, was man am Geraet
    // am haeufigsten anfasst.
    auto* ritZeile = new QHBoxLayout;
    ritZeile->setSpacing(4);
    m_rit = greenToggle(QStringLiteral("RIT"));
    m_rit->setObjectName(QStringLiteral("catRigRit"));
    m_ritRunter = new QPushButton(QStringLiteral("−"), this);
    m_ritRunter->setObjectName(QStringLiteral("catRigRitRunter"));
    m_ritRunter->setToolTip(tr("RIT down 10 Hz"));
    m_ritHoch = new QPushButton(QStringLiteral("+"), this);
    m_ritHoch->setObjectName(QStringLiteral("catRigRitHoch"));
    m_ritHoch->setToolTip(tr("RIT up 10 Hz"));
    m_ritNull = new QPushButton(QStringLiteral("0"), this);
    m_ritNull->setObjectName(QStringLiteral("catRigRitNull"));
    m_ritNull->setToolTip(tr("Clear RIT"));
    for (QPushButton* b : {m_ritRunter, m_ritHoch, m_ritNull}) { b->setFixedWidth(28); }
    m_ritWert = insetValue(QStringLiteral("0 Hz"), 64);
    m_ritWert->setObjectName(QStringLiteral("catRigRitWert"));
    m_notch = greenToggle(QStringLiteral("Notch"));
    m_notch->setObjectName(QStringLiteral("catRigNotch"));
    m_notch->setToolTip(tr("Auto notch"));
    ritZeile->addWidget(m_rit);
    ritZeile->addWidget(m_ritRunter);
    ritZeile->addWidget(m_ritHoch);
    ritZeile->addWidget(m_ritNull);
    ritZeile->addWidget(m_ritWert);
    ritZeile->addStretch(1);
    ritZeile->addWidget(m_notch);
    root->addLayout(ritZeile);

    auto* agcZeile = new QHBoxLayout;
    auto* agcText = new QLabel(QStringLiteral("AGC"), this);
    m_agc = new QComboBox(this);
    m_agc->setObjectName(QStringLiteral("catRigAgc"));
    m_agc->addItems({QStringLiteral("Off"), QStringLiteral("Slow"), QStringLiteral("Fast")});
    agcZeile->addWidget(agcText);
    agcZeile->addWidget(m_agc, 1);
    m_filter = new QPushButton(QStringLiteral("FIL"), this);
    m_filter->setObjectName(QStringLiteral("catRigFilter"));
    agcZeile->addWidget(m_filter);
    root->addLayout(agcZeile);

    m_rfGain = new QSlider(Qt::Horizontal, this);
    m_rfGain->setObjectName(QStringLiteral("catRigRfGain"));
    m_rfGain->setRange(0, 255);
    m_rfWert = insetValue(QStringLiteral("0"));
    root->addLayout(sliderRow(QStringLiteral("RF Gain"), m_rfGain, m_rfWert));

    m_nbPegel = new QSlider(Qt::Horizontal, this);
    m_nbPegel->setObjectName(QStringLiteral("catRigNbPegel"));
    m_nbPegel->setRange(1, 10);
    m_nbPegelWert = insetValue(QStringLiteral("—"));
    root->addLayout(sliderRow(QStringLiteral("NB level"), m_nbPegel, m_nbPegelWert));
    m_nrPegel = new QSlider(Qt::Horizontal, this);
    m_nrPegel->setObjectName(QStringLiteral("catRigNrPegel"));
    m_nrPegel->setRange(1, 10);
    m_nrPegelWert = insetValue(QStringLiteral("—"));
    root->addLayout(sliderRow(QStringLiteral("NR level"), m_nrPegel, m_nrPegelWert));

    m_messwerte = new QLabel(this);
    m_messwerte->setObjectName(QStringLiteral("catRigMesswerte"));
    root->addWidget(m_messwerte);
    root->addStretch(1);
}

void CatRigApplet::syncFromModel()
{
    auto* rig = m_model ? m_model->companionRig() : nullptr;
    if (!rig || !rig->verbunden()) {
        m_status->setText(tr("Not connected"));
        m_wege->clear();
        m_messwerte->clear();
        return;
    }

    const Ts590Cat::Zustand& z = rig->catZustand();
    const bool sendet = rig->sendet();
    m_status->setText(sendet ? tr("Transmitting at the rig") : tr("Receiving"));
    m_status->setStyleSheet(sendet
        ? QStringLiteral("color: %1;").arg(QString::fromLatin1(Style::kTxRed))
        : QString());

    if (z.antenne > 0) {
        m_wege->setText(tr("ANT %1 · RX ANT %2 · DRV %3")
                            .arg(z.antenne)
                            .arg(z.rxAnt == 1 ? tr("on") : tr("off"))
                            .arg(z.drive == 1 ? tr("on") : tr("off")));
    }
    const bool falsch = TransverterSetup::current().active()
                        && (z.rxAnt == 0 || z.drive == 0);
    m_wege->setStyleSheet(falsch
        ? QStringLiteral("color: %1;").arg(QString::fromLatin1(Style::kTxRed))
        : QString());

    const auto setzeTaste = [](QPushButton* t, int wert) {
        const QSignalBlocker sperre(t);
        t->setChecked(wert > 0);
    };
    setzeTaste(m_vorverstaerker, z.vorverstaerker);
    setzeTaste(m_daempfung, z.daempfung);
    setzeTaste(m_nb, z.nb);
    setzeTaste(m_nr, z.nr);
    if (z.agc >= 0 && z.agc < m_agc->count()) {
        const QSignalBlocker sperre(m_agc);
        m_agc->setCurrentIndex(z.agc);
    }
    setzeTaste(m_rit, z.ritAn);
    setzeTaste(m_notch, z.notch);
    if (z.ritAn >= 0) {
        m_ritWert->setText(z.ritHz == 0 ? QStringLiteral("0 Hz")
                                        : QStringLiteral("%1%2 Hz")
                                              .arg(z.ritHz > 0 ? QStringLiteral("+") : QString())
                                              .arg(z.ritHz));
    }
    // Den Pegel gibt es nur bei eingeschaltetem NB bzw. NR1 (sonst `?;`).
    const auto setzePegel = [](QSlider* regler, QLabel* wert, int stand, bool gilt) {
        regler->setEnabled(gilt);
        if (!gilt || stand < 1 || regler->isSliderDown()) {
            if (!gilt) { wert->setText(QStringLiteral("—")); }
            return;
        }
        const QSignalBlocker sperre(regler);
        regler->setValue(stand);
        wert->setText(QString::number(stand));
    };
    setzePegel(m_nbPegel, m_nbPegelWert, z.nbPegel, z.nb > 0);
    setzePegel(m_nrPegel, m_nrPegelWert, z.nrPegel, z.nr == 1);
    if (z.filter > 0) {
        m_filter->setText(z.filter == 1 ? QStringLiteral("FIL A") : QStringLiteral("FIL B"));
    }
    if (z.rfGain >= 0 && !m_rfGain->isSliderDown()) {
        const QSignalBlocker sperre(m_rfGain);
        m_rfGain->setValue(z.rfGain);
        m_rfWert->setText(QString::number(z.rfGain));
    }

    QString text = z.leistungW >= 0 ? tr("Power %1 W").arg(z.leistungW) : QString();
    if (sendet && z.swrPunkte >= 0) {
        text += tr(" · SWR %1 of 30").arg(z.swrPunkte);
    }
    if (sendet && z.alcPunkte >= 0) {
        text += tr(" · ALC %1 of 30").arg(z.alcPunkte);
    }
    m_messwerte->setText(text);
}

} // namespace Longpath
