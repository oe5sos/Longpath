// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/setup/FlexApdPage.cpp  (Longpath-eigen)
// =================================================================
//
// Siehe FlexApdPage.h.
//
// Modification history (Longpath):
//   2026-11-01 -- Neu. Martin Fischer (OE5SOS), KI-gestuetzt mit
//                 Claude Code.
// =================================================================

#include "FlexApdPage.h"

#include "core/FlexApdState.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QVBoxLayout>

namespace Longpath {

namespace {

/// Der Wert, mit dem die Maschine „kein Messpunkt gewaehlt" meldet —
/// und mit dem man die Wahl auch wieder zuruecknimmt.
constexpr auto kKeinMesspunkt = "INVALID";

}  // namespace

FlexApdPage::FlexApdPage(RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_model(model)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(12);

    m_banner = new QLabel(this);
    m_banner->setWordWrap(true);
    m_banner->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px; font-weight: bold;")
            .arg(QLatin1String(Style::kAmberText))
            .arg(Style::kFontSmall)));
    root->addWidget(m_banner);

    auto* helper = new QLabel(tr(
        "This is the radio's own predistortion, not PureSignal. On an "
        "OpenHPSDR radio Longpath computes the correction itself and puts it "
        "into its own transmit path; a FlexRadio modulates from the audio we "
        "send it, so the correction has to happen inside the radio. Longpath "
        "switches it on and tells it where to listen.\n\n"
        "With no external sampler selected the radio uses its built-in "
        "directional coupler and linearises its own PA. To correct an "
        "external amplifier instead, feed that amplifier's sampler port into "
        "one of the receive inputs below and select it here."), this);
    helper->setWordWrap(true);
    helper->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextTertiary))
            .arg(Style::kFontSmall)));
    root->addWidget(helper);

    m_enable = new QCheckBox(tr("Predistortion enabled"), this);
    m_enable->setToolTip(tr(
        "Sends 'apd enable=0|1'. The radio learns its correction curve while "
        "transmitting; nothing is learned and nothing changes until you key "
        "it. Longpath never switches this on by itself."));
    connect(m_enable, &QCheckBox::toggled, this, [this](bool on) {
        if (m_updatingFromModel || !m_model) { return; }
        m_model->flexApdEinschalten(on);
    });
    root->addWidget(m_enable);

    m_learned = new QLabel(this);
    m_learned->setWordWrap(true);
    m_learned->setStyleSheet(Style::themed(
        QStringLiteral("color: %1; font-size: %2px;")
            .arg(QLatin1String(Style::kTextPrimary))
            .arg(Style::kFontSmall)));
    root->addWidget(m_learned);

    m_samplerBox = new QGroupBox(tr("Sampler per transmit antenna"), this);
    m_samplerForm = new QFormLayout(m_samplerBox);
    root->addWidget(m_samplerBox);

    m_reset = new QPushButton(tr("Discard learned curve"), this);
    m_reset->setToolTip(tr(
        "Sends 'apd reset'. The radio throws the curve away and starts "
        "learning again on the next transmission. Worth doing after changing "
        "the sampler, the antenna or the amplifier."));
    connect(m_reset, &QPushButton::clicked, this, [this] {
        if (m_model) { m_model->flexApdZuruecksetzen(); }
    });
    root->addWidget(m_reset);

    root->addStretch(1);

    if (m_model) {
        connect(m_model, &RadioModel::flexApdGeaendert,
                this, &FlexApdPage::refreshFromRadio);
    }
    refreshFromRadio();
}

void FlexApdPage::refreshFromRadio()
{
    if (!m_model) { return; }

    const FlexApdState& apd = m_model->flexApd();
    const bool bekannt = apd.bekannt();

    // Abwesenheit als Abwesenheit zeigen: leere Regler sehen aus wie
    // „aus", und das waere eine Aussage ueber eine Maschine, die
    // nichts gesagt hat.
    m_enable->setEnabled(bekannt);
    m_samplerBox->setVisible(bekannt && apd.einstellbar());
    m_reset->setEnabled(bekannt);
    m_learned->setVisible(bekannt);

    if (!bekannt) {
        m_banner->setText(tr(
            "This radio reports no predistortion. FlexRadio's APD is in the "
            "8000 series from SmartSDR 4.2.18; older firmware and the 6000 "
            "series answer the subscription and send nothing. For an "
            "OpenHPSDR radio, use Tools > PureSignal instead."));
        return;
    }

    m_banner->setText(apd.eingeschaltet()
        ? tr("Predistortion is on.")
        : tr("Predistortion is off."));

    {
        const QSignalBlocker block(m_enable);
        m_enable->setChecked(apd.eingeschaltet());
    }

    // Was die Maschine zuletzt gelernt hat, gilt je Antenne und Band —
    // nicht allgemein. Darum stehen beide Zahlen dabei.
    QString gelernt = apd.kurveTraegt()
        ? tr("The learned curve is in circuit.")
        : tr("No learned curve in circuit yet.");
    if (!apd.antenne().isEmpty()) {
        gelernt += QLatin1Char(' ')
                 + tr("Last seen on %1 at %2 MHz, %3 % power.")
                       .arg(apd.antenne())
                       .arg(apd.frequenzMhz(), 0, 'f', 3)
                       .arg(apd.leistungProzent());
    }
    m_learned->setText(gelernt);

    rebuildSamplerRows();
}

void FlexApdPage::rebuildSamplerRows()
{
    if (!m_model) { return; }
    const FlexApdState& apd = m_model->flexApd();

    // Die Zeilen haengen daran, WELCHE Sendeantennen die Maschine
    // fuehrt. Sie meldet sie einzeln und nicht unbedingt auf einmal,
    // also wird die Liste neu gebaut, sobald sie sich aendert.
    const QStringList antennen = apd.sendeantennen();
    if (antennen.size() != m_samplerCombos.size()
        || !std::all_of(antennen.cbegin(), antennen.cend(),
                        [this](const QString& a) { return m_samplerCombos.contains(a); })) {
        while (m_samplerForm->rowCount() > 0) {
            m_samplerForm->removeRow(0);
        }
        m_samplerCombos.clear();

        for (const QString& ant : antennen) {
            auto* combo = new QComboBox(m_samplerBox);
            combo->setToolTip(tr(
                "Where the radio listens to its own output while transmitting "
                "on this antenna. 'Internal coupler' corrects the radio's own "
                "PA; the other entries are receive inputs you can feed from an "
                "external amplifier's sampler port."));
            connect(combo, &QComboBox::currentIndexChanged, this, [this, ant, combo](int) {
                if (m_updatingFromModel || !m_model) { return; }
                const QString punkt = combo->currentData().toString();
                if (punkt.isEmpty()) { return; }
                m_model->flexApdMesspunktSetzen(ant, punkt);
            });
            m_samplerCombos.insert(ant, combo);
            m_samplerForm->addRow(ant + QLatin1Char(':'), combo);
        }
    }

    // Inhalte fuellen, ohne dass ein Feld dabei einen Befehl ausloest.
    m_updatingFromModel = true;
    for (auto it = m_samplerCombos.cbegin(); it != m_samplerCombos.cend(); ++it) {
        QComboBox* combo = it.value();
        const QSignalBlocker block(combo);
        combo->clear();
        combo->addItem(tr("Internal coupler (own PA)"), QString::fromLatin1(kKeinMesspunkt));
        for (const QString& punkt : apd.moeglicheMesspunkte(it.key())) {
            combo->addItem(punkt, punkt);
        }
        const QString gewaehlt = apd.gewaehlterMesspunkt(it.key());
        const int idx = combo->findData(gewaehlt.isEmpty()
                                            ? QString::fromLatin1(kKeinMesspunkt)
                                            : gewaehlt);
        combo->setCurrentIndex(idx >= 0 ? idx : 0);
    }
    m_updatingFromModel = false;
}

}  // namespace Longpath
