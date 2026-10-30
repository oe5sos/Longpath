// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/applets/AppletFensterMasse.cpp  (Longpath)
// =================================================================
// Siehe AppletFensterMasse.h fuer den Grund, warum das neben dem
// Layoutprofil steht und nicht darin.
// =================================================================

#include "AppletFensterMasse.h"

#include "core/AppSettings.h"

namespace Longpath::AppletFensterMasse {

namespace {

QString schluessel(const QString& panelId, const char* teil)
{
    return QStringLiteral("AppletFenster/%1/%2")
        .arg(panelId, QString::fromLatin1(teil));
}

}  // namespace

QSize gemerkt(const QString& panelId)
{
    if (panelId.isEmpty()) { return {}; }
    auto& s = AppSettings::instance();
    bool okB = false;
    bool okH = false;
    const int b = s.value(schluessel(panelId, "w"), QStringLiteral("0"))
                      .toString().toInt(&okB);
    const int h = s.value(schluessel(panelId, "h"), QStringLiteral("0"))
                      .toString().toInt(&okH);
    if (!okB || !okH || b < kMinBreite || h < kMinHoehe) { return {}; }
    return QSize(b, h);
}

void merke(const QString& panelId, const QSize& groesse)
{
    if (panelId.isEmpty() || !groesse.isValid())        { return; }
    // Die Untergrenze ist kein Geschmack: beim Andocken faellt das
    // Fenster kurz auf fast nichts zusammen, und `resizeEvent` feuert
    // dabei. Ohne diese Zeile merkte es sich genau diesen Augenblick
    // und ging beim naechsten Abloesen als Briefmarke auf.
    if (groesse.width() < kMinBreite || groesse.height() < kMinHoehe) {
        return;
    }
    auto& s = AppSettings::instance();
    s.setValue(schluessel(panelId, "w"), QString::number(groesse.width()));
    s.setValue(schluessel(panelId, "h"), QString::number(groesse.height()));
}

void vergiss(const QString& panelId)
{
    if (panelId.isEmpty()) { return; }
    auto& s = AppSettings::instance();
    s.setValue(schluessel(panelId, "w"), QStringLiteral("0"));
    s.setValue(schluessel(panelId, "h"), QStringLiteral("0"));
}

}  // namespace Longpath::AppletFensterMasse
