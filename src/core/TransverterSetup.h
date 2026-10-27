#pragma once

// =================================================================
// src/core/TransverterSetup.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Was auf der Antenne liegt, ist nicht, was das Geraet zeigt ──────
//
// Anlass, 2026-10-27. Der Betreiber baut eine 2-m-Conteststation:
// ein klassisches Funkgeraet arbeitet als ZF hinter einem Transverter
// (HA1YA MV-2, 144-148 MHz auf 28-30 MHz), und Longpath haengt mit
// einem SDR am ZF-Ausgang und zeigt das Band.
//
// Longpath sieht damit 28,3 MHz. Auf der Antenne liegt 144,3 MHz.
// Ohne Umrechnung traegt jedes Logbuch die falsche Frequenz, jeder
// Spot zeigt ins falsche Band, und der Betreiber muss im Kopf
// 116 MHz addieren -- waehrend er ein QSO fuehrt.
//
// ── Warum ein Fenster und nicht ein Bandpaar ────────────────────────
//
// Das Contestprogramm loest dasselbe ueber ein Paar von Bandnamen
// ("144" -> "1296"), weil es UKW-Baender kennt. Longpath kennt sie
// nicht: seine Band-Aufzaehlung endet bei 6 m, und 144 MHz hat dort
// keinen Eintrag. Ein Bandpaar waere hier also eine Erfindung, die
// nirgends anschliesst.
//
// Darum ein ZF-FENSTER plus Versatz: von welcher Frequenz bis welcher
// Frequenz gilt die Rechnung, und wieviel kommt dazu. Das ist genau
// das, was der Transverter tut (er mischt einen Bereich, nicht ein
// Band), es braucht keine neue Bandtabelle, und es traegt auch die
// krummen Faelle -- eine 2-m-Station mit 14-MHz-ZF zum Beispiel.
//
// ── Keine Zustandsklasse ────────────────────────────────────────────
//
// Eine Struktur mit reinen Funktionen, kein QObject. So laesst sich
// die Rechnung ohne Geraet, ohne Oberflaeche und ohne Einstellungen
// pruefen: Werte hinein, Werte heraus.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-27 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude Code.
//                Muster: Contestprogramm/src/core/Transverter.h
//                (eigener Baum, eigene Fassung -- nichts kopiert).
// =================================================================

#include <QString>
#include <QVector>

#include <QtGlobal>

namespace Longpath {

/// Ein Transverter zwischen dem ZF-Geraet und der Antenne.
///
/// RF = ZF + offsetHz, solange die ZF im Fenster [ifLowHz, ifHighHz]
/// liegt. Ausserhalb bleibt jede Frequenz unberuehrt -- wer mit
/// eingeschaltetem Transverter auf 14 MHz abstimmt, soll 14 MHz sehen
/// und nicht 130 MHz.
struct TransverterSetup {
    bool   enabled  {false};
    qint64 ifLowHz  {28000000};
    qint64 ifHighHz {30000000};
    qint64 offsetHz {116000000};   ///< RF = ZF + offsetHz

    /// Ist das Fenster brauchbar und der Versatz von Null verschieden?
    bool configured() const
    {
        return ifHighHz > ifLowHz && ifLowHz > 0 && offsetHz != 0;
    }

    /// Eingeschaltet UND brauchbar eingestellt.
    bool active() const { return enabled && configured(); }

    /// Liegt diese ZF-Frequenz im Fenster?
    bool coversIf(qint64 ifHz) const
    {
        return ifHz >= ifLowHz && ifHz <= ifHighHz;
    }

    /// Liegt diese Antennenfrequenz im umgerechneten Fenster?
    bool coversRf(qint64 rfHz) const
    {
        return rfHz >= ifLowHz + offsetHz && rfHz <= ifHighHz + offsetHz;
    }

    /// Was das Geraet zeigt -> was auf der Antenne liegt.
    /// Unveraendert, wenn nicht aktiv oder ausserhalb des Fensters.
    qint64 rfFrequencyHz(qint64 ifHz) const
    {
        if (!active() || !coversIf(ifHz)) { return ifHz; }
        return ifHz + offsetHz;
    }

    /// Was auf der Antenne liegt -> was dem Geraet zu sagen ist.
    /// Unveraendert, wenn nicht aktiv oder ausserhalb des Fensters.
    qint64 ifFrequencyHz(qint64 rfHz) const
    {
        if (!active() || !coversRf(rfHz)) { return rfHz; }
        return rfHz - offsetHz;
    }

    /// "28.000-30.000 -> 144.000-146.000 MHz (+116 MHz)", oder leer,
    /// wenn nichts Brauchbares eingestellt ist. Fuer die Statuszeile.
    QString describe() const;

    // ── Vorgaben ────────────────────────────────────────────────────
    //
    // Die ueblichen Kombinationen, damit niemand 116000000 von Hand
    // tippt. Frei einstellbar bleibt alles: die Vorgabe fuellt nur die
    // drei Felder.
    struct Preset {
        const char* name;      ///< "2 m / 144 MHz (28 MHz IF)"
        qint64      ifLowHz;
        qint64      ifHighHz;
        qint64      offsetHz;
    };
    static QVector<Preset> presets();

    // ── Einstellungen ───────────────────────────────────────────────
    //
    // Schluessel: Transverter/Enabled, /IfLowHz, /IfHighHz, /OffsetHz.
    static TransverterSetup load();
    void save() const;
};

} // namespace Longpath
