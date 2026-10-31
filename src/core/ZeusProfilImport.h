#pragma once

// =================================================================
// src/core/ZeusProfilImport.h  (Longpath)
// =================================================================
//
// Longpath-original. Liest Zeus' TX-Audio-Profile.
//
// Die Feldbedeutungen stammen aus Zeus' eigenem, offengelegtem
// Maschinenteil (GPL-2.0-or-later, Douglas J. Cerrato KB2UKA /
// Christian Suarez N9WAR und Mitwirkende), Klon
// `~/Longpath/zeus-station-engine` auf v2.0.61:
//
//   Zeus.Contracts/Dtos.cs:3838-3886  TxAudioProfileDto — das Profil
//   Zeus.Contracts/Dtos.cs:952-967    TxLevelingConfig
//   Zeus.Contracts/Dtos.cs:976-992    TxPhaseRotatorConfig
//   Zeus.Contracts/Dtos.cs:3646-3684  CfcConfig + CfcBand:3610
//   Zeus.Contracts/Dtos.cs:3698-3730  DexpConfig
//   Zeus.Contracts/Dtos.cs:3614-3628  LegacyTxEqConfig
//
// Es ist KEIN Port: hier steht keine Zeile Zeus-Code, nur die
// Bedeutung seiner Felder, so wie `ThetisProfilImport` die Bedeutung
// der Thetis-Spalten traegt.
//
// ── Warum es kein Dateiimport ist ────────────────────────────────────
//
// Zeus hat kein Austauschformat. Auf der Platte liegen nur `.zeusdb`
// (rohe LiteDB, die ganze Produktdatenbank) und `.zeus` (Sicherung);
// die Audio-Suite ist eine JSON-Datei des PROPRIETAEREN Clients
// (UnifiedDatabaseBackup.cs:392, `StationOwned: false`) und ihre
// Struktur ist nicht offengelegt.
//
// Ein Profil holt man sich darum bei der LAUFENDEN Maschine:
//
//     GET http://127.0.0.1:<port>/api/tx-audio-profiles
//
// Denselben Weg geht der einzige fremde Konverter, der alle vier
// Programme kennt (Chris W4ORS, SDR-Profile-Convert): „live engine
// snapshots, via its API". Der Port steht nicht fest — Zeus bindet
// ihn dynamisch, und der Konverter sucht ihn ueber die lauschenden
// Zeus-Prozesse. Bei uns fragt `ZeusAbfrage` eine Portliste ab.
//
// Diese Datei kennt davon nichts: JSON hinein, Profile und Hinweise
// heraus, am Schreibtisch vollstaendig pruefbar. Dieselbe Trennung
// wie bei ThetisProfilImport — und derselbe Grund: eine Schicht, die
// das Netz braucht, um geprueft zu werden, wird nicht geprueft.
//
// ── Was NICHT abgebildet werden kann ─────────────────────────────────
//
// Zeus' Audio-Suite (Tor, De-Esser, Roehre, Hall, Anreicherer) steckt
// in `NativePluginStates` und `VstPluginStates` — zeusId auf einen
// Beutel von Einstellungen, deren SCHLUESSEL nicht offengelegt sind.
// Das waere unser Kanalzug. Es wird gemeldet und nicht geraten; wer
// hier raet, verstellt einen Sendeton und die Anzeige sieht dabei
// richtig aus.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include "core/ThetisProfilImport.h"   // ThetisProfil: dieselbe Ausbeute

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringList>

namespace Longpath {

/// Zeus' TX-Audio-Profile einlesen.
///
/// Die Ausbeute ist absichtlich `ThetisProfil` — nicht aus Bequem-
/// lichkeit, sondern weil Longpaths Profilfelder aus derselben
/// Tabelle stammen wie Thetis' und Zeus' (alle drei sind Thetis-
/// Nachfahren). Damit schreibt `MicProfileManager::einleseThetisProfile`
/// beide Herkuenfte ueber denselben Weg, und es gibt keinen zweiten
/// Schreiber, der beim naechsten Feld vergessen wird.
class ZeusProfilImport {
public:
    struct Ergebnis {
        QList<ThetisProfil> profile;
        /// Gruende, warum nichts (oder nicht alles) gelesen wurde.
        QStringList fehler;
        /// Hinweise zur ganzen Antwort, nicht zu einem Profil.
        QStringList hinweise;
    };

    /// Die Antwort von `/api/tx-audio-profiles` lesen.
    ///
    /// Angenommen wird beides: das Objekt mit `{"profiles": [...]}`
    /// (so antwortet der Endpunkt, `TxAudioProfilesResponse`) und eine
    /// nackte Liste — wer die Antwort von Hand in eine Datei legt,
    /// schneidet die Huelle gern weg.
    static Ergebnis lies(const QByteArray& json);

    /// Ob diese Antwort ueberhaupt nach Zeus aussieht. Fuer den
    /// Erkenner in ProfilHerkunft, damit er eine abgelegte
    /// Zeus-Antwort benennen kann.
    static bool siehtNachZeusAus(const QByteArray& json);

    /// Was Zeus fuehrt und Longpath nicht, mit dem Grund in Worten.
    /// Oeffentlich, weil die Oberflaeche es zeigen soll.
    static QStringList nichtUebernommen();

private:
    ZeusProfilImport() = delete;
};

} // namespace Longpath
