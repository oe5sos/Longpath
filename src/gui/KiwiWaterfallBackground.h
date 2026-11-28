// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port.
//
//   2026-11-28 — Neu angelegt.
//
// Hintergrund der Kiwi-Wasserfaelle (Betreiber 2026-11-28: "alles so
// schwarz" -- hellere Gruende nur beim Kiwi-Wasserfall, nur als zusaetzliche
// Wahl). Eine Einstellung fuer die Streifen im Fenster und den Streifen
// "Mein Signal". Vorgabe ist Schwarz: dann malt alles wie bisher.

#pragma once

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"

#include <QColor>
#include <QCoreApplication>
#include <QString>

namespace Longpath::KiwiBackground {

inline constexpr const char* kSettingsKey = "KiwiWaterfall/Background";

struct Choice { const char* key; const char* text; };
// text: Uebersetzung im Kontext "KiwiBackground".
inline constexpr Choice kChoices[] = {
    {"Black",      QT_TRANSLATE_NOOP("KiwiBackground", "Black")},
    {"Anthracite", QT_TRANSLATE_NOOP("KiwiBackground", "Anthracite")},
    {"WarmGrey",   QT_TRANSLATE_NOOP("KiwiBackground", "Warm grey")},
    {"MediumGrey", QT_TRANSLATE_NOOP("KiwiBackground", "Medium grey")},
};

inline QString current()
{
    return AppSettings::instance()
        .value(QLatin1String(kSettingsKey), QStringLiteral("Black")).toString();
}

inline void set(const QString& key)
{
    AppSettings::instance().setValue(QLatin1String(kSettingsKey), key);
}

// Der Grund der Wahl; ungueltige Farbe bei Schwarz (und Unbekanntem).
inline QColor greyColor()
{
    const QString k = current();
    if (k == QLatin1String("Anthracite")) { return QColor(Style::kKiwiBgAnthracite); }
    if (k == QLatin1String("WarmGrey"))   { return QColor(Style::kKiwiBgWarmGrey); }
    if (k == QLatin1String("MediumGrey")) { return QColor(Style::kKiwiBgMediumGrey); }
    return QColor();
}

inline QString menuTitle()
{
    return QCoreApplication::translate(
        "KiwiBackground", QT_TRANSLATE_NOOP("KiwiBackground", "Background"));
}

inline QString label(const Choice& c)
{
    return QCoreApplication::translate("KiwiBackground", c.text);
}

} // namespace Longpath::KiwiBackground
