// =================================================================
// src/core/FlexApdState.cpp  (Longpath)
// =================================================================
//
// Siehe FlexApdState.h fuer die Herkunft der Schluessel und die zwei
// Fallen (zwei Zeilen heiszen „apd"; die Maschine schickt
// Teilmeldungen).
//
// Modification history (Longpath):
//   2026-11-01 — Neu, von Martin Fischer, KI-gestuetzt ueber
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexApdState.h"

namespace Longpath {

namespace {

/// Die Maschine schreibt Wahrheitswerte als 0/1. Fehlt der Schluessel,
/// bleibt der alte Wert stehen — darum gibt es hier keinen Vorgabewert:
/// der Aufrufer fragt vorher, ob der Schluessel ueberhaupt da ist.
bool alsJaNein(const QString& wert)
{
    return wert.trimmed() == QLatin1String("1");
}

/// `valid_samplers=XVTA,RX_A` — Kommaliste, Leereintraege weg.
QStringList alsListe(const QString& wert)
{
    QStringList liste;
    const QStringList teile = wert.split(QLatin1Char(','), Qt::SkipEmptyParts);
    liste.reserve(teile.size());
    for (const QString& teil : teile) {
        const QString sauber = teil.trimmed();
        if (!sauber.isEmpty()) { liste.append(sauber); }
    }
    return liste;
}

constexpr auto kKeinMesspunkt = "INVALID";

}  // namespace

bool FlexApdState::gehoertZuApd(const QString& object)
{
    return object == QLatin1String("apd")
        || object.startsWith(QLatin1String("apd "));
}

void FlexApdState::clear()
{
    *this = FlexApdState{};
}

bool FlexApdState::apply(const QString& object, const QMap<QString, QString>& kvs)
{
    if (!gehoertZuApd(object)) { return false; }

    bool geaendert = false;
    if (!m_bekannt) {
        m_bekannt = true;
        geaendert = true;
    }

    // ── Der Messpunkt je Sendeantenne ────────────────────────────────
    //
    // `apd sampler tx_ant=ANT1 valid_samplers=XVTA,RX_A selected_sampler=INVALID`
    // Ohne `tx_ant` ist die Zeile nicht zuzuordnen; sie dann auf gut
    // Glueck dem zuletzt gesehenen Eintrag zuzuschlagen, waere geraten.
    if (object == QLatin1String("apd sampler")) {
        const QString ant = kvs.value(QStringLiteral("tx_ant")).trimmed();
        if (ant.isEmpty()) { return geaendert; }

        FlexApdSampler eintrag = m_messpunkte.value(ant);
        const FlexApdSampler vorher = eintrag;

        // Teilmeldung: nur uebernehmen, was wirklich dasteht. Eine
        // fehlende `valid_samplers` heisst „unveraendert", nicht
        // „keine" — siehe Falle 2 im Kopf der .h.
        if (kvs.contains(QStringLiteral("valid_samplers"))) {
            eintrag.moegliche = alsListe(kvs.value(QStringLiteral("valid_samplers")));
        }
        if (kvs.contains(QStringLiteral("selected_sampler"))) {
            eintrag.gewaehlt = kvs.value(QStringLiteral("selected_sampler")).trimmed();
        }

        if (eintrag.moegliche != vorher.moegliche || eintrag.gewaehlt != vorher.gewaehlt) {
            m_messpunkte.insert(ant, eintrag);
            geaendert = true;
        }
        return geaendert;
    }

    // ── Schalter und Scheibenzeile ───────────────────────────────────
    //
    // Beide heiszen „apd" (Falle 1). Also je SCHLUESSEL uebernehmen:
    // was nicht dasteht, bleibt stehen.
    auto nimm = [&](const char* schluessel, auto& ziel, auto wandeln) {
        const QString name = QString::fromLatin1(schluessel);
        if (!kvs.contains(name)) { return; }
        const auto neu = wandeln(kvs.value(name));
        if (neu != ziel) {
            ziel = neu;
            geaendert = true;
        }
    };

    nimm("enable",           m_eingeschaltet,   alsJaNein);
    nimm("configurable",     m_einstellbar,     alsJaNein);
    nimm("equalizer_active", m_kurveTraegt,     alsJaNein);
    nimm("ant",              m_antenne,         [](const QString& w) { return w.trimmed(); });

    if (kvs.contains(QStringLiteral("freq"))) {
        bool ok = false;
        const double mhz = kvs.value(QStringLiteral("freq")).toDouble(&ok);
        // Unlesbare Zahlen nicht als 0 uebernehmen: eine 0 MHz sieht
        // aus wie eine Messung und ist keine.
        if (ok && !qFuzzyCompare(mhz, m_frequenzMhz)) {
            m_frequenzMhz = mhz;
            geaendert = true;
        }
    }
    if (kvs.contains(QStringLiteral("rfpower"))) {
        bool ok = false;
        const int prozent = kvs.value(QStringLiteral("rfpower")).toInt(&ok);
        if (ok && prozent != m_leistungProzent) {
            m_leistungProzent = prozent;
            geaendert = true;
        }
    }

    return geaendert;
}

QStringList FlexApdState::sendeantennen() const
{
    return m_messpunkte.keys();
}

QStringList FlexApdState::moeglicheMesspunkte(const QString& sendeantenne) const
{
    return m_messpunkte.value(sendeantenne).moegliche;
}

QString FlexApdState::gewaehlterMesspunkt(const QString& sendeantenne) const
{
    return m_messpunkte.value(sendeantenne).gewaehlt;
}

bool FlexApdState::hatMesspunkt(const QString& sendeantenne) const
{
    const QString gewaehlt = gewaehlterMesspunkt(sendeantenne);
    return !gewaehlt.isEmpty()
        && gewaehlt != QLatin1String(kKeinMesspunkt);
}

}  // namespace Longpath
