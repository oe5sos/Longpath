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

    // ── Die eine Fassung, die die Anzeige liest (2026-10-28) ────────
    //
    // Den Versatz braucht nicht ein Ort, sondern vier: die Zahl in der
    // VFO-Anzeige, die Frequenzachse im Panadapter, die Bandplan-
    // Abschnitte darin und der Logbucheintrag. Alle vier liegen weit
    // auseinander -- VfoDisplayItem entsteht tief im Instrumenten-
    // system (ItemGroup), wo es keinen Weg gibt, ihm beim Anlegen
    // etwas mitzugeben, ohne die Eigenschaftsmasken und die
    // Speicherung mit anzufassen.
    //
    // Darum eine prozessweite Fassung, wie AppSettings::instance() es
    // vormacht: EINE Stationseinstellung, von vielen gelesen, von
    // genau einer Stelle geschrieben (CompanionRigController, nachdem
    // er geladen oder der Bediener gespeichert hat).
    //
    // Nur vom Hauptfaden aus. Alle vier Leser malen oder schreiben
    // Logbuch, und beides laeuft dort.
    //
    // Der Pruefstand setzt sie direkt -- das ist der Grund, warum
    // setCurrent() oeffentlich ist und nicht nur der Controller sie
    // kennt: so laesst sich "aus wenn aus" ohne Einstellungsdatei
    // belegen.
    static const TransverterSetup& current();
    static void setCurrent(const TransverterSetup& t);

    /// Kurzweg fuer die Anzeige: ZF -> RF nach der aktuellen Fassung.
    /// Ist kein Transverter aktiv, kommt die Zahl unveraendert zurueck.
    static qint64 displayHz(qint64 ifHz) { return current().rfFrequencyHz(ifHz); }

    /// Kurzweg fuer die Gegenrichtung: RF -> ZF. Fuer alles, was eine
    /// Antennenfrequenz auf die Achse des Panadapters legen muss --
    /// die Bandplan-Abschnitte zum Beispiel.
    static qint64 panHz(qint64 rfHz) { return current().ifFrequencyHz(rfHz); }

    // ── Das Logbuch (2026-10-28) ────────────────────────────────────
    //
    // Longpaths Band-Aufzaehlung endet bei 6 m (darum das ZF-Fenster
    // und kein Bandpaar, siehe oben). bandFromFrequency(144300000) hat
    // also keinen Eintrag -- und ein ADIF-Satz ohne BAND oder mit
    // BAND=10M bei einer 144-MHz-Frequenz ist falsch, nicht nur
    // unschoen: der Satz wandert in ein Diplom-Programm.
    //
    // Hier steht deshalb eine kleine, geschlossene Tabelle der
    // ADIF-Bandnamen -- NICHT eine neue Bandverwaltung fuer Longpath.
    // Sie wird nur betreten, wenn ein Transverter aktiv ist, sie
    // entscheidet nichts ueber das Senden, und sie kennt genau die
    // Baender, die ein Transverter ausgibt.
    //
    /// ADIF-Bandname fuer eine ANTENNENfrequenz ("2m", "70cm", …),
    /// oder leer, wenn keiner passt.
    static QString adifBand(qint64 rfHz);

    /// Frequenz und Band fuer einen Logbucheintrag.
    ///
    /// Gibt false zurueck, wenn kein Transverter im Spiel ist. Dann
    /// bleibt *mhz und *band unberuehrt und der Aufrufer behaelt seine
    /// eigene Rechnung -- bitgleich wie vorher. Das ist der Zweck
    /// dieser Form: an den Aufrufstellen soll NICHTS passieren, solange
    /// niemand einen Transverter eingestellt hat.
    static bool logbuchFrequenz(qint64 ifHz, double* mhz, QString* band);
};

} // namespace Longpath
