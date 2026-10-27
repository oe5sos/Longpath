// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-original. Siehe TransverterSetup.h fuer den Anlass.
//
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.

#include "core/TransverterSetup.h"

#include "core/AppSettings.h"

namespace Longpath {

namespace {

/// Hz als MHz mit drei Stellen, ohne Gebietsschema: die Zeichenkette
/// geht in die englische Oberflaeche und darf nicht je nach Rechner
/// zwischen Punkt und Komma wechseln. QString::number rechnet immer
/// in der C-Variante.
QString mhz(qint64 hz)
{
    return QString::number(static_cast<double>(hz) / 1e6, 'f', 3);
}

} // namespace

QString TransverterSetup::describe() const
{
    if (!configured()) { return QString(); }

    const QString sign = offsetHz >= 0 ? QStringLiteral("+") : QStringLiteral("-");
    const qint64  mag  = offsetHz >= 0 ? offsetHz : -offsetHz;

    return QStringLiteral("%1-%2 MHz → %3-%4 MHz (%5%6 MHz)")
        .arg(mhz(ifLowHz),
             mhz(ifHighHz),
             mhz(ifLowHz + offsetHz),
             mhz(ifHighHz + offsetHz),
             sign,
             mhz(mag));
}

QVector<TransverterSetup::Preset> TransverterSetup::presets()
{
    // Die ZF-Fenster sind 2 MHz breit, weil die Transverter so gebaut
    // sind: der MV-2 mischt 144-148 auf 28-32, die 70-cm- und
    // 23-cm-Geraete ebenso in 2- bis 4-MHz-Scheiben. Wer eine andere
    // ZF hat, stellt die drei Zahlen von Hand ein.
    return {
        {"2 m (144 MHz) from 28 MHz IF",    28000000, 30000000,  116000000},
        {"2 m (144 MHz) from 14 MHz IF",    14000000, 16000000,  130000000},
        {"70 cm (432 MHz) from 28 MHz IF",  28000000, 30000000,  404000000},
        {"23 cm (1296 MHz) from 28 MHz IF", 28000000, 30000000, 1268000000},
        {"23 cm (1296 MHz) from 144 MHz IF", 144000000, 146000000, 1152000000},
    };
}

TransverterSetup TransverterSetup::load()
{
    auto& s = AppSettings::instance();

    TransverterSetup t;
    // Wahrheitswerte stehen in dieser Ablage als "True"/"False" --
    // siehe CLAUDE.md, Abschnitt Settings Persistence. toBool() auf
    // einen QVariant mit "True" liefert zwar auch true, aber der
    // Rueckweg (setValue(bool)) schriebe "true" und waere dann die
    // eine Stelle, die aus der Reihe tanzt.
    t.enabled  = s.value(QStringLiteral("Transverter/Enabled"),
                         QStringLiteral("False")).toString()
                 == QStringLiteral("True");
    t.ifLowHz  = s.value(QStringLiteral("Transverter/IfLowHz"),
                         QVariant::fromValue<qint64>(t.ifLowHz)).toLongLong();
    t.ifHighHz = s.value(QStringLiteral("Transverter/IfHighHz"),
                         QVariant::fromValue<qint64>(t.ifHighHz)).toLongLong();
    t.offsetHz = s.value(QStringLiteral("Transverter/OffsetHz"),
                         QVariant::fromValue<qint64>(t.offsetHz)).toLongLong();
    return t;
}

void TransverterSetup::save() const
{
    auto& s = AppSettings::instance();
    s.setValue(QStringLiteral("Transverter/Enabled"),
               enabled ? QStringLiteral("True") : QStringLiteral("False"));
    s.setValue(QStringLiteral("Transverter/IfLowHz"),  QVariant::fromValue(ifLowHz));
    s.setValue(QStringLiteral("Transverter/IfHighHz"), QVariant::fromValue(ifHighHz));
    s.setValue(QStringLiteral("Transverter/OffsetHz"), QVariant::fromValue(offsetHz));
}

} // namespace Longpath
