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
// Zweiter Weg zu denselben Kacheln (2026-11-10, abends): Betreiber
// „ich habe keine kreditkarte … und geld will ich auch keines zahlen".
// Google verlangt fuer den eigenen Schluessel ein Rechnungskonto, auch im
// Freikontingent. Cesium ion reicht Googles Photorealistic 3D Tiles
// (Asset 2275207) an jedes kostenlose Community-Konto weiter -- ohne
// Google-Cloud-Konto, 1.000 Abrufe im Monat. Darum zwei Quellen:
// CesiumIonToken und GoogleMapsKey, und Map3dAccess sagt, welche gilt.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-10 — Cesium ion als kostenlose Quelle, Map3dAccess.
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

// ── Welcher Zugang gilt ──────────────────────────────────────────────
//
// Der Bediener waehlt im Einrichtungsdialog die Quelle (Vorgabe: Cesium
// ion, weil kostenlos ohne Karte); secret() ist der abgelegte Zugang fuer
// GENAU diese Quelle, leer wenn dort nichts liegt. Das Objekt ist nur der
// Melder, damit offene Kartenfenster einen neuen Zugang ohne Neustart
// sehen.
class Map3dAccess : public QObject {
    Q_OBJECT
public:
    enum class Source { CesiumIon, Google };
    Q_ENUM(Source)

    static Map3dAccess& notifier();

    static Source chosenSource();             // AppSettings "Map3dSource"
    static void setChosenSource(Source s);    // meldet changed()
    static QString secret();                  // fuer chosenSource()
    static QString secretFor(Source s);
    static bool isSet() { return !secret().isEmpty(); }
    static QString sourceName(Source s);      // "Cesium ion" / "Google"

signals:
    void changed();

private:
    Map3dAccess() = default;
};

// ── Ablage: Cesium-ion-Zugang ────────────────────────────────────────
class CesiumIonToken {
public:
    static QString load();
    static bool isSet() { return !load().isEmpty(); }
    /// Ablegen (getrimmt). Leer heisst loeschen. Meldet Map3dAccess::changed().
    static bool save(const QString& token);
    static void erase();
    static void forgetCachedForTest();
    /// Ein ion-Zugang ist ein JWT: drei base64url-Teile mit Punkten,
    /// der erste beginnt mit "eyJ". Nur ein Hinweis beim Einfuegen.
    static bool looksLikeToken(const QString& token);
    static QString credentialKey();          // "cesium.iontoken"
    static QString credentialAccount();
    /// Googles Photorealistic 3D Tiles in Cesiums Asset Depot.
    static constexpr int kGoogle3dAssetId = 2275207;
};

// ── Ablage: eigener Google-Schluessel ────────────────────────────────
class GoogleMapsKey {
public:
    /// Der gespeicherte Schluessel, leer wenn keiner. Beim ersten Aufruf
    /// aus dem Schluesselbund, danach aus dem Speicher.
    static QString load();
    static bool isSet() { return !load().isEmpty(); }

    /// Ablegen (getrimmt). Leer heisst loeschen. Meldet Map3dAccess::changed().
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
};

// ── Probe: taugt der Zugang? ─────────────────────────────────────────
//
// Google: root.json der Map Tiles API. Cesium ion: der Endpunkt von
// Asset 2275207 -- genau das, was die 3D-Ansicht als Erstes holt. Der
// Zugang geht dort im Kopf mit (Authorization: Bearer), nicht in der URL.
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
        AssetMissing,    // ion: Googles Kacheln fehlen im Konto / Zugang ohne assets:read
        Quota,           // Tageskontingent erschoepft
        Network,         // keine Antwort / kein Netz
        Other,
    };
    Q_ENUM(Verdict)

    explicit GoogleTilesProbe(QObject* parent = nullptr);
    ~GoogleTilesProbe() override;

    /// Eine Anfrage an root.json; Ergebnis kommt ueber finished().
    void check(const QString& key);
    /// Eine Anfrage an Cesium ion (Asset 2275207); Ergebnis ueber finished().
    void checkIon(const QString& token);
    void check(Map3dAccess::Source s, const QString& secret);
    bool isBusy() const { return !m_reply.isNull(); }

    /// Fuer den Pruefstand: eigene Endpunkte statt der echten Dienste.
    void setEndpointForTest(const QUrl& base) { m_base = base; }
    void setIonEndpointForTest(const QUrl& base) { m_ionBase = base; }
    void setTimeoutMsForTest(int ms) { m_timeoutMs = ms; }

    /// root.json mit Schluessel, wie die 3D-Ansicht es laedt.
    static QUrl rootTilesetUrl(const QString& key,
                               const QUrl& base = defaultEndpoint());
    static QUrl defaultEndpoint();
    /// https://api.cesium.com/v1/assets/2275207/endpoint
    static QUrl ionEndpointUrl(const QUrl& base = ionDefaultEndpoint());
    static QUrl ionDefaultEndpoint();

    /// Googles Antwort in ein Urteil und einen Satz fuer den Bediener.
    static Verdict interpret(int httpStatus, const QByteArray& body,
                             QString* message = nullptr);
    static QString sentence(Verdict v, const QString& detail = QString());
    /// Dasselbe fuer Cesium ion.
    static Verdict interpretIon(int httpStatus, const QByteArray& body,
                                QString* message = nullptr);
    static QString sentenceIon(Verdict v, const QString& detail = QString());
    static QString sentenceFor(Map3dAccess::Source s, Verdict v,
                               const QString& detail = QString());
    /// Verlangt dieses Urteil eine neue Einrichtung (statt eines neuen Versuchs)?
    static bool isAccessProblem(Verdict v);

signals:
    void finished(Longpath::GoogleTilesProbe::Verdict verdict,
                  const QString& message);

private:
    void onFinished();

    QNetworkAccessManager* m_nam{nullptr};
    QPointer<QNetworkReply> m_reply;
    QTimer* m_timer{nullptr};
    QUrl    m_base;
    QUrl    m_ionBase;
    int     m_timeoutMs{15000};
    Map3dAccess::Source m_asking{Map3dAccess::Source::Google};
};

} // namespace Longpath
