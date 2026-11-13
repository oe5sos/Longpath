// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

// =================================================================
// src/core/Ts590Sprache.h  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original.
//
// Der Sprachspeicher ueber den TS-590 (Betreiber, 2026-11-12: Punkt 2
// der Liste „was beim 590 noch heraus zu holen ist").
//
// ── Die Ausnahme von der Regel ─────────────────────────────────────
//
// Longpath tastet ein fremdes Geraet nicht -- das gilt weiter fuer MOX,
// die Sprechtaste und jeden anderen Weg. DIESE Klasse ist die eine,
// abgesicherte Ausnahme, und sie tastet nur, wenn ALLES stimmt:
//
//   1. Eingeschaltet in Setup → Companion rig (Vorgabe AUS).
//   2. Der 590 haengt per CAT an, Longpath ist mit ihm verbunden.
//   3. Ein Transverter ist eingestellt und nicht „nur Empfang" (IFo2).
//   4. Am Geraet steht DRV an: sonst ginge die Leistung auf die
//      Antennenbuchse statt auf den Transverter.
//   5. Die Leistung ist gelesen und hoechstens kMaxLeistungW.
//   6. Eine Sprachbetriebsart (USB, LSB, FM, AM).
//
// Faellt eine Bedingung WAEHREND der Sendung (DRV aus, Verbindung weg),
// geht der Sender sofort aus. Nach kMaxSendenMs geht er auf jeden Fall
// aus.
//
// ── Wie gesendet wird ──────────────────────────────────────────────
//
// `TX1;` ist beim 590 DATA SEND: gesendet wird der Ton der Datenleitung,
// die Menue 63 waehlt (USB). Das Mikrofon bleibt stumm. `RX;` beendet.
// Der Ton geht als 48 kHz Stereo an das USB-Tongeraet des 590; die
// Ansage holt diese Klasse selbst aus der WavTxSource des Spielers.
//
// ── Contesttauglich (2026-11-13) ───────────────────────────────────
//
// Zwischen dem 590 und der Ansage liegt keine Sprachbearbeitung von
// Longpath (an der ANAN laeuft dafuer die ganze WDSP-Kette). Darum hier:
//
//   * Vorlauf kVorlaufMs Stille nach TX1: der 590 und der Transverter
//     dahinter schalten erst um. Ohne ihn verschluckt der Sender die
//     erste Silbe -- und die Aufnahme beginnt seit heute direkt mit ihr.
//   * Hochpass und ein fester Pegel je Ansage (AnsageAufbereitung):
//     jede Ansage treibt die ALC gleich, egal wie laut sie aufgenommen
//     wurde. Die Verstaerkung wird bei jeder neu geladenen Ansage aus der
//     GANZEN Datei bestimmt, nicht im Lauf geregelt.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-12 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
//   2026-11-13 — Vorlauf nach TX1, Hochpass und fester Pegel je
//                 Ansage; KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/audio/AnsageAufbereitung.h"

#include <QObject>
#include <QPointer>
#include <QString>
#include <QTimer>
#include <QVector>

#include <memory>

class QAudioSink;
class QIODevice;

namespace Longpath {

class CompanionRigController;
class WavTxSource;

class Ts590Sprache : public QObject
{
    Q_OBJECT

public:
    static constexpr int kMaxLeistungW = 5;
    static constexpr int kMaxSendenMs  = 30000;
    static constexpr int kPumpeMs      = 10;
    static constexpr int kRateHz       = 48000;
    // Stille nach TX1, bevor die Ansage beginnt. Der 590 schaltet in
    // einigen zehn ms um, der Transverter (PTT ueber REMOTE) dahinter
    // auch; 150 ms decken beides mit Reserve und fallen im Ruf nicht auf.
    static constexpr int kVorlaufMs    = 150;

    static QByteArray befehlSenden()   { return QByteArrayLiteral("W TX1; 0\n"); }
    static QByteArray befehlEmpfangen() { return QByteArrayLiteral("W RX; 0\n"); }

    Ts590Sprache(CompanionRigController* rig, WavTxSource* quelle,
                 QObject* parent = nullptr);
    ~Ts590Sprache() override;

    /// Ist der Weg ueberhaupt gemeint? (Einstellung an, 590 per CAT.)
    /// Erst dann uebernimmt diese Klasse den Sprachspeicher.
    bool zustaendig() const;

    /// Leer: darf senden. Sonst ein Satz fuer den Betreiber.
    QString sperrgrund() const;

    /// Den Ton an den 590 legen. false, wenn das Tongeraet fehlt.
    bool legeWeg();
    void nimmWegZurueck();

    /// Tasten oder los. Leer bei Erfolg, sonst der Grund.
    QString taste(bool an);
    bool getastet() const { return m_getastet; }

    /// Fuer den Pruefstand: statt des USB-Tongeraets hier hinein schreiben.
    void setTonZielForTest(QIODevice* ziel) { m_testZiel = ziel; }
    void setMaxSendenMsForTest(int ms) { m_hoechstens.setInterval(ms); }
    float verstaerkungForTest() const { return m_verstaerkung; }
    void pumpeForTest() { pumpe(); }

signals:
    /// Der Sender ist durch diese Klasse an- oder ausgegangen.
    void getastetGeaendert(bool an);
    /// Ein Satz fuer den Betreiber (Sperre waehrend der Sendung, Frist).
    void hinweis(const QString& text);

private:
    void pumpe();
    void pruefeWaehrendDerSendung();
    void lasLos(const QString& grund);

    CompanionRigController* m_rig;
    WavTxSource*            m_quelle;
    bool                    m_getastet{false};

    std::unique_ptr<QAudioSink> m_senke;
    QPointer<QIODevice>     m_ziel;
    QPointer<QIODevice>     m_testZiel;
    QTimer                  m_pumpe;
    QTimer                  m_hoechstens;
    QVector<float>          m_mono;
    QByteArray              m_stereo;

    int                     m_vorlaufRest{0};
    int                     m_geladen{-1};       // loadCount der Quelle
    float                   m_verstaerkung{1.0f};
    AnsageAufbereitung::Hochpass m_hochpass;
};

} // namespace Longpath
