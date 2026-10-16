// =================================================================
// src/core/CrashReportDigest.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Am 2026-10-14 lagen sieben Absturzberichte zu Longpath in
// ~/Library/Logs/DiagnosticReports -- drei SIGSEGV vom 13.10. abends,
// alle mit demselben Weg (Rotor), und vier vom selben Tag, weil die
// installierte App eine Bibliothek nicht fand. Gefunden wurden sie,
// weil der Betreiber danach fragte. Im Protokoll von Longpath stand
// darueber nichts: ein Programm, das abstuerzt, schreibt seinen
// letzten Satz nicht mehr selbst.
//
// Das kann es aber beim NAECHSTEN Start nachholen. macOS legt zu jedem
// Absturz eine .ips-Datei ab; wer beim Start nachsieht, was seit dem
// letzten Lauf dazugekommen ist, findet seinen eigenen Absturz, bevor
// ihn jemand sucht.
//
// Was hier bewusst NICHT passiert: kein Versenden, kein Dialog, keine
// Nachfrage. Nur eine Zeile je Bericht im eigenen Protokoll -- an der
// Stelle, an der ein Mensch bei der Fehlersuche ohnehin zuerst liest.
//
// Die Auswertung ist eine reine Funktion ueber ein Verzeichnis und
// einen Zeitpunkt; sie laesst sich ohne echten Absturz pruefen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-14 -- Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace Longpath {

class CrashReportDigest {
public:
    /// Der Ort, an dem macOS die Berichte ablegt. Leer auf anderen
    /// Plattformen -- dort gibt es nichts zu lesen.
    static QString reportDir();

    /// Eine Zeile je Bericht, der NACH `since` entstanden ist, aelteste
    /// zuerst. Ein ungueltiger `since` heisst "alles" -- der erste Lauf
    /// nach dem Einbau soll zeigen, was schon da liegt.
    ///
    /// Jede Zeile: "<Zeit> · <Version> · <Signal> · <Grund>".
    static QStringList summarise(const QString& dir, const QDateTime& since,
                                 int maxReports = 10);

    /// Was aus einer einzelnen .ips-Datei zu holen ist. Leer, wenn sie
    /// sich nicht lesen oder nicht deuten laesst -- ein kaputter
    /// Bericht darf den Start nicht aufhalten.
    static QString summariseFile(const QString& path);

    /// Einstellungsschluessel: bis wohin schon gelesen wurde.
    static QString lastScanKey() { return QStringLiteral("LastCrashScanUtc"); }
};

} // namespace Longpath
