// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/GoogleMapTiles.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Der eigene Schluessel fuer Googles Map Tiles API, und die Probe, ob
// er taugt. Optional und je Bediener: wer keinen hat, sieht die Karte
// wie bisher mit den NASA-Bildern; wer einen eintraegt, bekommt beim
// Nachschlagen einer Station den Flug ueber Googles fotorealistische
// 3D-Kacheln bis aufs Dach und die Antenne (Betreiber 2026-11-10: „das
// Logbuch soll herausstechen").
//
// Der Schluessel ist ein Zugang und liegt darum im Schluesselbund
// (CredentialStore), nie in der Einstellungsdatei -- die wandert in
// Sicherungen, Bildschirmfotos und Support-Buendel. Gelesen wird er
// einmal je Programmlauf; der Abruf ist ein Prozessstart von
// `security`, und die Karte fragt bei jedem Flug.
//
// Die Probe holt genau das, was die 3D-Ansicht als Erstes braucht:
// root.json der Photorealistic 3D Tiles. Googles Absage kommt als
// JSON mit einem `reason` (API_KEY_INVALID, SERVICE_DISABLED,
// BILLING_DISABLED …); jede bekommt einen Satz, der sagt, welcher
// Schritt der Anleitung fehlt. interpret() ist rein und ohne Netz
// pruefbar.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include <QObject>
#include <QPointer>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QTimer;

namespace Longpath {

// ── Ablage ───────────────────────────────────────────────────────────
//
// Statisch, weil es genau einen Schluessel gibt; das Objekt dahinter
// ist nur der Melder, damit offene Kartenfenster einen neu
// eingetragenen Schluessel ohne Neustart sehen.
class GoogleMapsKey : public QObject {
    Q_OBJECT
public:
    static GoogleMapsKey& notifier();

    /// Der gespeicherte Schluessel, leer wenn keiner. Beim ersten Aufruf
    /// aus dem Schluesselbund, danach aus dem Speicher.
    static QString load();
    static bool isSet() { return !load().isEmpty(); }

    /// Ablegen (getrimmt). Leer heisst loeschen. Meldet changed().
    static bool save(const QString& key);
    static void erase();

    /// Den Speicher vergessen, damit der naechste load() wieder den
    /// Schluesselbund fragt -- fuer den Rundlauf im Pruefstand.
    static void forgetCachedForTest();

    /// Wie Googles Schluessel aussehen: "AIza" und 35 Zeichen aus
    /// [A-Za-z0-9_-]. Nur ein Hinweis beim Einfuegen (abgeschnitten,
    /// Leerzeichen mitkopiert); der Test fragt Google selbst.
    static bool looksLikeApiKey(const QString& key);

    /// Name des Eintrags im Schluesselbund ("Longpath: google.maptiles").
    static QString credentialKey();
    static QString credentialAccount();

signals:
    void changed();

private:
    GoogleMapsKey() = default;
};

// ── Probe gegen die Map Tiles API ────────────────────────────────────
class GoogleTilesProbe : public QObject {
    Q_OBJECT
public:
    enum class Verdict {
        Ok,
        Empty,           // nichts eingetragen
        InvalidKey,      // Google kennt den Schluessel nicht
        ApiNotEnabled,   // Map Tiles API im Projekt nicht eingeschaltet
        KeyRestricted,   // API-Beschraenkung schliesst Map Tiles aus
        AppRestricted,   // Website-/IP-Beschraenkung sperrt Longpath aus
        NoBilling,       // kein Rechnungskonto am Projekt
        Quota,           // Tageskontingent erschoepft
        Network,         // keine Antwort / kein Netz
        Other,
    };
    Q_ENUM(Verdict)

    explicit GoogleTilesProbe(QObject* parent = nullptr);
    ~GoogleTilesProbe() override;

    /// Eine Anfrage an root.json; Ergebnis kommt ueber finished().
    void check(const QString& key);
    bool isBusy() const { return !m_reply.isNull(); }

    /// Fuer den Pruefstand: ein eigener Endpunkt statt tile.googleapis.com.
    void setEndpointForTest(const QUrl& base) { m_base = base; }
    void setTimeoutMsForTest(int ms) { m_timeoutMs = ms; }

    /// root.json mit Schluessel, wie die 3D-Ansicht es laedt.
    static QUrl rootTilesetUrl(const QString& key,
                               const QUrl& base = defaultEndpoint());
    static QUrl defaultEndpoint();

    /// Googles Antwort in ein Urteil und einen Satz fuer den Bediener.
    static Verdict interpret(int httpStatus, const QByteArray& body,
                             QString* message = nullptr);
    static QString sentence(Verdict v, const QString& detail = QString());

signals:
    void finished(Longpath::GoogleTilesProbe::Verdict verdict,
                  const QString& message);

private:
    void onFinished();

    QNetworkAccessManager* m_nam{nullptr};
    QPointer<QNetworkReply> m_reply;
    QTimer* m_timer{nullptr};
    QUrl    m_base;
    int     m_timeoutMs{15000};
};

} // namespace Longpath
