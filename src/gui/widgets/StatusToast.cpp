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

#include <QHBoxLayout>
#include <QLabel>
#include <QShowEvent>
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

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 8, 12, 8);

    auto* msgLbl = new QLabel(message, this);
    msgLbl->setWordWrap(true);
    msgLbl->setStyleSheet(
        QStringLiteral("color: %1; font-size: 11px; border: none;")
            .arg(QString::fromLatin1(Style::kTextPrimary)));
    layout->addWidget(msgLbl, 1);

    setFixedWidth(kToastWidth);
    // Height follows the wrapped text rather than a fixed value: the
    // messages range from "TX > Slice B" to a two-line auto-connect
    // explanation, and a fixed height would clip the long ones.
    //
    // adjustSize() allein reichte nicht: es fragt den sizeHint, und der
    // kennt bei einem umbrechenden QLabel die Zielbreite noch nicht —
    // ein vierzeiliger Watchdog-Text stand unten abgeschnitten
    // (2026-09-22). Die Hoehe fuer GENAU diese Breite fragen.
    setFixedHeight(layout->totalHeightForWidth(kToastWidth));

    m_dismissTimer.setSingleShot(true);
    connect(&m_dismissTimer, &QTimer::timeout, this, &QWidget::close);
    m_dismissTimer.start(timeoutMs);
}

StatusToast::~StatusToast() = default;

void StatusToast::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);

    // Die Hoehe NOCH EINMAL rechnen, jetzt wo der Kasten wirklich
    // angezeigt wird.
    //
    // Der Konstruktor rechnet sie auch, aber zu einem Zeitpunkt, an dem
    // Qt das Stylesheet noch nicht angewandt hat: `font-size: 11px`
    // steht erst nach dem Polieren in der Schriftmetrik der
    // Beschriftung, und bis dahin antwortet heightForWidth() fuer eine
    // andere Schrift als die, die spaeter gemalt wird. Jede Abweichung
    // nach unten schneidet den Text ab.
    //
    // Betreiber am 2026-09-23, zum ZWEITEN Mal (das erste Mal am
    // 2026-09-22, damals von adjustSize() auf totalHeightForWidth()
    // umgestellt): der sechszeilige Hinweis "Das Geraet wurde gefunden,
    // liefert aber binnen 6 s keinen Datenstrom ..." stand unten
    // abgeschnitten. Hier liess er sich nicht nachstellen -- weder
    // offscreen noch mit dem echten Schriftsatz. Statt weiter zu raten,
    // was diesmal anders war, faellt die Abhaengigkeit von den
    // Metriken zum Bauzeitpunkt ganz weg.
    if (auto* lay = layout()) {
        const int needed = lay->totalHeightForWidth(width());
        if (needed > height()) { setFixedHeight(needed); }
    }
}

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
