// =================================================================
// src/gui/applets/AcomApplet.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/ten9876/AetherSDR), GPLv3,
// primary author Jeremy [KK7GWY]:
//   src/gui/AcomApplet.h at d58e2b8a
//   src/gui/AcomApplet.cpp at d58e2b8a
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level. Both projects are GPLv3, so the code carries forward under the
// same licence per GPLv3 §5.
//
// Schritt 3 von drei. Was das Feld ANZEIGT, ist uebernommen und richtet
// sich nach dem, was der 0x2F-Rahmen wirklich hergibt; WIE es aussieht,
// richtet sich nach dem Haus (Rf2ksApplet, SpeApplet, Kpa500Applet).
//
// ── DREI BALKEN, KEIN UMSCHALTER ────────────────────────────────────
//
// Vorlauf, Ruecklauf UND Stehwelle sind im selben Rahmen drei
// eigenstaendig echte Felder. AetherSDRs erster Entwurf hatte dafuer
// einen Umschalter zwischen Stehwelle und Ruecklauf -- noetig nur,
// solange das Feld sich die drei Balkenplaetze mit dem PGXL teilte. Mit
// eigenem Feld entfaellt der Zwang: alle drei stehen dauerhaft da, ohne
// Klick, mit mehr Auskunft auf einen Blick.
//
// Der Drainstrom (Id) ist eine ZAHL, kein Balken: das Protokoll gibt
// fuer ihn keinen Soll- oder Hoechstwert her, an dem sich eine Achse
// aufziehen liesse. Dieselbe Begruendung wie bei Spannung und
// Temperatur beim SPE.
//
// ── DREI KNOEPFE, KEIN UMSCHALTER ───────────────────────────────────
//
// STANDBY, OPERATE und AUS sind drei eigene Knoepfe, nicht ein
// umschaltender wie beim SPE und KPA500. Grund ist das Protokoll: der
// ACOM kennt drei ZIELZUSTAENDE (0x05/0x06/0x0A), und einer davon
// (Aus) ist keine Gegenseite des anderen. Ein Umschalter muesste
// raten, was „das Gegenteil" von Aus ist.
//
// ── DIE BETRIEBSSTUNDEN SIND KEINE EINSCHALTDAUER ───────────────────
//
// Das Feld „system clock" zaehlt die GESAMTE Betriebszeit des Geraets
// und wird beim Ausschalten nicht zurueckgesetzt. Am echten 600S
// bestaetigt (Front 127:43:17 gegen dekodierte 459.797 s). Darum steht
// hier „BETRIEB", nicht „an seit".
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Ported to Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude (Claude Code).
//
//   Abweichungen, alle weil das Haus es anders macht:
//     * Erbt AppletWidget (nicht QWidget), nimmt RadioModel* und
//       liefert appletId()/appletTitle()/syncFromModel().
//     * Longpaths HGauge-Setter statt AetherSDRs Konstruktor mit
//       Stellungen; die Messwerte stehen ueber setReadout() in der
//       Mulde statt in eigenen Beschriftungsfeldern.
//     * Style::themed() + StyleConstants.h statt ThemeManager mit
//       {{color.*}}-Vorlagen; Knopffarben WOERTLICH die benannten aus
//       StyleConstants.h (kAmpOperateActiveBtnStyle /
//       kAmpNeutralBtnStyle), keine neue Farbe, keine neue
//       Schriftstufe.
//     * Eigener Pill-enum statt einer geteilten Kopfdatei.
//     * Sechs Lesezugriffe fuer den Pruefstand.
//
//   Anzeigeinhalt, Knopfumfang und die Begruendungen unveraendert --
//   einschliesslich der Entscheidung, bei getrennter Verbindung ALLE
//   Messwerte zu leeren.
// =================================================================

#pragma once
#include "AppletWidget.h"

#include <QPushButton>
#include <QString>

#include "core/AcomProtocol.h"

class QLabel;

namespace Longpath {

class HGauge;
class RadioModel;

class AcomApplet : public AppletWidget {
    Q_OBJECT

public:
    explicit AcomApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("Acom"); }
    QString appletTitle() const override { return QStringLiteral("ACOM"); }
    void    syncFromModel() override {}

    // Balkenachsen -- werden bei jeder Modellmeldung neu gesetzt, nicht
    // nur einmal beim Verbinden (die Selbstskalierung kann die Stufe
    // mitten in der Sitzung anheben).
    void setPowerRange(float nominalW, float maxW);
    void setReflectedRange(float nominalW, float maxW);

    void setForwardPower(float watts);
    void setReflectedPower(float watts);
    void setSwr(float swr);
    void setDrainCurrent(float amps);    // Zahl, kein Balken
    void setDrainVoltage(float volts);   // HV1
    void setTemp(float degC);
    void setBand(const QString& band);
    // GESAMTE Betriebszeit, nicht die Dauer seit dem Einschalten.
    void setUptime(quint32 totalSeconds);
    void setSource(const QString& text);
    void setModelName(const QString& name);
    void setMode(Acom::Mode mode);
    void setFaultText(const QString& text);
    // Der Loeschknopf bleibt SICHTBAR und wird nur bedienbar, wenn
    // wirklich ein Fehler anliegt -- anders als beim KPA500, wo er
    // erscheint und verschwindet. Grund: beim ACOM gehoert er in die
    // Reihe der drei Zustandsknoepfe, und eine Reihe, die ihre Breite
    // aendert, springt bei jedem Fehler.
    void setClearFaultEnabled(bool enabled);
    void setConnected(bool connected);
    // „Warum ist die Skala, wie sie ist" -- als Sprechblase an der
    // Zustandspille, nicht als dauerhaftes Feld.
    void setDiagnosticTooltip(const QString& text);

    // Pruefstand-Nahtstellen.
    QString statusPillTextForTesting() const;
    QString infoTextForTesting() const;
    QString faultTextForTesting() const;
    QString diagnosticTooltipForTesting() const;
    // Die gesetzten Achsen. Ohne sie laesst sich nicht belegen, dass BEIDE
    // mit der Modellstufe nachgezogen werden -- die Sprechblase traegt nur
    // die Vorlaufzahlen, und eine Gegenprobe, die nur die Vorlauf-Achse
    // entfernt, liefe damit gruen durch. Genau so passiert.
    float reflectedNominalForTesting() const { return m_reflNominal; }
    float reflectedMaxForTesting() const { return m_reflMax; }
    float powerNominalForTesting() const { return m_pwrNominal; }
    float powerMaxForTesting() const { return m_pwrMax; }
    bool    commandsEnabledForTesting() const;
    bool    clearFaultEnabledForTesting() const;

signals:
    void standbyClicked();
    void operateClicked();
    void offClicked();
    void clearFaultClicked();

private:
    void applyPill();
    void updateCommandsEnabled();
    void clearTelemetry();

    HGauge* m_pwrGauge{nullptr};
    HGauge* m_reflGauge{nullptr};
    HGauge* m_swrGauge{nullptr};

    QLabel* m_statusPill{nullptr};
    QLabel* m_sourceLabel{nullptr};
    QLabel* m_modelLabel{nullptr};

    QLabel* m_tempLabel{nullptr};
    QLabel* m_hvLabel{nullptr};
    QLabel* m_idLabel{nullptr};
    QLabel* m_bandLabel{nullptr};
    QLabel* m_uptimeLabel{nullptr};
    QLabel* m_faultLabel{nullptr};

    QPushButton* m_standbyBtn{nullptr};
    QPushButton* m_operateBtn{nullptr};
    QPushButton* m_offBtn{nullptr};
    QPushButton* m_clearBtn{nullptr};

    bool       m_connected{false};
    Acom::Mode m_mode{Acom::Mode::Unknown};
    float      m_fwdWatts{0.0f};
    float      m_pwrNominal{0.0f};
    float      m_pwrMax{0.0f};
    float      m_reflNominal{0.0f};
    float      m_reflMax{0.0f};
};

}  // namespace Longpath
