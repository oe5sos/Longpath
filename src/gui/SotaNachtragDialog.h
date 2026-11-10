// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/gui/SotaNachtragDialog.h  (Longpath)
// =================================================================
//
// Die Vorschlaege aus dem SOTAdata-Log zeigen, bevor etwas geschrieben
// wird (2026-11-11). Eine Zeile je QSO mit Haekchen; vorausgewaehlt ist,
// was die Datei bestaetigt (siehe core/SotaNachtrag.h). Der Dialog
// schreibt selbst nichts -- er liefert die gewaehlten Vorschlaege, und
// das Logbuch sichert die Datei und traegt sie ein.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include "core/SotaNachtrag.h"
#include "models/LogEntry.h"

#include <QDialog>

class QLabel;
class QPushButton;
class QTableWidget;

namespace Longpath {

class SotaNachtragDialog : public QDialog {
    Q_OBJECT
public:
    SotaNachtragDialog(const QVector<LogEntry>& log, QWidget* parent = nullptr);

    /// Eine CSV-Datei lesen und die Vorschlaege zeigen. false, wenn die
    /// Datei nicht lesbar ist oder nichts Brauchbares enthaelt -- der
    /// Grund steht dann im Dialog.
    bool ladeCsv(const QString& pfad);
    bool ladeCsvDaten(const QByteArray& csv, const QString& name);

    /// Was der Betreiber angehakt hat.
    QVector<SotaNachtrag::Vorschlag> gewaehlt() const;

    QTableWidget* tabelleForTest() const { return m_tabelle; }
    QPushButton*  anwendenForTest() const { return m_anwenden; }
    QString zusammenfassung() const;

private:
    void waehleDatei();
    void zeige();
    void zaehle();

    QVector<LogEntry> m_log;
    SotaNachtrag::Ergebnis m_erg;
    QLabel*       m_erklaerung{nullptr};
    QLabel*       m_summe{nullptr};
    QTableWidget* m_tabelle{nullptr};
    QPushButton*  m_anwenden{nullptr};
};

} // namespace Longpath
