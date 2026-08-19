#pragma once

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
// Die Uhr zaehlt hoch MIT ZIEL („04:17 von 30:00"), damit der Deckel
// sichtbar ist, bevor er zuschlaegt.
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

namespace NereusSDR {

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
    QLabel*      m_capLabel{nullptr};
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

} // namespace NereusSDR
