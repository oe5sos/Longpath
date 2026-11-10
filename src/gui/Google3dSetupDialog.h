// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/gui/Google3dSetupDialog.h  (Longpath)
// =================================================================
// Longpath-original. No Thetis port.
//
// Wo der Bediener seinen eigenen Google-Schluessel eintraegt: Feld,
// „Test" gegen die Map Tiles API, und eine Anleitung Schritt fuer
// Schritt, die jeder versteht -- Projekt anlegen, Rechnungskonto (das
// Google auch fuer das Freikontingent verlangt), Map Tiles API
// einschalten, Schluessel anlegen und auf diese eine API beschraenken.
//
// Ohne Schluessel bleibt alles, wie es war: NASA-Bilder auf der Karte,
// Google Earth im Browser auf Knopfdruck. Der Dialog sagt das, damit
// niemand glaubt, er MUESSE hier etwas eintragen.
//
// Nicht modal und auf dem Haufen (tst_kein_kind_auf_dem_stapel): ein
// Bediener soll nebenher in der Google-Konsole klicken koennen.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================
#pragma once

#include "core/GoogleMapTiles.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;

namespace Longpath {

class Google3dSetupDialog : public QDialog {
    Q_OBJECT
public:
    explicit Google3dSetupDialog(QWidget* parent = nullptr);

    /// Oeffnet den Dialog (oder holt den offenen nach vorne). Gehoert
    /// sich selbst: schliesst er, gibt er sich frei.
    static Google3dSetupDialog* showFor(QWidget* parent);

    /// Die Anleitung als Rich Text -- eigene Funktion, damit der
    /// Pruefstand den Inhalt lesen kann, ohne einen Dialog zu bauen.
    static QString guideHtml();

    QLineEdit*   keyEditForTest() const { return m_keyEdit; }
    QPushButton* testButtonForTest() const { return m_testBtn; }
    QPushButton* saveButtonForTest() const { return m_saveBtn; }
    QPushButton* removeButtonForTest() const { return m_removeBtn; }
    QLabel*      statusForTest() const { return m_status; }
    GoogleTilesProbe* probeForTest() const { return m_probe; }

private:
    void runTest();
    void onProbeFinished(GoogleTilesProbe::Verdict v, const QString& message);
    void save();
    void removeKey();
    void setStatus(const QString& text, bool good);

    QLineEdit*   m_keyEdit{nullptr};
    QPushButton* m_testBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_removeBtn{nullptr};
    QLabel*      m_status{nullptr};
    GoogleTilesProbe* m_probe{nullptr};
};

} // namespace Longpath
