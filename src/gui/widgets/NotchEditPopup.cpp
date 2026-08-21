// SPDX-License-Identifier: GPL-3.0-or-later
//
// Siehe NotchEditPopup.h fuer den Grund, dass es dieses Fenster gibt.

#include "NotchEditPopup.h"

#include <QCheckBox>
#include <QDoubleSpinBox>
#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>

#include "gui/StyleConstants.h"

namespace Longpath {

namespace {

// Die Grenzen sind die des Modells, nicht erfundene: NotchModel nimmt
// Breiten in Hz entgegen, und WDSP setzt zu schmale selbst herauf
// (nbp.c:122-125). Oben gedeckelt, damit ein Vertipper nicht das halbe
// Band wegnotcht.
constexpr int kMinWidthHz = 10;
constexpr int kMaxWidthHz = 20000;

} // namespace

NotchEditPopup::NotchEditPopup(QWidget* parent)
    : QWidget(parent, Qt::Popup)
{
    buildUi();
}

void NotchEditPopup::buildUi()
{
    setObjectName(QStringLiteral("notchEditPopup"));
    setStyleSheet(QStringLiteral(
        "QWidget#notchEditPopup {"
        "  background: %1; border: 1px solid %2; border-radius: 8px;"
        "}"
        "QLabel { color: %3; font-size: 11px; }")
        .arg(Style::hexRole(Style::kPanelBg),
             Style::hexRole(Style::kBorder),
             Style::hexRole(Style::kLabelMid)));

    auto* grid = new QGridLayout(this);
    grid->setContentsMargins(10, 8, 10, 8);
    grid->setHorizontalSpacing(8);
    grid->setVerticalSpacing(6);

    grid->addWidget(new QLabel(tr("Mitte")), 0, 0);
    m_centre = new QDoubleSpinBox(this);
    m_centre->setDecimals(3);
    m_centre->setRange(0.0, 1'000'000.0);      // kHz
    m_centre->setSingleStep(0.1);
    m_centre->setSuffix(QStringLiteral(" kHz"));
    m_centre->setToolTip(tr("Mittenfrequenz des Notch-Filters."));
    m_centre->setStyleSheet(Style::doubleSpinBoxStyle());
    grid->addWidget(m_centre, 0, 1);

    grid->addWidget(new QLabel(tr("Breite")), 1, 0);
    m_width = new QSpinBox(this);
    m_width->setRange(kMinWidthHz, kMaxWidthHz);
    m_width->setSingleStep(10);
    m_width->setSuffix(QStringLiteral(" Hz"));
    m_width->setToolTip(tr(
        "Breite des Notch-Filters. Sehr schmale Werte setzt die "
        "Signalverarbeitung selbst herauf — dann steht hier danach die "
        "Zahl, die wirklich gilt."));
    m_width->setStyleSheet(Style::spinBoxStyle());
    grid->addWidget(m_width, 1, 1);

    m_active = new QCheckBox(tr("Aktiv"), this);
    m_active->setToolTip(tr(
        "Ausgeschaltet bleibt der Filter liegen und wirkt nicht — "
        "praktisch zum Vergleichen, ohne ihn wegzuwerfen."));
    grid->addWidget(m_active, 2, 0, 1, 2);

    auto* del = new QPushButton(tr("Löschen"), this);
    del->setToolTip(tr("Diesen Notch-Filter entfernen."));
    del->setStyleSheet(Style::redCheckedStyle());
    grid->addWidget(del, 3, 0, 1, 2);

    connect(m_centre, &QDoubleSpinBox::valueChanged, this, [this](double kHz) {
        if (m_loading || m_id < 0) { return; }
        emit centreRequested(m_id, kHz * 1000.0);
    });
    connect(m_width, &QSpinBox::valueChanged, this, [this](int hz) {
        if (m_loading || m_id < 0) { return; }
        emit widthRequested(m_id, static_cast<double>(hz));
    });
    connect(m_active, &QCheckBox::toggled, this, [this](bool on) {
        if (m_loading || m_id < 0) { return; }
        emit activeRequested(m_id, on);
    });
    connect(del, &QPushButton::clicked, this, [this]() {
        if (m_id < 0) { return; }
        const int id = m_id;
        m_id = -1;               // kein zweites Loeschen beim Zugehen
        close();
        emit removeRequested(id);
    });
}

void NotchEditPopup::showFor(int id, double centreHz, double widthHz,
                             bool active, const QPoint& globalPos)
{
    applyFromModel(id, centreHz, widthHz, active);
    adjustSize();
    move(globalPos);
    show();
    m_centre->setFocus();
}

void NotchEditPopup::applyFromModel(int id, double centreHz, double widthHz,
                                    bool active)
{
    // Der Wächter: sonst schickt jedes Nachfuehren aus dem Modell einen
    // neuen Wunsch zurueck, und die beiden schaukeln sich hoch. Dieselbe
    // Regel wie ueberall sonst im Programm (m_updatingFromModel).
    m_loading = true;
    m_id = id;
    m_centre->setValue(centreHz / 1000.0);
    m_width->setValue(qBound(kMinWidthHz, qRound(widthHz), kMaxWidthHz));
    m_active->setChecked(active);
    m_loading = false;
}

} // namespace Longpath
