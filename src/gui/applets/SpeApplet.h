// =================================================================
// src/gui/applets/SpeApplet.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/ten9876/AetherSDR), GPLv3,
// primary author Jeremy [KK7GWY]:
//   src/gui/SpeApplet.h at d58e2b8a
//   src/gui/SpeApplet.cpp at d58e2b8a
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Schritt 3 von Zeus-Punkt 7: das Bedienfeld. Was es ANZEIGT und was
// es NICHT anzeigt, ist aus AetherSDR uebernommen und richtet sich nach
// dem, was der Zustandsstring des Geraets wirklich hergibt. Wie es
// aussieht, richtet sich nach dem Haus: Rf2ksApplet ist das Vorbild fuer
// Aufbau und Farben, und es ist KEINE Farbe und KEINE Schriftstufe neu
// dazugekommen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
//
//   Abweichungen, alle weil das Haus es anders macht:
//     * Erbt AppletWidget (nicht QWidget), nimmt RadioModel* im
//       Konstruktor und liefert appletId()/appletTitle()/
//       syncFromModel() -- so wie AmpApplet und Rf2ksApplet.
//     * HGauge: Longpaths Setter-Form (setRange/setYellowStart/
//       setRedStart/setTitle/setUnit/setTickLabels) statt AetherSDRs
//       Konstruktor mit Stellungen. setBallistics(), clearPeak() und
//       setValueImmediate() gibt es hier nicht; der Spitzenwert wird
//       mit setPeakValue(Skalenanfang) geloescht.
//     * Die drei Messwerte stehen in der Mulde statt in eigenen
//       Beschriftungsfeldern links: Longpaths HGauge kann das selbst
//       (setReadout, 2026-09-02), einschliesslich des Gedankenstrichs
//       solange der Wert auf dem Skalenanfang steht -- genau die
//       Schwelle, die AetherSDR mit `m_fwdWatts >= 1.0f` von Hand
//       nachbaut. Damit entfallen drei QLabel und die 10-Hz-Bremse
//       fuer sie.
//     * ThemeManager mit {{color.*}}-Vorlagen gibt es hier nicht:
//       Style::themed() und die Konstanten aus StyleConstants.h. Die
//       Knopffarben fuer OPERATE/STANDBY sind WOERTLICH die aus
//       Rf2ksApplet::setOperateMode -- keine neue Farbe.
//     * Schriftstufen auf die Leiter geholt (AetherSDR nimmt 9/10/11,
//       die 10 kennt scripts/verify-style-drift.py nicht).
//     * markTxKeying() auf dem TUNE-Knopf entfaellt -- AetherSDRs
//       TxKeyingMarker hat hier kein Gegenstueck. Der Hinweis, dass
//       TUNE mit Vorlauf laeuft, steht im Sprechblasentext.
//     * Die Zustandspille nimmt kein AmpPillState aus einer geteilten
//       Kopfdatei (gibt es hier nicht), sondern einen eigenen,
//       gleichwertigen enum.
//
//   Verhalten, Anzeigeinhalt, Knopfumfang und die Begruendungen in den
//   Kommentaren sind unveraendert -- einschliesslich der Entscheidung,
//   bei STILLE ALLE Messwerte zu leeren statt sie stehen zu lassen.
// =================================================================

#pragma once
#include "AppletWidget.h"

#include <QChar>
#include <QPushButton>
#include <QString>
#include <QTimer>

class QLabel;

namespace Longpath {

class HGauge;
class RadioModel;

// Dedicated applet for an SPE Expert linear amplifier (1.3K-FA/1.5K-FA/
// 2K-FA) — a sibling of AmpApplet (PGXL) and Rf2ksApplet, not a variant of
// either: a station can run a PGXL, an RF2K-S and an SPE at once.
//
// The SPE's Status string reports output power, antenna-side SWR, AND
// ATU-input-side SWR as independently real fields, so all three get a
// permanent gauge row. Supply voltage/current and the heatsink
// temperatures are text readouts — the protocol defines no nominal/max
// scale to size an axis against.
//
// Buttons mirror the amplifier's own front panel keys (every remote command
// IS a keystroke in this protocol): OPER/STBY toggle, power-level cycle
// (the button label shows the CURRENT level, LOW/MID/HIGH), TUNE, OFF,
// INPUT, ANT, and the ▲/▼ arrow keys — which on the Expert adjust the
// drive power the amplifier requests from the radio over CAT. Band keys
// are not exposed: the amp follows the radio's band via CAT/RF sensing.
// SET/DISPLAY menu navigation and manual L/C tuning are deliberately not
// exposed — SPE reserves complex operations for their own KTerm
// application, and blind menu navigation without the amp's display is a
// foot-gun.
class SpeApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit SpeApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("Spe"); }
    QString appletTitle() const override { return QStringLiteral("SPE Expert"); }
    void    syncFromModel() override {}

    // Gauge scale configuration — call when the model is known (the Status
    // ID field identifies it on the very first poll reply; see
    // SpeConnection::modelChanged).
    void setPowerRange(float nominalW, float warnW, float maxW);
    void setModelName(const QString& displayName);

    // Telemetry
    void setForwardPower(float watts);
    void setSwrAnt(float swr);
    void setSwrAtu(float swr);
    void setSupplyVoltage(float volts);   // text readout, not a gauge
    void setSupplyCurrent(float amps);    // text readout, not a gauge
    // Heatsink temperatures. The amp reports degrees in whichever unit its
    // own display is configured for (C or F, not indicated on the wire —
    // spec §5), so values are shown verbatim with a bare ° sign rather
    // than guessing a unit or offering a conversion toggle. Lower/combiner
    // are only real on the 2K-FA; hasCombiner hides them elsewhere.
    void setTemps(int upper, int lower, int combiner, bool hasCombiner);
    void setBand(const QString& band);
    void setAntenna(int antenna, QChar atuState);
    void setInputPort(int input);
    // "LOW"/"MID"/"HIGH" — shown in the info grid AND as the power-level
    // button's own label, mirroring the amplifier's own display.
    void setPowerLevel(const QString& levelName);
    void setMode(bool operate, bool transmitting);  // drives pill + OPR/STBY button
    // empty clears/hides the banner. `alarm` picks the tone: ein ALARM ist
    // die Warnfarbe des Hauses (kRedBorder, "Sendet / Gefahr"), eine
    // WARNUNG das Bernstein daneben. AetherSDR hat nur einen Ton und
    // nimmt dafuer immer die Gefahrenfarbe; der Hausstil haelt das
    // kraeftige Rot der Gefahr vor (siehe HAUSSTIL.md §Bedeutung), und
    // "ATU bypassed" ist keine.
    void setFaultText(const QString& text, bool alarm = false);
    void setSource(const QString& text);            // "SERIAL" or "NETWORK"
    void setConnected(bool connected);   // shows/hides live controls, resets on disconnect
    // Transport up but the amplifier isn't answering polls (ser2net with the
    // amp switched off). Commands are disabled and the pill goes neutral
    // until it answers again — the transport-level connected state alone
    // can't tell this apart.
    void setResponding(bool responding);

    // Pruefstand-Nahtstellen. Longpath-Zusatz: AetherSDRs Fassung hat
    // keinen Pruefstand fuer dieses Feld, und ohne Lesezugriff auf die
    // Beschriftungen laesst sich "bei Stille wird GELEERT" nicht
    // belegen -- genau der Satz, auf den es hier ankommt.
    QString statusPillTextForTesting() const;
    QString infoTextForTesting() const;
    QString faultTextForTesting() const;
    bool    faultIsAlarmForTesting() const;
    QString powerLevelButtonTextForTesting() const;
    bool    commandsEnabledForTesting() const;
    bool    powerOnEnabledForTesting() const;

signals:
    void powerOnClicked();     // hardware power-ON pulse (works while the amp is silent)
    void operateClicked();     // OPERATE key — toggles STANDBY <-> OPERATE
    void powerLevelClicked();  // POWER key — cycles LOW/MID/HIGH
    void tuneClicked();        // TUNE key
    void offClicked();         // SWITCH OFF key
    void inputClicked();       // INPUT key — toggles input 1/2
    void antennaClicked();     // ANTENNA key
    void driveUpClicked();     // ▲ (RIGHT-arrow key) — raise requested drive power
    void driveDownClicked();   // ▼ (LEFT-arrow key) — lower requested drive power

private:
    enum class Pill { Neutral, Standby, OperateRx, OperateTx };

    void updateValueLabels();  // 10 Hz throttled label text refresh
    void updateCommandsEnabled();
    void applyModePill();
    // Blanks every reading back to its not-yet-known state. Shared by the
    // disconnect path and the stopped-answering path — both mean "what is on
    // screen is no longer telemetry", and a frozen-but-plausible panel is the
    // worse failure of the two.
    void clearTelemetry();

    HGauge* m_pwrGauge{nullptr};
    HGauge* m_swrAntGauge{nullptr};
    HGauge* m_swrAtuGauge{nullptr};

    QLabel* m_statusPill{nullptr};
    QLabel* m_sourceLabel{nullptr};
    QLabel* m_modelLabel{nullptr};

    // Info grid — 3 cells per row: temp / V / I, then band / antenna / input·level.
    QLabel* m_tempLabel{nullptr};
    QLabel* m_voltLabel{nullptr};
    QLabel* m_currLabel{nullptr};
    QLabel* m_bandLabel{nullptr};
    QLabel* m_antLabel{nullptr};
    QLabel* m_inputLabel{nullptr};

    QLabel* m_faultLabel{nullptr};
    bool    m_faultIsAlarm{false};

    QPushButton* m_onBtn{nullptr};
    QPushButton* m_operateBtn{nullptr};
    QPushButton* m_pwrLevelBtn{nullptr};
    QPushButton* m_tuneBtn{nullptr};
    QPushButton* m_offBtn{nullptr};
    QPushButton* m_inputBtn{nullptr};
    QPushButton* m_antBtn{nullptr};
    QPushButton* m_driveDownBtn{nullptr};
    QPushButton* m_driveUpBtn{nullptr};

    QTimer m_labelTimer;
    QTimer* m_peakTimer{nullptr};
    float m_peakFwd{0.0f};

    // Last applied power-gauge scale — setPowerRange() no-ops on repeats so
    // the wiring can re-derive the level-dependent scale on every status
    // frame (10/s) without triggering a repaint per frame.
    float m_rangeNominal{-1.0f};
    float m_rangeWarn{-1.0f};
    float m_rangeMax{-1.0f};

    float m_fwdWatts{0.0f};
    float m_swrAntVal{1.0f};
    float m_swrAtuVal{1.0f};
    float m_supplyVolts{0.0f};
    float m_supplyAmps{0.0f};
    bool  m_operate{false};
    bool  m_transmitting{false};
    bool  m_connected{false};
    bool  m_responding{false};

    // Telemetry arrives at the 10 Hz poll rate, but the dirty-flag/10 Hz
    // timer throttle is kept so label repaints stay bounded regardless of
    // what a future faster poll (or a chatty firmware) delivers.
    bool m_tempDirty{false};
    QString m_pendingTempText;
    bool m_diagDirty{false};
    bool m_bandDirty{false};
    QString m_pendingBand;
    bool m_antDirty{false};
    QString m_pendingAntText;
    bool m_inputDirty{false};
    int m_inputPort{0};       // 0 = not yet reported
    QString m_levelName;      // empty = not yet reported
};

}  // namespace Longpath
