// src/gui/widgets/StationBlock.h
#pragma once

#include <QWidget>
#include <QString>

class QLabel;

namespace Longpath {

class StationPowerKnopf;

// StationBlock — the radio's on/off switch in the bottom bar.
//
// Entwurf A „Netzschalter", vom Betreiber am 2026-11-27 gewaehlt
// (werkzeug/entwuerfe-ein-aus-pille-2026-11-27/A-netzschalter.png):
// a round power button on the left, the radio name beside it, a second
// row underneath. Three states, all in the brass of the transmit key —
// no green, no dashed red:
//
//   off         grey border and button, last radio's name in grey,
//               "off · click to switch on"
//   connecting  dark brass, "connecting…"
//   on          brass border and lit button, "on · <hardware line>"
//
// The whole block is the switch: click → emits clicked(); the host decides
// (connected → disconnect, otherwise → connect). Right-click → emits
// contextMenuRequested(globalPos) while connected.
class StationBlock : public QWidget {
    Q_OBJECT

public:
    explicit StationBlock(QWidget* parent = nullptr);

    // Name of the CONNECTED radio. Empty → off (or connecting).
    void setRadioName(const QString& name);
    QString radioName() const noexcept { return m_radioName; }

    // Name shown while off: the radio a click would switch on. Empty →
    // "Click to connect" (nothing remembered yet).
    void setTargetName(const QString& name);
    QString targetName() const noexcept { return m_targetName; }

    // Between the click and the first data: dark brass, "connecting…".
    void setConnecting(bool connecting);
    bool isConnecting() const noexcept { return m_connecting; }

    bool isConnectedAppearance() const noexcept { return !m_radioName.isEmpty(); }

    // Second row while on: "ANAN-G2 · v27". Either part may be empty; the
    // middle dot appears only when both are present. Cleared automatically
    // whenever the radio name is cleared.
    void setHardwareLine(const QString& model, const QString& firmware);
    QString hardwareLine() const noexcept { return m_hardwareLine; }

    // What the second row currently says (for tests).
    QString statusText() const;

signals:
    void clicked();
    void contextMenuRequested(const QPoint& globalPos);

protected:
    void mousePressEvent(QMouseEvent* event) override;

private:
    void refresh();

    QString  m_radioName;
    QString  m_targetName;
    bool     m_connecting{false};
    QString  m_hardwareLine;
    StationPowerKnopf* m_knopf{nullptr};
    QLabel*  m_label{nullptr};
    QLabel*  m_hardwareLabel{nullptr};
};

} // namespace Longpath
