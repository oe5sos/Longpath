#pragma once

// no-port-check: Longpath-original RadioConnection subclass, no Thetis
// precedent. Thetis drives only radios it computes for.

// =================================================================
// src/core/CatRigConnection.h  (Longpath)
// =================================================================
//
// Ein klassisches Funkgeraet, das Longpath nur per CAT fuehrt, zuerst
// der Kenwood TS-590S.
//
// Es liefert kein I/Q. Diese Verbindung schiebt darum keine einzige
// Probe: Panadapter und Wasserfall bleiben leer, und Longpaths Filter
// und NR haben nichts zu rechnen. Sie sorgt nur dafuer, dass das Geraet
// im Verbinden-Fenster steht wie jedes andere und dass VFO, Band- und
// Modus-Knoepfe den ueblichen Weg gehen. Die Frequenz wandert ueber
// core/CatRigBruecke.h, nicht ueber diese Klasse.
//
// ── Senden ──────────────────────────────────────────────────────
//
// Diese Klasse KANN nicht tasten. setMox() ist leer, und sie haelt
// nichts, womit sie das Geraet ueberhaupt erreichen koennte.
// RigctldClient hat keinen PTT-Befehl, und das bleibt so: Longpath
// tastet kein fremdes Geraet. Gesendet wird am Geraet, mit Mikrofon und
// PTT-Taste. So laeuft der Funkbetrieb weiter, wenn der Rechner ausfaellt
// (Betreiber, 2026-10-27).
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "RadioConnection.h"

namespace Longpath {

class CatRigConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit CatRigConnection(QObject* parent = nullptr);
    ~CatRigConnection() override;

    // ProtocolVersion::CatRig
    int protocolVersion() const override { return 6; }

public slots:
    void init() override;
    void connectToRadio(const Longpath::RadioInfo& info) override;
    void disconnect() override;

    // Alles leer: siehe Klassenkommentar. Ein Aufrufer soll fuer dieses
    // Geraet keinen Sonderfall brauchen.
    void setReceiverFrequency(int receiverIndex, quint64 frequencyHz) override;
    void setActiveReceiverCount(int count) override;
    void setSampleRate(int sampleRate) override;
    void setTxFrequency(quint64 frequencyHz) override;
    void setAttenuator(int dB) override;
    void setPreamp(bool enabled) override;
    void setTxDrive(int level) override;
    void setMox(bool enabled) override;
    void setAntennaRouting(AntennaRouting routing) override;
    void sendTxIq(const float* iq, int n) override;
    void setTrxRelay(bool enabled) override;
    void setMicBoost(bool on) override;
    void setLineIn(bool on) override;
    void setMicTipRing(bool tipHot) override;
    void setMicBias(bool on) override;
    void setLineInGain(int gain) override;
    void setUserDigOut(quint8 dig) override;
    void setPuresignalRun(bool run) override;
    void setMicPTTDisabled(bool disabled) override;
    void setMicXlr(bool xlrJack) override;
    void setWatchdogEnabled(bool enabled) override;
};

} // namespace Longpath
