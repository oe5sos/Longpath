// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe docs/architecture/
// 2026-08-27-kiwisdr-self-report-concept.md, Variante C.
//
//   2026-08-27 — Neu angelegt.

#include "gui/KiwiWaterfallStripWidget.h"

#include "gui/StyleConstants.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cstring>

namespace Longpath {

namespace {
QString ink()   { return Style::role("text",       Style::kTextPrimary); }
QString scale() { return Style::role("text-scale", Style::kTextScale);   }
QString trace() { return Style::role("trace",      Style::kSpectrumTrace); }
QString border(){ return Style::role("border",     Style::kBorder);      }
} // namespace

KiwiWaterfallStripWidget::KiwiWaterfallStripWidget(const QString& profileId,
                                                    const QString& displayName,
                                                    QWidget* parent)
    : QWidget(parent)
    , m_profileId(profileId)
{
    setObjectName(QStringLiteral("kiwiWaterfallStrip"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "QWidget#kiwiWaterfallStrip { background: %1; "
        "border: 1px solid %2; border-radius: 6px; }")
            .arg(QString::fromLatin1(Style::kInsetBg), border()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(9, 7, 9, 9);
    layout->setSpacing(5);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(6);

    m_nameLabel = new QLabel(displayName, this);
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_nameLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(ink()));
    header->addWidget(m_nameLabel, 1);

    m_peakLabel = new QLabel(QStringLiteral("—"), this);
    m_peakLabel->setTextFormat(Qt::PlainText);
    m_peakLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_peakLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 10.5px; "
                        "font-family: monospace; }").arg(trace()));
    header->addWidget(m_peakLabel, 0);

    layout->addLayout(header);
    layout->addStretch(1);

    m_history = QImage(1, kHistoryRows, QImage::Format_RGB32);
    m_history.fill(QColor(Style::kAppBg));
}

void KiwiWaterfallStripWidget::setDisplayName(const QString& name)
{
    m_nameLabel->setText(name);
}

void KiwiWaterfallStripWidget::pushRow(const QVector<float>& binsDbm)
{
    if (binsDbm.isEmpty()) {
        return;
    }

    if (m_history.width() != binsDbm.size()) {
        m_history = QImage(binsDbm.size(), kHistoryRows, QImage::Format_RGB32);
        m_history.fill(QColor(Style::kAppBg));
    } else {
        // Scroll the existing buffer down by one row IN PLACE instead of
        // allocating a brand-new kHistoryRows-tall QImage on every incoming
        // waterfall row (a live KiwiSDR sends several per second). The old
        // code (`const QImage old = m_history; m_history = QImage(...); ...`)
        // reallocated + repainted the full buffer every single call — pure
        // churn on the hot path, found 2026-09-06 while chasing a runaway
        // memory event that force-quit the whole app during a live KiwiSDR
        // session. memmove() is safe on the overlapping same-buffer move a
        // QPainter blit is not. scanLine() below already calls detach()
        // internally, so the buffer is never shared with another QImage
        // while this mutates it in place.
        const int bytesPerLine = m_history.bytesPerLine();
        std::memmove(m_history.scanLine(1), m_history.scanLine(0),
                     static_cast<size_t>(bytesPerLine) * (kHistoryRows - 1));
    }

    const QColor floor(Style::kAppBg);
    const QColor peak(Style::kSpectrumTrace);
    float maxDbm = kMinDbm;
    for (int x = 0; x < binsDbm.size(); ++x) {
        const float dbm = binsDbm[x];
        maxDbm = std::max(maxDbm, dbm);
        const float t = std::clamp((dbm - kMinDbm) / (kMaxDbm - kMinDbm), 0.0f, 1.0f);
        const QColor c(
            floor.red()   + int((peak.red()   - floor.red())   * t),
            floor.green() + int((peak.green() - floor.green()) * t),
            floor.blue()  + int((peak.blue()  - floor.blue())  * t));
        m_history.setPixelColor(x, 0, c);
    }

    m_peakLabel->setText(QStringLiteral("%1 dBm").arg(qRound(maxDbm)));
    update();
}

void KiwiWaterfallStripWidget::reset()
{
    m_history.fill(QColor(Style::kAppBg));
    m_peakLabel->setText(QStringLiteral("—"));
    update();
}

QSize KiwiWaterfallStripWidget::sizeHint() const
{
    return QSize(320, 78);
}

void KiwiWaterfallStripWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    const QRect target(9, 30, width() - 18, height() - 39);
    if (target.height() > 0 && target.width() > 0 && m_history.width() > 0) {
        painter.drawImage(target, m_history);
    }
}

} // namespace Longpath
