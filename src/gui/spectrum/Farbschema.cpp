// no-port-check: Longpath-original, keine Thetis-Logik.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Siehe Farbschema.h fuer die Begruendung.

#include "gui/spectrum/Farbschema.h"

#include <algorithm>

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

namespace Longpath::Farbschema {

namespace {
// Kanten deckend genug, um auf jeder Spur zu stehen.
QColor kante(const char* hex)
{
    QColor c(hex);
    c.setAlpha(190);
    return c;
}
}  // namespace

// Am echten Bild des Betreibers entworfen (Blaetter S1-S5).
Durchlass durchlassFuer(PanColourScheme s)
{
    switch (s) {
    case PanColourScheme::Brass:
        return {true, QColor(Style::kPassbandBrass), 0.20f, 0.03f, kante(Style::kPassbandBrassEdge)};
    case PanColourScheme::SlateBlue:
        return {true, QColor(Style::kPassbandSlate), 0.32f, 0.08f, kante(Style::kPassbandSlateEdge)};
    case PanColourScheme::Night:
        return {true, QColor(Style::kPassbandNight), 0.20f, 0.04f, kante(Style::kPassbandNightEdge)};
    case PanColourScheme::Ft8Amber:
        return {true, QColor(Style::kPassbandFt8Amber), 0.48f, 0.14f, kante(Style::kPassbandFt8Edge)};
    case PanColourScheme::Teal:
        return {true, QColor(Style::kPassbandTeal), 0.36f, 0.10f, kante(Style::kPassbandTealEdge)};
    case PanColourScheme::Standard:
    case PanColourScheme::Count:
        break;
    }
    return {};
}

namespace {
const QString kBandbreitenSchluessel = QStringLiteral("BandwidthPassbandScheme");
}

Bote::Bote()
{
    bool ok = false;
    const int w = AppSettings::instance()
                      .value(kBandbreitenSchluessel, kFolgtPanadapter).toInt(&ok);
    if (ok && w >= kFolgtPanadapter && w < static_cast<int>(PanColourScheme::Count)) {
        m_bandbreitenWahl = w;
    }
}

Bote& Bote::instance()
{
    static Bote bote;
    return bote;
}

void Bote::setDurchlass(PanColourScheme s)
{
    if (m_durchlass == s) { return; }
    const PanColourScheme vorher = bandbreite();
    m_durchlass = s;
    emit durchlassGeaendert(s);
    if (bandbreite() != vorher) { emit bandbreiteGeaendert(bandbreite()); }
}

void Bote::setBandbreitenWahl(int wahl)
{
    wahl = std::clamp(wahl, kFolgtPanadapter, static_cast<int>(PanColourScheme::Count) - 1);
    if (m_bandbreitenWahl == wahl) { return; }
    const PanColourScheme vorher = bandbreite();
    m_bandbreitenWahl = wahl;
    AppSettings::instance().setValue(kBandbreitenSchluessel, QString::number(wahl));
    if (bandbreite() != vorher) { emit bandbreiteGeaendert(bandbreite()); }
}

PanColourScheme Bote::bandbreite() const
{
    return m_bandbreitenWahl == kFolgtPanadapter
        ? m_durchlass
        : static_cast<PanColourScheme>(m_bandbreitenWahl);
}

}  // namespace Longpath::Farbschema
