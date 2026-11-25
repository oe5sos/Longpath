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
class QToolButton;

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
    void pushRow(const QVector<float>& binsDbm, double lowHz = 0.0, double highHz = 0.0);
    // Frequenzachse der letzten Zeile (0/0: unbekannt, keine Achse).
    double lowHz() const { return m_lowHz; }
    double highHz() const { return m_highHz; }
    QString frequencyText() const;
    void reset();

    QSize sizeHint() const override;

    // Pegelbereich der Anzeige. Auto: folgt Rauschen und Spitze der Zeile.
    void setRange(float minDbm, float maxDbm);
    void setAutoRange(bool on);
    bool autoRange() const { return m_auto; }
    float minDbm() const { return m_minDbm; }
    float maxDbm() const { return m_maxDbm; }
    QToolButton* closeButtonForTest() const { return m_closeButton; }

signals:
    // Der Streifen entscheidet nichts selbst: er meldet den Wunsch.
    void closeRequested(const QString& profileId);
    void renameRequested(const QString& profileId);

protected:
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;

private:
    static constexpr int kHistoryRows = 48;
    // Feste Anzeigespanne: dieser Streifen hat keine eigene
    // Auto-Scale-Regelung (die haengt am Profil, nicht an der
    // Vorschau). Grosszuegig genug fuer Rauschen bis zu einem starken
    // lokalen Signal — bei Bedarf von Hand nachjustieren, sobald ein
    // echter Kiwi das zeigt.
    static constexpr float kDefaultMinDbm = -130.0f;
    static constexpr float kDefaultMaxDbm = -40.0f;
    float m_minDbm{kDefaultMinDbm};
    float m_maxDbm{kDefaultMaxDbm};
    bool m_auto{false};
    double m_lowHz{0.0};
    double m_highHz{0.0};
    QLabel* m_freqLabel{nullptr};
    QToolButton* m_closeButton{nullptr};

    QString m_profileId;
    QLabel* m_nameLabel{nullptr};
    QLabel* m_peakLabel{nullptr};
    QImage m_history;
};

} // namespace Longpath
