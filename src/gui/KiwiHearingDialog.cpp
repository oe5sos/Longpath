// SPDX-License-Identifier: GPL-3.0-or-later
//
// Longpath-eigen. Siehe KiwiHearingDialog.h.
//
//   2026-11-25 — Neu angelegt.

#include "gui/KiwiHearingDialog.h"

#include "core/KiwiGroup.h"
#include "gui/StyleConstants.h"

#include <QFrame>
#include <QHBoxLayout>
#include <QSet>
#include <QSignalBlocker>
#include <QHeaderView>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTableWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace Longpath {

namespace {

QColor col(const char* hex) { return QColor(QLatin1String(hex)); }

QString buttonStyle()
{
    return QStringLiteral(
        "QPushButton { background: %1; color: %2; border: 1px solid %3; "
        "border-radius: 6px; padding: 2px 10px; font-size: 11px; }"
        "QPushButton:hover { background: %4; }"
        "QPushButton:checked { border-color: %5; color: %6; }"
        "QPushButton:disabled { color: %7; }")
        .arg(QLatin1String(Style::kButtonBg), QLatin1String(Style::kTextSecondary),
             QLatin1String(Style::kBorderMuted), QLatin1String(Style::kButtonHover),
             QLatin1String(Style::kBlueBorder), QLatin1String(Style::kTextPrimary),
             QLatin1String(Style::kTextGroupHead));
}

KiwiMapPoint::Heard heardOf(const KiwiHearMeMeter::Result& r)
{
    switch (r.verdict) {
    case KiwiHearMeMeter::Verdict::Hears:
        return KiwiGroup::isWeak(r) ? KiwiMapPoint::Heard::Weak
                                    : KiwiMapPoint::Heard::Hears;
    case KiwiHearMeMeter::Verdict::Nothing: return KiwiMapPoint::Heard::Nothing;
    case KiwiHearMeMeter::Verdict::Unknown: return KiwiMapPoint::Heard::Unknown;
    default:                                return KiwiMapPoint::Heard::None;
    }
}

QColor verdictColor(const KiwiHearMeMeter::Result& r)
{
    switch (r.verdict) {
    case KiwiHearMeMeter::Verdict::Hears:
        return KiwiGroup::isWeak(r) ? col(Style::kAmberWarn) : col(Style::kGreenText);
    case KiwiHearMeMeter::Verdict::Nothing: return col(Style::kTextGroupHead);
    default:                                return col(Style::kTextSecondary);
    }
}

} // namespace

// ── Mini-Wasserfall ─────────────────────────────────────────────────

KiwiMiniWaterfall::KiwiMiniWaterfall(QWidget* parent) : QWidget(parent)
{
    setObjectName(QStringLiteral("kiwiMiniWaterfall"));
}

void KiwiMiniWaterfall::pushRow(const QVector<float>& binsDbm, double lowHz, double highHz)
{
    if (binsDbm.isEmpty() || highHz <= lowHz) { return; }
    m_rows.prepend({binsDbm, lowHz, highHz});
    if (m_rows.size() > kRows) { m_rows.resize(kRows); }
    update();
}

void KiwiMiniWaterfall::clearRows() { m_rows.clear(); update(); }

void KiwiMiniWaterfall::setMarkHz(double hz)
{
    if (hz == m_markHz) { return; }
    m_markHz = hz;
    update();
}

void KiwiMiniWaterfall::render(QImage& img) const
{
    img.fill(col(Style::kInsetBg));
    if (m_rows.isEmpty()) { return; }
    const int W = img.width();
    const int H = img.height();
    for (int y = 0; y < H; ++y) {
        const int r = std::min<int>(int(m_rows.size()) - 1, y * kRows / std::max(1, H));
        if (r >= m_rows.size()) { break; }
        const Row& row = m_rows[r];
        QVector<float> sorted = row.bins;
        std::nth_element(sorted.begin(), sorted.begin() + sorted.size() / 2, sorted.end());
        const float median = sorted[sorted.size() / 2];
        QRgb* line = reinterpret_cast<QRgb*>(img.scanLine(y));
        for (int x = 0; x < W; ++x) {
            const int b = std::clamp(int(double(x) / W * row.bins.size()), 0,
                                     int(row.bins.size()) - 1);
            const float v = std::clamp((row.bins[b] - median) / 40.0f, 0.0f, 1.0f);
            const int g = 12 + int(v * 200);
            line[x] = qRgb(g, g, g);
        }
    }
    const Row& first = m_rows.first();
    if (m_markHz > first.lowHz && m_markHz < first.highHz) {
        const int mx = int((m_markHz - first.lowHz) / (first.highHz - first.lowHz) * W);
        QPainter p(&img);
        p.setPen(QPen(col(Style::kRedBorder), std::max(1, W / 400)));
        p.drawLine(mx, 0, mx, H);
    }
}

QImage KiwiMiniWaterfall::image(int width) const
{
    if (m_rows.isEmpty()) { return {}; }
    QImage img(width, std::max(60, width / 5), QImage::Format_RGB32);
    render(img);
    return img;
}

void KiwiMiniWaterfall::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    QImage img(size(), QImage::Format_RGB32);
    render(img);
    p.drawImage(0, 0, img);
    p.setPen(col(Style::kBorderMuted));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

// ── Dialog ───────────────────────────────────────────────────────────

QString KiwiHearingDialog::verdictText(const KiwiHearMeMeter::Result& r)
{
    switch (r.verdict) {
    case KiwiHearMeMeter::Verdict::Waiting:
        return tr("waiting for a transmission");
    case KiwiHearMeMeter::Verdict::Unknown:
        return tr("unknown");
    case KiwiHearMeMeter::Verdict::Hears:
        return KiwiGroup::isWeak(r)
            ? tr("weak  %1").arg(kiwiSUnitsText(r.sUnits, r.overS9Db))
            : tr("hears me  %1").arg(kiwiSUnitsText(r.sUnits, r.overS9Db));
    case KiwiHearMeMeter::Verdict::Nothing:
        return tr("hears nothing");
    }
    return QString();
}

KiwiHearingDialog::KiwiHearingDialog(QWidget* parent) : QDialog(parent)
{
    setObjectName(QStringLiteral("kiwiHearingDialog"));
    setWindowTitle(tr("Where am I heard"));
    resize(960, 780);

    auto* outer = new QVBoxLayout(this);
    auto* top = new QHBoxLayout;
    m_summary = new QLabel(this);
    m_summary->setObjectName(QStringLiteral("kiwiHearingSummary"));
    m_summary->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; }")
                                 .arg(QLatin1String(Style::kTextPrimary)));
    top->addWidget(m_summary, 1);
    m_zoom = new QPushButton(tr("Zoom to my Kiwis"), this);
    m_zoom->setCheckable(true);
    m_zoom->setChecked(true);
    m_zoom->setStyleSheet(buttonStyle());
    m_add = new QPushButton(tr("Add Kiwi …"), this);
    m_add->setStyleSheet(buttonStyle());
    m_add->setObjectName(QStringLiteral("kiwiHearingAdd"));
    top->addWidget(m_zoom);
    top->addWidget(m_add);
    outer->addLayout(top);

    m_map = new KiwiWorldMap(this);
    m_map->setHeardMode(true);
    m_map->setAutoFit(true);

    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(190);
    auto* tilesHost = new QWidget;
    m_tileLayout = new QHBoxLayout(tilesHost);
    m_tileLayout->setContentsMargins(0, 0, 0, 0);
    m_tileLayout->setSpacing(8);
    m_tileLayout->addStretch(1);
    scroll->setWidget(tilesHost);

    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Kiwi"), tr("Distance"), tr("Level"), tr("Verdict"), tr("Delay"), tr("Rec")});
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);
    m_table->verticalHeader()->setVisible(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setAccessibleName(tr("Where I am heard, per Kiwi"));

    auto* split = new QSplitter(Qt::Vertical, this);
    split->setChildrenCollapsible(false);
    split->addWidget(m_map);
    split->addWidget(scroll);
    split->addWidget(m_table);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 3);
    split->setStretchFactor(2, 2);
    outer->addWidget(split, 1);

    connect(m_add, &QPushButton::clicked, this, &KiwiHearingDialog::addRequested);
    connect(m_zoom, &QPushButton::toggled, m_map, &KiwiWorldMap::setAutoFit);
    updateSummary({});
}

void KiwiHearingDialog::setSlotsFree(int free)
{
    m_slotsFree = free;
    m_add->setEnabled(free > 0);
    m_add->setToolTip(free > 0
        ? tr("Open another Kiwi to listen along (%1 of %2 places free).")
              .arg(free).arg(kKiwiMaxTotal)
        : tr("All %1 places are taken. Close one to add another.").arg(kKiwiMaxTotal));
}

void KiwiHearingDialog::setMarkHz(double hz)
{
    m_markHz = hz;
    for (Tile& t : m_tiles) { t.wf->setMarkHz(hz); }
}

void KiwiHearingDialog::setHome(double lat, double lon, bool valid, const QString& label)
{
    m_map->setHome(lat, lon, valid, label);
}

void KiwiHearingDialog::setTransmitting(bool on)
{
    m_transmitting = on;
}

void KiwiHearingDialog::pushWaterfallRow(const QString& id, const QVector<float>& binsDbm,
                                         double lowHz, double highHz)
{
    auto it = m_tiles.find(id);
    if (it != m_tiles.end()) { it->wf->pushRow(binsDbm, lowHz, highHz); }
}

void KiwiHearingDialog::clearWaterfall(const QString& id)
{
    auto it = m_tiles.find(id);
    if (it != m_tiles.end()) { it->wf->clearRows(); }
}

QImage KiwiHearingDialog::waterfallImage(const QString& id) const
{
    auto it = m_tiles.constFind(id);
    return it == m_tiles.constEnd() ? QImage() : it->wf->image();
}

QString KiwiHearingDialog::tileVerdictText(const QString& id) const
{
    auto it = m_tiles.constFind(id);
    return it == m_tiles.constEnd() ? QString() : it->verdict->text();
}

QString KiwiHearingDialog::summaryText() const { return m_summary->text(); }

QPushButton* KiwiHearingDialog::recordButtonForTest(const QString& id) const
{
    auto it = m_tiles.constFind(id);
    return it == m_tiles.constEnd() ? nullptr : it->record;
}

QPushButton* KiwiHearingDialog::removeButtonForTest(const QString& id) const
{
    auto it = m_tiles.constFind(id);
    return it == m_tiles.constEnd() ? nullptr : it->remove;
}

int KiwiHearingDialog::miniRowsForTest(const QString& id) const
{
    auto it = m_tiles.constFind(id);
    return it == m_tiles.constEnd() ? -1 : it->wf->rowCount();
}

void KiwiHearingDialog::setRows(const QVector<KiwiHearingRow>& rows)
{
    // Kacheln abgleichen: neue anlegen, bestehende nachfuehren (damit der
    // Wasserfall nicht jede Sekunde verschwindet), weggefallene entfernen.
    QSet<QString> wanted;
    for (const KiwiHearingRow& r : rows) { wanted.insert(r.id); }
    for (auto it = m_tiles.begin(); it != m_tiles.end();) {
        if (!wanted.contains(it.key())) {
            it->frame->deleteLater();
            it = m_tiles.erase(it);
        } else {
            ++it;
        }
    }
    m_order.clear();
    int position = 0;
    for (const KiwiHearingRow& r : rows) {
        m_order.append(r.id);
        auto it = m_tiles.find(r.id);
        if (it == m_tiles.end()) {
            Tile t;
            t.frame = new QFrame;
            t.frame->setObjectName(QStringLiteral("kiwiHearingTile"));
            t.frame->setFixedWidth(196);
            t.frame->setStyleSheet(QStringLiteral(
                "QFrame#kiwiHearingTile { background: %1; border: 1px solid %2; "
                "border-radius: 6px; }")
                .arg(QLatin1String(Style::kPanelBg), QLatin1String(Style::kBorderMuted)));
            auto* v = new QVBoxLayout(t.frame);
            v->setContentsMargins(8, 6, 8, 6);
            v->setSpacing(4);
            t.name = new QLabel(t.frame);
            t.name->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; "
                                                 "font-weight: bold; border: none; }")
                                      .arg(QLatin1String(Style::kTextPrimary)));
            t.sub = new QLabel(t.frame);
            t.sub->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 9px; border: none; }")
                                     .arg(QLatin1String(Style::kTextGroupHead)));
            t.wf = new KiwiMiniWaterfall(t.frame);
            t.wf->setMarkHz(m_markHz);
            t.verdict = new QLabel(t.frame);
            t.verdict->setStyleSheet(QStringLiteral("QLabel { font-size: 13px; border: none; }"));
            auto* buttons = new QHBoxLayout;
            buttons->setSpacing(4);
            t.record = new QPushButton(tr("Record"), t.frame);
            t.record->setCheckable(true);
            t.record->setStyleSheet(buttonStyle());
            t.remove = new QPushButton(tr("Close"), t.frame);
            t.remove->setStyleSheet(buttonStyle());
            buttons->addWidget(t.record);
            buttons->addWidget(t.remove);
            v->addWidget(t.name);
            v->addWidget(t.sub);
            v->addWidget(t.wf, 1);
            v->addWidget(t.verdict);
            v->addLayout(buttons);
            const QString id = r.id;
            connect(t.record, &QPushButton::toggled, this,
                    [this, id](bool on) { emit recordToggled(id, on); });
            connect(t.remove, &QPushButton::clicked, this,
                    [this, id]() { emit removeRequested(id); });
            it = m_tiles.insert(r.id, t);
        }
        Tile& t = *it;
        m_tileLayout->insertWidget(position++, t.frame);
        t.name->setText(r.name);
        t.name->setToolTip(r.name);
        QStringList sub;
        if (r.km >= 0.0) { sub << tr("%1 km").arg(qRound(r.km)); }
        sub << r.stateText;
        if (r.result.delayMs > 0) {
            sub << tr("delay %1 s").arg(r.result.delayMs / 1000.0, 0, 'f', 1);
        }
        t.sub->setText(sub.join(QStringLiteral(" · ")));
        t.verdict->setText(verdictText(r.result));
        t.verdict->setStyleSheet(QStringLiteral("QLabel { color: %1; font-size: 13px; border: none; }")
                                     .arg(verdictColor(r.result).name()));
        {
            const QSignalBlocker b(t.record);
            t.record->setChecked(r.recording);
        }
        t.remove->setVisible(!r.primary);
        t.remove->setToolTip(r.primary ? QString() : tr("Disconnect this Kiwi."));
    }
    rebuildMapAndTable(rows);
    updateSummary(rows);
}

void KiwiHearingDialog::rebuildMapAndTable(const QVector<KiwiHearingRow>& rows)
{
    QVector<KiwiMapPoint> points;
    for (const KiwiHearingRow& r : rows) {
        if (!r.hasPosition) { continue; }
        KiwiMapPoint p;
        p.id = r.id;
        p.name = r.name.left(14);
        p.lat = r.lat;
        p.lon = r.lon;
        p.state = r.connected ? KiwiMapPoint::State::Free : KiwiMapPoint::State::Offline;
        p.heard = heardOf(r.result);
        p.open = true;
        if (r.result.verdict == KiwiHearMeMeter::Verdict::Hears) {
            p.label = kiwiSUnitsText(r.result.sUnits, r.result.overS9Db);
        }
        p.detail = QStringLiteral("%1\n%2").arg(r.name, verdictText(r.result));
        points.append(p);
    }
    m_map->setPoints(points);

    m_table->setRowCount(rows.size());
    for (int i = 0; i < rows.size(); ++i) {
        const KiwiHearingRow& r = rows[i];
        const auto cell = [this, i](int c, const QString& s, const QColor& color = QColor()) {
            auto* item = new QTableWidgetItem(s);
            if (color.isValid()) { item->setForeground(color); }
            m_table->setItem(i, c, item);
        };
        cell(0, r.name);
        cell(1, r.km >= 0 ? tr("%1 km").arg(qRound(r.km)) : QString());
        cell(2, r.result.verdict == KiwiHearMeMeter::Verdict::Hears
                    ? kiwiSUnitsText(r.result.sUnits, r.result.overS9Db) : QStringLiteral("—"));
        cell(3, verdictText(r.result), verdictColor(r.result));
        cell(4, r.result.delayMs > 0
                    ? tr("%1 s").arg(r.result.delayMs / 1000.0, 0, 'f', 1) : QString());
        cell(5, r.recording ? QStringLiteral("●") : QString());
    }
}

void KiwiHearingDialog::updateSummary(const QVector<KiwiHearingRow>& rows)
{
    QVector<KiwiHearMeMeter::Result> results;
    QVector<double> kms;
    QStringList names;
    for (const KiwiHearingRow& r : rows) {
        results.append(r.result);
        kms.append(r.km);
        names.append(r.name);
    }
    const KiwiGroup::Summary s = KiwiGroup::summarize(results, kms);
    if (rows.isEmpty()) {
        m_summary->setText(tr("No Kiwi listening along yet — add one."));
    } else if (s.judged == 0) {
        m_summary->setText(tr("%1 Kiwis listening along. Transmit to see who hears you.")
                               .arg(rows.size()));
    } else {
        QString text = tr("%1 of %2 hear me").arg(s.hears).arg(s.judged);
        if (s.farthestKm >= 0.0) {
            text += tr(" · farthest: %1 km").arg(qRound(s.farthestKm));
        }
        m_summary->setText(text);
    }
}

} // namespace Longpath
