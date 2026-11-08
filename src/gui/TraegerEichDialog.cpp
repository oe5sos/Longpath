// no-port-check: Longpath-original dialog; see TraegerEichDialog.h.
//
// src/gui/TraegerEichDialog.cpp  (Longpath)

#include "gui/TraegerEichDialog.h"

#include "core/FrequenzEichung.h"
#include "core/TransverterSetup.h"
#include "gui/StyleConstants.h"
#include "models/RadioModel.h"

#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QVBoxLayout>

#include <cmath>

namespace Longpath {

namespace {
QString mhz(double hz)
{
    return QString::number(hz / 1.0e6, 'f', 6) + QStringLiteral(" MHz");
}
} // namespace

TraegerEichDialog::TraegerEichDialog(RadioModel* radio, bool gefunden, double geraetHz,
                                     double anzeigeHz, float snrDb, QWidget* parent)
    : QDialog(parent)
    , m_radio(radio)
    , m_geraetHz(geraetHz)
    , m_anzeigeHz(anzeigeHz)
{
    setWindowTitle(tr("Calibrate frequency"));
    setAttribute(Qt::WA_DeleteOnClose);
    auto* lay = new QVBoxLayout(this);

    if (!gefunden) {
        auto* t = new QLabel(tr("No carrier found within ±2.5 kHz of that point "
                                "(at least 10 dB above the noise). Zoom in on the "
                                "carrier and try again."), this);
        t->setWordWrap(true);
        lay->addWidget(t);
        auto* box = new QDialogButtonBox(QDialogButtonBox::Close, this);
        connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
        lay->addWidget(box);
        return;
    }

    auto* form = new QFormLayout();
    form->addRow(tr("Measured:"),
                 new QLabel(tr("%1 (SNR %2 dB)").arg(mhz(anzeigeHz)).arg(qRound(snrDb)), this));

    m_wahr = new QDoubleSpinBox(this);
    m_wahr->setObjectName(QStringLiteral("eichWahr"));
    m_wahr->setStyleSheet(Style::doubleSpinBoxStyle());
    m_wahr->setLocale(QLocale::c());
    m_wahr->setDecimals(6);
    m_wahr->setRange(0.001, 30000.0);
    m_wahr->setSingleStep(0.001);
    m_wahr->setSuffix(QStringLiteral(" MHz"));
    // Baken und Normalfrequenzsender liegen auf ganzen kHz.
    m_wahr->setValue(std::round(anzeigeHz / 1000.0) / 1000.0);
    m_wahr->setToolTip(tr("The carrier's true frequency, e.g. from a beacon list."));
    form->addRow(tr("True frequency:"), m_wahr);

    m_abweichung = new QLabel(this);
    form->addRow(tr("Deviation:"), m_abweichung);
    m_ziel = new QLabel(this);
    m_ziel->setWordWrap(true);
    m_ziel->setStyleSheet(QString::fromLatin1(Style::kSecondaryLabelStyle));
    form->addRow(tr("Corrects:"), m_ziel);
    lay->addLayout(form);

    auto* box = new QDialogButtonBox(this);
    m_uebernehmen = box->addButton(tr("Apply"), QDialogButtonBox::AcceptRole);
    box->addButton(QDialogButtonBox::Close);
    connect(box, &QDialogButtonBox::accepted, this, [this]() {
        if (uebernimm()) { accept(); }
    });
    connect(box, &QDialogButtonBox::rejected, this, &QDialog::reject);
    lay->addWidget(box);

    connect(m_wahr, &QDoubleSpinBox::valueChanged, this, [this]() { aktualisiere(); });
    aktualisiere();
}

void TraegerEichDialog::aktualisiere()
{
    if (!m_wahr) { return; }
    const double wahr = m_wahr->value() * 1.0e6;
    const double hz = m_anzeigeHz - wahr;
    m_abweichung->setText(tr("%1%2 Hz (%3 ppm)")
                              .arg(hz >= 0 ? QStringLiteral("+") : QString())
                              .arg(qRound(hz))
                              .arg(FrequenzEichung::ppm(m_anzeigeHz, wahr), 0, 'f', 2));
    bool moeglich = false;
    switch (m_radio ? m_radio->eichZielFuer(m_geraetHz) : RadioModel::EichZiel::NurAnzeige) {
    case RadioModel::EichZiel::Transverter: {
        const TransverterSetup t = TransverterSetup::current();
        const double neu = t.loErrorFuer(qRound64(m_anzeigeHz), qRound64(wahr));
        m_ziel->setText(tr("Transverter LO error: %1 → %2 kHz")
                            .arg(t.loErrorKhz, 0, 'f', 3).arg(neu, 0, 'f', 3));
        moeglich = std::abs(neu) <= TransverterSetup::kLoErrorGrenzeKhz;
        break;
    }
    case RadioModel::EichZiel::Geraet: {
        const auto& cal = m_radio->calibrationController();
        if (cal.using10MHzRef()) {
            m_ziel->setText(tr("Nothing — the radio runs on an external 10 MHz "
                               "reference."));
            break;
        }
        const double alt = cal.freqCorrectionFactor();
        // Am Geraet ist wahr = Anzeige (kein Transverter im Spiel).
        const double neu = FrequenzEichung::neuerFaktor(alt, m_geraetHz,
                                                        m_geraetHz + (wahr - m_anzeigeHz));
        m_ziel->setText(tr("Radio reference (correction factor): %1 → %2")
                            .arg(alt, 0, 'f', 9).arg(neu, 0, 'f', 9));
        moeglich = true;
        break;
    }
    case RadioModel::EichZiel::NurAnzeige:
        m_ziel->setText(tr("Nothing — this radio's reference can't be set from "
                           "Longpath. The deviation is shown only."));
        break;
    }
    m_uebernehmen->setEnabled(moeglich);
}

bool TraegerEichDialog::uebernimm()
{
    if (!m_radio || !m_wahr) { return false; }
    const double wahr = m_wahr->value() * 1.0e6;
    return m_radio->uebernimmEichung(m_geraetHz, m_anzeigeHz, wahr);
}

void TraegerEichDialog::setWahrHzForTest(double hz) { if (m_wahr) { m_wahr->setValue(hz / 1.0e6); } }
bool TraegerEichDialog::uebernehmenForTest() { return uebernimm(); }
QString TraegerEichDialog::zielTextForTest() const { return m_ziel ? m_ziel->text() : QString(); }
QString TraegerEichDialog::abweichungTextForTest() const
{
    return m_abweichung ? m_abweichung->text() : QString();
}
bool TraegerEichDialog::uebernehmenMoeglichForTest() const
{
    return m_uebernehmen && m_uebernehmen->isEnabled();
}

} // namespace Longpath
