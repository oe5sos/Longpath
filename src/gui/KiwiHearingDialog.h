// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. "Wo werde ich gehoert" (Entwurf B + C,
// 2026-11-25): bis zu fuenf Kiwis, die beim Senden mithoeren -- als
// Kacheln mit kleinem Wasserfall, auf der Karte und in einer Tabelle.
//
// Der Dialog zeigt nur, was ihm gegeben wird, und meldet Wuensche
// (Hinzufuegen, Entfernen, Aufnehmen). Er hat weder Netz noch Zugriff auf
// Scheiben oder Funkgeraet -- das macht MainWindow_KiwiGroup.cpp.

#pragma once

#include "core/KiwiOwnSignal.h"
#include "gui/widgets/KiwiWorldMap.h"

#include <QDialog>
#include <QHash>
#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

class QFrame;
class QHBoxLayout;
class QLabel;
class QPushButton;
class QTableWidget;

namespace Longpath {

struct KiwiHearingRow {
    QString id;
    QString name;
    double km{-1.0};
    double lat{0.0};
    double lon{0.0};
    bool hasPosition{false};
    QString stateText;                 // "Connected", "Connecting…" ...
    bool connected{false};
    KiwiHearMeMeter::Result result;
    bool primary{false};               // der Kiwi von "Mein Signal"
    bool recording{false};
};

// Ein kleiner Wasserfall: neueste Zeile oben, Marke auf der Sendefrequenz.
class KiwiMiniWaterfall : public QWidget {
    Q_OBJECT
public:
    explicit KiwiMiniWaterfall(QWidget* parent = nullptr);
    static constexpr int kRows = 60;
    void pushRow(const QVector<float>& binsDbm, double lowHz, double highHz);
    void clearRows();
    void setMarkHz(double hz);
    int rowCount() const { return m_rows.size(); }
    QImage image(int width = 800) const;
    QSize sizeHint() const override { return QSize(180, 84); }
    QSize minimumSizeHint() const override { return QSize(120, 60); }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    struct Row { QVector<float> bins; double lowHz{0}; double highHz{0}; };
    void render(QImage& img) const;
    QVector<Row> m_rows;
    double m_markHz{0.0};
};

class KiwiHearingDialog : public QDialog {
    Q_OBJECT
public:
    explicit KiwiHearingDialog(QWidget* parent = nullptr);

    void setRows(const QVector<KiwiHearingRow>& rows);
    void pushWaterfallRow(const QString& id, const QVector<float>& binsDbm,
                          double lowHz, double highHz);
    void clearWaterfall(const QString& id);
    void setMarkHz(double hz);
    void setHome(double lat, double lon, bool valid, const QString& label);
    void setTransmitting(bool on);
    // Aufnahme-Bild der Kachel (fuer die PNG neben der WAV).
    QImage waterfallImage(const QString& id) const;
    // Freier Platz fuer weitere Kiwis (fuer den "+"-Knopf).
    void setSlotsFree(int free);

    // Fuer die Pruefung.
    int tileCount() const { return m_tiles.size(); }
    QString tileVerdictText(const QString& id) const;
    QString summaryText() const;
    KiwiWorldMap* mapForTest() const { return m_map; }
    QTableWidget* tableForTest() const { return m_table; }
    QPushButton* addButtonForTest() const { return m_add; }
    QPushButton* recordButtonForTest(const QString& id) const;
    QPushButton* removeButtonForTest(const QString& id) const;
    int miniRowsForTest(const QString& id) const;

    static QString verdictText(const KiwiHearMeMeter::Result& r);

signals:
    void addRequested();
    void removeRequested(const QString& id);
    void recordToggled(const QString& id, bool on);

private:
    struct Tile {
        QFrame* frame{nullptr};
        QLabel* name{nullptr};
        QLabel* sub{nullptr};
        KiwiMiniWaterfall* wf{nullptr};
        QLabel* verdict{nullptr};
        QPushButton* record{nullptr};
        QPushButton* remove{nullptr};
    };
    void rebuildMapAndTable(const QVector<KiwiHearingRow>& rows);
    void updateSummary(const QVector<KiwiHearingRow>& rows);

    QLabel* m_summary{nullptr};
    QPushButton* m_add{nullptr};
    QPushButton* m_zoom{nullptr};
    KiwiWorldMap* m_map{nullptr};
    QHBoxLayout* m_tileLayout{nullptr};
    QTableWidget* m_table{nullptr};
    QHash<QString, Tile> m_tiles;
    QVector<QString> m_order;
    double m_markHz{0.0};
    bool m_transmitting{false};
    int m_slotsFree{0};
};

} // namespace Longpath
