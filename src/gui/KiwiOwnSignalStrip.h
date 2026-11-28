// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Entwurf C „Mein Signal am Kiwi“ — der
// Kiwi-Wasserfallstreifen direkt unter dem eigenen Panadapter
// (~/Longpath/entwuerfe-kiwi-2026-11-24/C-streifen-hauptfenster.png).
//
//   2026-11-24 — Neu angelegt.
//
// Dieselbe Frequenzachse wie der Panadapter darueber: setView() bekommt
// Mitte und Breite des Panadapters, jede Kiwi-Zeile wird mit ihrem
// EIGENEN Bereich (low/high) auf diese Achse abgebildet. Der Kiwi
// liefert Zoomstufen in Zweierpotenzen und trifft die verlangte Breite
// nur ungefaehr -- der Streifen verlaesst sich darum nicht darauf, dass
// beide Achsen gleich sind, sondern rechnet es je Zeile aus. Wo der Kiwi
// nichts zeigt (Bild schmaler als die Achse), bleibt die Flaeche leer.
//
// Hausstil: Grau auf Fast-Schwarz. Farbe nur an drei Stellen -- der
// schmale Balken links in der Kopfzeile (das Warm der Kurve), die Marke
// auf der Sendefrequenz (#a86b6d) und die Status-Aussage rechts (gruen
// bestaetigt, bernstein Warnung).

#pragma once

#include "core/KiwiOwnSignal.h"

#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

class QMenu;
class QToolButton;

namespace Longpath {

class KiwiLostOverlay;

class KiwiOwnSignalStrip : public QWidget {
    Q_OBJECT

public:
    explicit KiwiOwnSignalStrip(QWidget* parent = nullptr);

    // Zeilen, die der Streifen vorhaelt (neueste oben).
    static constexpr int kRows = 120;

    void setKiwiName(const QString& name);
    // km < 0: Entfernung unbekannt, wird nicht gezeigt.
    void setDistanceKm(double km);
    // measured=false: angenommener Verzug, wird mit "ca." gezeigt.
    void setDelay(int ms, bool measured);

    // Achse des Panadapters darueber.
    void setView(double centerHz, double bandwidthHz);
    // Sendefrequenz; <= 0: keine Marke.
    void setMarkHz(double hz);
    // Seitenraender der Achse, damit sie unter der des Panadapters
    // fluchtet (dBm-Skala, Rahmen).
    void setPlotMargins(int left, int right);

    void pushRow(const QVector<float>& binsDbm, double lowHz, double highHz);
    // Mindestabstand zwischen Zeilen in ms (0: jede Zeile). Dasselbe Tempo
    // wie im Fenster KIWI WASSERFAELLE ("KiwiWaterfall/RowIntervalMs").
    void setRowIntervalMs(int ms) { m_rowIntervalMs = ms; }
    void clearRows();

    // S-Wert des Kiwi aus dem Tonstrom; overload: sein Wandler uebersteuert
    // (bleibt kurz stehen). Leerer Text: kein Wert.
    void setSignal(const QString& sUnits, bool overload);
    void setVerdict(const KiwiHearMeMeter::Result& result);
    // Fusszeile: laeuft eine Aufnahme, mit Dauer und Dateiname.
    void setRecording(bool recording, qint64 elapsedMs, const QString& fileName);

    // Regler im Kopf des Streifens (Betreiber 2026-11-25: "unter dem
    // Panadapter ist der Kiwi schoen, aber dort kann ich nichts aendern").
    // Ton ein/aus und Zoom +/- fuer den Kiwi von "Mein Signal".
    void setSoundActive(bool on);

    // ── Ansicht, Zahnrad rechts oben (Betreiber 2026-11-27: "die Grafik
    // und Geschwindigkeit des Wasserfalls aendern koennen") ────────────
    struct Options {
        int speed{-1};      // -1 wie das Fenster KIWI WASSERFAELLE,
                            // 0 langsam, 1 normal, 2 schnell
        int brightness{1};  // 0 dunkel, 1 normal, 2 hell
        int contrast{1};    // 0 weich, 1 normal, 2 kraeftig
        bool colour{false}; // Farbe statt Grau
        bool trace{false};  // Spektrumlinie ueber dem Wasserfall
        int height{0};      // 0 klein, 1 mittel, 2 gross
    };
    static constexpr int kHeights[3] = {176, 270, 380};
    const Options& options() const { return m_opt; }
    // Wendet an und merkt sich die Wahl.
    void setOptions(const Options& options);
    QMenu* buildOptionsMenu(QWidget* parent);
    bool hasOwnSpeed() const { return m_opt.speed >= 0; }
    // Zeilentakt, den der Kiwi dazu liefern soll (wf_speed; 0: Vorgabe).
    int serverRate() const;
    QToolButton* optionsButtonForTest() const { return m_gear; }
    // Wo die Spektrumlinie steht (leer, wenn ausgeschaltet).
    QRect traceRect() const;

    bool soundActiveForTest() const;
    double viewWidthHzForTest() const { return m_bandwidthHz; }
    double viewCenterHzForTest() const { return m_centerHz; }
    QToolButton* soundButtonForTest() const { return m_soundButton; }

    // Kiwi weg: Bild abgedunkelt, Grund und Knopf darueber; title leer = weg.
    void setLost(const QString& title, const QString& detail = QString());
    KiwiLostOverlay* lostOverlayForTest() const { return m_lost; }

    // Was gemalt in der Mitte steht: "wartet" bis zur ersten Zeile --
    // nicht, wenn der Ueberzug (setLost) schon sagt, was los ist.
    QString bodyText() const;

    // Das Bild des Streifens (Wasserfall und Marke, ohne Kopf/Fuss) fuer
    // die Aufnahme als PNG. Leer, wenn noch keine Zeile kam.
    QImage waterfallImage(int width = 1200) const;

    int rowCount() const { return m_rows.size(); }
    QString headerText() const;
    QString statusText() const;
    QString footerText() const;
    // Fuer die Pruefung: die Farbe, in der die Aussage steht.
    QColor statusColor() const;
    // x-Lage der Marke im Widget, -1 wenn ausserhalb der Achse.
    int markX() const;

    QSize sizeHint() const override { return QSize(480, 176); }
    QSize minimumSizeHint() const override { return QSize(160, 120); }

signals:
    void soundToggled(bool on);
    void speedChanged(int serverRate);
    void zoomRequested(int direction);   // +1 enger, -1 weiter
    void chooseOtherRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    struct Row {
        QVector<float> bins;
        double lowHz{0.0};
        double highHz{0.0};
    };

    QRect bodyRect() const;
    // Der Wasserfall: der ganze Koerper oder der Teil unter der Linie.
    QRect waterfallRect() const;
    void renderBody(QImage& img) const;
    void loadOptions();
    void paintTrace(QPainter& p, const QRect& r) const;
    double noiseFloorDbm() const;

    QVector<Row> m_rows;       // [0] = neueste
    QString m_name;
    double m_km{-1.0};
    int m_delayMs{0};
    bool m_delayMeasured{false};
    double m_centerHz{0.0};
    double m_bandwidthHz{0.0};
    double m_markHz{0.0};
    int m_marginLeft{0};
    int m_marginRight{0};
    KiwiHearMeMeter::Result m_verdict;
    bool m_recording{false};
    qint64 m_recordMs{0};
    QString m_recordFile;
    QString m_sUnits;
    qint64 m_overloadUntilMs{0};
    qint64 m_signalPaintMs{0};
    int m_rowIntervalMs{0};
    qint64 m_lastRowMs{0};
    QToolButton* m_soundButton{nullptr};
    QToolButton* m_gear{nullptr};
    Options m_opt;
    QToolButton* m_zoomOut{nullptr};
    QToolButton* m_zoomIn{nullptr};
    KiwiLostOverlay* m_lost{nullptr};
    void layoutButtons();
    int buttonsWidth() const;
};

} // namespace Longpath
