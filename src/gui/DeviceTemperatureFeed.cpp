// no-port-check: Longpath-original. No upstream port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/DeviceTemperatureFeed.h"

#include "core/AcomConnection.h"
#include "core/AcomProtocol.h"
#include "core/Kpa500Connection.h"
#include "core/PgxlConnection.h"
#include "core/RadioStatus.h"
#include "core/Rf2ksConnection.h"
#include "core/SpeConnection.h"
#include "core/SpeProtocol.h"
#include "gui/widgets/DeviceTemperatureStrip.h"
#include "models/RadioModel.h"

#include <QMap>

namespace Longpath {

using Strip = DeviceTemperatureStrip;

DeviceTemperatureFeed::DeviceTemperatureFeed(RadioModel* model,
                                             DeviceTemperatureStrip* strip,
                                             QObject* parent)
    // Ohne ausdruecklichen Besitzer gehoert die Verdrahtung der Leiste
    // und geht mit ihr.
    : QObject(parent ? parent : static_cast<QObject*>(strip))
    , m_model(model)
    , m_strip(strip)
{
    if (!m_model || !m_strip) { return; }

    // Die Reihenfolge hier ist die Reihenfolge im Kopf: erst das
    // Funkgeraet, dann die Endstufen.
    m_strip->addSlot(QLatin1String(kRadio),
                     radioLabel(false, HPSDRModel::HERMES),
                     radioDescription(false, HPSDRModel::HERMES));
    m_strip->addSlot(QLatin1String(kPgxl), QStringLiteral("PGXL"),
                     tr("Power Genius XL temperature"));
    m_strip->addSlot(QLatin1String(kSpe), QStringLiteral("SPE"),
                     tr("SPE Expert heatsink temperature, in the unit set "
                        "on the amplifier's own display"));
    m_strip->addSlot(QLatin1String(kKpa500), QStringLiteral("KPA"),
                     tr("KPA500 temperature"));
    m_strip->addSlot(QLatin1String(kAcom), QStringLiteral("ACOM"),
                     tr("ACOM PA temperature"));
    m_strip->addSlot(QLatin1String(kRf2ks), QStringLiteral("RF2K"),
                     tr("RF2K-S temperature"));

    // ── Funkgeraet ───────────────────────────────────────────────────
    m_radioConnected = m_model->isConnected();
    connect(m_model, &RadioModel::connectionStateChanged,
            this, &DeviceTemperatureFeed::onRadioState);
    connect(&m_model->radioStatus(), &RadioStatus::paTemperatureChanged,
            this, &DeviceTemperatureFeed::onRadioTemperature);

    // ── PGXL ─────────────────────────────────────────────────────────
    if (PgxlConnection* pgxl = m_model->pgxlConnection()) {
        connect(pgxl, &PgxlConnection::statusUpdated, this,
                [this](const QMap<QString, QString>& kvs) {
            const auto it = kvs.constFind(QStringLiteral("temp"));
            if (it == kvs.cend()) { return; }
            bool ok = false;
            const double c = it.value().toDouble(&ok);
            // Nicht lesbar oder null: kein Wert, also kein Eintrag.
            if (ok && c > 0.0) {
                m_strip->setReading(QLatin1String(kPgxl), c);
            }
        });
        connect(pgxl, &PgxlConnection::disconnected, this, [this]() {
            m_strip->clearReading(QLatin1String(kPgxl));
        });
    }

    // ── SPE Expert ───────────────────────────────────────────────────
    if (SpeConnection* spe = m_model->speConnection()) {
        connect(spe, &SpeConnection::statusUpdated, this,
                [this](const Spe::Status& s) {
            if (s.tempUpper <= 0) { return; }
            // Einheit: das Protokoll sagt nicht, ob Celsius oder
            // Fahrenheit (SpeApplet.h, „spec §5"). Also die Zahl, wie
            // sie kommt, und keine Umrechnung.
            m_strip->setReading(QLatin1String(kSpe),
                                static_cast<double>(s.tempUpper),
                                Strip::Unit::AsReported);
            // Das Geraet meldet Uebertemperatur SELBST. Nur diese drei
            // Kennungen handeln von der Temperatur; alle anderen
            // Warnungen (Stehwelle, Antenne, …) gehoeren nicht an eine
            // Temperaturzahl.
            const char16_t warnung = s.warning.unicode();
            if (s.alarm == QLatin1Char('H')) {
                m_strip->setAlert(QLatin1String(kSpe), Strip::Alert::Alarm,
                                  Spe::alarmText(s.alarm));
            } else if (warnung == u'O' || warnung == u'T') {
                m_strip->setAlert(QLatin1String(kSpe), Strip::Alert::Warning,
                                  Spe::warningText(s.warning));
            } else {
                m_strip->setAlert(QLatin1String(kSpe), Strip::Alert::None);
            }
        });
        // Ueber ser2net ueberlebt die TCP-Strecke das Ausschalten der
        // Endstufe; „antwortet nicht mehr" ist dort das Zeichen, dass
        // sie weg ist (SpeConnection.h).
        connect(spe, &SpeConnection::respondingChanged, this,
                [this](bool responding) {
            if (!responding) { m_strip->clearReading(QLatin1String(kSpe)); }
        });
        connect(spe, &SpeConnection::disconnected, this, [this]() {
            m_strip->clearReading(QLatin1String(kSpe));
        });
    }

    // ── KPA500 ───────────────────────────────────────────────────────
    if (Kpa500Connection* kpa = m_model->kpa500Connection()) {
        m_kpaRunning =
            kpa->liveness() == Kpa500Connection::Liveness::Running;
        connect(kpa, &Kpa500Connection::telemetryUpdated, this,
                [this, kpa]() {
            // Im Bootmodus („aus, aber am Draht") gibt es keine
            // Temperatur, nur die letzte von vorher.
            if (!m_kpaRunning) { return; }
            const int c = kpa->lastTemperature();
            if (c > 0) {
                m_strip->setReading(QLatin1String(kKpa500),
                                    static_cast<double>(c));
            }
        });
        connect(kpa, &Kpa500Connection::livenessChanged, this,
                [this](Kpa500Connection::Liveness l) {
            m_kpaRunning = (l == Kpa500Connection::Liveness::Running);
            if (!m_kpaRunning) {
                m_strip->clearReading(QLatin1String(kKpa500));
            }
        });
        connect(kpa, &Kpa500Connection::disconnected, this, [this]() {
            m_strip->clearReading(QLatin1String(kKpa500));
        });
    }

    // ── ACOM ─────────────────────────────────────────────────────────
    if (AcomConnection* acom = m_model->acomConnection()) {
        connect(acom, &AcomConnection::telemetryUpdated, this,
                [this, acom](const Acom::Telemetry& t) {
            if (t.paTempRaw == 0) { return; }
            // Roh minus Modellversatz — dieselbe Rechnung wie im Applet
            // (MainWindow, AcomApplet::setTemp).
            const auto& spec = Acom::modelSpec(acom->currentModel());
            m_strip->setReading(QLatin1String(kAcom),
                                static_cast<double>(t.paTempRaw)
                                    - static_cast<double>(spec.temperatureOffset));
        });
        connect(acom, &AcomConnection::disconnected, this, [this]() {
            m_strip->clearReading(QLatin1String(kAcom));
        });
    }

    // ── RF2K-S ───────────────────────────────────────────────────────
    if (Rf2ksConnection* rf2k = m_model->rfKitConnection()) {
        connect(rf2k, &Rf2ksConnection::powerUpdated, this,
                [this](const RfKitPowerSnapshot& snap) {
            if (snap.temperatureC > 0.0f) {
                m_strip->setReading(QLatin1String(kRf2ks),
                                    static_cast<double>(snap.temperatureC));
            }
        });
        connect(rf2k, &Rf2ksConnection::disconnected, this, [this]() {
            m_strip->clearReading(QLatin1String(kRf2ks));
        });
    }
}

QString DeviceTemperatureFeed::radioLabel(bool flex, HPSDRModel model)
{
    if (flex) { return QStringLiteral("FLEX"); }
    if (model == HPSDRModel::HERMESLITE) { return QStringLiteral("HL2"); }
    return QStringLiteral("RADIO");
}

QString DeviceTemperatureFeed::radioDescription(bool flex, HPSDRModel model)
{
    if (flex) { return tr("FlexRadio PA temperature"); }
    // Die HL2 meldet die Temperatur ihres FPGA, nicht die der Endstufe.
    // Das gehoert gesagt, sonst liest man die Zahl falsch.
    if (model == HPSDRModel::HERMESLITE) {
        return tr("Hermes Lite 2 FPGA temperature (not the PA)");
    }
    return tr("Temperature reported by the radio");
}

void DeviceTemperatureFeed::onRadioState(ConnectionState s)
{
    m_radioConnected = (s == ConnectionState::Connected);
    // RadioStatus behaelt seinen letzten Wert ueber das Trennen hinweg.
    // Der Kopf nicht: ohne Geraet kein Eintrag.
    if (!m_radioConnected) {
        m_strip->clearReading(QLatin1String(kRadio));
    }
}

void DeviceTemperatureFeed::onRadioTemperature(double celsius)
{
    // 0,0 ist der Ruhewert von RadioStatus, keine Messung.
    if (!m_radioConnected || celsius <= 0.0) { return; }
    const bool flex = m_model->istFlexVerbunden();
    const HPSDRModel hw = m_model->hardwareProfile().model;
    m_strip->setSlotLabel(QLatin1String(kRadio), radioLabel(flex, hw),
                          radioDescription(flex, hw));
    m_strip->setReading(QLatin1String(kRadio), celsius);
}

} // namespace Longpath
