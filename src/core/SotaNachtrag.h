// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/core/SotaNachtrag.h  (Longpath)
// =================================================================
//
// ── MY_SOTA_REF nachtragen aus dem SOTAdata-Log (2026-11-11) ────────
//
// Martins Log hat keinen einzigen MY_SOTA_REF, obwohl er Gipfel
// aktiviert -- der Logger hat das Feld nie geschrieben. Was es gibt, ist
// sein Aktivierer-Log in der SOTA-Datenbank: sotadata.org.uk, Logs ->
// Activator -> "Download complete log", eine CSV-Datei.
//
// Ihr genaues Spaltenformat ist nirgends beschrieben (am 2026-11-11
// gesucht: Reflektor, GitHub, nichts). Der Leser nimmt darum, was er
// erkennt:
//   * das SOTA-Hochladeformat V2: V2,MeinRuf,MeinGipfel,Datum,Zeit,Band,
//     Betriebsart,SeinRuf,SeinGipfel,Notiz -- feste Spalten;
//   * eine Kopfzeile mit "Date" und "Summit" (und, falls da, "Time" und
//     einer Rufzeichenspalte) -- Spalten nach Namen;
//   * sonst je Zeile die erste Gipfelreferenz und das erste Datum.
// Datum als dd/mm/yyyy (die SOTA-Datenbank ist britisch), dd/mm/yy,
// yyyy-mm-dd oder yyyymmdd; steht irgendwo ein Monat > 12 an zweiter
// Stelle, gilt fuer die ganze Datei mm/dd.
//
// Zuordnung nach UTC-Tag. Hat die Datei je QSO eine Zeile mit dem
// Rufzeichen der Gegenstation, wird ein QSO des Logs, das an diesem Tag
// NICHT darin steht, zwar vorgeschlagen, aber nicht vorausgewaehlt --
// am 04.07.2026 steht morgens um 09:56 ein 20-m-QSO im Log, abends ab
// 19:08 die 2-m-QSOs vom Gipfel. Zwei Gipfel an einem Tag ohne
// Rufzeichen, das sie trennt: kein Vorschlag, sondern ein Hinweis.
//
// Geschrieben wird nur, was der Betreiber im Dialog bestaetigt
// (SotaNachtragDialog); ein schon gesetzter MY_SOTA_REF wird nie
// ueberschrieben.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include "models/LogEntry.h"

#include <QDate>
#include <QString>
#include <QStringList>
#include <QTime>
#include <QVector>

namespace Longpath::SotaNachtrag {

struct Zeile {
    QDate   datum;          ///< UTC
    QString gipfel;         ///< eigener Gipfel, normalisiert
    QTime   zeit;           ///< UTC, ungueltig wenn unbekannt
    QString gegenCall;      ///< leer, wenn die Datei keinen nennt
    QString gegenGipfel;    ///< S2S, normalisiert, sonst leer
};

struct Datei {
    QVector<Zeile> zeilen;
    bool        mitQsos{false};   ///< die Datei nennt Gegenstationen
    QString     format;           ///< "SOTA V2", "header", "loose"
    int         uebersprungen{0}; ///< Zeilen ohne Gipfel oder Datum
};

/// Die CSV-Datei lesen. Leer bei einer Datei ohne eine einzige
/// brauchbare Zeile; `fehler` sagt dann, warum.
Datei leseCsv(const QByteArray& csv, QString* fehler = nullptr);

struct Vorschlag {
    int     index{-1};          ///< im uebergebenen Log
    QString gipfel;             ///< fuer MY_SOTA_REF
    QString gegenGipfel;        ///< fuer SOTA_REF, sonst leer
    bool    imSotaLog{false};   ///< Rufzeichen steht an dem Tag in der Datei
    bool    vorgewaehlt{false};
    QString hinweis;            ///< englisch, fuer die Tabelle
};

struct Ergebnis {
    QVector<Vorschlag> vorschlaege;
    QStringList mehrdeutig;     ///< "2026-07-04: OE/OO-106, OE/OO-107"
    int schonGesetzt{0};        ///< QSOs mit MY_SOTA_REF, nicht angefasst
    int tageOhneQso{0};         ///< Aktivierungstage ohne QSO im Log
};

/// Vorschlaege fuer alle QSOs ohne MY_SOTA_REF an einem Aktivierungstag.
Ergebnis vorschlaege(const QVector<LogEntry>& log, const Datei& datei);

/// Die gewaehlten Vorschlaege eintragen. Setzt nur leere Felder.
/// Liefert die Zahl der geaenderten QSOs.
int anwenden(QVector<LogEntry>& log, const QVector<Vorschlag>& gewaehlt);

/// "OE5PEN/P" -> "OE5PEN", "DL/OE5SOS/P" -> "OE5SOS".
QString basisRufzeichen(const QString& call);

} // namespace Longpath::SotaNachtrag
