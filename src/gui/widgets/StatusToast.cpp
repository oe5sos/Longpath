// =================================================================
// src/gui/widgets/StatusToast.cpp  (Longpath)
// =================================================================
//
// Longpath-original; no upstream port. See header for description.
//
// =================================================================
// Modification history (Longpath):
//   2026-07-30 Created in C++20/Qt6 for NereusSDR by J.J. Boyd (KG4VCF),
//              with AI-assisted transformation via Anthropic Claude Code.
// =================================================================
//
// no-port-check: Longpath-original

#include "gui/widgets/StatusToast.h"

#include "gui/StyleConstants.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>

namespace Longpath {

namespace {

/// Left accent colour per severity, taken from the same badge palette
/// the bottom bar uses so the two surfaces agree.
QString accentFor(ToastSeverity severity)
{
    switch (severity) {
        case ToastSeverity::Warning: return QString::fromLatin1(Style::kAmberText);
        case ToastSeverity::Error:   return QString::fromLatin1(Style::kRedBorder);
        case ToastSeverity::Info:    break;
    }
    return QString::fromLatin1(Style::kAccent);
}

// Wide enough for the longest notice in the tree (the multi-radio
// auto-connect notice) at two lines, without covering the panadapter.
constexpr int kToastWidth = 380;

} // namespace

StatusToast::StatusToast(const QString& message,
                         ToastSeverity severity,
                         int timeoutMs,
                         QWidget* parent)
    : QWidget(parent, Qt::FramelessWindowHint | Qt::Tool),
      m_message(message)
{
    setAttribute(Qt::WA_DeleteOnClose);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFocusPolicy(Qt::NoFocus);
    setToolTip(QStringLiteral("Click to dismiss"));

    setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border: 1px solid %2;"
        " border-left: 3px solid %3; border-radius: 6px; }"
    ).arg(QString::fromLatin1(Style::kPanelBg),
          QString::fromLatin1(Style::kBorder),
          accentFor(severity)));

    constexpr int kMarginLeft = 12;
    constexpr int kMarginTop = 8;
    constexpr int kMarginRight = 12;
    constexpr int kMarginBottom = 8;
    constexpr int kLabelFontPx = 11;

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(kMarginLeft, kMarginTop, kMarginRight, kMarginBottom);

    auto* msgLbl = new QLabel(message, this);
    msgLbl->setWordWrap(true);
    msgLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: %2px; border: none;")
            .arg(QString::fromLatin1(Style::kTextPrimary))
            .arg(kLabelFontPx));
    layout->addWidget(msgLbl, 1);

    setFixedWidth(kToastWidth);
    // Operator, 2026-09-15, live bench test (the ANVELINA connect-failure
    // toast): "wieder leider" -- adjustSize()'s own heightForWidth() pass
    // was still clipping a 3-sentence detail message to roughly one
    // line's height. Root cause: a QLabel's wordWrap-driven heightForWidth
    // needs the widget to already know its constrained width, which a
    // freshly-constructed, not-yet-shown/polished QHBoxLayout does not
    // reliably propagate on the very first sizeHint() query (a known Qt
    // limitation for a horizontal layout wrapping a single grown-text
    // child) -- so adjustSize() locked in a too-short height before the
    // real wrap height was ever known. Compute the needed height directly
    // from font metrics against the label's actual available width
    // instead of trusting layout heightForWidth propagation.
    //
    // font-size in the stylesheet above is a CSS string Qt applies at
    // polish/paint time, not reflected by msgLbl->font() yet at this
    // point in the constructor -- kLabelFontPx duplicates that same
    // value for the metrics query so the two stay in sync; if one
    // changes, so must the other.
    QFont measureFont = msgLbl->font();
    measureFont.setPixelSize(kLabelFontPx);
    const QFontMetrics fm(measureFont);
    const int textWidth = kToastWidth - kMarginLeft - kMarginRight;
    const int textHeight = fm.boundingRect(QRect(0, 0, textWidth, 0), Qt::TextWordWrap, message).height();
    const int fixedHeight = textHeight + kMarginTop + kMarginBottom;
    setFixedHeight(fixedHeight);

    m_dismissTimer.setSingleShot(true);
    connect(&m_dismissTimer, &QTimer::timeout, this, &QWidget::close);
    m_dismissTimer.start(timeoutMs);
}

StatusToast::~StatusToast() = default;

void StatusToast::refresh(int timeoutMs)
{
    m_dismissTimer.start(timeoutMs);
}

void StatusToast::mousePressEvent(QMouseEvent* event)
{
    // Any button dismisses. A notice the operator has already read
    // should not sit there for its full timeout blocking the view.
    event->accept();
    close();
}

} // namespace Longpath
