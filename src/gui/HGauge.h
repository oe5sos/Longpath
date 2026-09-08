// src/gui/HGauge.h
#pragma once
#include <QWidget>

class QPainter;

namespace Longpath {

class HGauge : public QWidget {
    Q_OBJECT
public:
    explicit HGauge(QWidget* parent = nullptr);

    void setRange(double min, double max);
    void setYellowStart(double val);
    void setRedStart(double val);
    void setReversed(bool rev);
    void setTitle(const QString& t);
    void setUnit(const QString& u);
    void setValue(double val);
    void setPeakValue(double val);
    void setTickLabels(const QStringList& labels);

    // ── Ablesbare Bauform (2026-09-02) ──────────────────────────────
    //
    // Standard bleibt der Streifen mit der Beschriftung IN der Mulde.
    // Im TX-Feld steht die Beschriftung LINKS und der Messwert als
    // ZAHL rechts daneben: eine Fuellhoehe liest man im Vorbeigehen
    // nicht, eine Zahl schon. Nur die TxApplet schaltet das ein — die
    // uebrigen sechs Anwender (PhoneCwApplet, VaxApplet, TunerApplet,
    // AudioTxInputPage …) sollen unveraendert aussehen.
    //
    // `nachkommastellen` und `einheit` bestimmen die Zahl; solange der
    // Wert auf dem Skalenanfang steht (kein Vorlauf, SWR 1,0), zeigt
    // das Feld einen Gedankenstrich statt einer Scheingenauigkeit.
    void setReadout(bool on, int nachkommastellen = 1,
                    const QString& einheit = QString());
    void setLabelWidth(int px);
    bool hasReadout() const noexcept { return m_readout; }

    // Read-only accessor for unit tests + state inspection.  No
    // corresponding signal — HGauge is a passive read-only widget driven
    // only by its setValue setter.  Phase 3M-4 Task 13 (added for
    // PureSignalApplet wiring tests).
    double value() const noexcept { return m_value; }

    // Skala und Rotbeginn, ebenfalls nur lesend. Dazugekommen
    // 2026-08-18: die Leistungsanzeige der TxApplet springt jetzt auf
    // 2 kW, wenn ein aeusserer Verstaerker arbeitet, und ohne diese
    // beiden koennte kein Test unterscheiden, ob sie es wirklich tut
    // oder nur nicht abstuerzt.
    double maximum() const noexcept { return m_max; }
    double minimum() const noexcept { return m_min; }
    double redStart() const noexcept { return m_redStart; }

    QSize sizeHint() const override { return {200, 30}; }
    QSize minimumSizeHint() const override { return {100, 26}; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paintReadout(QPainter& p);

protected:

private:
    double m_min = 0.0;
    double m_max = 100.0;
    double m_value = 0.0;
    double m_peak = -999.0;
    double m_yellowStart = 80.0;
    double m_redStart = 90.0;
    bool   m_reversed = false;
    QString m_title;
    QString m_unit;
    QStringList m_tickLabels;
    bool    m_readout = false;
    int     m_readoutDecimals = 1;
    QString m_readoutUnit;
    int     m_labelWidth = 52;
};

} // namespace Longpath
