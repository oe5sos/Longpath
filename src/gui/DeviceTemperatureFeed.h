// no-port-check: Longpath-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "core/ConnectionState.h"
#include "core/HpsdrModel.h"

#include <QObject>
#include <QString>

namespace Longpath {

class DeviceTemperatureStrip;
class RadioModel;

/// Reicht die Temperaturen, die die Geraete ohnehin melden, an die
/// Leiste im Kopf weiter.
///
/// Diese Klasse fragt KEIN Geraet ab und startet keinen Zeitgeber. Sie
/// haengt sich an Signale, die es schon gibt — dieselben, aus denen die
/// Applets und die Fussleiste ihre Zahlen nehmen.
///
/// Wer was liefert (Stand 2026-11-18, am Quelltext und am Protokoll
/// nachgelesen, nicht angenommen):
///
///   Funkgeraet  RadioStatus::paTemperatureChanged
///     FlexRadio      PATEMP aus dem Messwertstrom (Endstufe)
///     Hermes Lite 2  FPGA-Temperatur — NICHT die Endstufe
///     SunSDR2 QRP    erster von zwei Fuehlern
///     ANAN / uebrige OpenHPSDR: nichts. `scalePaTemperatureCelsius`
///                    liefert fuer sie 0, es gibt keinen Messwert.
///   PGXL        PgxlConnection::statusUpdated, Schluessel `temp`
///   SPE Expert  SpeConnection::statusUpdated, oberer Kuehlkoerper;
///               Einheit unbekannt (wie am Geraet eingestellt). Das
///               EINZIGE Geraet, das selbst Uebertemperatur meldet
///               (Warnung O/T, Alarm H) — nur dort gibt es eine Stufe.
///   KPA500      Kpa500Connection::telemetryUpdated, ^TM
///   ACOM        AcomConnection::telemetryUpdated, roh minus Modellversatz
///   RF2K-S      Rf2ksConnection::powerUpdated
///
/// Fuer KPA500 und RF2K-S stehen im Baum Schwellen (55/80 Grad) — die
/// sind dort ausdruecklich als geraten vermerkt und werden hier NICHT
/// verwendet.
class DeviceTemperatureFeed : public QObject {
    Q_OBJECT

public:
    // Die Plaetze, in der Reihenfolge des Kopfes.
    static constexpr auto kRadio  = "radio";
    static constexpr auto kPgxl   = "pgxl";
    static constexpr auto kSpe    = "spe";
    static constexpr auto kKpa500 = "kpa500";
    static constexpr auto kAcom   = "acom";
    static constexpr auto kRf2ks  = "rf2ks";

    /// Ohne `parent` gehoert die Verdrahtung der Leiste und geht mit
    /// ihr — dann kann kein spaetes Signal eines Geraets in eine Leiste
    /// greifen, die es nicht mehr gibt.
    DeviceTemperatureFeed(RadioModel* model, DeviceTemperatureStrip* strip,
                          QObject* parent = nullptr);

    /// Das Kuerzel fuer das Funkgeraet. Eigene, reine Funktion, damit
    /// ein Pruefstand sie ohne Geraet fahren kann.
    static QString radioLabel(bool flex, HPSDRModel model);
    static QString radioDescription(bool flex, HPSDRModel model);

private:
    void onRadioState(ConnectionState s);
    void onRadioTemperature(double celsius);

    RadioModel*             m_model{nullptr};
    DeviceTemperatureStrip* m_strip{nullptr};
    bool                    m_radioConnected{false};
    bool                    m_kpaRunning{false};
};

} // namespace Longpath
