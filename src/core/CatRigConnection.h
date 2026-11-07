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
// Es liefert kein I/Q, aber seinen Ton ueber USB. Diese Verbindung liest
// den Ton und schickt ihn, in I/Q gewandelt (core/AudioZuIq.h), in
// denselben Weg wie ein SDR: Ton in Longpath, ein schmaler Wasserfall ueber
// die Filterbreite des Geraets, Filter und NR. Ein Bandbild ist das nicht.
// Die Frequenz wandert ueber core/CatRigBruecke.h, nicht ueber diese
// Klasse. Fehlt der Tonanschluss, bleibt die Verbindung stehen, nur ohne
// Ton: CAT allein ist auch ein Betrieb.
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

#include "core/AudioZuIq.h"

#include <QByteArray>
#include <QString>
#include <QVector>

#include <atomic>

class QAudioSource;
class QIODevice;

namespace Longpath {

class CatRigConnection : public RadioConnection {
    Q_OBJECT

public:
    explicit CatRigConnection(QObject* parent = nullptr);
    ~CatRigConnection() override;

    // ProtocolVersion::CatRig
    int protocolVersion() const override { return 6; }

    /// Name des Toneingangs, oder ein Teil davon. Vor connectToRadio().
    void setTonEingang(const QString& name) { m_tonEingang = name; }

    /// LSB liegt unter dem Traeger. Aus jedem Faden aufrufbar.
    void setGespiegelt(bool gespiegelt) { m_gespiegelt.store(gespiegelt); }

    /// Wo der Traeger des Geraets steht. Aus jedem Faden aufrufbar.
    ///
    /// Der Empfangsweg rechnet wie bei einem SDR: die Mitte des Stroms
    /// kommt ueber setReceiverFrequency(), und WDSP stimmt um den Abstand
    /// VFO minus Mitte ab (CTUN). Der Ton sitzt aber immer am Traeger.
    /// Darum wird er genau um diesen Abstand verschoben, und er liegt in
    /// Panadapter und Demodulator dort, wo er auf dem Band liegt.
    void setTraegerHz(double hz) { m_traegerHz.store(hz); }

    /// So viele Tonproben ergeben einen Block: 10 ms bei 48 kHz.
    static constexpr int kBlockProben = 480;

    /// Liefert der Eingang gerade Ton?
    bool tonLaeuft() const { return m_quelle != nullptr; }

    /// 16-Bit-Ton hinein, mit `kanaele` Kanaelen. Oeffentlich fuer den
    /// Pruefstand: so geht der Weg ohne Tongeraet.
    void verarbeiteTon(const QByteArray& pcm, int kanaele);

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

private:
    void starteTon();
    void stoppeTon();
    void lieferBloecke();
    void schiebeAnDenTraeger(QVector<float>& iq);

    QString m_tonEingang;
    QAudioSource* m_quelle{nullptr};
    QIODevice*    m_tonGeraet{nullptr};
    int           m_kanaele{2};
    AudioZuIq     m_wandler;
    QVector<float> m_vorrat;
    std::atomic<bool> m_gespiegelt{false};
    std::atomic<double> m_traegerHz{0.0};
    std::atomic<double> m_mitteHz{0.0};
    double m_phase{0.0};
};

} // namespace Longpath
