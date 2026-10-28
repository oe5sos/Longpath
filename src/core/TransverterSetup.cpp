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

// ── Die prozessweite Fassung ────────────────────────────────────────
//
// Absichtlich NICHT aus load() vorbelegt. Eine Vorbelegung, die beim
// ersten Zugriff die Einstellungsdatei liest, haette zwei Folgen, die
// beide schlecht sind: jeder Pruefstand, der die Anzeige prueft, haengt
// plötzlich an einer Datei, und in einem Lauf ohne Einstellungen
// entscheidet der Zufall der Aufrufreihenfolge, ob der Versatz greift.
//
// Die Vorgabe ist also "kein Transverter" -- enabled{false} aus der
// Struktur -- und wer ihn will, setzt ihn. Das ist genau einmal im
// ganzen Programm der Fall: CompanionRigController, nach dem Laden und
// nach jedem Speichern des Bedieners.
static TransverterSetup& fassung()
{
    static TransverterSetup g;
    return g;
}

const TransverterSetup& TransverterSetup::current()
{
    return fassung();
}

void TransverterSetup::setCurrent(const TransverterSetup& t)
{
    fassung() = t;
}

// ── ADIF-Bandnamen ─────────────────────────────────────────────────
//
// Die Grenzen sind die der ADIF-Spezifikation (Feld BAND), nicht die
// der oesterreichischen Bandplaene: ein Logbuchsatz muss sich nach dem
// Austauschformat richten, nicht nach dem Land. Darum sind die
// Spannen weiter als das, worauf hier gesendet werden darf -- was
// erlaubt ist, entscheidet BandPlanGuard, nicht diese Tabelle.
QString TransverterSetup::adifBand(qint64 rfHz)
{
    struct Spanne { qint64 von; qint64 bis; const char* name; };
    static constexpr Spanne kBaender[] = {
        {   50000000,    54000000, "6m"    },
        {   70000000,    71000000, "4m"    },
        {  144000000,   148000000, "2m"    },
        {  222000000,   225000000, "1.25m" },
        {  420000000,   450000000, "70cm"  },
        {  902000000,   928000000, "33cm"  },
        { 1240000000,  1300000000, "23cm"  },
        { 2300000000,  2450000000, "13cm"  },
        { 3300000000,  3500000000, "9cm"   },
        { 5650000000,  5925000000, "6cm"   },
        {10000000000, 10500000000, "3cm"   },
    };
    for (const auto& b : kBaender) {
        if (rfHz >= b.von && rfHz <= b.bis) {
            return QString::fromLatin1(b.name);
        }
    }
    return {};
}

bool TransverterSetup::logbuchFrequenz(qint64 ifHz, double* mhz, QString* band)
{
    const TransverterSetup& t = current();
    if (!t.active() || !t.coversIf(ifHz)) { return false; }

    const qint64 rf = t.rfFrequencyHz(ifHz);
    if (mhz) { *mhz = static_cast<double>(rf) / 1e6; }

    // Faellt kein Bandname aus der Tabelle, bleibt das Feld des
    // Aufrufers stehen. Ein falsches Band ist schlimmer als ein
    // ungenaues: 10M bei 144 MHz behauptet etwas, ein alter Wert
    // behauptet nur dasselbe wie vorher.
    if (band) {
        const QString b = adifBand(rf);
        if (!b.isEmpty()) { *band = b; }
    }
    return true;
}

} // namespace Longpath
