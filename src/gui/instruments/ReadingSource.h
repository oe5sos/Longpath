#pragma once

// no-port-check: nennt MeterManager.cs nur als Fundort des woertlichen
// readingName()-Ports, der in MeterItem.cpp lebt und dort registriert
// ist. Diese Datei uebernimmt nichts aus Thetis -- die Beschriftungen
// stammen aus BaseItemEditor.cpp (NereusSDR-original), die Teilung aus
// den Entwuerfen des Betreibers, und die Skalen aus dem NereusSDR-Baum
// mit Herkunftsangabe je Zahl. Die Erwaehnung steht da, damit niemand
// hier eine zweite Namensliste anlegt.

// =================================================================
// src/gui/instruments/ReadingSource.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Eine Liste, nicht eine zweite ────────────────────────────────────
//
// OE5SOS, 2026-08-17: „die Instrumente sollen sich aus derselben Liste
// bedienen wie die vorhandenen Gauges, nicht aus einer zweiten."
//
// Die Liste ist MeterBinding (gui/meters/MeterPoller.h). Sie stand beim
// Nachsehen VIERMAL im Baum:
//
//   1. als Konstanten            — MeterPoller.h:82-123
//   2. als beschrifteter Kombi   — BaseItemEditor.cpp (27 Einträge)
//   3. noch einmal als Kombi     — HistoryGraphItemEditor.cpp (15)
//   4. als Thetis-Namen          — readingName(), MeterItem.cpp:128
//
// Diese Datei ist NICHT die fünfte. Sie ist die Stelle, aus der 2 und 3
// abgeleitet werden; 1 bleibt der Schlüssel, und 4 bleibt unangetastet,
// weil es ein wörtlicher Port aus MeterManager.cs:2258-2318 ist —
// label() ruft dorthin, statt danebenzuschreiben.
//
// ── Was die Liste bisher NICHT trug: die Skala ───────────────────────
//
// MeterBinding kennt nur Kennung und Name. Bereich, Schwelle und
// Teilung standen an jeder Aufrufstelle einzeln — und stimmten nicht
// überein. SWR war 1,0-5,0 in ItemGroup.cpp:730 und 1,0-3,0 mit Rot ab
// 2,5 in TxApplet.h:175. Drei Skalen für eine Größe, wenn man den
// Entwurf mitzählt.
//
// Ein Zeigerinstrument braucht Bereich, Schwelle und Teilung. Also
// stehen sie hier, EINMAL, mit Herkunftsangabe je Zahl.
//
// ── Was hier bewusst FEHLT ───────────────────────────────────────────
//
// Wo keine belastbare Skala im Baum steht, trägt der Eintrag keine.
// hasScale() ist dann false, und das Instrument bietet die Größe nicht
// an. Eine erfundene Skala wäre schlimmer als eine fehlende: sie sähe
// aus wie eine Messung.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-17 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QList>
#include <QString>

#include <optional>

namespace NereusSDR {

/// Ein beschrifteter Teilstrich.
struct ReadingTick {
    double  value;
    QString label;
};

/// Beschreibung einer Messgröße: woher der Wert kommt (bindingId), wie
/// er heißt, und — wenn bekannt — auf welcher Skala er steht.
struct ReadingDescriptor {
    int     bindingId{-1};
    QString label;          ///< Für Auswahllisten, z. B. „TX: SWR".
    QString unit;           ///< Leer, wenn einheitenlos.

    // ── Skala. Nur gültig, wenn hasScale true ist. ───────────────────
    bool    hasScale{false};
    double  min{0.0};
    double  max{1.0};

    /// Ab hier ist es keine Messung mehr, sondern eine Warnung. Ohne
    /// Wert gibt es keinen roten Abschnitt — auf der Empfangsskala gibt
    /// es keine Schwelle, und ein Instrument soll dort auch keine
    /// zeichnen.
    std::optional<double> threshold;

    QList<ReadingTick> ticks;       ///< beschriftet
    QList<double>      minorTicks;  ///< fein, ohne Beschriftung
    int                decimals{1};

    /// Wert → Anteil 0..1. Nullzeiger heißt linear zwischen min und
    /// max. Nichtlinear ist die Empfangsskala: bis S9 gleichmäßig,
    /// darüber gestaucht.
    double (*fractionOf)(double) {nullptr};

    /// Wert → Anzeigetext. Nullzeiger heißt „mit `decimals` Stellen".
    QString (*format)(double) {nullptr};

    /// Anteil für einen Wert, mit Klemmung auf 0..1.
    double fraction(double value) const;

    /// Anzeigetext für einen Wert.
    QString text(double value) const;

    /// Der Thetis-wörtliche Name aus readingName(). Eigene Funktion,
    /// damit hier keine zweite Namensliste entsteht.
    QString thetisName() const;
};

/// Alle beschriebenen Größen, in der Reihenfolge von MeterPoller.h.
const QList<ReadingDescriptor>& allReadings();

/// Die Beschreibung zu einer Kennung, oder nullptr.
const ReadingDescriptor* readingFor(int bindingId);

/// Nur die Größen, die eine Skala tragen — das ist die Auswahl, die
/// ein Zeiger- oder Balkeninstrument anbieten darf.
QList<const ReadingDescriptor*> readingsWithScale();

// ── Die Empfangsskala ────────────────────────────────────────────────
//
// Dieselbe Kennlinie steht auch in SMeterWidget.h:317-320 (S0_DBM,
// S9_DBM, MAX_DBM, DB_PER_S). SMeterWidget wurde NICHT umgestellt —
// das wäre eine sichtbare Änderung an etwas Bestehendem und gehört dem
// Betreiber.
//
// Damit stehen die Zahlen an zwei Stellen, und ein Test kann das nicht
// heilen: die Konstanten dort sind private. tst_reading_source nagelt
// DIESE Seite fest (mit den Zeilen aus SMeterWidget.h als Herkunft);
// wer die andere ändert, muss hierher sehen. Bis SMeterWidget von hier
// liest, ist das eine Handreichung und keine Sicherung.
constexpr double kS0Dbm  = -127.0;   ///< SMeterWidget.h:62
constexpr double kS9Dbm  =  -73.0;   ///< 6 dB je S-Stufe ab S0
constexpr double kSMaxDbm = -13.0;   ///< S9+60, SMeterWidget.h:319

/// Wo S9 auf der Skala sitzt. Aus dem Entwurf des Betreibers
/// (~/Downloads/zeiger-verfeinert.html, sigFrac): 0.66, nicht 2/3.
/// Der Unterschied ist 0,7 % der Skalenlänge und damit unsichtbar —
/// aber ein Test muss eine Zahl festnageln, und die gezeichnete ist
/// die Vorlage.
constexpr double kS9Fraction = 0.66;

/// dBm → S-Stufen. Bis S9 sechs dB je Stufe, darüber zehn.
double sUnitsFromDbm(double dbm);

/// dBm → Anteil 0..1 mit der Stauchung über S9.
double signalFraction(double dbm);

} // namespace NereusSDR
