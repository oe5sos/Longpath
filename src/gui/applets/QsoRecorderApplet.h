#pragma once

// Ported from Thetis sources:
//   Project Files/Source/Console/clsAudioRecordPlayback.cs, original licence
//   from Thetis source is included below
//
// Thetis v2.10.3.15 (@852bf0e). Diese Datei ist NereusSDR/Longpath-original
// und uebernimmt KEINEN C#-Code; sie leitet Verhalten und Feldauswahl aus
// der oben genannten Quelle ab. Der Kopf steht hier trotzdem vollstaendig,
// weil die Herkunftstabelle sie fuehrt und weil eine Nennung mehr niemandem
// schadet — eine fehlende dagegen schon.

/*  clsAudioRecordPlayback.cs

This file is part of a program that implements a Software-Defined Radio.

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2020-2026 Richard Samphire MW0LGE

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

The author can be reached by email at

mw0lge@grange-lane.co.uk
*/
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//


// =================================================================
// src/gui/applets/QsoRecorderApplet.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original, Entwurf 3 vom 2026-08-19.
//
// Die QSO-Aufnahme, sichtbar.
//
// ── Was das Fenster beantworten muss ────────────────────────────────
//
// Eine einzige Frage: laeuft es, und kommt auf BEIDEN Spuren etwas an?
// Deshalb zwei gleich grosse Felder nebeneinander — links die
// Gegenstation, rechts die eigene Stimme — statt einer gemeinsamen
// Anzeige, in der man Stille nicht von „Mikrofon aus" unterscheiden
// kann. Das ist der ganze Grund fuer die Aufteilung.
//
// Die Uhr zaehlt einfach hoch — seit 2026-09-02 ohne Ziel, weil die
// Aufnahme zeitlich unbegrenzt laeuft (siehe QsoRecorder.h). Der
// Deckel, den es vorher daneben zu sehen gab („04:17 von 30:00"), ist
// mit ihm verschwunden; was jetzt schuetzt, ist eine Speicherplatz-
// Wache im Hintergrund (QsoRecorderController), keine Uhr zum Anschauen.
//
// Frequenz, Modus und Band stehen im Kopf: dieselben Angaben, die in
// die Beschreibungsdatei wandern. Was gespeichert wird, soll man sehen.
//
// ── Der Pegel kommt aus dem Zwischenspeicher, nicht aus dem Ton ─────
//
// Beide Balken zeigen den Spitzenwert dessen, was der Recorder im
// letzten Abholvorgang bekommen hat. Nicht schoen (der Ton selbst
// laeuft am Hauptfaden vorbei), aber ehrlich: der Balken bewegt sich
// genau dann, wenn wirklich etwas in der Aufnahme landet. Ein Balken,
// der aus einer anderen Quelle kommt als die Aufnahme, kann zappeln,
// waehrend die Datei still bleibt.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "AppletWidget.h"

#include "core/audio/WavPlayer.h"

class QLabel;
class QPushButton;
class QListWidget;

namespace Longpath {

class QsoRecorderApplet : public AppletWidget {
    Q_OBJECT
public:
    explicit QsoRecorderApplet(RadioModel* model, QWidget* parent = nullptr);

    QString appletId()    const override { return QStringLiteral("qsorec"); }
    QString appletTitle() const override { return QStringLiteral("QSO Recorder"); }
    void    syncFromModel() override;

private:
    void buildUI();
    void refreshState();
    void refreshList();
    void onRecordClicked();

    // Wohin die Aufnahmen gehen. Neben die Einstellungsdatei, wie beim
    // Sprachspeicher — ein Ordner, den der Benutzer wiederfindet.
    QString recordingFolder() const;

    QPushButton* m_recBtn{nullptr};
    QLabel*      m_clock{nullptr};
    QLabel*      m_headInfo{nullptr};
    QLabel*      m_lossLabel{nullptr};

    // Links Gegenstation, rechts eigene Stimme.
    QLabel*      m_rxPeak{nullptr};
    QLabel*      m_txPeak{nullptr};
    QWidget*     m_rxBar{nullptr};
    QWidget*     m_txBar{nullptr};
    QWidget*     m_rxFill{nullptr};
    QWidget*     m_txFill{nullptr};

    QListWidget* m_list{nullptr};

    // Nachhoeren ueber die Lautsprecher. Sendet nichts.
    WavPlayer    m_player;

    int m_lastRxFrames{0};
    int m_lastTxFrames{0};
};

} // namespace Longpath
