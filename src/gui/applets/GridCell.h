#pragma once

// =================================================================
// src/gui/applets/GridCell.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original.
//
// ── Ein Feld ist ein BEHAELTER, kein Widget ──────────────────────────
//
// Festlegung des Betreibers, 2026-08-18, vor dem ersten Strich Code:
//
//   „Ein Feld im Raster ist ein Behälter, kein Widget. Es hat Ort und
//    Größe — und eine Liste von Widgets, nicht eines. Das ist die
//    Voraussetzung dafür, Stehwelle und S-Meter nebeneinander in ein
//    Fenster zu legen … Baust du das Feld erst als Einzel-Widget und
//    rüstest die Liste später nach, muss die gespeicherte Anordnung
//    zweimal wandern — und der Kennungs-Fehler von heute Abend hat
//    gezeigt, was eine Anordnungswanderung still verlieren kann."
//
// Darum steht die Liste ab dem ersten Schritt hier, obwohl Schritt 1
// nur ein Widget je Feld hineinlegt. Das Datenmodell ist fertig, bevor
// etwas Sichtbares davon abhaengt.
//
// Dasselbe Muster traegt der Baum schon einmal: ContainerWidget haelt
// ein MeterWidget, das MeterWidget haelt N MeterItems und zeichnet sie
// in EINEM Durchgang. Ein Behaelter, viele Inhalte.
//
// ── Was hier NICHT steht ─────────────────────────────────────────────
//
// Keine Zeiger, keine Widgets, kein Qt-Elternteil. GridCell ist reine
// Anordnung und laesst sich darum ohne Fenster pruefen und ohne
// Umschweife ins Profil schreiben. Wer das Feld auf dem Schirm braucht,
// nimmt GridCellWidget.
//
// ── Die Kennung ──────────────────────────────────────────────────────
//
// Jedes Feld traegt eine eigene, stabile Id. Sie ist NICHT die
// Panelkennung eines Applets darin — ein Feld kann mehrere tragen und
// im Lauf seines Lebens andere. Ein Feld, das sich ueber den Inhalt
// identifiziert, verliert seine Stelle, sobald der Inhalt wechselt.
// (Siehe AppletKeys.h fuer die Kennungssorten der Applets selbst.)
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-18 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include <QString>
#include <QStringList>
#include <QVariantMap>

namespace NereusSDR {

struct GridCell {
    /// Stabile Kennung des FELDES. Bleibt, wenn der Inhalt wechselt.
    QString id;

    /// Ort im Raster, nullbasiert.
    int row{0};
    int col{0};

    /// Spannweite. Der Panadapter belegt im Zielbild alle Spalten.
    int rowSpan{1};
    int colSpan{1};

    /// Die Panelkennungen der Widgets IN diesem Feld, in Anzeigefolge.
    /// Eine Liste, nicht ein Wert — siehe den Kopf dieser Datei.
    QStringList applets;

    /// Ueberschrift des Feldes. Leer heisst „nimm den Titel des einen
    /// Inhalts"; bei mehreren braucht das Feld einen eigenen Namen,
    /// weil sonst zwei Titel um dieselbe Zeile streiten.
    QString title;

    /// Gegen Verschieben und Schliessen gesperrt (Schloss in der
    /// Kopfleiste des Zielbilds). Traegt heute nichts, steht aber im
    /// Datenmodell, damit es nicht spaeter nachwandern muss.
    bool locked{false};

    bool isValid() const { return !id.isEmpty() && row >= 0 && col >= 0
                                  && rowSpan >= 1 && colSpan >= 1; }

    /// Fuers Profil. Bewusst flach und benannt statt einer Positions-
    /// liste: eine Aufnahme von morgen soll lesbar bleiben, wenn ein
    /// Feld dazukommt.
    QVariantMap toVariant() const;
    static GridCell fromVariant(const QString& id, const QVariantMap& m);
};

} // namespace NereusSDR
