#pragma once
// =================================================================
// src/gui/applets/RecordingApplet.h  (Longpath)
// =================================================================
//
// Die Flaeche fuer Phase 3M: Tonaufnahme, I/Q-Aufnahme, Zeitplan.
//
// WARUM ES SIE ERST JETZT GIBT. Die vier Klassen dahinter sind seit
// August gebaut und von sechs Prueflaeufen gedeckt -- aber bis zum
// 2026-10-21 konstruierte sie NIEMAND. Der Plan hatte die Flaeche
// ausdruecklich dem Betreiber ueberlassen (§0.4, „Technik Nereus,
// Design ich"); gewaehlt hat er sie am 2026-10-21 aus drei
// Entwurfsblaettern: Variante 1, ein eigenes Applet in der rechten
// Spalte.
//
// WAS HIER NICHT STEHT: wie aufgenommen wird. Diese Klasse ruft
// RecordingModel und zeigt dessen Eigenschaften an. Jede Entscheidung
// darueber, ob eine Aufnahme laufen DARF -- Verbindung, Abtastrate,
// I/Q-Strom -- faellt dort und wird hier nur gelesen. Zwei Orte mit
// derselben Regel laufen auseinander.
//
// Modification history (Longpath):
//   2026-10-21 — Original fuer Longpath von Martin Fischer,
//                geschrieben mit Claude Opus 5 (Anthropic).
// =================================================================

#include "AppletWidget.h"

#include <QPointer>

class QLabel;
class QPushButton;

namespace Longpath {

class RecordingApplet : public AppletWidget
{
    Q_OBJECT

public:
    explicit RecordingApplet(RadioModel* model, QWidget* parent = nullptr);

    // EINE Schreibweise. Dieselbe steht in m_appletsById, im
    // Sichtbarkeitsregler und in der Beschreibung -- check-applet-ids.py
    // schlaegt an, sobald eine zweite entsteht.
    QString appletId()    const override { return QStringLiteral("Rec"); }
    QString appletTitle() const override { return QStringLiteral("Recording"); }
    void    syncFromModel() override;

    /// Das Zahnrad im Panelkopf. Oeffnet das Zeitplan-Fenster -- dort
    /// werden Eintraege angelegt, hier wird nur ihr Zustand gezeigt.
    /// Kein Popup: darin wird getippt und eine Liste gelesen, das
    /// ueberlebt kein `Qt::Popup`, das beim Klick daneben zugeht.
    void    openExtendedSettings() override;

private:
    void buildUI();
    void refreshState();
    void onAudioClicked();
    void onIqClicked();

    QPushButton* m_audioBtn{nullptr};
    QLabel*      m_audioClock{nullptr};
    QLabel*      m_audioGap{nullptr};

    QPushButton* m_iqBtn{nullptr};
    QLabel*      m_iqClock{nullptr};
    QLabel*      m_iqNote{nullptr};

    QPointer<class RecordingScheduleWindow> m_planFenster;

    QLabel*      m_planText{nullptr};
    QLabel*      m_planState{nullptr};
    QLabel*      m_error{nullptr};
};

}  // namespace Longpath
