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

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
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
        tr("No Kiwi running — double-click one in the KiwiSDR panel."), this);
    m_emptyLabel->setTextFormat(Qt::PlainText);
    m_emptyLabel->setWordWrap(true);
    m_emptyLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; padding: 4px 2px; }")
            .arg(label()));
    root->addWidget(m_emptyLabel);

    m_stripsLayout = new QVBoxLayout;
    m_stripsLayout->setContentsMargins(0, 0, 0, 0);
    m_stripsLayout->setSpacing(8);
    // Die Streifen teilen sich die Hoehe: ein hoeheres Fenster macht sie
    // hoeher (kein Rest-Stretch darunter).
    root->addLayout(m_stripsLayout, 1);

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
        m_stripsLayout->addWidget(strip, 1);
        connect(strip, &KiwiWaterfallStripWidget::closeRequested,
                this, &KiwiWaterfallPanel::removeRequested);
        connect(strip, &KiwiWaterfallStripWidget::renameRequested,
                this, &KiwiWaterfallPanel::renameRequested);
        connect(strip, &KiwiWaterfallStripWidget::expandToggled, this,
                [this](const QString& id, bool on) {
            m_expandedId = on ? id : QString();
            for (auto it = m_strips.cbegin(); it != m_strips.cend(); ++it) {
                it.value()->setVisible(!on || it.key() == id);
                it.value()->setExpandedState(on && it.key() == id);
            }
        });
        if (!m_expandedId.isEmpty()) { strip->setVisible(false); }
        connect(strip, &KiwiWaterfallStripWidget::soundToggled,
                this, &KiwiWaterfallPanel::soundToggled);
        connect(strip, &KiwiWaterfallStripWidget::zoomRequested,
                this, &KiwiWaterfallPanel::zoomRequested);
        connect(strip, &KiwiWaterfallStripWidget::chooseOtherRequested,
                this, &KiwiWaterfallPanel::chooseOtherRequested);
        // Allgemeine Einstellung: eine Aenderung gilt fuer alle Streifen.
        connect(strip, &KiwiWaterfallStripWidget::optionsChanged, this, [this, strip]() {
            for (KiwiWaterfallStripWidget* other : std::as_const(m_strips)) {
                if (other != strip) { other->loadOptions(); }
            }
        });
    } else if (auto* strip = m_strips.take(profileId)) {
        m_stripsLayout->removeWidget(strip);
        strip->deleteLater();
        if (m_expandedId == profileId) {
            m_expandedId.clear();
            for (KiwiWaterfallStripWidget* other : std::as_const(m_strips)) {
                other->setVisible(true);
                other->setExpandedState(false);
            }
        }
    }
    updateEmptyState();
}

void KiwiWaterfallPanel::pushRow(const QString& profileId,
                                 const QVector<float>& binsDbm,
                                 double lowHz, double highHz)
{
    if (auto* strip = m_strips.value(profileId, nullptr)) {
        strip->pushRow(binsDbm, lowHz, highHz);
    }
}

void KiwiWaterfallPanel::setSoundActive(const QString& profileId, bool on)
{
    if (auto* strip = m_strips.value(profileId, nullptr)) {
        strip->setSoundActive(on);
    }
}

void KiwiWaterfallPanel::setNotice(const QString& profileId, const QString& text)
{
    if (auto* strip = m_strips.value(profileId, nullptr)) {
        strip->setNotice(text);
    }
}

void KiwiWaterfallPanel::setLost(const QString& profileId, const QString& title,
                                 const QString& detail)
{
    if (auto* strip = m_strips.value(profileId, nullptr)) {
        strip->setLost(title, detail);
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
    if (m_wasEmpty != m_strips.isEmpty()) {
        m_wasEmpty = m_strips.isEmpty();
        emit emptyChanged(m_wasEmpty);
    }
}

} // namespace Longpath
