#pragma once

// =================================================================
// src/core/ZeusAbfrage.h  (Longpath)
// =================================================================
//
// Longpath-original. Holt Zeus' TX-Audio-Profile bei der LAUFENDEN
// Maschine ab:
//
//     GET http://<wirt>:<port>/api/tx-audio-profiles
//
// Warum ueberhaupt ueber das Netz und nicht aus einer Datei, steht im
// Kopf von ZeusProfilImport.h: Zeus hat kein Austauschformat.
//
// ── Der Port wird nicht geraten ──────────────────────────────────────
//
// Zeus bindet seinen Port dynamisch; im offengelegten Maschinenteil
// (v2.0.61) steht keine feste Nummer — `StationEngineEndpoints.cs:26`
// nennt nur 5173, und das ist der Entwicklungsserver der Oberflaeche,
// nicht die Maschine. Der einzige fremde Konverter sucht darum die
// lauschenden Zeus-PROZESSE ab (Chris W4ORS, `Probe-SdrStores.ps1`).
//
// Longpath tut das NICHT. Es fragt genau die Adresse, die der
// Betreiber eingetragen hat — er liest sie an seiner Zeus-Oberflaeche
// ab. Eine geratene Portliste waere ein Scanner auf dem eigenen
// Rechner, und sie waere bei der naechsten Zeus-Fassung falsch.
//
// Die Adresse steht in AppSettings unter `Zeus/Basis`. Als Vorgabe
// steht der Port, der am 2026-10-31 an Martins Station gemessen wurde
// (55111) — er ist je Installation ANDERS und nur deshalb da, damit
// das Feld nicht leer ist.
//
// Nicht 5000, obwohl das die uebliche Hausnummer fuer so etwas waere:
// auf macOS haelt diesen Port der AirPlay-Empfaenger (`ControlCenter`,
// am selben Tag nachgesehen), und eine Vorgabe, die zuverlaessig auf
// das falsche Programm zeigt, ist schlimmer als keine.
//
// ── Zwei Prozesse, ein Endpunkt ──────────────────────────────────────
//
// Zeus laeuft als ZWEI lauschende Prozesse, und nur einer kann diese
// Frage beantworten (gemessen am 2026-10-31):
//
//     StationEn  127.0.0.1:60787  ->  404 auf /api/tx-audio-profiles
//     ZeusProdu  127.0.0.1:55111  ->  200, die Profile
//
// Der Endpunkt gehoert dem PRODUKT, nicht der offengelegten Maschine.
// Wer den Port der Station Engine eintraegt, bekommt darum ein 404 und
// keine Verbindungsverweigerung — die Meldung sagt das.
//
// ── Nur der eigene Rechner ───────────────────────────────────────────
//
// Abgefragt wird ausschliesslich eine Adresse auf diesem Rechner
// (127.0.0.1, ::1, localhost). Zeus' eigene Wache laesst ohnehin nur
// oertliche Anfragen durch (`LocalRequestGuard.cs`), und ein
// Profilholer, der beliebige Wirte anfragen darf, ist ein Werkzeug,
// nach dem niemand gefragt hat.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Created in C++20/Qt6 for Longpath by Martin Fischer,
//                 AI-assisted via Anthropic Claude Code.
// =================================================================

#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace Longpath {

class ZeusAbfrage : public QObject {
    Q_OBJECT

public:
    explicit ZeusAbfrage(QObject* parent = nullptr);
    ~ZeusAbfrage() override;

    /// Die eingestellte Basisadresse, z. B. „http://127.0.0.1:5000".
    static QString basis();
    static void    setzeBasis(const QString& basis);

    /// Ob diese Adresse abgefragt werden darf: http(s) und ein Wirt
    /// auf diesem Rechner. Oeffentlich, damit der Prueffall sie ohne
    /// Netz fahren kann — und damit die Oberflaeche beim Eintippen
    /// schon sagen kann, was nicht geht.
    static bool istOertlich(const QString& basis, QString* grund = nullptr);

    /// Die Profile holen. Antwortet IMMER mit genau einem Signal.
    /// Frist in Millisekunden; 0 nimmt die Vorgabe (4000).
    void hole(int fristMs = 0);

signals:
    /// Die rohe Antwort. Gelesen wird sie von ZeusProfilImport — diese
    /// Klasse deutet nichts, sie holt nur.
    void gekommen(const QByteArray& json);
    /// Mit einem Satz, der dem Betreiber sagt, was zu tun ist.
    void gescheitert(const QString& grund);

private:
    QNetworkAccessManager* m_netz{nullptr};
};

} // namespace Longpath
