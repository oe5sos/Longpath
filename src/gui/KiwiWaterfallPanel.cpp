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

    // Kopfzeile: Ton fuer alle gezeigten Kiwis auf einen Knopf.
    auto* bar = new QHBoxLayout;
    bar->setContentsMargins(0, 0, 0, 0);
    bar->setSpacing(6);
    auto* barLabel = new QLabel(tr("All Kiwis:"), this);
    barLabel->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(label()));
    bar->addWidget(barLabel);
    const QString btnStyle = QStringLiteral(
        "QPushButton { color: %1; background: %2; border: 1px solid %3; border-radius: 6px; "
        "padding: 2px 10px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }")
            .arg(QString::fromLatin1(Style::kTextSecondary), QString::fromLatin1(Style::kButtonBg),
                 QString::fromLatin1(Style::kBorderMuted), QString::fromLatin1(Style::kButtonHover));
    m_allSoundOn = new QPushButton(tr("Sound on"), this);
    m_allSoundOn->setObjectName(QStringLiteral("kiwiAllSoundOn"));
    m_allSoundOn->setToolTip(tr("Listen to every Kiwi shown here that is connected"));
    m_allSoundOn->setStyleSheet(btnStyle);
    m_allSoundOff = new QPushButton(tr("Sound off"), this);
    m_allSoundOff->setObjectName(QStringLiteral("kiwiAllSoundOff"));
    m_allSoundOff->setToolTip(tr("Switch the sound of all these Kiwis off"));
    m_allSoundOff->setStyleSheet(btnStyle);
    bar->addWidget(m_allSoundOn);
    bar->addWidget(m_allSoundOff);
    bar->addStretch(1);
    root->addLayout(bar);
    connect(m_allSoundOn, &QPushButton::clicked, this, [this]() { emit allSoundRequested(true); });
    connect(m_allSoundOff, &QPushButton::clicked, this, [this]() { emit allSoundRequested(false); });

    m_emptyLabel = new QLabel(
        tr("No waterfall switched on — switch one on per receiver in the "
            "KiwiSDR panel."), this);
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
        connect(strip, &KiwiWaterfallStripWidget::soundToggled,
                this, &KiwiWaterfallPanel::soundToggled);
        connect(strip, &KiwiWaterfallStripWidget::zoomRequested,
                this, &KiwiWaterfallPanel::zoomRequested);
        // Allgemeine Einstellung: eine Aenderung gilt fuer alle Streifen.
        connect(strip, &KiwiWaterfallStripWidget::optionsChanged, this, [this, strip]() {
            for (KiwiWaterfallStripWidget* other : std::as_const(m_strips)) {
                if (other != strip) { other->loadOptions(); }
            }
        });
    } else if (auto* strip = m_strips.take(profileId)) {
        m_stripsLayout->removeWidget(strip);
        strip->deleteLater();
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
