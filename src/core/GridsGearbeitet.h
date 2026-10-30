#pragma once
// =================================================================
// src/core/GridsGearbeitet.h  (Longpath)
// =================================================================
//
// Welche Grossfelder (Maidenhead, vier Stellen) schon im Logbuch
// stehen — und welche nicht.
//
// Betreiber am 2026-10-30: „interessant waere zu sehen, welche grids
// ich noch nicht habe und diese dann zb blau zu markieren" — „somit
// sehe ich dann, was ich noch benoetige."
//
// ── Warum vier Stellen ──────────────────────────────────────────────
//
// FT8 schickt vier (`CQ G4MHJ IO83`). Sechs stehen nur in
// Nicht-Standard-Nachrichten und im Logbuch; sie hier mitzuzaehlen
// hiesse, IO83 und IO83WJ fuer verschiedene Felder zu halten — und
// dann waere jedes QSO ein neues.
//
// ── Warum die Datei und nicht das Logbuchfenster ────────────────────
//
// `LogbookWindow::m_all` haelt dieselben Eintraege, ist aber privat
// und existiert nur, solange das Fenster offen ist. Die Markierung
// im FT8-Fenster soll auch dann stimmen, wenn das Logbuch nie
// geoeffnet wurde. Die ADIF-Datei ist die Wahrheit, an der beide
// haengen.
//
// ── Was NICHT hier steht ────────────────────────────────────────────
//
// Band und Betriebsart. „Neu" heisst hier: dieses Feld steht
// nirgends im Logbuch. Wer Grids je Band jagen will, braucht eine
// andere Rechnung -- und zuerst eine Ansage, dass er das will.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-30 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QSet>
#include <QString>

namespace Longpath {

class GridsGearbeitet
{
public:
    /// Aus einer ADIF-Datei lesen. Gibt die Zahl der gefundenen
    /// Felder; 0 heisst „nichts gelesen" -- und das ist kein Befund
    /// ueber das Band, sondern ueber die Datei.
    int liesAus(const QString& adifPfad);

    /// Ein Feld dazunehmen (ein frisches QSO).
    void merke(const QString& grid);

    /// Steht es noch nicht im Logbuch? Ein LEERES Feld ist nie neu --
    /// „unbekannt" ist kein Fund.
    bool istNeu(const QString& grid) const;

    /// Das Grossfeld aus einem beliebigen Locator: vier Stellen,
    /// grossgeschrieben. Leer, wenn es keiner ist.
    static QString grossfeld(const QString& locator);

    int anzahl() const { return m_felder.size(); }
    bool leer()  const { return m_felder.isEmpty(); }
    void leeren() { m_felder.clear(); }

    QSet<QString> felder() const { return m_felder; }

private:
    QSet<QString> m_felder;
};

}  // namespace Longpath
