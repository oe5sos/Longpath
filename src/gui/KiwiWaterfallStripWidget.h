// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe docs/architecture/
// 2026-08-27-kiwisdr-self-report-concept.md, Variante C.
//
//   2026-08-27 — Neu angelegt.

#pragma once

#include <QImage>
#include <QString>
#include <QVector>
#include <QWidget>

class QLabel;

namespace Longpath {

// Ein einzelner, schmaler Wasserfallstreifen fuer EIN KiwiSDR-Profil im
// KIWI-WASSERFAELLE-Panel. Bewusst kein SpectrumWidget: kein Zoom, kein
// Cursor, keine Bandplan-Ueberlagerung — nur "reicht mein Signal
// ueberhaupt hin", auf einen Blick.
class KiwiWaterfallStripWidget : public QWidget {
    Q_OBJECT

public:
    explicit KiwiWaterfallStripWidget(const QString& profileId,
                                       const QString& displayName,
                                       QWidget* parent = nullptr);

    QString profileId() const { return m_profileId; }
    void setDisplayName(const QString& name);
    void pushRow(const QVector<float>& binsDbm);
    void reset();

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    static constexpr int kHistoryRows = 48;
    // Feste Anzeigespanne: dieser Streifen hat keine eigene
    // Auto-Scale-Regelung (die haengt am Profil, nicht an der
    // Vorschau). Grosszuegig genug fuer Rauschen bis zu einem starken
    // lokalen Signal — bei Bedarf von Hand nachjustieren, sobald ein
    // echter Kiwi das zeigt.
    static constexpr float kMinDbm = -130.0f;
    static constexpr float kMaxDbm = -40.0f;

    QString m_profileId;
    QLabel* m_nameLabel{nullptr};
    QLabel* m_peakLabel{nullptr};
    QImage m_history;
};

} // namespace Longpath
