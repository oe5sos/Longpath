// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe KiwiLostOverlay.h.
//
//   2026-11-25 — Neu angelegt.

#include "gui/widgets/KiwiLostOverlay.h"

#include "gui/StyleConstants.h"

#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace Longpath {

KiwiLostOverlay::KiwiLostOverlay(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("kiwiLostOverlay"));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 2, 4, 2);
    layout->setSpacing(4);
    layout->addStretch(1);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("kiwiLostTitle"));
    m_label->setTextFormat(Qt::PlainText);
    m_label->setAlignment(Qt::AlignCenter);
    // Schmale Streifen: der Text darf abgeschnitten werden, statt den
    // Streifen breiter zu zwingen.
    m_label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    m_label->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; font-size: 15px; font-weight: 600; }")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    layout->addWidget(m_label);

    m_choose = new QPushButton(tr("Choose another Kiwi"), this);
    m_choose->setObjectName(QStringLiteral("kiwiLostChoose"));
    m_choose->setCursor(Qt::PointingHandCursor);
    m_choose->setFixedHeight(22);
    // Blau ist anfassbar (HAUSSTIL).
    m_choose->setStyleSheet(QStringLiteral(
        "QPushButton { color: %1; background: %2; border: 1px solid %3; "
        "border-radius: 6px; padding: 0 12px; font-size: 11px; }"
        "QPushButton:hover { border-color: %4; }")
            .arg(QString::fromLatin1(Style::kTextPrimary),
                 QString::fromLatin1(Style::kInsetBg),
                 QString::fromLatin1(Style::kBlueBorder),
                 QString::fromLatin1(Style::kAccent)));
    connect(m_choose, &QPushButton::clicked, this, &KiwiLostOverlay::chooseRequested);
    layout->addWidget(m_choose, 0, Qt::AlignHCenter);
    layout->addStretch(1);

    hide();
}

void KiwiLostOverlay::setLost(const QString& title, const QString& detail)
{
    m_title = title;
    m_label->setText(title);
    setToolTip(detail);
    setVisible(!title.isEmpty());
    if (!title.isEmpty()) { raise(); }
}

void KiwiLostOverlay::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    QColor dim(Style::hexRole(Style::kAppBg));
    dim.setAlpha(200);
    painter.fillRect(rect(), dim);
}

} // namespace Longpath
