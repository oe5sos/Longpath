// =================================================================
// src/gui/applets/eq/ClientEqOutputFader.cpp  (NereusSDR)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/ClientEqOutputFader.cpp at 31b29583
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 §5.
//
// Ported at the bench's request to reproduce AetherSDR's equaliser
// exactly — its display and its behaviour — rather than to approximate
// it. The DSP it drives, core/strip/ClientEq, was already a verbatim
// port of the same upstream, so the pair are back together.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-09 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 NereusSDR; include paths rebased onto
//                 core/strip/ and gui/applets/eq/. Behaviour unchanged.
// =================================================================

#include "gui/applets/eq/ClientEqOutputFader.h"
#include "gui/applets/eq/EqPalette.h"
#include "gui/StyleConstants.h"

#include <QEvent>
#include <QFocusEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLinearGradient>
#include <QLocale>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include "gui/StyleConstants.h"


namespace {

// Same theme seam as StripEqPanel — see the long note there. Two token
// substitutions, so the ported stylesheet strings stay untouched.
void applyThemed(QWidget* w, QString sheet)
{
    if (!w) { return; }
    sheet.replace(QStringLiteral("{{color.text.primary}}"),
                  QString::fromLatin1(Longpath::Style::kTextPrimary));
    sheet.replace(QStringLiteral("{{color.text.secondary}}"),
                  QString::fromLatin1(Longpath::Style::kTextSecondary));
    sheet.replace(QStringLiteral("{{color.background.0}}"),
                  QString::fromLatin1(Longpath::Style::kAppBg));
    sheet.replace(QStringLiteral("{{color.background.1}}"),
                  QString::fromLatin1(Longpath::Style::kButtonBg));
    sheet.replace(QStringLiteral("{{color.background.3}}"),
                  Longpath::EqPalette::textScale().name());
    w->setStyleSheet(sheet);
}

} // namespace

namespace Longpath {

namespace {

float linearToDb(float linear)
{
    if (linear <= 1e-6f) return -120.0f;
    return 20.0f * std::log10(linear);
}

float dbToLinear(float db)
{
    return std::pow(10.0f, db / 20.0f);
}

// Attack / release for the displayed peak — fast rise, slow fall so the
// bar tracks transients but doesn't flicker.
constexpr float kPeakAttack  = 0.6f;
constexpr float kPeakRelease = 0.08f;

} // namespace

ClientEqOutputFader::ClientEqOutputFader(QWidget* parent) : QWidget(parent)
{
    // Total width = label column + gap + bar + overhang each side.
    setFixedWidth(kLabelColW + kGap + kBarW + kHandleOverhang * 2 + 2);
    setMinimumHeight(160);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::ArrowCursor);
    setMouseTracking(false);
    setToolTip(
        "Output gain (dB). Drag to set, wheel for fine step,\n"
        "double-click to reset to 0 dB.");

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 2, 0, 2);
    root->setSpacing(0);

    auto* top = new QLabel("OUT");
    top->setAlignment(Qt::AlignCenter);
    applyThemed(top, "QLabel { color: {{color.text.secondary}}; font-size: 11px; font-weight: bold;"
        " background: transparent; border: none; }");
    root->addWidget(top);

    root->addStretch(1);

    // Inline-editable value at the bottom of the fader.  Click to focus,
    // type a dB value, Enter or focus-out to commit (clamped to range).
    // Looks identical to a label until focused; subtle inset + cyan
    // border on focus indicates edit mode (matches ClientCompKnob).
    m_valueEdit = new QLineEdit;
    m_valueEdit->setAlignment(Qt::AlignCenter);
    m_valueEdit->setFrame(false);
    applyThemed(m_valueEdit, "QLineEdit { color: {{color.text.primary}}; font-size: 11px; font-weight: bold;"
        " background: transparent; border: 1px solid transparent;"
        " border-radius: 6px; padding: 0;"
        " selection-background-color: {{color.background.2}}; }"
        "QLineEdit:focus { background: {{color.background.0}}; border: 1px solid {{color.accent}}; }");
    m_valueEdit->installEventFilter(this);
    root->addWidget(m_valueEdit);

    connect(m_valueEdit, &QLineEdit::returnPressed, this, [this] {
        commitValueEdit();
        m_valueEdit->clearFocus();
    });
    connect(m_valueEdit, &QLineEdit::editingFinished, this, [this] {
        commitValueEdit();
    });

    refreshValueLabel();
}

void ClientEqOutputFader::commitValueEdit()
{
    if (!m_valueEdit) return;
    static thread_local bool s_committing = false;
    if (s_committing) return;
    s_committing = true;
    const QString raw = m_valueEdit->text().trimmed();
    bool ok = false;
    double v = QLocale().toDouble(raw, &ok);
    if (!ok) {
        QString cleaned;
        cleaned.reserve(raw.size());
        for (QChar c : raw) {
            if (c.isDigit() || c == QChar('.') || c == QChar('-')
                || c == QChar('+') || c == QChar('e') || c == QChar('E'))
                cleaned.append(c);
        }
        v = cleaned.toDouble(&ok);
    }
    if (ok) {
        const float db = std::clamp(static_cast<float>(v),
                                    kGainMinDb, kGainMaxDb);
        m_gain = dbToLinear(db);
        refreshValueLabel();
        emit gainChanged(m_gain);
        update();
    } else {
        refreshValueLabel();
    }
    s_committing = false;
}

bool ClientEqOutputFader::eventFilter(QObject* obj, QEvent* ev)
{
    if (obj == m_valueEdit) {
        if (ev->type() == QEvent::Wheel) {
            wheelEvent(static_cast<QWheelEvent*>(ev));
            return true;
        }
        if (ev->type() == QEvent::KeyPress) {
            auto* ke = static_cast<QKeyEvent*>(ev);
            if (ke->key() == Qt::Key_Escape) {
                QSignalBlocker b(m_valueEdit);
                refreshValueLabel();
                m_valueEdit->clearFocus();
                return true;
            }
        }
        if (ev->type() == QEvent::FocusIn) {
            // Show bare number on focus so the user types just digits.
            QSignalBlocker b(m_valueEdit);
            const float db = linearToDb(m_gain);
            m_valueEdit->setText(QString::number(db, 'f', 1));
            m_valueEdit->selectAll();
        } else if (ev->type() == QEvent::FocusOut) {
            refreshValueLabel();
        }
    }
    return QWidget::eventFilter(obj, ev);
}

void ClientEqOutputFader::setGainLinear(float linear)
{
    m_gain = std::clamp(linear, 0.0f, 4.0f);
    refreshValueLabel();
    update();
}

void ClientEqOutputFader::setPeakLinear(float peakLinear)
{
    const float peakDb = linearToDb(std::max(peakLinear, 1e-6f));
    const float alpha = (peakDb > m_smoothedPeak) ? kPeakAttack : kPeakRelease;
    m_smoothedPeak += alpha * (peakDb - m_smoothedPeak);
    update();
}

void ClientEqOutputFader::refreshValueLabel()
{
    if (!m_valueEdit || m_valueEdit->hasFocus()) return;
    const float db = linearToDb(m_gain);
    QSignalBlocker b(m_valueEdit);
    if (db <= kGainMinDb + 0.05f) {
        m_valueEdit->setText("-inf");
    } else {
        m_valueEdit->setText(QString::asprintf("%+.1f dB", db));
    }
}

void ClientEqOutputFader::setGainFromY(int y)
{
    const float norm = 1.0f - std::clamp(
        static_cast<float>(y - m_stripTop) / std::max(1, m_stripH), 0.0f, 1.0f);
    const float db = kGainMinDb + norm * (kGainMaxDb - kGainMinDb);
    m_gain = dbToLinear(db);
    refreshValueLabel();
    emit gainChanged(m_gain);
    update();
}

void ClientEqOutputFader::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    // The strip lives between the OUT label at the top and the value
    // label at the bottom — paintEvent receives the full widget rect and
    // we carve out a fixed vertical band in the middle.
    const int topLabelH = 16;
    const int botLabelH = 14;
    const int stripTop  = topLabelH + kStripTopPad;
    const int stripBot  = height() - botLabelH - kStripBottomPad;
    m_stripTop = stripTop;
    m_stripH   = std::max(1, stripBot - stripTop);

    const int barLeft = kLabelColW + kGap + kHandleOverhang;
    const QRect barR(barLeft, stripTop, kBarW, m_stripH);

    // Background for the bar — dark inset.
    p.fillRect(barR, QColor(EqPalette::panelBg().name()));

    // Level fill — gradient bottom green → top red, clipped to the level
    // height derived from the smoothed peak.
    const float peakNorm = std::clamp(
        (m_smoothedPeak - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb),
        0.0f, 1.0f);
    const int fillH = static_cast<int>(peakNorm * m_stripH);
    if (fillH > 0) {
        const QRect fill(barR.x(), barR.y() + m_stripH - fillH,
                         kBarW, fillH);
        QLinearGradient grad(0, barR.y() + m_stripH, 0, barR.y());
        grad.setColorAt(0.0, QColor(EqPalette::goodDim().name()));   // green bottom
        grad.setColorAt(0.55, QColor(EqPalette::good().name()));  // lime
        grad.setColorAt(0.80, QColor(EqPalette::warn().name()));  // amber
        grad.setColorAt(0.95, QColor(EqPalette::bad().name()));  // red top
        grad.setColorAt(1.0, QColor(EqPalette::badBg().name()));
        p.fillRect(fill, grad);
    }

    // Bar outline.
    p.setPen(QPen(QColor(EqPalette::buttonHi().name()), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(barR.adjusted(0, 0, -1, -1));

    // dB scale labels + tick marks on the left side.
    QFont f = p.font();
    f.setPixelSize(9);
    p.setFont(f);
    const QFontMetrics fm(f);
    const int textRight = kLabelColW - 2;

    struct Tick { float db; const char* label; };
    static constexpr Tick kTicks[] = {
        {   0.0f,  "0" },
        {  -6.0f,  "-6" },
        { -12.0f,  "-12" },
        { -20.0f,  "-20" },
        { -40.0f,  "-40" },
    };
    for (const auto& t : kTicks) {
        const float norm = (t.db - kMeterMinDb) / (kMeterMaxDb - kMeterMinDb);
        const int y = stripTop + static_cast<int>((1.0f - norm) * m_stripH);

        p.setPen(QColor(EqPalette::textDim().name()));
        const QString s = QString::fromLatin1(t.label);
        const int tw = fm.horizontalAdvance(s);
        const int ty = std::clamp(y + fm.ascent() / 2 - 1,
                                  stripTop + fm.ascent() - 1,
                                  stripTop + m_stripH - 1);
        p.drawText(textRight - tw, ty, s);

        p.setPen(QColor(QString::fromLatin1(Style::kTextInactive)));
        p.drawLine(textRight, y, barLeft - 1, y);
    }

    // Fader handle — horizontal bar that overhangs the meter on both
    // sides so it's easy to grab without covering the level colour.
    const float gainDb = std::clamp(linearToDb(m_gain),
                                    kGainMinDb, kGainMaxDb);
    const float gainNorm = (kGainMaxDb - gainDb) / (kGainMaxDb - kGainMinDb);
    const int handleY = stripTop + static_cast<int>(gainNorm * m_stripH);
    const QRect handleR(barLeft - kHandleOverhang,
                        handleY - kHandleH / 2,
                        kBarW + kHandleOverhang * 2,
                        kHandleH);
    p.setPen(QPen(QColor(EqPalette::insetBg().name()), 1));
    p.setBrush(QColor(EqPalette::textBright().name()));   // cream / bright off-white
    p.drawRect(handleR);
    // Centre line — a single pixel on the handle so the exact gain level
    // reads clearly against the bar's colour.
    p.setPen(QColor(EqPalette::buttonBg().name()));
    p.drawLine(handleR.left() + 1, handleY,
               handleR.right() - 1, handleY);

    Q_UNUSED(barR);
}

void ClientEqOutputFader::mousePressEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
        setGainFromY(ev->pos().y());
        ev->accept();
        return;
    }
    QWidget::mousePressEvent(ev);
}

void ClientEqOutputFader::mouseMoveEvent(QMouseEvent* ev)
{
    if (m_dragging) {
        setGainFromY(ev->pos().y());
        ev->accept();
        return;
    }
    QWidget::mouseMoveEvent(ev);
}

void ClientEqOutputFader::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_dragging && ev->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
        ev->accept();
        return;
    }
    QWidget::mouseReleaseEvent(ev);
}

void ClientEqOutputFader::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (ev->button() == Qt::LeftButton) {
        m_gain = 1.0f;  // 0 dB
        refreshValueLabel();
        emit gainChanged(m_gain);
        update();
        ev->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(ev);
}

void ClientEqOutputFader::wheelEvent(QWheelEvent* ev)
{
    // 0.5 dB per notch (12 notches for a full deg of the wheel).
    const int notches = ev->angleDelta().y() / 120;
    if (notches == 0) { QWidget::wheelEvent(ev); return; }
    const float db = std::clamp(linearToDb(m_gain) + 0.5f * notches,
                                kGainMinDb, kGainMaxDb);
    m_gain = dbToLinear(db);
    refreshValueLabel();
    emit gainChanged(m_gain);
    update();
    ev->accept();
}

} // namespace Longpath
