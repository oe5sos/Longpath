// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe docs/architecture/
// 2026-08-27-kiwisdr-self-report-concept.md, Variante C.
//
//   2026-08-27 — Neu angelegt.
//
// Aufbau bewusst nach dem Muster von KiwiSdrApplet::KiwiSdrApplet
// (eigenes QVBoxLayout(this), kein appletTitleBar() — das ist ein
// optionaler zweiter Kopf, den nicht jedes Applet benutzt).

#include "gui/KiwiWaterfallPanel.h"

#include "gui/KiwiWaterfallStripWidget.h"
#include "gui/StyleConstants.h"

#include <QLabel>
#include <QVBoxLayout>

namespace Longpath {

namespace {
QString label() { return Style::role("text-scale", Style::kTextScale); }
} // namespace

KiwiWaterfallPanel::KiwiWaterfallPanel(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 2, 4, 4);
    root->setSpacing(8);

    m_emptyLabel = new QLabel(
        tr("Kein Wasserfall eingeschaltet — im KiwiSDR-Panel je Empfänger "
           "einschalten."), this);
    m_emptyLabel->setTextFormat(Qt::PlainText);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 10px; padding: 4px 2px; }")
            .arg(label()));
    root->addWidget(m_emptyLabel);

    m_stripsLayout = new QVBoxLayout;
    m_stripsLayout->setContentsMargins(0, 0, 0, 0);
    m_stripsLayout->setSpacing(8);
    root->addLayout(m_stripsLayout);
    root->addStretch(1);

    updateEmptyState();
}

void KiwiWaterfallPanel::setStripEnabled(const QString& profileId, bool enabled,
                                         const QString& displayName)
{
    if (enabled) {
        if (auto* existing = m_strips.value(profileId, nullptr)) {
            existing->setDisplayName(displayName);
            return;
        }
        auto* strip = new KiwiWaterfallStripWidget(profileId, displayName, this);
        m_strips.insert(profileId, strip);
        m_stripsLayout->addWidget(strip);
    } else if (auto* strip = m_strips.take(profileId)) {
        m_stripsLayout->removeWidget(strip);
        strip->deleteLater();
    }
    updateEmptyState();
}

void KiwiWaterfallPanel::pushRow(const QString& profileId,
                                 const QVector<float>& binsDbm)
{
    if (auto* strip = m_strips.value(profileId, nullptr)) {
        strip->pushRow(binsDbm);
    }
}

void KiwiWaterfallPanel::resetStrip(const QString& profileId)
{
    if (auto* strip = m_strips.value(profileId, nullptr)) {
        strip->reset();
    }
}

void KiwiWaterfallPanel::updateEmptyState()
{
    m_emptyLabel->setVisible(m_strips.isEmpty());
}

} // namespace Longpath
