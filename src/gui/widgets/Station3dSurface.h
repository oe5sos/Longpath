// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/widgets/Station3dSurface.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Die Flaeche, auf der eine nachgeschlagene Station in Googles
// fotorealistischem 3D angeflogen wird -- vom eigenen QTH den
// Grosskreis entlang, dann hinunter bis aufs Dach und die Antenne.
//
// Warum eine Schnittstelle und eine Fabrik statt der Klasse selbst:
// die echte Flaeche ist ein QWebEngineView mit CesiumJS, und Qt
// WebEngine ist OPTIONAL (Homebrew-Qt auf dem Mac hat es, die Linux-
// und Windows-Laeufer der CI nicht). Haengte LongpathObjs daran, linkte
// jeder der ~600 Pruefstaende WebEngine mit. So liegt hier nur der
// Vertrag; die WebEngine-Fassung (Station3dView) lebt in einer eigenen
// Bibliothek, die nur das Programm bekommt, und meldet sich beim Start
// ueber setFactory() an. Ohne Anmeldung -- kein WebEngine im Bau, oder
// ein Pruefstand -- ist isAvailable() falsch und die Karte nimmt den
// alten Weg: NASA-Bilder plus Google Earth im Browser.
//
// Daneben die reinen Bausteine der Seite (Station3dPage): die
// festgenagelte CesiumJS-Fassung, die Flugbeschreibung als JSON, und
// der Google-Earth-Verweis fuer den Rueckfall. Rein, damit sie ohne
// WebEngine pruefbar sind.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include <QString>
#include <QUrl>
#include <QWidget>

#include <functional>

namespace Longpath {

// Ein Flug: wohin, woher, und was die Anzeige unterwegs sagt.
struct Station3dFlight {
    QString call;
    QString caption;        // Klartext (kein HTML), z. B. Name · Ort
    double  lat{0.0};
    double  lon{0.0};
    bool    hasHome{false};
    double  homeLat{0.0};
    double  homeLon{0.0};
    double  km{-1.0};        // Grosskreis vom QTH; < 0 = unbekannt
    double  bearingDeg{-1.0};
    QString apiKey;
};

class Station3dSurface : public QWidget {
    Q_OBJECT
public:
    using Factory = std::function<Station3dSurface*(QWidget* parent)>;

    explicit Station3dSurface(QWidget* parent = nullptr) : QWidget(parent) {}

    virtual void fly(const Station3dFlight& flight) = 0;

    /// Vom Programm beim Start gesetzt, wenn es mit WebEngine gebaut ist.
    static void setFactory(Factory f);
    static bool isAvailable();
    /// nullptr ohne Fabrik.
    static Station3dSurface* create(QWidget* parent);

signals:
    /// Unten angekommen; der Bediener kann jetzt selbst drehen.
    void arrived();
    /// Der Flug ging nicht. keyProblem: Google hat den Schluessel
    /// abgelehnt (dann hilft nur die Einrichtung, kein neuer Versuch).
    void failed(const QString& reason, bool keyProblem);
};

namespace Station3dPage {

/// Festgenagelt; die SRI-Pruefsummen in resources/flug3d/flug3d.html
/// gehoeren zu genau dieser Fassung.
inline constexpr const char* kCesiumVersion = "1.146.0";

/// https://cdn.jsdelivr.net/npm/cesium@<Fassung>/Build/Cesium/
QString cesiumBaseUrl();

/// Die Seite aus der Ressource, mit eingesetzter Basis-URL.
QString html(const QString& cesiumBase = cesiumBaseUrl());

/// Der JS-Aufruf, der einen Flug startet.
QString flightScript(const Station3dFlight& flight);

/// Rueckfall ohne Schluessel oder WebEngine: dieselbe Station in
/// Google Earth im Browser, Blick in Flugrichtung vom eigenen QTH.
QUrl googleEarthWebUrl(double lat, double lon, double headingDeg = 0.0);

} // namespace Station3dPage

} // namespace Longpath
