// no-port-check: Longpath-original, no Thetis logic.
//
// =================================================================
// src/core/HttpAbruf.h  (Longpath)
// =================================================================
//
// Ein GET als austauschbare Funktion (2026-11-11).
//
// Das SOTA-Gipfelverzeichnis und der Hoehenabruf fuer das UKW-Profil
// holen beide JSON ueber HTTPS. Ihre Logik (Cache, einmal je Gipfel,
// einmal je Sekunde, Stueckelung zu 100 Punkten) soll ein Pruefstand
// fahren koennen, ohne ins Netz zu gehen -- also steckt das Netz hinter
// dieser einen Funktion, und der Pruefstand setzt seine eigene ein, die
// aufgezeichnete Antworten liefert und mitzaehlt.
//
// Die Vorgabe geht im Pruefmodus (QStandardPaths::isTestModeEnabled)
// NICHT ins Netz, sondern antwortet sofort mit einem Netzfehler. Das Tor
// sitzt hier, beim Transport -- nicht am Anfang der Logik, die dadurch
// sonst in jedem Lauf toter Code waere.
//
// Modification history (Longpath):
//   2026-11-11  AI (Anthropic Claude Code), operator Martin Fischer
//               (OE5SOS). Original.

#pragma once

#include <QByteArray>
#include <QString>
#include <QUrl>

#include <functional>

class QObject;

namespace Longpath {

struct HttpAntwort {
    int        status{0};    ///< HTTP-Status, 0 = keine Antwort
    QByteArray body;
    QString    netzFehler;   ///< leer, wenn der Server geantwortet hat
};

using HttpAbruf =
    std::function<void(const QUrl& url,
                       std::function<void(const HttpAntwort&)> fertig)>;

/// GET ueber einen QNetworkAccessManager, der `besitzer` gehoert.
/// `userAgent` wird gesetzt, wie ihn der jeweilige Dienst verlangt.
HttpAbruf standardHttpAbruf(QObject* besitzer, const QString& userAgent);

} // namespace Longpath
