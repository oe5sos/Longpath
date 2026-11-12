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
//
// Zwei Quellen (2026-11-10, abends -- Betreiber: „ich habe keine
// kreditkarte … und geld will ich auch keines zahlen"): oben die Wahl,
// darunter Anleitung, Feld und Test fuer GENAU diese Quelle. Vorgewaehlt
// ist Cesium ion, weil es ohne Rechnungskonto geht.
// =================================================================
// Modification history (Longpath):
//   2026-11-10 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
//   2026-11-10 — Quelle waehlbar: Cesium ion (kostenlos) oder Google.
// =================================================================
#pragma once

#include "core/GoogleMapTiles.h"

#include <QDialog>

class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;

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
    static QString guideHtml(Map3dAccess::Source s);

    /// Die Quelle, die der Dialog gerade zeigt.
    Map3dAccess::Source shownSource() const { return m_source; }
    QRadioButton* ionChoiceForTest() const { return m_ionChoice; }
    QRadioButton* googleChoiceForTest() const { return m_googleChoice; }
    QLabel*       guideForTest() const { return m_guide; }

    QLineEdit*   keyEditForTest() const { return m_keyEdit; }
    QPushButton* testButtonForTest() const { return m_testBtn; }
    QPushButton* saveButtonForTest() const { return m_saveBtn; }
    QPushButton* removeButtonForTest() const { return m_removeBtn; }
    QLabel*      statusForTest() const { return m_status; }
    GoogleTilesProbe* probeForTest() const { return m_probe; }

private:
    void showSource(Map3dAccess::Source s);
    void runTest();
    void onProbeFinished(GoogleTilesProbe::Verdict v, const QString& message);
    void save();
    void removeKey();
    void setStatus(const QString& text, bool good);

    Map3dAccess::Source m_source{Map3dAccess::Source::CesiumIon};
    QRadioButton* m_ionChoice{nullptr};
    QRadioButton* m_googleChoice{nullptr};
    QLabel*      m_guide{nullptr};
    QLabel*      m_keyCaption{nullptr};
    QLabel*      m_costNote{nullptr};
    QLineEdit*   m_keyEdit{nullptr};
    QPushButton* m_testBtn{nullptr};
    QPushButton* m_saveBtn{nullptr};
    QPushButton* m_removeBtn{nullptr};
    QLabel*      m_status{nullptr};
    GoogleTilesProbe* m_probe{nullptr};
};

} // namespace Longpath
