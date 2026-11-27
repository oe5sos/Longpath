// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen, kein Port. Siehe docs/architecture/
// 2026-08-27-kiwisdr-self-report-concept.md, Variante C.
//
//   2026-08-27 — Neu angelegt.

#include "gui/KiwiWaterfallStripWidget.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"
#include "gui/widgets/KiwiLostOverlay.h"

#include <QContextMenuEvent>
#include <QDateTime>
#include <QHBoxLayout>
#include <QMenu>
#include <QSignalBlocker>
#include <QToolButton>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Longpath {

namespace {
QString ink()   { return Style::role("text",       Style::kTextPrimary); }
QString scale() { return Style::role("text-scale", Style::kTextScale);   }
QString trace() { return Style::role("trace",      Style::kSpectrumTrace); }
QString border(){ return Style::role("border",     Style::kBorder);      }
} // namespace

KiwiWaterfallStripWidget::KiwiWaterfallStripWidget(const QString& profileId,
                                                    const QString& displayName,
                                                    QWidget* parent)
    : QWidget(parent)
    , m_profileId(profileId)
{
    setObjectName(QStringLiteral("kiwiWaterfallStrip"));
    setAttribute(Qt::WA_StyledBackground, true);
    setStyleSheet(QStringLiteral(
        "QWidget#kiwiWaterfallStrip { background: %1; "
        "border: 1px solid %2; border-radius: 6px; }")
            .arg(QString::fromLatin1(Style::kInsetBg), border()));

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(9, 7, 9, 9);
    layout->setSpacing(5);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(6);

    m_nameLabel = new QLabel(displayName, this);
    m_nameLabel->setTextFormat(Qt::PlainText);
    m_nameLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; }").arg(ink()));
    header->addWidget(m_nameLabel, 1);

    m_peakLabel = new QLabel(QStringLiteral("—"), this);
    m_peakLabel->setTextFormat(Qt::PlainText);
    m_peakLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_peakLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 10.5px; "
                        "font-family: " LP_MONO_QSS "; }").arg(trace()));
    // Auf welcher Frequenz der Kiwi steht (Mitte der Zeile) -- Betreiber
    // 2026-11-25: "man sieht oben auch nicht im kiwi, auf welcher Frequenz
    // man ist".
    m_freqLabel = new QLabel(this);
    m_freqLabel->setTextFormat(Qt::PlainText);
    m_freqLabel->setObjectName(QStringLiteral("kiwiStripFrequency"));
    m_freqLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 11px; "
                        "font-family: " LP_MONO_QSS "; }").arg(ink()));
    header->addWidget(m_freqLabel, 0);
    header->addWidget(m_peakLabel, 0);

    // Verkleinern / Vergroessern (Betreiber 2026-11-25: "im Display das Plus und
    // Minus ... zu verkleinern und vergroessern"): der Frequenzbereich des Kiwi.
    const QString zoomStyle = QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: 1px solid %2; "
        "border-radius: 6px; font-size: 13px; }"
        "QToolButton:hover { color: %3; }")
            .arg(QString::fromLatin1(Style::kTextSecondary),
                 QString::fromLatin1(Style::kBorderMuted), ink());
    m_spanLabel = new QLabel(this);
    m_spanLabel->setTextFormat(Qt::PlainText);
    m_spanLabel->setObjectName(QStringLiteral("kiwiStripSpan"));
    m_spanLabel->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 9px; "
                        "font-family: " LP_MONO_QSS "; }").arg(scale()));
    header->addWidget(m_spanLabel, 0);
    m_zoomOut = new QToolButton(this);
    m_zoomOut->setText(QStringLiteral("−"));
    m_zoomOut->setObjectName(QStringLiteral("kiwiStripZoomOut"));
    m_zoomOut->setToolTip(tr("Show a wider frequency range"));
    m_zoomOut->setAccessibleName(tr("Zoom out"));
    m_zoomOut->setFixedSize(20, 18);
    m_zoomOut->setStyleSheet(zoomStyle);
    connect(m_zoomOut, &QToolButton::clicked, this,
            [this]() { emit zoomRequested(m_profileId, -1); });
    header->addWidget(m_zoomOut, 0);
    m_zoomIn = new QToolButton(this);
    m_zoomIn->setText(QStringLiteral("+"));
    m_zoomIn->setObjectName(QStringLiteral("kiwiStripZoomIn"));
    m_zoomIn->setToolTip(tr("Show a narrower frequency range"));
    m_zoomIn->setAccessibleName(tr("Zoom in"));
    m_zoomIn->setFixedSize(20, 18);
    m_zoomIn->setStyleSheet(zoomStyle);
    connect(m_zoomIn, &QToolButton::clicked, this,
            [this]() { emit zoomRequested(m_profileId, +1); });
    header->addWidget(m_zoomIn, 0);

    // Ton ein/aus (Betreiber 2026-11-25: "jeden Kiwi im Kiwiwasserfall Ton ein
    // und ausschalten koennen"). Eingerastet = der Kiwi ist zu hoeren.
    m_soundButton = new QToolButton(this);
    m_soundButton->setText(tr("Sound"));
    m_soundButton->setCheckable(true);
    m_soundButton->setObjectName(QStringLiteral("kiwiStripSound"));
    m_soundButton->setCursor(Qt::PointingHandCursor);
    m_soundButton->setToolTip(tr("Listen to this Kiwi (audio on / off)"));
    m_soundButton->setAccessibleName(tr("Kiwi sound"));
    m_soundButton->setFixedHeight(18);
    m_soundButton->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; background: transparent; border: 1px solid %2; "
        "border-radius: 6px; padding: 0 8px; font-size: 9px; }"
        "QToolButton:hover { color: %3; }"
        "QToolButton:checked { color: %3; border-color: %4; }")
            .arg(QString::fromLatin1(Style::kTextSecondary),
                 QString::fromLatin1(Style::kBorderMuted), ink(),
                 QString::fromLatin1(Style::kBlueBorder)));
    connect(m_soundButton, &QToolButton::toggled, this,
            [this](bool on) { emit soundToggled(m_profileId, on); });
    header->addWidget(m_soundButton, 0);

    // Vergroessern: dieser Streifen fuellt das ganze Fenster (Splatter sehen).
    m_expandButton = new QToolButton(this);
    m_expandButton->setText(QStringLiteral("\u2922"));
    m_expandButton->setCheckable(true);
    m_expandButton->setAutoRaise(true);
    m_expandButton->setCursor(Qt::PointingHandCursor);
    m_expandButton->setToolTip(tr("Enlarge this waterfall to the whole window (click again to go back)"));
    m_expandButton->setAccessibleName(tr("Enlarge waterfall"));
    m_expandButton->setFixedSize(20, 18);
    m_expandButton->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; border: none; font-size: 13px; }"
        "QToolButton:hover, QToolButton:checked { color: %2; }")
            .arg(QString::fromLatin1(Style::kTextSecondary), ink()));
    connect(m_expandButton, &QToolButton::toggled, this,
            [this](bool on) { emit expandToggled(m_profileId, on); });
    header->addWidget(m_expandButton, 0);

    // Schliessen: der Streifen verschwindet, die Verbindung wird getrennt
    // (siehe KiwiSdrManager::setWaterfallPreviewEnabled).
    m_closeButton = new QToolButton(this);
    m_closeButton->setText(QStringLiteral("×"));
    m_closeButton->setAutoRaise(true);
    m_closeButton->setCursor(Qt::PointingHandCursor);
    m_closeButton->setToolTip(tr("Remove this waterfall and disconnect the Kiwi"));
    m_closeButton->setAccessibleName(tr("Remove waterfall"));
    m_closeButton->setFixedSize(18, 18);
    m_closeButton->setStyleSheet(QStringLiteral(
        "QToolButton { color: %1; border: none; font-size: 13px; }"
        "QToolButton:hover { color: %2; }")
            .arg(QString::fromLatin1(Style::kTextSecondary), ink()));
    connect(m_closeButton, &QToolButton::clicked, this,
            [this]() { emit closeRequested(m_profileId); });
    header->addWidget(m_closeButton, 0);

    layout->addLayout(header);
    layout->addStretch(1);
    // Waechst mit dem Fenster (Betreiber 2026-11-25: "die Fenster oben soll
    // man groesser machen koennen").
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumHeight(92);
    loadOptions();

    m_history = QImage(1, kHistoryRows, QImage::Format_RGB32);
    m_history.fill(QColor(Style::hexRole(Style::kAppBg)));

    m_lost = new KiwiLostOverlay(this);
    connect(m_lost, &KiwiLostOverlay::chooseRequested, this,
            [this]() { emit chooseOtherRequested(m_profileId); });
}

void KiwiWaterfallStripWidget::setLost(const QString& title, const QString& detail)
{
    m_lost->setLost(title, detail);
}

void KiwiWaterfallStripWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    // Alles unter der Kopfzeile (Bild und Achse).
    m_lost->setGeometry(1, 28, width() - 2, height() - 29);
}

void KiwiWaterfallStripWidget::setDisplayName(const QString& name)
{
    m_nameLabel->setText(name);
}

QString KiwiWaterfallStripWidget::frequencyText() const
{
    if (m_highHz <= m_lowHz) { return QString(); }
    const double centerKhz = (m_lowHz + m_highHz) / 2.0 / 1000.0;
    return QStringLiteral("%1 kHz").arg(centerKhz, 0, 'f', 1);
}

void KiwiWaterfallStripWidget::pushRow(const QVector<float>& binsDbm,
                                       double lowHz, double highHz)
{
    if (binsDbm.isEmpty()) {
        return;
    }
    if (highHz > lowHz) {
        // Anderer Bereich (+/-): der alte Verlauf passt nicht mehr zur Achse.
        const double old = m_highHz - m_lowHz;
        if (old > 0.0 && std::abs((highHz - lowHz) - old) > old * 0.01) {
            m_history.fill(QColor(Style::hexRole(Style::kAppBg)));
            m_lastRowMs = -1000000;
        }
        m_lowHz = lowHz;
        m_highHz = highHz;
        m_freqLabel->setText(frequencyText());
        m_spanLabel->setText(QStringLiteral("±%1 kHz").arg((highHz - lowHz) / 2000.0, 0, 'f', 0));
    }
    // Bremse: nur jede n-te Millisekunde eine neue Zeile.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_rowIntervalMs > 0 && now - m_lastRowMs < m_rowIntervalMs) {
        return;
    }
    m_lastRowMs = now;
    ++m_acceptedRows;

    if (m_history.width() != binsDbm.size()) {
        m_history = QImage(binsDbm.size(), kHistoryRows, QImage::Format_RGB32);
        m_history.fill(QColor(Style::hexRole(Style::kAppBg)));
    } else {
        // Scroll the existing buffer down by one row IN PLACE instead of
        // allocating a brand-new kHistoryRows-tall QImage on every incoming
        // waterfall row (a live KiwiSDR sends several per second). The old
        // code (`const QImage old = m_history; m_history = QImage(...); ...`)
        // reallocated + repainted the full buffer every single call — pure
        // churn on the hot path, found 2026-09-06 while chasing a runaway
        // memory event that force-quit the whole app during a live KiwiSDR
        // session. memmove() is safe on the overlapping same-buffer move a
        // QPainter blit is not. scanLine() below already calls detach()
        // internally, so the buffer is never shared with another QImage
        // while this mutates it in place.
        const int bytesPerLine = m_history.bytesPerLine();
        std::memmove(m_history.scanLine(1), m_history.scanLine(0),
                     static_cast<size_t>(bytesPerLine) * (kHistoryRows - 1));
    }

    const QColor floor(Style::hexRole(Style::kAppBg));
    const QColor peak(Style::kSpectrumTrace);
    if (m_auto) {
        // Rauschen (Median) bis zur Spitze der Zeile, mit einem Mindestabstand.
        QVector<float> sorted = binsDbm;
        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
        const float median = sorted[sorted.size() / 2];
        const float top = *std::max_element(binsDbm.begin(), binsDbm.end());
        m_minDbm = median - 6.0f;
        m_maxDbm = std::max(top, median + 25.0f);
    }
    float maxDbm = m_minDbm;
    for (int x = 0; x < binsDbm.size(); ++x) {
        const float dbm = binsDbm[x];
        maxDbm = std::max(maxDbm, dbm);
        const float t = std::clamp((dbm - m_minDbm) / (m_maxDbm - m_minDbm), 0.0f, 1.0f);
        const QColor c(
            floor.red()   + int((peak.red()   - floor.red())   * t),
            floor.green() + int((peak.green() - floor.green()) * t),
            floor.blue()  + int((peak.blue()  - floor.blue())  * t));
        m_history.setPixelColor(x, 0, c);
    }

    m_peakLabel->setText(QStringLiteral("%1 dBm").arg(qRound(maxDbm)));
    update();
}

void KiwiWaterfallStripWidget::reset()
{
    m_history.fill(QColor(Style::hexRole(Style::kAppBg)));
    m_peakLabel->setText(QStringLiteral("—"));
    m_lowHz = 0.0;
    m_highHz = 0.0;
    m_freqLabel->clear();
    m_spanLabel->clear();
    update();
}

void KiwiWaterfallStripWidget::setSoundActive(bool on)
{
    const QSignalBlocker blocker(m_soundButton);
    m_soundButton->setChecked(on);
}

bool KiwiWaterfallStripWidget::soundActive() const { return m_soundButton->isChecked(); }

void KiwiWaterfallStripWidget::loadOptions()
{
    AppSettings& st = AppSettings::instance();
    m_rowIntervalMs = std::max(0, st.value("KiwiWaterfall/RowIntervalMs",
                                           QString::number(kDefaultRowIntervalMs)).toString().toInt());
    m_auto = st.value("KiwiWaterfall/RangeAuto", "False").toString() == "True";
    if (m_auto) { return; }
    bool okLo = false, okHi = false;
    const float lo = st.value("KiwiWaterfall/MinDbm", QString::number(double(kDefaultMinDbm))).toString().toFloat(&okLo);
    const float hi = st.value("KiwiWaterfall/MaxDbm", QString::number(double(kDefaultMaxDbm))).toString().toFloat(&okHi);
    if (okLo && okHi && hi > lo) { m_minDbm = lo; m_maxDbm = hi; }
    update();
}

void KiwiWaterfallStripWidget::saveOptions() const
{
    AppSettings& st = AppSettings::instance();
    st.setValue("KiwiWaterfall/RowIntervalMs", QString::number(m_rowIntervalMs));
    st.setValue("KiwiWaterfall/RangeAuto", m_auto ? "True" : "False");
    if (!m_auto) {
        st.setValue("KiwiWaterfall/MinDbm", QString::number(double(m_minDbm)));
        st.setValue("KiwiWaterfall/MaxDbm", QString::number(double(m_maxDbm)));
    }
}

void KiwiWaterfallStripWidget::setRowIntervalMs(int ms)
{
    m_rowIntervalMs = std::max(0, ms);
}

void KiwiWaterfallStripWidget::setRange(float minDbm, float maxDbm)
{
    m_auto = false;
    m_minDbm = minDbm;
    m_maxDbm = std::max(maxDbm, minDbm + 1.0f);
}

void KiwiWaterfallStripWidget::setAutoRange(bool on)
{
    m_auto = on;
    if (!on) {
        m_minDbm = kDefaultMinDbm;
        m_maxDbm = kDefaultMaxDbm;
    }
}

void KiwiWaterfallStripWidget::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu menu(this);
    QAction* rename = menu.addAction(tr("Rename …"));
    menu.addSeparator();
    struct Speed { const char* text; int ms; };
    static const Speed kSpeeds[] = {
        {QT_TR_NOOP("Speed: slow (1 line/s)"), 1000},
        {QT_TR_NOOP("Speed: normal (4 lines/s)"), kDefaultRowIntervalMs},
        {QT_TR_NOOP("Speed: fast (every line)"), 0},
    };
    QList<QAction*> speedActs;
    for (const Speed& sp : kSpeeds) {
        QAction* a = menu.addAction(tr(sp.text));
        a->setCheckable(true);
        a->setChecked(m_rowIntervalMs == sp.ms);
        speedActs << a;
    }
    menu.addSeparator();
    QAction* autoAct = menu.addAction(tr("Level range: automatic"));
    autoAct->setCheckable(true);
    autoAct->setChecked(m_auto);
    struct Preset { const char* text; float lo; float hi; };
    static const Preset kPresets[] = {
        {QT_TR_NOOP("Quiet band (−130 … −80 dBm)"), -130.0f, -80.0f},
        {QT_TR_NOOP("Normal (−130 … −40 dBm)"), kDefaultMinDbm, kDefaultMaxDbm},
        {QT_TR_NOOP("Strong signals (−110 … −20 dBm)"), -110.0f, -20.0f},
    };
    QList<QAction*> presetActs;
    for (const Preset& pr : kPresets) {
        QAction* a = menu.addAction(tr(pr.text));
        a->setCheckable(true);
        a->setChecked(!m_auto && qFuzzyCompare(m_minDbm, pr.lo) && qFuzzyCompare(m_maxDbm, pr.hi));
        presetActs << a;
    }
    menu.addSeparator();
    QAction* close = menu.addAction(tr("Remove waterfall"));
    QAction* chosen = menu.exec(event->globalPos());
    if (!chosen) { return; }
    if (chosen == rename) {
        emit renameRequested(m_profileId);
    } else if (chosen == autoAct) {
        setAutoRange(autoAct->isChecked());
        saveOptions();
        emit optionsChanged();
    } else if (chosen == close) {
        emit closeRequested(m_profileId);
    } else if (speedActs.contains(chosen)) {
        setRowIntervalMs(kSpeeds[int(speedActs.indexOf(chosen))].ms);
        saveOptions();
        emit optionsChanged();
    } else {
        const int i = int(presetActs.indexOf(chosen));
        if (i >= 0) {
            setRange(kPresets[i].lo, kPresets[i].hi);
            saveOptions();
            emit optionsChanged();
        }
    }
    update();
}

QSize KiwiWaterfallStripWidget::sizeHint() const
{
    return QSize(320, 78);
}

void KiwiWaterfallStripWidget::setExpandedState(bool on)
{
    const QSignalBlocker blocker(m_expandButton);
    m_expandButton->setChecked(on);
}

void KiwiWaterfallStripWidget::setNotice(const QString& text)
{
    if (m_notice == text) { return; }
    // Statt eines Bildes: ein alter Verlauf darunter saehe wie ein haengendes
    // Bild aus (Betreiber 2026-11-27: "kiwi hat sich gerade aufgehaengt").
    if (m_notice.isEmpty()) { reset(); }
    m_notice = text;
    update();
}

void KiwiWaterfallStripWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);
    QPainter painter(this);
    const bool axis = m_highHz > m_lowHz;
    const int axisH = axis ? 14 : 0;
    const QRect target(9, 30, width() - 18, height() - 39 - axisH);
    if (target.height() > 0 && target.width() > 0 && m_history.width() > 0) {
        painter.drawImage(target, m_history);
    }
    if (!m_notice.isEmpty() && target.width() > 20) {
        painter.setPen(QColor(Style::hexRole(Style::kTextScale)));
        painter.drawText(target.adjusted(8, 0, -8, 0),
                         Qt::AlignCenter | Qt::TextWordWrap, m_notice);
    }
    if (axis && target.width() > 60) {
        // Frequenzachse unter dem Bild, Marke auf der Mitte (dort steht der Kiwi).
        QFont f = font();
        f.setPixelSize(9);
        painter.setFont(f);
        painter.setPen(QColor(Style::hexRole(Style::kTextScale)));
        const int ticks = 4;
        for (int i = 0; i <= ticks; ++i) {
            const int x = target.left() + i * target.width() / ticks;
            const double hz = m_lowHz + (m_highHz - m_lowHz) * i / ticks;
            painter.drawLine(x, target.bottom() + 1, x, target.bottom() + 4);
            const QString t = QString::number(hz / 1000.0, 'f', 1);
            const int w = painter.fontMetrics().horizontalAdvance(t);
            const int tx = std::clamp(x - w / 2, 0, std::max(0, width() - w));
            painter.drawText(tx, target.bottom() + 13, t);
        }
        QColor mark(Style::kTextPrimary);
        mark.setAlpha(190);
        painter.setPen(QPen(mark, 2));
        const int mx = target.center().x();
        painter.drawLine(mx, target.top(), mx, target.bottom() + 4);
    }
}

} // namespace Longpath
