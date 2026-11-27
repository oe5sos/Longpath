// src/gui/widgets/StationBlock.cpp
#include "StationBlock.h"
#include "gui/StyleConstants.h"   // LP_MONO_QSS
#include "gui/styles/ThemeQss.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QVBoxLayout>

namespace Longpath {

namespace {

// Entwurf A (2026-11-27): the brass of the transmit key, three steps.
constexpr auto kOffBorder   = "#3a4656";   // off: quiet slate
constexpr auto kOffGlyph    = "#6a7888";
constexpr auto kOffName     = "#8090a0";
constexpr auto kBusyBrass   = "#8a6a30";   // connecting: dark brass (kTxKeyBottom)
constexpr auto kOnBrass     = "#c9a04e";   // on: brass
constexpr auto kOnKnopfBg   = "#1a1610";
constexpr auto kOffKnopfBg  = "#12161e";
constexpr auto kNameOn      = "#c8d8e8";
constexpr auto kDim         = "#607080";

} // namespace

// The round power button. Paints only; presses fall through to the block.
class StationPowerKnopf : public QWidget {
public:
    explicit StationPowerKnopf(QWidget* parent) : QWidget(parent)
    {
        setFixedSize(28, 28);
        setAttribute(Qt::WA_TransparentForMouseEvents, true);
    }

    void setFarben(const QColor& ring, const QColor& glyph, const QColor& grund,
                   bool leuchtet)
    {
        m_ring = ring;
        m_glyph = glyph;
        m_grund = grund;
        m_leuchtet = leuchtet;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        const QRectF r = QRectF(rect()).adjusted(1.5, 1.5, -1.5, -1.5);
        if (m_leuchtet) {
            // A soft brass halo, inside the widget so nothing spills.
            QColor halo = m_glyph;
            halo.setAlpha(60);
            p.setPen(QPen(halo, 3.0));
            p.setBrush(Qt::NoBrush);
            p.drawEllipse(r.adjusted(0.5, 0.5, -0.5, -0.5));
        }
        p.setPen(QPen(m_ring, 1.0));
        p.setBrush(m_grund);
        p.drawEllipse(r.adjusted(1.5, 1.5, -1.5, -1.5));

        // ⏻: an open arc and a bar through the gap.
        const QPointF c = r.center();
        const qreal rad = 6.5;
        QPen pen(m_glyph, 1.8);
        pen.setCapStyle(Qt::RoundCap);
        p.setPen(pen);
        p.setBrush(Qt::NoBrush);
        const QRectF arc(c.x() - rad, c.y() - rad + 0.5, 2 * rad, 2 * rad);
        // Qt angles: 0 = 3 o'clock, counter-clockwise, 1/16 degree. The gap
        // sits at 12 o'clock, 60 degrees wide.
        p.drawArc(arc, 120 * 16, 300 * 16);
        p.drawLine(QPointF(c.x(), c.y() - rad - 0.5), QPointF(c.x(), c.y() + 0.5));
    }

private:
    QColor m_ring{kOffBorder};
    QColor m_glyph{kOffGlyph};
    QColor m_grund{kOffKnopfBg};
    bool   m_leuchtet{false};
};

StationBlock::StationBlock(QWidget* parent) : QWidget(parent)
{
    auto* hbox = new QHBoxLayout(this);
    hbox->setContentsMargins(8, 2, 12, 2);
    hbox->setSpacing(10);

    m_knopf = new StationPowerKnopf(this);
    hbox->addWidget(m_knopf, 0, Qt::AlignVCenter);

    auto* vbox = new QVBoxLayout();
    vbox->setContentsMargins(0, 0, 0, 0);
    vbox->setSpacing(0);

    m_label = new QLabel(this);
    m_label->setObjectName(QStringLiteral("StationBlock_Label"));
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_label->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    vbox->addWidget(m_label);

    m_hardwareLabel = new QLabel(this);
    m_hardwareLabel->setObjectName(QStringLiteral("StationBlock_Status"));
    m_hardwareLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    // Presses must reach StationBlock::mousePressEvent, not stop here.
    m_hardwareLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    vbox->addWidget(m_hardwareLabel);

    hbox->addLayout(vbox, 1);

    setMinimumWidth(170);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_StyledBackground, true);  // QSS background paint on QWidget subclass
    refresh();
}

void StationBlock::setRadioName(const QString& name)
{
    if (m_radioName == name) { return; }
    m_radioName = name;
    if (name.isEmpty()) {
        m_hardwareLine.clear();
    } else {
        // Connected: whoever was connecting has arrived; remember the
        // name for the off state.
        m_connecting = false;
        m_targetName = name;
    }
    refresh();
}

void StationBlock::setTargetName(const QString& name)
{
    if (m_targetName == name) { return; }
    m_targetName = name;
    refresh();
}

void StationBlock::setConnecting(bool connecting)
{
    if (m_connecting == connecting) { return; }
    m_connecting = connecting;
    refresh();
}

void StationBlock::setHardwareLine(const QString& model, const QString& firmware)
{
    QString line = model;
    if (!model.isEmpty() && !firmware.isEmpty()) {
        line += QStringLiteral(" · ");
    }
    line += firmware;

    if (m_hardwareLine == line) {
        return;
    }
    m_hardwareLine = line;
    refresh();
}

QString StationBlock::statusText() const
{
    if (isConnectedAppearance()) {
        return m_hardwareLine.isEmpty()
                   ? QStringLiteral("on · click to switch off")
                   : QStringLiteral("on · ") + m_hardwareLine;
    }
    if (m_connecting) {
        return QStringLiteral("connecting…");
    }
    if (m_targetName.isEmpty()) {
        return QString();
    }
    return QStringLiteral("off · click to switch on");
}

void StationBlock::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    } else if (event->button() == Qt::RightButton) {
        // Only emit contextMenu when in connected appearance — disconnected
        // STATION has nothing to disconnect / edit / forget.
        if (isConnectedAppearance()) {
            emit contextMenuRequested(event->globalPosition().toPoint());
        }
    }
    QWidget::mousePressEvent(event);
}

void StationBlock::refresh()
{
    const bool an = isConnectedAppearance();
    const bool busy = !an && m_connecting;

    QString name;
    if (an) {
        name = m_radioName;
    } else if (!m_targetName.isEmpty()) {
        name = m_targetName;
    } else {
        name = QStringLiteral("Click to connect");
    }
    m_label->setText(name);

    const QString status = statusText();
    m_hardwareLabel->setText(status);
    m_hardwareLabel->setVisible(!status.isEmpty());

    const QString border = an ? QString::fromLatin1(kOnBrass)
                         : busy ? QString::fromLatin1(kBusyBrass)
                                : QString::fromLatin1(kOffBorder);
    const QString nameColour = (an || busy) ? QString::fromLatin1(kNameOn)
                                            : QString::fromLatin1(kOffName);
    const QString statusColour = busy ? QString::fromLatin1(kBusyBrass)
                                      : QString::fromLatin1(kDim);

    setStyleSheet(Style::themed(QStringLiteral(
        "Longpath--StationBlock { border: 1px solid %1;"
        " background: #0a0a14; border-radius: 7px; }"
        "QLabel#StationBlock_Label { color: %2; font-family: " LP_MONO_QSS ";"
        " font-size: 13px; font-weight: bold; background: transparent; border: none; }"
        "QLabel#StationBlock_Status { color: %3; font-size: 11px;"
        " background: transparent; border: none; }")
        .arg(border, nameColour, statusColour)));

    if (an) {
        m_knopf->setFarben(QColor(kOnBrass), QColor(kOnBrass),
                           QColor(kOnKnopfBg), true);
    } else if (busy) {
        m_knopf->setFarben(QColor(kBusyBrass), QColor(kBusyBrass),
                           QColor(kOffKnopfBg), false);
    } else {
        m_knopf->setFarben(QColor(kOffBorder), QColor(kOffGlyph),
                           QColor(kOffKnopfBg), false);
    }
}

} // namespace Longpath
