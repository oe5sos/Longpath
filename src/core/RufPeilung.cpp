#include "core/RufPeilung.h"

#include "core/BeamHeading.h"
#include "core/CtyDatParser.h"
#include "core/Maidenhead.h"

namespace Longpath {
namespace RufPeilung {

bool istLocator(const QString& text)
{
    return isValidGridSquare(text.trimmed());
}

Ergebnis rechne(const CtyDatParser& cty, const QString& rufzeichen,
                const QString& eigenerLocator)
{
    Ergebnis e;
    const QString ruf = rufzeichen.trimmed().toUpper();
    if (ruf.isEmpty()) { e.grund = QStringLiteral("rufzeichen fehlt"); return e; }

    const QString mein = eigenerLocator.trimmed().toUpper();
    if (!isValidGridSquare(mein)) {
        // Ohne eigenen Standort gibt es keine Richtung -- und eine Peilung
        // "von irgendwo" waere eine Zahl, die nach etwas aussieht.
        e.grund = QStringLiteral("eigener locator fehlt");
        return e;
    }

    if (istLocator(ruf)) {
        // Der genaue Ort -- kein Land, keine Schaetzung.
        e.kurzGrad   = BeamHeading::wrap360(calculateBearingInDegrees(mein, ruf));
        e.langGrad   = BeamHeading::longPath(e.kurzGrad);
        e.km         = calculateDistanceKm(mein, ruf);
        e.ausLocator = true;
        e.ok         = true;
        return e;
    }

    const QString prefix = cty.resolvePrimaryPrefix(ruf);
    const DxccEntity* ent = prefix.isEmpty() ? nullptr : cty.entityByPrefix(prefix);
    if (!ent) { e.grund = QStringLiteral("unbekanntes prefix"); return e; }
    e.land = ent->name;
    if (!ent->hasLatLon) { e.grund = QStringLiteral("keine koordinaten"); return e; }

    // Wie RotorLogbookPanel::onCallsignEdited: die Mitte der Einheit als
    // Locator, dann dieselben beiden Funktionen.
    const QString ziel = gridSquareFromLatLon(ent->latitude, ent->longitude);
    e.kurzGrad = BeamHeading::wrap360(calculateBearingInDegrees(mein, ziel));
    e.langGrad = BeamHeading::longPath(e.kurzGrad);
    e.km       = calculateDistanceKm(mein, ziel);
    e.ok       = true;
    return e;
}

}  // namespace RufPeilung
}  // namespace Longpath
