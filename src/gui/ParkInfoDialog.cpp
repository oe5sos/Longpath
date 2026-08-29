// SPDX-License-Identifier: GPL-3.0-or-later
//
// NereusSDR - Park Info dialog (implementation). See ParkInfoDialog.h.
//
// NereusSDR-native, no upstream equivalent.

#include "ParkInfoDialog.h"

#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace Longpath {

namespace {

QString escaped(const QString& s)
{
    return s.toHtmlEscaped();
}

// "Automobile,Foot,Other" -> "Automobile, Foot, Other"
QString commaListToProse(const QString& csv)
{
    return QString(csv).replace(',', ", ");
}

} // namespace

ParkInfoDialog::ParkInfoDialog(QWidget* parent)
    : QDialog(parent)
{
    setObjectName("parkInfoDialog");
    setWindowTitle("Park Info");
    setModal(false);
    setMinimumWidth(420);
    setStyleSheet("QDialog { background: #0f0f1a; }");

    auto* layout = new QVBoxLayout(this);

    m_titleLabel = new QLabel;
    m_titleLabel->setObjectName("parkInfoTitleLabel");
    m_titleLabel->setStyleSheet("QLabel { color: #4a7ba8; font-size: 16px; font-weight: bold; }");
    m_titleLabel->setWordWrap(true);
    layout->addWidget(m_titleLabel);

    m_bodyLabel = new QLabel;
    m_bodyLabel->setObjectName("parkInfoBodyLabel");
    m_bodyLabel->setStyleSheet("QLabel { color: #c8d8e8; font-size: 12px; }");
    m_bodyLabel->setTextFormat(Qt::RichText);
    m_bodyLabel->setWordWrap(true);
    m_bodyLabel->setOpenExternalLinks(true);
    m_bodyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);

    auto* scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setStyleSheet("QScrollArea { background: transparent; border: none; }");
    scroll->setWidget(m_bodyLabel);
    layout->addWidget(scroll, 1);
}

void ParkInfoDialog::showLoading(const QString& reference)
{
    m_titleLabel->setText(escaped(reference));
    m_bodyLabel->setText("<i>Fetching park information...</i>");
    show();
    raise();
    activateWindow();
}

void ParkInfoDialog::showInfo(const PotaParkInfo& info)
{
    m_titleLabel->setText(QString("%1 %2%3")
        .arg(escaped(info.reference), escaped(info.name),
             info.active ? QString() : QString(" <span style='color:#c2635a;'>(inactive)</span>")));

    QStringList rows;
    if (!info.parktypeDesc.isEmpty()) {
        rows << QString("<b>Type:</b> %1").arg(escaped(info.parktypeDesc));
    }
    if (!info.entityName.isEmpty()) {
        rows << QString("<b>Entity:</b> %1").arg(escaped(info.entityName));
    }
    if (!info.locationName.isEmpty()) {
        rows << QString("<b>Location(s):</b> %1").arg(escaped(commaListToProse(info.locationName)));
    }
    if (!info.grid6.isEmpty()) {
        rows << QString("<b>Grid:</b> %1 &nbsp;&nbsp; <b>Coordinates:</b> %2, %3")
                    .arg(escaped(info.grid6))
                    .arg(info.latitude, 0, 'f', 4)
                    .arg(info.longitude, 0, 'f', 4);
    }
    if (!info.firstActivator.isEmpty()) {
        rows << QString("<b>First Activation:</b> %1 on %2")
                    .arg(escaped(info.firstActivator), escaped(info.firstActivationDate));
    }
    if (!info.accessMethods.isEmpty()) {
        rows << QString("<b>Access:</b> %1").arg(escaped(commaListToProse(info.accessMethods)));
    }
    if (!info.activationMethods.isEmpty()) {
        rows << QString("<b>Activation Methods:</b> %1").arg(escaped(commaListToProse(info.activationMethods)));
    }
    if (!info.agencies.isEmpty()) {
        rows << QString("<b>Managing Agency:</b> %1").arg(escaped(info.agencies));
    }
    if (!info.website.isEmpty()) {
        rows << QString("<b>Website:</b> <a href=\"%1\" style='color:#4a7ba8;'>%1</a>")
                    .arg(escaped(info.website));
    }
    if (!info.parkComments.isEmpty()) {
        rows << QString("<br><b>Notes:</b><br>%1").arg(escaped(info.parkComments));
    }

    m_bodyLabel->setText(rows.join("<br>"));
    show();
    raise();
    activateWindow();
}

void ParkInfoDialog::showError(const QString& reference, const QString& error)
{
    m_titleLabel->setText(escaped(reference));
    m_bodyLabel->setText(QString("<span style='color:#c2635a;'>Lookup failed: %1</span>")
        .arg(escaped(error)));
    show();
    raise();
    activateWindow();
}

} // namespace Longpath
