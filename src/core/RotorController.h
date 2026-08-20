#pragma once

// =================================================================
// src/core/RotorController.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Thetis drives rotators through an external
// program; there is no upstream to port.
//
// What a rotator has to be able to tell us, and be told.
//
// The interface exists before the second implementation because the
// operator asked for network and serial both. Hamlib's rotctld covers
// the network case and, run locally, most serial rotators too — so a
// direct serial backend is a later subclass, not a rewrite.
//
// The contract that matters is about honesty. A rotator is slow
// machinery at the end of a wire: it can be unreachable, it can be
// turning, and it can be lying because nobody has asked it lately.
// Implementations must distinguish "the rotator says 143 degrees" from
// "143 degrees is the last thing it said, four minutes ago" — a needle
// showing a stale position with no hint that it is stale is worse than
// a needle showing nothing.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-07 — Created in C++20/Qt6 for NereusSDR, AI-assisted via
//                 Anthropic Claude (Cowork), operator Martin Fischer.
// =================================================================

#include <QObject>
#include <QString>

namespace Longpath {

class RotorController : public QObject {
    Q_OBJECT
public:
    using QObject::QObject;
    ~RotorController() override = default;

    enum class State {
        Disconnected,   // nothing is talking to a rotator
        Connecting,
        Idle,           // connected, position known, not moving
        Moving,         // a target was accepted and it is turning
        Error,          // connected but the rotator refused something
    };
    Q_ENUM(State)

    // For the UI: "rotctld 192.168.1.20:4533".
    virtual QString description() const = 0;

    virtual State state() const = 0;
    bool isConnected() const
    {
        const State s = state();
        return s == State::Idle || s == State::Moving || s == State::Error;
    }

    // Last reported azimuth in degrees true, and whether that report is
    // recent enough to steer by. A caller that ignores hasFreshPosition
    // will draw a needle that looks live and is not.
    virtual double azimuth() const = 0;
    virtual bool hasFreshPosition() const = 0;

    virtual void connectToRotor() = 0;
    virtual void disconnectFromRotor() = 0;

    // Turn to this bearing, degrees true. Ignored when not connected —
    // callers get the refusal through errorOccurred rather than a
    // return code, because the real refusal arrives later anyway.
    virtual void moveTo(double azimuthDeg) = 0;

    // Halt where it is.
    virtual void stop() = 0;

signals:
    void stateChanged(Longpath::RotorController::State state);

    // A fresh reading from the rotator. Not emitted for values the
    // software worked out for itself.
    void positionChanged(double azimuthDeg);

    // Human-readable, already suitable for a status line. Emitted for
    // refusals and transport failures alike, because to the operator
    // "the rotator said no" and "the rotator is not there" are the same
    // problem: the antenna did not move.
    void errorOccurred(const QString& message);
};

} // namespace Longpath
