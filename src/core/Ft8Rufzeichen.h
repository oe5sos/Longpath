// =================================================================
// src/core/Ft8Rufzeichen.h  (Longpath)
// =================================================================
//
// Die Rufzeichen, die nur als HASH über den Draht kommen.
//
// Im 77-Bit-Format steht nicht jedes Rufzeichen im Klartext. Wo eines
// nicht in die 28 Bit der Normalform passt — Sonderrufzeichen,
// Präfixe, alles mit Schrägstrich —, trägt die Nachricht statt dessen
// eine 22-, 12- oder 10-Bit-Prüfsumme. Der Empfänger kann sie nur
// auflösen, wenn er dasselbe Rufzeichen VORHER schon einmal im
// Klartext gehört hat und sich gemerkt hat.
//
// `ft8_lib` sieht dafür eine Schnittstelle vor
// (`ftx_callsign_hash_interface_t`, message.h:64-70) und ruft sie an
// beiden Enden: beim Auspacken eines Klartext-Rufzeichens `save_hash`,
// beim Auftauchen eines Hashes `lookup_hash`. Longpath übergab dort
// bis zum 2026-10-28 `nullptr` (Ft8Decoder.cpp:129). Die Folge stand
// in jedem Fund: `lookup_callsign` schreibt bei einem Fehlschlag
// wortwörtlich `<...>` (message.c:734), und genau das sah der
// Betreiber statt des Rufzeichens.
//
// Das betrifft den gewöhnlichen Betrieb, nicht nur Conteste. Beim
// EU-VHF-Contestsatz (i3=5) sind BEIDE Rufzeichen gehasht
// (WSJT-X packjt77.f90:593-610 [@b4f9a43] — `h12 h22 r1 s3 S11 g25`),
// dort ist die Tabelle also nicht Verbesserung, sondern Bedingung.
//
// Die kurzen Hashes leiten sich aus dem langen ab, nicht aus einer
// eigenen Rechnung (message.c:707-708 [ft8_lib @vendored]):
//
//     n12 = n22 >> 10
//     n10 = n22 >> 12
//
// Deshalb genügt `save(rufzeichen, n22)`; alle drei Schlüssel fallen
// daraus ab. Eine zweite Stelle, die dasselbe rechnet, liefe
// auseinander.
//
// Faden: `decodeBlock` läuft im Arbeitsfaden, die Bedienung liest aus
// dem Hauptfaden. Beides geht durch dieselbe Sperre.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-28 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original. Die zwei Schiebeweiten stammen aus
// ft8_lib (zitiert oben), portiert ist hier nichts.

#pragma once

#include <QHash>
#include <QMutex>
#include <QString>
#include <QStringList>

#include <cstdint>

namespace Longpath {

/// Gemerkte Rufzeichen, nach ihren drei Hashlängen durchsuchbar.
///
/// Absichtlich KEIN Verfall nach Zeit: ein Rufzeichen, das vor einer
/// Stunde im Klartext kam, ist eine Stunde später dasselbe. Begrenzt
/// wird über die Zahl der Einträge, damit ein Dauerlauf nicht wächst.
class Ft8Rufzeichenspeicher {
public:
    /// Wie viele Rufzeichen gemerkt werden. 2000 ist reichlich für
    /// einen Abend an einem Band und kostet wenige Zehntel Megabyte.
    static constexpr int kHoechstzahl = 2000;

    static Ft8Rufzeichenspeicher& instanz();

    /// Ein im Klartext gehörtes Rufzeichen merken. `n22` kommt von
    /// `ft8_lib`; die kürzeren Hashes werden daraus abgeleitet.
    void merke(const QString& rufzeichen, quint32 n22);

    /// Ein gehashtes Rufzeichen auflösen. Leer, wenn unbekannt.
    QString suche(int hashBits, quint32 hash) const;

    /// Wie viele Rufzeichen gemerkt sind. Für Anzeige und Prüfstand.
    int anzahl() const;

    /// Alles vergessen. Nur für Prüfstände und einen Bandwechsel von
    /// Hand — im Betrieb schadet ein alter Eintrag nicht.
    void leeren();

private:
    Ft8Rufzeichenspeicher() = default;

    mutable QMutex        m_sperre;
    QHash<quint32, QString> m_n22;
    QHash<quint32, QString> m_n12;
    QHash<quint32, QString> m_n10;
    QStringList           m_reihenfolge;   ///< ältestes zuerst
};

/// Die Schnittstelle, die `ft8_lib` erwartet — ein Zeiger darauf
/// gehört in jeden `ftx_message_decode`-Aufruf.
void* ft8RufzeichenSchnittstelle();

}  // namespace Longpath
