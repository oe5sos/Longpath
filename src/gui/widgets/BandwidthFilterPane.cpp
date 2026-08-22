// =================================================================
// src/gui/widgets/BandwidthFilterPane.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/widgets/BandwidthFilterPane.h"

#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"

#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Rand um die Zeichenflaeche. Oben mehr, weil dort die Beschriftungen
// der Kanten und die Breitenpille sitzen; unten fuer die Skala.
constexpr int kPadX      = 6;
constexpr int kPadTop    = 30;
constexpr int kPadBottom = 20;

// Eine Zahl auf der Achse: Megahertz mit drei Nachkommastellen, wie in
// der Vorlage („13.137"). Kilohertz waeren eine Ziffer mehr und sagen
// dasselbe.
QString axisLabel(double hz)
{
    return QString::number(hz / 1.0e6, 'f', 3);
}

// Eine Kantenangabe. Unter 1 kHz in Hertz, darueber in Kilohertz mit
// zwei Stellen — so macht es die Vorlage, und so bleibt die Zahl kurz
// genug, um ueber der Kante zu stehen.
QString cutLabel(int hz)
{
    if (std::abs(hz) < 1000) {
        return QStringLiteral("%1 Hz").arg(hz);
    }
    return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 2);
}

QString widthLabel(int hz)
{
    if (hz < 1000) { return QStringLiteral("%1 Hz").arg(hz); }
    return QStringLiteral("%1 kHz").arg(hz / 1000.0, 0, 'f', 1);
}

} // namespace

BandwidthFilterPane::BandwidthFilterPane(QWidget* parent)
    : QWidget(parent)
    , m_accent(QColor(Style::role("accent", Style::kAccent)))
{
    // Klein genug fuer den Platz im RxApplet (dort rund 92 px), gross
    // genug, dass Beschriftungen und Achse hineinpassen.
    setMinimumHeight(84);
    setMinimumWidth(180);
    setMouseTracking(true);
}

void BandwidthFilterPane::setLabel(const QString& text)
{
    if (m_label == text) { return; }
    m_label = text;
    update();
}

void BandwidthFilterPane::setAccent(const QColor& c)
{
    if (m_accent == c) { return; }
    m_accent = c;
    update();
}

void BandwidthFilterPane::setVfoFrequency(double hz)
{
    if (qFuzzyCompare(m_vfoHz, hz)) { return; }
    m_vfoHz = hz;
    update();
}

void BandwidthFilterPane::setTrace(const QVector<float>& dbm)
{
    if (m_trace == dbm) { return; }
    m_trace = dbm;
    update();
}

void BandwidthFilterPane::setSpan(int hz)
{
    // Unter 2 kHz wird die Achse unlesbar, ueber 40 kHz verschwindet
    // der Durchlass zu einem Strich.
    const int clamped = std::clamp(hz, 2000, 40000);
    if (m_spanHz == clamped) { return; }
    m_spanHz = clamped;
    update();
}

void BandwidthFilterPane::setFilter(int low, int high)
{
    if (m_low == low && m_high == high) { return; }
    m_low  = low;
    m_high = high;
    update();
}

void BandwidthFilterPane::setHasFrequency(bool on)
{
    if (m_hasFrequency == on) { return; }
    m_hasFrequency = on;
    update();
}

QRect BandwidthFilterPane::plotRect() const
{
    return QRect(kPadX, kPadTop,
                 std::max(1, width() - 2 * kPadX),
                 std::max(1, height() - kPadTop - kPadBottom));
}

int BandwidthFilterPane::hzToX(int hz) const
{
    const QRect r = plotRect();
    const double frac = (static_cast<double>(hz) + m_spanHz / 2.0) / m_spanHz;
    return r.left() + static_cast<int>(std::lround(frac * r.width()));
}

int BandwidthFilterPane::xToHz(int x) const
{
    const QRect r = plotRect();
    const double frac = static_cast<double>(x - r.left())
                      / std::max(1, r.width());
    return static_cast<int>(std::lround(frac * m_spanHz - m_spanHz / 2.0));
}

BandwidthFilterPane::Zone BandwidthFilterPane::zoneAt(int x) const
{
    const int xl = hzToX(m_low);
    const int xh = hzToX(m_high);

    // Bei beiden in Reichweite gewinnt die naehere — sonst laesst sich
    // ein sehr schmaler Durchlass nur an einer Seite anfassen.
    const bool nearLow  = std::abs(x - xl) <= kGrabPx;
    const bool nearHigh = std::abs(x - xh) <= kGrabPx;
    if (nearLow && nearHigh) {
        return (std::abs(x - xl) <= std::abs(x - xh)) ? Zone::LowEdge
                                                      : Zone::HighEdge;
    }
    if (nearLow)  { return Zone::LowEdge; }
    if (nearHigh) { return Zone::HighEdge; }
    if (x > xl && x < xh) { return Zone::Body; }
    return Zone::None;
}

// ── Zeichnen ─────────────────────────────────────────────────────────

void BandwidthFilterPane::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);

    const QRect r = plotRect();

    // Grund: eine Mulde, oben minimal heller. Dieselbe Richtung wie bei
    // den Eingabefeldern — versenkt, nicht aufgelegt.
    QLinearGradient bg(0, 0, 0, height());
    bg.setColorAt(0.0, QColor(0x10, 0x10, 0x14));
    bg.setColorAt(1.0, QColor(Style::role("inset-bg", Style::kInsetBg)));
    p.fillRect(rect(), bg);

    p.setPen(QColor(Style::role("border-subtle", Style::kBorderSubtle)));
    p.drawRect(0, 0, width() - 1, height() - 1);

    // Senkrechte Hilfslinien alle 2 kHz — dieselbe Teilung wie die
    // Beschriftungen darunter, damit die Zahlen etwas zum Festhalten
    // haben.
    const QColor grid(0x14, 0x14, 0x18);
    p.setPen(grid);
    for (int hz = -m_spanHz / 2; hz <= m_spanHz / 2; hz += 2000) {
        const int x = hzToX(hz);
        p.drawLine(x, r.top(), x, r.bottom());
    }

    // ── Das Signal ──────────────────────────────────────────────────
    //
    // Vorbild Zeus Link, vorgefuehrt am 2026-08-22: der Bandfilter
    // zeigt dort das ECHTE Spektrum, und erst dadurch sieht man, ob
    // die Kante an der richtigen Stelle sitzt. Ohne Kurve ist das
    // Fenster ein Zahlenformular.
    //
    // Der Ausschnitt kommt vom Panadapter (SpectrumWidget::
    // dbmOverRange) — dieselbe Abbildung, dieselbe Kalibrierung. Eine
    // zweite eigene waere ein zweiter Ort, an dem sie falsch sein
    // kann.
    //
    // Massstab: der Kopf des Fensters gehoert den Beschriftungen, also
    // beginnt die Kurve darunter. Der Pegelbereich richtet sich nach
    // dem, was da ist (mit Mindestspanne), sonst klebt eine leise
    // Band-Mitte am Boden und man sieht nichts.
    if (m_trace.size() >= 2) {
        float lo = m_trace.first(), hi = m_trace.first();
        for (float v : m_trace) { lo = qMin(lo, v); hi = qMax(hi, v); }
        if (hi - lo < 12.0f) {
            const float mid = 0.5f * (lo + hi);
            lo = mid - 6.0f;
            hi = mid + 6.0f;
        }
        const int top = r.top() + 30;          // Platz fuer die Marken
        const int bot = r.bottom() - 2;
        const double yScale = (bot - top) / static_cast<double>(hi - lo);

        QPolygonF poly;
        poly.reserve(m_trace.size() + 2);
        poly << QPointF(r.left(), bot);
        for (int i = 0; i < m_trace.size(); ++i) {
            const double x = r.left()
                + (r.width() - 1.0) * i / (m_trace.size() - 1.0);
            const double y = bot - (m_trace[i] - lo) * yScale;
            poly << QPointF(x, qBound<double>(top, y, bot));
        }
        poly << QPointF(r.right(), bot);

        // ── Pegelraster ─────────────────────────────────────────────
        //
        // Drei waagrechte Linien mit dBm-Marke. Ohne sie ist die Kurve
        // eine Form ohne Massstab: man sieht, DASS da etwas ist, aber
        // nicht, wie stark. Zeus zeigt an derselben Stelle einen
        // Pegelwert im Durchlass.
        {
            QFont tiny = font();
            tiny.setPointSizeF(std::max(6.0, tiny.pointSizeF() - 3.5));
            p.setFont(tiny);
            const QColor gridLine(0x1c, 0x1c, 0x22);
            const QColor gridText(Style::role("text-scale", Style::kTextScale));
            for (int k = 1; k <= 3; ++k) {
                const double frac = k / 4.0;
                const int y = static_cast<int>(bot - (bot - top) * frac);
                p.setPen(gridLine);
                p.drawLine(r.left() + 1, y, r.right() - 1, y);
                const int dbm = static_cast<int>(std::lround(lo + (hi - lo) * frac));
                p.setPen(gridText);
                p.drawText(QRect(r.right() - 46, y - 7, 42, 12),
                           Qt::AlignRight | Qt::AlignVCenter,
                           QStringLiteral("%1").arg(dbm));
            }
        }

        // ── Die Kurve ───────────────────────────────────────────────
        //
        // Eigener Ton, NICHT der Akzent des Empfaengers: der gehoert
        // dem Durchlass. Vorher waren beide blau, und man sah nicht,
        // was im Filter liegt und was daneben — genau das ist die
        // Frage, fuer die man dieses Fenster aufmacht.
        QColor traceLine(Style::role("trace", "#c8a06a"));
        QColor fill(traceLine);
        fill.setAlpha(40);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        p.drawPolygon(poly);

        p.setPen(QPen(traceLine, 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawPolyline(poly.constData() + 1, poly.size() - 2);

        // ── Was AUSSERHALB liegt, tritt zurueck ─────────────────────
        //
        // Nicht den Durchlass hervorheben, sondern die Umgebung
        // daempfen: das liest sich sofort und ohne eine weitere Farbe.
        {
            const int xl2 = hzToX(m_low);
            const int xh2 = hzToX(m_high);
            QColor veil(Style::role("app-bg", Style::kAppBg));
            veil.setAlpha(120);
            p.setPen(Qt::NoPen);
            p.setBrush(veil);
            if (xl2 > r.left()) {
                p.drawRect(QRect(r.left(), top, xl2 - r.left(), bot - top));
            }
            if (xh2 < r.right()) {
                p.drawRect(QRect(xh2, top, r.right() - xh2, bot - top));
            }
        }
    }

    // Die Nulllinie ist die VFO-Frequenz. Sie traegt einen eigenen Ton,
    // sonst ist sie eine Hilfslinie unter vielen.
    {
        const int x0 = hzToX(0);
        p.setPen(QColor(Style::role("border", Style::kBorder)));
        p.drawLine(x0, r.top(), x0, r.bottom());
    }

    // ── Der Durchlass ────────────────────────────────────────────────
    const int xl = hzToX(m_low);
    const int xh = hzToX(m_high);

    QColor fill = m_accent;
    fill.setAlphaF(0.10f);
    p.fillRect(QRect(xl, r.top(), std::max(1, xh - xl), r.height()), fill);

    QColor topEdge = m_accent;
    topEdge.setAlphaF(0.55f);
    p.setPen(topEdge);
    p.drawLine(xl, r.top(), xh, r.top());

    p.setPen(QPen(m_accent, 2));
    p.drawLine(xl, r.top(), xl, r.bottom());
    p.drawLine(xh, r.top(), xh, r.bottom());

    // Griffe als Pillen in halber Hoehe. Die ganze Kante als Griff
    // waere zwar groesser, sagt aber nicht, WO man fassen soll.
    auto drawHandle = [&](int x, bool active) {
        const QRect h(x - 4, r.center().y() - 11, 9, 22);
        p.setBrush(QColor(active ? 0x1e : 0x16, active ? 0x2a : 0x20,
                          active ? 0x36 : 0x2a));
        p.setPen(QPen(m_accent, active ? 2 : 1));
        p.drawRoundedRect(h, 3, 3);
        QColor tick = m_accent;
        tick.setAlphaF(0.8f);
        p.setPen(tick);
        p.drawLine(x, h.top() + 5, x, h.bottom() - 5);
        p.setBrush(Qt::NoBrush);
    };
    drawHandle(xl, m_hover == Zone::LowEdge || m_drag == Zone::LowEdge);
    drawHandle(xh, m_hover == Zone::HighEdge || m_drag == Zone::HighEdge);

    // ── Beschriftungen oben ──────────────────────────────────────────
    QFont small = font();
    small.setPointSizeF(std::max(6.0, small.pointSizeF() - 3.0));
    QFont value = font();
    value.setPointSizeF(std::max(7.0, value.pointSizeF() - 2.0));

    const QColor faint(Style::role("text-scale", Style::kTextScale));
    const QColor ink(Style::role("text", Style::kTextPrimary));

    // ── Eng: die Wortmarken weichen, die Zahlen bleiben ─────────────
    //
    // Der Betreiber hat es am 2026-08-22 fotografiert: beim
    // Verkleinern schoben sich "LOW CUT" und "HIGH CUT" ineinander und
    // ueber das Kaestchen mit der Breite. Drei Beschriftungen wollen
    // Platz, den es nicht mehr gibt.
    //
    // Rangfolge, wie ueberall sonst in diesem Fenster: zuerst faellt
    // das WORT, dann die Zahl. Die Zahl traegt die Information; das
    // Wort sagt nur, was ohnehin an der Kante steht, an der es klebt.
    //
    // Gemessen wird gegen den tatsaechlichen Platz zwischen den beiden
    // Griffen, nicht gegen die Fensterbreite: bei schmalem Durchlass
    // ist es auch in einem breiten Fenster eng.
    const int labelRoom = xh - xl;
    const bool wordMarks = labelRoom >= 190;
    const bool numbers   = labelRoom >= 110;

    // Die Wortmarken "LOW CUT"/"HIGH CUT" sind ersatzlos entfallen.
    //
    // Sie sagten, was ohnehin an der Stelle steht, an der sie klebten,
    // und waren die Haelfte des Gedraenges oben. Seit die Zahlen unten
    // an ihren Kanten stehen, braucht es sie nicht mehr. (wordMarks
    // bleibt als Groessenmass fuer die Zahlen erhalten.)
    Q_UNUSED(small);
    Q_UNUSED(faint);

    if (numbers) {
        // ── Die Kantenwerte AN DIE KANTEN, unten ────────────────────
        //
        // Sie standen oben und stiessen dort mit dem Breitenkaestchen
        // zusammen — auf dem Bild des Betreibers vom 2026-08-22 lagen
        // "LOW CUT", "HIGH CUT" und "2.9 kHz" uebereinander.
        //
        // Unten an der jeweiligen Kante ist ohnehin der bessere Platz:
        // die Zahl steht dort, wo sie gilt, und muss nicht sagen,
        // wozu sie gehoert.
        p.setFont(value);
        p.setPen(ink);
        const int yNum = r.bottom() - 15;
        p.drawText(QRect(xl + 4, yNum, 74, 13),
                   Qt::AlignLeft | Qt::AlignVCenter, cutLabel(m_low));
        p.drawText(QRect(xh - 78, yNum, 74, 13),
                   Qt::AlignRight | Qt::AlignVCenter, cutLabel(m_high));
    }

    // Die Breite in der Mitte, in einem eigenen Kaestchen: sie ist die
    // Zahl, nach der man den Filter benennt.
    {
        const QString t = widthLabel(m_high - m_low);
        const QFontMetrics fm(value);
        const int tw = fm.horizontalAdvance(t) + 14;
        const int cx = (xl + xh) / 2;
        const QRect box(cx - tw / 2, 8, tw, 17);
        p.setBrush(QColor(0x0b, 0x0b, 0x0d));
        p.setPen(QColor(Style::role("border", Style::kBorder)));
        p.drawRoundedRect(box, 3, 3);
        p.setPen(ink);
        p.drawText(box, Qt::AlignCenter, t);
        p.setBrush(Qt::NoBrush);
    }

    // ── Die Beschriftung des Empfaengers ─────────────────────────────
    {
        p.setFont(small);
        const QFontMetrics fm(small);
        const int tw = fm.horizontalAdvance(m_label) + 10;
        const QRect box(4, 4, tw, 13);
        QColor bgc = m_accent;
        bgc.setAlphaF(0.22f);
        p.setBrush(bgc);
        p.setPen(m_accent);
        p.drawRoundedRect(box, 2, 2);
        p.setPen(ink);
        p.drawText(box, Qt::AlignCenter, m_label);
        p.setBrush(Qt::NoBrush);
    }

    // ── Die Achse ────────────────────────────────────────────────────
    //
    // Ohne Verbindung steht hier NICHTS. Eine erfundene Frequenz waere
    // eine Behauptung — dieselbe Regel wie beim Panadapter-Kopf.
    p.setFont(small);
    if (!m_hasFrequency) {
        p.setPen(faint);
        p.drawText(QRect(0, height() - kPadBottom, width(), kPadBottom),
                   Qt::AlignCenter, QStringLiteral("no radio"));
        return;
    }

    // ── Abstand statt Abschnitt am Rand ─────────────────────────────
    //
    // Die Zahlen standen mittig unter ihrer Linie, auch wenn die Linie
    // am Bildrand lag — die aeusseren wurden dadurch angeschnitten
    // (".111" statt "7.111"). Eine halb gelesene Frequenz ist
    // schlimmer als keine.
    //
    // Schrittweite richtet sich nach der Spanne: bei 2 kHz Fenster
    // waeren 2-kHz-Schritte eine einzige Marke.
    const int stepHz = (m_spanHz <= 6000) ? 1000
                     : (m_spanHz <= 24000) ? 2000 : 5000;
    for (int hz = -(m_spanHz / 2 / stepHz) * stepHz;
         hz <= m_spanHz / 2; hz += stepHz) {
        const int x = hzToX(hz);
        const bool isCentre = (hz == 0);
        const QString t = axisLabel(m_vfoHz + hz);
        const int tw = p.fontMetrics().horizontalAdvance(t) + 8;
        int left = x - tw / 2;
        if (left < 1) { left = 1; }
        if (left + tw > width() - 1) { left = width() - 1 - tw; }
        p.setPen(isCentre ? QColor(Style::role("text-secondary",
                                               Style::kTextSecondary))
                          : faint);
        p.drawText(QRect(left, height() - kPadBottom + 2, tw, 13),
                   Qt::AlignCenter, t);
    }
}

// ── Ziehen ───────────────────────────────────────────────────────────

void BandwidthFilterPane::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }

    m_drag = zoneAt(ev->pos().x());
    if (m_drag == Zone::None) { return; }

    m_dragStartX    = ev->pos().x();
    m_dragStartLow  = m_low;
    m_dragStartHigh = m_high;
    update();
}

void BandwidthFilterPane::mouseMoveEvent(QMouseEvent* ev)
{
    const int x = ev->pos().x();

    if (m_drag == Zone::None) {
        // Der Zeiger sagt VORHER, was ein Ziehen tun wird. Nur setzen,
        // wenn er sich aendert — jedes setCursor kostet auf macOS ein
        // CGImageCreate, und bei jeder Mausbewegung ist das zu viel.
        const Zone z = zoneAt(x);
        if (z != m_hover) {
            m_hover = z;
            const Qt::CursorShape want =
                (z == Zone::LowEdge || z == Zone::HighEdge)
                    ? Qt::SizeHorCursor
                    : (z == Zone::Body ? Qt::SizeAllCursor : Qt::ArrowCursor);
            if (cursor().shape() != want) { setCursor(want); }
            update();
        }
        return;
    }

    const int deltaHz = ((xToHz(x) - xToHz(m_dragStartX)) / kStepHz) * kStepHz;

    if (m_drag == Zone::Body) {
        // Die Breite bleibt. Deshalb die MITTE melden und nicht zwei
        // Kanten: nur so kann das Modell mit filterShift begrenzen und
        // die Breite am Rand erhalten.
        const int centre = (m_dragStartLow + m_dragStartHigh) / 2 + deltaHz;
        emit filterCentreChanged(centre);
        return;
    }

    int low  = m_dragStartLow;
    int high = m_dragStartHigh;
    if (m_drag == Zone::LowEdge)  { low  = m_dragStartLow  + deltaHz; }
    if (m_drag == Zone::HighEdge) { high = m_dragStartHigh + deltaHz; }

    // Die Kanten duerfen einander nicht ueberholen. Alles Weitere —
    // Seitenband, Deckel — entscheidet das Modell.
    if (high - low < kStepHz) {
        if (m_drag == Zone::LowEdge) { low  = high - kStepHz; }
        else                          { high = low  + kStepHz; }
    }
    emit filterChanged(low, high);
}

void BandwidthFilterPane::mouseReleaseEvent(QMouseEvent* ev)
{
    Q_UNUSED(ev)
    if (m_drag == Zone::None) { return; }
    m_drag = Zone::None;
    update();
}

void BandwidthFilterPane::leaveEvent(QEvent*)
{
    if (m_hover == Zone::None) { return; }
    m_hover = Zone::None;
    unsetCursor();
    update();
}

} // namespace Longpath
