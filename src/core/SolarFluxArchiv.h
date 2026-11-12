// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/core/SolarFluxArchiv.h  (Longpath)
// =================================================================
//
// ── Solar-Flux je Monat, hoechstens einmal am Tag geholt (2026-11-10) ─
//
// Fuer das Bild "Log gegen den Sonnenzyklus" im Logbuch: F10.7 als
// Monatsmittel von NOAA SWPC (observed-solar-cycle-indices.json, rund
// 300 kB, reicht bis 1749 zurueck). Die Datei wird im Cache-Ordner der
// App abgelegt und hoechstens einmal am Tag neu geholt:
//
//   * mit Netz:            alt genug -> holen, sonst Cache;
//   * ohne Netz:           Cache, so alt er ist;
//   * ohne Netz und Cache: leer -- das Bild zeigt dann nur die Balken
//                          und sagt es.
//
// Ob ueberhaupt geholt werden darf, entscheidet der Aufrufer (Pruef-
// staende holen nie); WANN, entscheidet brauchtAbruf() -- eine reine
// Funktion, damit die Regel ohne Uhr und ohne Netz pruefbar ist.

#pragma once

#include "core/LogbuchGesamtbilder.h"

#include <QDateTime>
#include <QObject>
#include <QString>

class QNetworkAccessManager;

namespace Longpath {

class SolarFluxArchiv : public QObject {
    Q_OBJECT
public:
    static constexpr const char* kUrl =
        "https://services.swpc.noaa.gov/json/solar-cycle/observed-solar-cycle-indices.json";

    /// `cachePfad` leer = <CacheLocation>/swpc-solar-cycle.json.
    explicit SolarFluxArchiv(const QString& cachePfad = {}, QObject* parent = nullptr);

    static QString standardPfad();

    /// Liest den Cache (falls noch nicht gelesen). true = Werte da.
    bool ladeCache();
    const LogbuchBilder::FluxJeMonat& werte() const { return m_werte; }
    bool hatWerte() const { return !m_werte.isEmpty(); }
    /// Zeitpunkt der Datei im Cache (ungueltig = keine).
    QDateTime stand() const { return m_stand; }
    QString cachePfad() const { return m_pfad; }

    /// Die Regel: ohne Cache sofort; sonst erst, wenn er einen Tag alt
    /// ist; nach einem Versuch (auch einem gescheiterten) eine Stunde
    /// Ruhe, damit ein Bild ohne Netz nicht bei jedem Oeffnen fragt.
    static bool brauchtAbruf(const QDateTime& cacheStand, const QDateTime& letzterVersuch,
                             const QDateTime& jetzt);

    /// Holt, wenn brauchtAbruf() es sagt. Kein Prueftor hier: das
    /// gehoert zum Aufrufer.
    void holeWennNoetig(const QDateTime& jetzt);

    /// Eine Antwort von SWPC verarbeiten (auch fuer Pruefstaende):
    /// gueltig -> Cache schreiben, Werte uebernehmen, true.
    bool uebernimm(const QByteArray& json, const QDateTime& jetzt);

    bool laeuft() const { return m_laeuft; }

signals:
    /// Neue Werte (aus dem Netz) sind da.
    void aktualisiert();

private:
    QString m_pfad;
    LogbuchBilder::FluxJeMonat m_werte;
    QDateTime m_stand;
    QDateTime m_letzterVersuch;
    bool m_cacheGelesen{false};
    bool m_laeuft{false};
    QNetworkAccessManager* m_netz{nullptr};
};

} // namespace Longpath
