// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/audio/VoiceKeyerPlayer.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Siehe den Kopf in VoiceKeyerPlayer.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/audio/VoiceKeyerPlayer.h"

#include "core/AppSettings.h"
#include "core/TxAudioRecorder.h"
#include "core/TxWorkerThread.h"

#include <QLoggingCategory>

#include <algorithm>
#include <cmath>

namespace Longpath {

Q_LOGGING_CATEGORY(lcVoiceKeyer, "longpath.voicekeyer")

namespace {

const QString& repeatSecondsKey()
{
    static const QString kKey = QStringLiteral("VoiceKeyer_RepeatSeconds");
    return kKey;
}

// Der Regler im Panel geht von 1 bis 60 s.
constexpr int kMinRepeatSeconds = 1;
constexpr int kMaxRepeatSeconds = 60;

} // namespace

VoiceKeyerPlayer::VoiceKeyerPlayer(VoiceKeyerStore* store, QObject* parent)
    : QObject(parent)
    , m_store(store)
{
    m_pollTimer.setInterval(kPollMs);
    m_pollTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_pollTimer, &QTimer::timeout, this, &VoiceKeyerPlayer::poll);

    m_tailTimer.setSingleShot(true);
    m_tailTimer.setTimerType(Qt::PreciseTimer);
    m_tailTimer.setInterval(kTailMs);
    connect(&m_tailTimer, &QTimer::timeout, this, &VoiceKeyerPlayer::finishPlayback);

    m_gapTimer.setSingleShot(true);
    connect(&m_gapTimer, &QTimer::timeout, this, [this]() {
        if (m_state != State::Gap) { return; }
        const int slot = m_slot;
        const QString refusal = play(slot);
        if (!refusal.isEmpty()) {
            // Der naechste Ruf ging nicht hinaus (Band gewechselt, Geraet
            // weg …). Nicht still weiterwarten: sagen und aufhoeren.
            setState(State::Idle, -1);
            emit notice(refusal);
        }
    });

    loadSettings();
}

VoiceKeyerPlayer::~VoiceKeyerPlayer()
{
    // Beim Beenden nichts mehr tasten: der MoxController kann schon weg
    // sein. Nur den Abgriff loesen, damit der Sendefaden nicht in einen
    // geloeschten Rekorder schreibt.
    m_source.stop();
    if (m_tapWorker) {
        m_tapWorker->setVoiceTapUser(TxWorkerThread::VoiceTapUser::Dvk, false);
    }
    QObject::disconnect(m_micTap);
}

void VoiceKeyerPlayer::loadSettings()
{
    const int s = AppSettings::instance().value(repeatSecondsKey(), m_repeatSeconds).toInt();
    m_repeatSeconds = std::clamp(s, kMinRepeatSeconds, kMaxRepeatSeconds);
}

void VoiceKeyerPlayer::saveSettings() const
{
    AppSettings::instance().setValue(repeatSecondsKey(), m_repeatSeconds);
}

void VoiceKeyerPlayer::setState(State state, int slot)
{
    if (m_state == state && m_slot == slot) { return; }
    m_state = state;
    m_slot = slot;
    emit stateChanged(state, slot);
}

void VoiceKeyerPlayer::setRepeat(bool on)
{
    if (m_repeat == on) { return; }
    m_repeat = on;
    // Abschalten in der Pause beendet die Wiederholung sofort.
    if (!on && m_state == State::Gap) {
        m_gapTimer.stop();
        setState(State::Idle, -1);
    }
    emit repeatChanged(m_repeat, m_repeatSeconds);
}

void VoiceKeyerPlayer::setRepeatSeconds(int seconds)
{
    const int s = std::clamp(seconds, kMinRepeatSeconds, kMaxRepeatSeconds);
    if (s == m_repeatSeconds) { return; }
    m_repeatSeconds = s;
    saveSettings();
    emit repeatChanged(m_repeat, m_repeatSeconds);
}

// ── Senden ───────────────────────────────────────────────────────────

QString VoiceKeyerPlayer::play(int slot)
{
    if (!m_store || slot < 0 || slot >= m_store->slotCount()) {
        return QStringLiteral("There is no voice message %1.").arg(slot + 1);
    }
    if (m_state == State::Recording) {
        return QStringLiteral("A recording is running — stop it first (Esc).");
    }
    const VoiceKeyerSlot& s = m_store->slot(slot);
    if (s.isEmpty()) {
        return QStringLiteral("Voice message %1 is empty — record it first.").arg(slot + 1);
    }
    if (!m_hooks.key || !m_hooks.keyed || !m_hooks.routeToVoiceKeyer || !m_hooks.restoreRoute) {
        return QStringLiteral("Voice messages can't be sent: the transmit path is not set up.");
    }

    const bool sending = m_state == State::Playing && m_keyedByUs;
    // Ein fremd getasteter Sender (gehaltene Sprechtaste, TUNE) bleibt
    // unangetastet: das Ende der Ansage wuerde ihn sonst abschalten.
    if (!sending && m_hooks.keyed()) {
        return QStringLiteral("The transmitter is already on — voice message %1 not sent.").arg(slot + 1);
    }

    m_gapTimer.stop();
    m_tailTimer.stop();

    QString error;
    // load() haelt die laufende Ansage an und wartet auf den Sendefaden;
    // der Sender bleibt dabei an (eine Ansage loest die andere ab).
    if (!m_source.load(s.wavPath, kRateHz, &error)) {
        qCWarning(lcVoiceKeyer) << "Ansage" << slot + 1 << "nicht lesbar:" << error;
        if (sending) { releaseKey(); }
        setState(State::Idle, -1);
        return QStringLiteral("Voice message %1 can't be read: %2").arg(slot + 1).arg(error);
    }

    // Erst den Weg legen, dann tasten -- andersherum ginge der Sender
    // mit dem Mikrofon auf statt mit der Ansage (wie Ft8Applet).
    if (!m_routed) {
        if (!m_hooks.routeToVoiceKeyer()) {
            setState(State::Idle, -1);
            return QStringLiteral("Voice messages can't be sent with this radio yet.");
        }
        m_routed = true;
    }
    m_source.play();

    if (!sending) {
        m_ownKeyChange = true;
        const QString refusal = m_hooks.key(true);
        m_ownKeyChange = false;
        if (!refusal.isEmpty()) {
            m_source.stop();
            m_hooks.restoreRoute();
            m_routed = false;
            setState(State::Idle, -1);
            return QStringLiteral("Not transmitting: %1").arg(refusal);
        }
        m_keyedByUs = true;
    }

    qCInfo(lcVoiceKeyer) << "Ansage" << slot + 1 << "auf Sendung," << m_source.seconds() << "s";
    setState(State::Playing, slot);
    m_pollTimer.start();
    return {};
}

void VoiceKeyerPlayer::poll()
{
    if (m_state != State::Playing) {
        m_pollTimer.stop();
        return;
    }
    if (m_source.isPlaying() || m_tailTimer.isActive()) { return; }
    m_pollTimer.stop();
    m_tailTimer.start();
}

void VoiceKeyerPlayer::finishPlayback()
{
    if (m_state != State::Playing) { return; }
    const int slot = m_slot;
    releaseKey();
    if (m_repeat) {
        // Empfangspause, dann derselbe Ruf noch einmal.
        setState(State::Gap, slot);
        m_gapTimer.start(m_repeatSeconds * 1000);
        return;
    }
    setState(State::Idle, -1);
}

void VoiceKeyerPlayer::releaseKey()
{
    m_pollTimer.stop();
    m_tailTimer.stop();
    m_source.stop();
    if (m_keyedByUs && m_hooks.key) {
        m_ownKeyChange = true;
        m_hooks.key(false);
        m_ownKeyChange = false;
    }
    m_keyedByUs = false;
    if (m_routed && m_hooks.restoreRoute) {
        m_hooks.restoreRoute();
    }
    m_routed = false;
}

void VoiceKeyerPlayer::stop()
{
    m_gapTimer.stop();
    switch (m_state) {
    case State::Recording:
        finishRecording();
        return;
    case State::Playing:
        qCInfo(lcVoiceKeyer) << "Ansage" << m_slot + 1 << "abgebrochen";
        releaseKey();
        break;
    case State::Gap:
    case State::Idle:
        break;
    }
    setState(State::Idle, -1);
}

void VoiceKeyerPlayer::onKeyedChanged(bool on)
{
    if (m_ownKeyChange) { return; }
    if (!on && m_state == State::Playing && m_keyedByUs) {
        // Jemand anders hat den Sender ausgemacht (MOX-Knopf, Bandplan,
        // Schutzabschaltung). Die Ansage ist damit zu Ende, die
        // Wiederholung auch -- der Betreiber hat entschieden.
        qCInfo(lcVoiceKeyer) << "Sender von aussen abgeschaltet, Ansage endet";
        m_keyedByUs = false;   // nicht noch einmal loslassen
        releaseKey();
        setState(State::Idle, -1);
        return;
    }
    if (on && m_state == State::Gap) {
        // In der Pause selbst getastet: der Betreiber spricht, der Ruf
        // hoert auf.
        m_gapTimer.stop();
        setState(State::Idle, -1);
    }
}

// ── Aufnehmen ────────────────────────────────────────────────────────
//
// Derselbe Weg wie bisher im Panel und im Sprach-Selbsttest: der
// Abgriff preStripAudioReady laeuft auf dem Sendefaden und reicht die
// Mikrofonwerte VOR der Sprachbearbeitung direkt in einen
// TxAudioRecorder. Eine Ansage soll klingen wie die Stimme; beim Senden
// laeuft die Bearbeitung ohnehin darueber. ES WIRD NICHTS GETASTET.

QString VoiceKeyerPlayer::record(int slot)
{
    if (!m_store || slot < 0 || slot >= m_store->slotCount()) {
        return QStringLiteral("There is no voice message %1.").arg(slot + 1);
    }
    if (m_state == State::Recording) {
        const bool same = m_slot == slot;
        finishRecording();
        if (same) { return {}; }
    } else if (m_state != State::Idle) {
        stop();
    }

    TxWorkerThread* worker = m_hooks.txWorker ? m_hooks.txWorker() : nullptr;
    if (!worker) {
        return QStringLiteral("The transmit path is not up yet — connect to the radio first, then record.");
    }

    if (!m_recorder) {
        m_recorder = new TxAudioRecorder(this);
        m_recorder->setSampleRate(kRateHz);
        connect(m_recorder, &TxAudioRecorder::recordingFull, this, [this]() {
            if (m_state != State::Recording) { return; }
            const int slot = m_slot;
            finishRecording();
            emit notice(QStringLiteral("Voice message %1: recording stopped at %2 s.")
                            .arg(slot + 1).arg(TxAudioRecorder::kMaxSeconds));
        });
    }
    m_recorder->clear();
    m_recorder->start();
    m_recordPeak.store(0.0f, std::memory_order_relaxed);

    QObject::disconnect(m_micTap);
    m_micTap = connect(worker, &TxWorkerThread::preStripAudioReady, m_recorder,
                       [this](const float* samples, int frames) {
        // Sendefaden: nur rechnen und atomar ablegen.
        float peak = 0.0f;
        for (int i = 0; i < frames; ++i) {
            peak = std::max(peak, std::fabs(samples[i]));
        }
        float seen = m_recordPeak.load(std::memory_order_relaxed);
        while (peak > seen
               && !m_recordPeak.compare_exchange_weak(seen, peak, std::memory_order_relaxed)) {
        }
        m_recorder->feed(samples, frames);
    }, Qt::DirectConnection);
    worker->setVoiceTapUser(TxWorkerThread::VoiceTapUser::Dvk, true);
    m_tapWorker = worker;

    qCInfo(lcVoiceKeyer) << "Aufnahme Platz" << slot + 1 << "beginnt";
    setState(State::Recording, slot);
    return {};
}

void VoiceKeyerPlayer::finishRecording()
{
    if (m_state != State::Recording || !m_recorder) { return; }
    const int slot = m_slot;

    // Reihenfolge wie im Sprach-Selbsttest: erst das Tor zu, dann die
    // Verbindung loesen. Andersherum liefe der Abgriff noch, waehrend
    // niemand mehr zuhoert.
    if (m_tapWorker) {
        m_tapWorker->setVoiceTapUser(TxWorkerThread::VoiceTapUser::Dvk, false);
    }
    m_tapWorker = nullptr;
    QObject::disconnect(m_micTap);
    m_recorder->stop();
    setState(State::Idle, -1);

    QVector<float> samples(m_recorder->recordedFrames());
    if (!samples.isEmpty()) {
        std::copy_n(m_recorder->samples(), samples.size(), samples.begin());
    }

    QString err;
    if (!m_store->setRecording(slot, samples, kRateHz, &err)) {
        qCWarning(lcVoiceKeyer) << "Aufnahme Platz" << slot + 1 << "nicht gespeichert:" << err;
        emit notice(QStringLiteral("Voice message %1: nothing was stored: %2").arg(slot + 1).arg(err));
        return;
    }
    qCInfo(lcVoiceKeyer) << "Aufnahme Platz" << slot + 1 << "gespeichert," << samples.size() << "Proben";
}

float VoiceKeyerPlayer::takeRecordPeak()
{
    return m_recordPeak.exchange(0.0f, std::memory_order_relaxed);
}

} // namespace Longpath
