#pragma once

// =================================================================
// src/core/TciFeld.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Ein Feld fuer eine TCI-Zeile sicher machen.
//
// Longpaths eigene Antworten sind zeilenweise und durch Kommas
// geteilt:
//
//     spot_zeile:3,14074000,OE5SOS,SSB,CLUSTER,12;
//     log_qso_zeile:0,20261007,041530,OE5SOS,20M,SSB,59,59;
//
// Die Felder darin kommen NICHT von uns. `ruf`, `mode` und `quelle`
// stammen aus den Spotquellen -- und eine davon ist ein Telnet-Strom
// von einem fremden Rechner im Internet. `call`, `band`, `mode` und
// die RST-Felder stammen aus der ADIF-Datei, in der auch Einfuhren aus
// anderen Programmen liegen.
//
// Steht in so einem Feld ein Komma, verschieben sich alle folgenden
// Felder: aus dem Rufzeichen wird die Betriebsart. Steht ein
// Semikolon darin, endet die Zeile mitten im Feld -- und was danach
// kommt, liest die Gegenseite als NEUEN BEFEHL. Ein Clusterbetreiber
// koennte der App damit Befehle unterschieben, ohne dass irgendwo ein
// Fehler sichtbar waere.
//
// Darum gehen Komma, Semikolon und Zeilenumbrueche heraus, bevor ein
// Feld in eine Zeile kommt. Dass dabei ein Zeichen verlorengeht, ist
// der kleinere Schaden: in einem Rufzeichen, einem Band, einer
// Betriebsart oder einem RST hat keines dieser Zeichen etwas zu
// suchen, und eine verschobene Zeile ist in jedem Fall schlimmer als
// ein fehlendes Komma.
//
// Dazu eine Laengengrenze. Ein Feld, das laenger ist als jedes
// Rufzeichen und jede Betriebsart zusammen, ist keine Auskunft
// sondern ein Unfall -- und in einer zeilenweisen Verbindung wird
// daraus eine Zeile, die niemand mehr liest.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-07 — Created in C++20/Qt6 for Longpath, AI-assisted via
//                 Anthropic Claude (Claude Code), operator Martin
//                 Fischer.
// =================================================================

#include <QChar>
#include <QString>

namespace Longpath::TciFeld {

/// Die Obergrenze fuer ein Feld. Das laengste Rufzeichen der Welt hat
/// elf Zeichen, die laengste Betriebsart in ADIF sieben; 64 laesst
/// jedem echten Wert Luft und haelt den Unfall draussen.
constexpr int kMaxZeichen = 64;

/// Macht `rohes` fuer eine Feldliste sicher: ohne Komma, ohne
/// Semikolon, ohne Zeilenumbruch, gekuerzt auf `kMaxZeichen`.
inline QString sicher(const QString& rohes)
{
    QString raus;
    raus.reserve(qMin(rohes.size(), kMaxZeichen));
    for (const QChar c : rohes) {
        if (c == QLatin1Char(',') || c == QLatin1Char(';')
            || c == QLatin1Char('\n') || c == QLatin1Char('\r')) {
            continue;
        }
        // Auch alles andere Unsichtbare heraus: ein Steuerzeichen in
        // einer Zeile, die ein Mensch im Protokoll lesen soll, ist
        // bestenfalls unsichtbar und schlimmstenfalls ein Trennzeichen
        // auf der Gegenseite.
        if (c.category() == QChar::Other_Control) { continue; }
        raus.append(c);
        if (raus.size() >= kMaxZeichen) { break; }
    }
    return raus.trimmed();
}

}  // namespace Longpath::TciFeld
