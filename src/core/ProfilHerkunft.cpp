// =================================================================
// src/core/ProfilHerkunft.cpp  (Longpath)
// =================================================================
// no-port-check: Longpath-eigene Datei; die beiden erkannten Formate
// sind in AetherPreset.h und ThetisProfilImport.h belegt.
//
// See ProfilHerkunft.h for the full header.
// =================================================================

#include "core/ProfilHerkunft.h"

#include "core/strip/AetherPreset.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QXmlStreamReader>

namespace Longpath {
namespace ProfilHerkunft {

namespace {

// Wie viel Datei fuer die Erkennung angesehen wird. Eine
// Thetis-Sicherung ist mehrere Megabyte gross und traegt ihr
// eingebettetes XSD vor den Daten; die Wurzel und die ersten
// Tabellennamen stehen aber weit vorn. Mehr zu lesen hiesze, die
// ganze Datei zweimal durchzugehen — einmal hier, einmal im Leser.
constexpr int kBlickfeld = 256 * 1024;

// ── Thetis ───────────────────────────────────────────────────────────
//
// Beide Dateiarten haben dieselbe Form: Wurzel `<Data>`, darin ein
// eingebettetes XSD und je Profil ein `<TXProfile>`. Der Unterschied
// ist, wie viele TABELLEN darin stehen — die Einzelausfuhr wirft alle
// ausser TXProfile weg. Dieselbe Unterscheidung wie in
// ThetisProfilImport::lies; sie steht hier noch einmal, weil der
// Erkenner vor dem Leser laeuft und ihn nicht anwerfen soll, nur um
// zu wissen, was da liegt.
bool istThetis(const QByteArray& inhalt, bool* nurProfile)
{
    QXmlStreamReader r(inhalt);
    if (!r.readNextStartElement()) { return false; }
    if (r.name() != QLatin1String("Data")) { return false; }

    bool sahProfil  = false;
    bool sahAnderes = false;
    // Nur die DIREKTEN Kinder von `<Data>` sind Tabellenzeilen. Die
    // Felder INNERHALB eines `<TXProfile>` (Name, TXEQPreamp, …) sind
    // es nicht — und genau daran ist die erste Fassung dieser Funktion
    // gescheitert: sie hielt `<Name>` fuer eine zweite Tabelle und
    // erklaerte jede Einzelausfuhr zur ganzen Datenbank. Der
    // Pruefstand hat es gefangen, nicht der Betrieb.
    int tiefe = 0;
    while (!r.atEnd() && !r.hasError()) {
        const auto t = r.readNext();
        if (t == QXmlStreamReader::EndElement) { --tiefe; continue; }
        if (t != QXmlStreamReader::StartElement) { continue; }
        ++tiefe;
        if (tiefe != 1) { continue; }
        if (r.name() == QLatin1String("TXProfile")) {
            sahProfil = true;
        } else if (r.prefix() == QLatin1String("xs")
                   || r.prefix() == QLatin1String("xsd")
                   || r.prefix() == QLatin1String("msdata")) {
            // Das eingebettete XSD. Keine Tabelle.
            continue;
        } else {
            sahAnderes = true;
        }
        // Nicht bis zum Ende einer Sicherung lesen: sobald beides
        // feststeht, ist die Frage beantwortet.
        if (sahProfil && sahAnderes) { break; }
    }
    if (!sahProfil) { return false; }
    if (nurProfile != nullptr) { *nurProfile = !sahAnderes; }
    return true;
}

} // namespace

Befund erkenne(const QByteArray& inhalt)
{
    Befund b;

    if (inhalt.isEmpty()) {
        b.warumNicht = QStringLiteral("Die Datei ist leer.");
        return b;
    }

    const QByteArray blick = inhalt.left(kBlickfeld);

    // ── 1. JSON? ─────────────────────────────────────────────────────
    QJsonParseError jf{};
    const QJsonDocument doc = QJsonDocument::fromJson(inhalt, &jf);
    if (jf.error == QJsonParseError::NoError && doc.isObject()) {
        const QString schema =
            doc.object().value(QStringLiteral("schema")).toString();
        if (schema.startsWith(QStringLiteral("AetherSDR ChannelStripPreset"))) {
            b.art   = Art::AetherKanalzug;
            b.was   = QStringLiteral("ein Kanalzugprofil aus AetherSDR");
            b.wohin = QStringLiteral("der Kanalzug");
            return b;
        }
        // Ein JSON-Objekt, aber keines, das wir kennen. Der Satz nennt
        // das Feld, an dem es haengt — damit der Betreiber an der
        // eigenen Datei nachsehen kann, statt zu raten.
        b.warumNicht = QStringLiteral(
            "Das ist eine JSON-Datei, aber kein Profil, das Longpath "
            "kennt: das Feld „schema\" lautet „%1\". Gelesen werden "
            "AetherSDRs Kanalzugprofile („AetherSDR ChannelStripPreset "
            "v1\") und Thetis-TX-Profile (XML).")
            .arg(schema.isEmpty() ? QStringLiteral("—") : schema);
        return b;
    }

    // ── 2. Thetis-XML? ───────────────────────────────────────────────
    bool nurProfile = false;
    if (istThetis(blick, &nurProfile)) {
        b.art = nurProfile ? Art::ThetisProfilausfuhr : Art::ThetisDatenbank;
        b.was = nurProfile
                    ? QStringLiteral("eine Thetis-Profilausfuhr")
                    : QStringLiteral("eine ganze Thetis-Datenbank");
        b.wohin = QStringLiteral("die TX-Profile");
        return b;
    }

    // ── 3. Nichts davon ──────────────────────────────────────────────
    //
    // Hier wird nicht geraten. Steht eine Herkunft nicht oben, gibt es
    // keinen Leser dafuer, und das zu verschweigen waere schlimmer als
    // die Absage.
    b.warumNicht = QStringLiteral(
        "Diese Datei kennt Longpath nicht. Gelesen werden: "
        "AetherSDRs Kanalzugprofile (.json) und Thetis-TX-Profile "
        "(.xml, einzeln oder als ganze Datenbank). Erkannt wird am "
        "Inhalt — eine umbenannte Datei ist kein Hindernis.");
    return b;
}

QString dateiFilter()
{
    // Absichtlich nicht eng: die Erkennung laeuft am Inhalt, also
    // waere ein Filter, der nach Endung aussperrt, genau der Fehler,
    // den die Erkennung vermeiden soll. „Alle Dateien" steht darum
    // gleichberechtigt daneben und nicht als Notnagel am Ende.
    return QStringLiteral(
        "Profile (*.json *.xml *.JSON *.XML);;Alle Dateien (*)");
}

} // namespace ProfilHerkunft
} // namespace Longpath
