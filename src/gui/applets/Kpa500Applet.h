// no-port-check: Longpath-eigene Datei, kein Port.
// =================================================================
// src/gui/applets/Kpa500Applet.h  (Longpath-eigen)
// =================================================================
//
// Elecraft KPA500: das Bedienfeld.
//
// Kein Port -- fuer den KPA500 gibt es bei AetherSDR keine Vorlage.
// Aufbau und Farben folgen dem Haus (Rf2ksApplet, SpeApplet); was
// angezeigt wird, folgt dem, was Elecrafts Beschreibung wirklich
// hergibt (siehe core/Kpa500Protocol.h).
//
// DREI DINGE SIND HIER ANDERS ALS BEIM SPE, und alle drei stehen so im
// Dokument:
//
//   1. Die Temperatur ist ein STREIFEN, nicht eine Zahl. Beim SPE gibt
//      das Protokoll keinen Bereich her, beim KPA500 schon
//      (0..150 °C, §^TM) -- und die Einheit ist bekannt, nicht „die
//      des eigenen Displays".
//   2. Der Zustand hat DREI Stufen statt zwei. Der KPA500 kann sagen
//      „ich bin aus, aber am Draht" (Boot-Zustand); genau dann ist der
//      Einschaltknopf sinnvoll und alles andere nicht.
//   3. Ein Fehler bleibt eine ZAHL. Das Dokument gibt die Zuordnung
//      Nummer -> Name nicht her, also steht „Fehler 03" da und kein
//      erfundener Name. Dafuer gibt es einen Knopf, der ihn loescht
//      (^FLC;) -- den hat der SPE nicht.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 -- Neu, Zeus-Punkt 7. Martin Fischer, AI-assisted via
//                 Anthropic Claude (Claude Code).
// =================================================================

#pragma once
#include "AppletWidget.h"

#include <QPushButton>
#include <QString>

#include "core/Kpa500Connection.h"

class QLabel;

namespace Longpath {

class HGauge;
class RadioModel;

class Kpa500Applet : public AppletWidget {
    Q_OBJECT

public:
    explicit Kpa500Applet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("Kpa500"); }
    QString appletTitle() const override { return QStringLiteral("Elecraft KPA500"); }
    void    syncFromModel() override {}

    void setSource(const QString& text);
    void setIdentity(const QString& firmware, const QString& serial);
    void setConnected(bool connected);
    void setLiveness(Kpa500Connection::Liveness state);
    void setPowerSwr(const Kpa500::PowerSwr& ps);
    void setVoltsAmps(const Kpa500::VoltsAmps& va);
    void setTemperature(int degC);
    void setOperate(bool operate);
    void setBand(const QString& band);
    void setFault(int code);

    // Pruefstand-Nahtstellen.
    QString statusPillTextForTesting() const;
    QString infoTextForTesting() const;
    QString faultTextForTesting() const;
    bool    commandsEnabledForTesting() const;
    bool    powerOnEnabledForTesting() const;
    bool    faultClearVisibleForTesting() const;

signals:
    void powerOnClicked();      // 'P' im Boot-Zustand
    void operateClicked();      // ^OSn;
    void offClicked();          // ^ON0;
    void clearFaultClicked();   // ^FLC;

private:
    void updateCommandsEnabled();
    void applyPill();
    void clearTelemetry();

    HGauge* m_pwrGauge{nullptr};
    HGauge* m_swrGauge{nullptr};
    HGauge* m_tempGauge{nullptr};

    QLabel* m_statusPill{nullptr};
    QLabel* m_sourceLabel{nullptr};
    QLabel* m_identityLabel{nullptr};
    QLabel* m_voltLabel{nullptr};
    QLabel* m_currLabel{nullptr};
    QLabel* m_bandLabel{nullptr};
    QLabel* m_faultLabel{nullptr};

    QPushButton* m_onBtn{nullptr};
    QPushButton* m_operateBtn{nullptr};
    QPushButton* m_offBtn{nullptr};
    QPushButton* m_clearFaultBtn{nullptr};

    bool m_connected{false};
    Kpa500Connection::Liveness m_liveness{Kpa500Connection::Liveness::Unknown};
    bool m_operate{false};
    int  m_faultCode{0};
    int  m_watts{0};
};

}  // namespace Longpath
