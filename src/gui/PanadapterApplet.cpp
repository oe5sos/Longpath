// no-port-check: AetherSDR-derived NereusSDR file. Per-pan container
// (SpectrumWidget host, slice association) is adapted structurally from
// AetherSDR src/gui/PanadapterApplet.{h,cpp} [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterApplet.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterApplet.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterApplet.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/widgets/SpectrumStatusOverlay.h"
#include "models/SliceModel.h"
#include "core/AppSettings.h"

#include <QAction>
#include <QContextMenuEvent>
#include <QMenu>
#include <QResizeEvent>
#include <QVBoxLayout>
#include "gui/styles/ThemeQss.h"
#include "gui/StyleConstants.h"
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>

namespace NereusSDR {

PanadapterApplet::PanadapterApplet(const QString& panId, QWidget* parent)
    : QWidget(parent)
    , m_panId(panId)
    , m_spectrum(new SpectrumWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Kopfleiste ────────────────────────────────────────────────────
    //
    // Zeus setzt ueber den Panadapter eine Zeile mit Punkt, Namen und
    // laufender Mittenfrequenz. Die fehlte hier ganz: der Panadapter
    // war die einzige Flaeche ohne Kopf, obwohl jede Applet daneben
    // einen hat.
    //
    // ABSICHTLICH OHNE die Zoom- und Geschwindigkeitsregler, die Zeus
    // dort ebenfalls zeigt. Zeus hat keine Overlay-Leiste; wir haben
    // eine, und sie traegt beide Regler bereits. Sie hier zu
    // wiederholen waere genau die Doppelung, die an diesem Tag zweimal
    // aufgeraeumt wurde.
    // Entwurf 4 vom 2026-08-19, auf Ansage des Betreibers („padapter
    // noch immer nicht in einem window"): DIESELBE Handschrift wie der
    // Kopf jedes Containers — gelber Griffstrich links, Name in
    // Versalien, rechts der Schalter. Vorher trug der Panadapter einen
    // eigenen Kopf mit eigenen Abstaenden und keinem Schalter; er sah
    // aus wie ein Fremdkoerper zwischen den Kacheln.
    auto* head = new QWidget(this);
    head->setFixedHeight(22);
    auto* headLay = new QHBoxLayout(head);
    headLay->setContentsMargins(5, 0, 3, 0);
    headLay->setSpacing(6);

    // Der gelbe Strich wie bei Zeus Link und wie im Container-Kopf.
    m_grip = new QLabel(head);
    m_grip->setFixedWidth(3);
    m_grip->setStyleSheet(QStringLiteral(
        "background: %1; border-radius: 1px; margin: 4px 0;")
        .arg(QLatin1String(Style::kAmberText)));
    headLay->addWidget(m_grip);

    // Der gruene Punkt bleibt: er sagt, dass Daten fliessen. Ein leeres
    // Bild ohne ihn ist ein Fehler, mit ihm eine stille Frequenz.
    auto* dot = new QLabel(QString::fromUtf8("\xe2\x97\x8f"), head);
    dot->setStyleSheet(Style::themed(QStringLiteral(
        "QLabel { color: %1; background: transparent; }")
        .arg(Style::kGreenText)));
    headLay->addWidget(dot);

    m_titleLabel = new QLabel(QStringLiteral("PANADAPTER"), head);
    m_titleLabel->setFont(Style::capsFont(m_titleLabel->font(),
                                          Style::kFontCaption));
    // Ziffernbreite: die Mittenfrequenz laeuft beim Wischen mit, und
    // eine Zahl, die dabei die Breite wechselt, zappelt.
    QFont headFont = m_titleLabel->font();
    headFont.setStyleHint(QFont::Monospace);
    m_titleLabel->setFont(headFont);
    m_titleLabel->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: transparent; letter-spacing: 1px; }")
        .arg(QLatin1String(Style::kTextPrimary)));
    headLay->addWidget(m_titleLabel);
    headLay->addStretch(1);

    // Ablösen und zurueck, EIN Schalter. Bisher lag „Float this pan" nur
    // im Rechtsklick-Menue und der Rueckweg gar nirgends — man musste
    // das Fenster schliessen und wissen, dass genau das zurueckdockt.
    m_btnFloat = new QPushButton(QStringLiteral("\u2197"), head);
    m_btnFloat->setFixedSize(20, 20);
    m_btnFloat->setStyleSheet(QStringLiteral(
        "QPushButton { background: transparent; border: none; color: %1;"
        "  font-size: 11px; padding: 0; }"
        "QPushButton:hover { background: %2; color: %3; }")
        .arg(QLatin1String(Style::kTextSecondary),
             QLatin1String(Style::kButtonHover),
             QLatin1String(Style::kTextPrimary)));
    headLay->addWidget(m_btnFloat);
    connect(m_btnFloat, &QPushButton::clicked, this, [this]() {
        if (m_floating) { emit dockRequested(m_panId); }
        else            { emit floatRequested(m_panId); }
    });
    setFloatingIndicator(false);

    head->setStyleSheet(QStringLiteral(
        "QWidget { background: %1; border-bottom: 1px solid %2; }")
        .arg(QLatin1String(Style::kTitleGradBot),
             QLatin1String(Style::kBorderSubtle)));
    layout->addWidget(head, 0);

    layout->addWidget(m_spectrum, 1);

    // Die Mitte laeuft mit. Ohne Verbindung steht nur der Name da —
    // eine Frequenz ohne Radio waere eine Behauptung.
    connect(m_spectrum, &SpectrumWidget::frequencyRangeChanged, this,
            [this](double centerHz, double) {
        m_titleLabel->setText(QString::fromUtf8("PANADAPTER \xc2\xb7 %1 MHz")
            .arg(centerHz / 1.0e6, 0, 'f', 6));
    });

    // Phase 3F Sub-Epic E Task 2: per-pan status overlay in top-right.
    // Positioned manually in resizeEvent so the spectrum host owns the full
    // applet area underneath. Parented to `this`, not m_spectrum, so it sits
    // above the SpectrumWidget's QRhi surface without becoming a child of it.
    m_statusOverlay = new SpectrumStatusOverlay(this);
    m_statusOverlay->raise();

    // Phase 3F: clicking anywhere in this pan makes it the active pan.
    //
    // `activated` was declared with the comment "emitted on any click within
    // applet" and had no emitter, so PanadapterStack::setActivePan was only
    // ever called for the first pan created. Every consumer of activePanId()
    // was therefore pinned to pan-0 for the session -- including "Add slice on
    // active pan", which is the ONLY route that puts a slice on a pan (the
    // per-pan +RX button is still disabled, NYI). A second pan could be
    // created but never given a slice, so it showed no VFO flag, no dial
    // frequency, and no FFT subscription.
    //
    // Ported from AetherSDR PanadapterApplet.cpp:629 [@6a142807]:
    //     if (ev->type() == QEvent::MouseButtonPress)
    //         emit activated(m_panId);
    // in eventFilter rather than mousePressEvent, because the spectrum host
    // and the overlay consume presses before they ever reach this widget.
    m_spectrum->installEventFilter(this);
    m_statusOverlay->installEventFilter(this);
    installEventFilter(this);

    connect(m_statusOverlay, &SpectrumStatusOverlay::txBadgeClicked, this,
            [this]() { emit txBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::wideBadgeClicked, this,
            [this]() { emit wideBadgeClicked(m_panId); });
    connect(m_statusOverlay, &SpectrumStatusOverlay::chainTagClicked, this,
            [this](int chainIdx) { emit chainTagClicked(m_panId, chainIdx); });

    // Phase 3F Sub-Epic F Task 13: restore persisted extended-view
    // toggle (default true) and apply it to the embedded spectrum immediately.
    auto& s = AppSettings::instance();
    const QString stored = s.value(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                                   QStringLiteral("True")).toString();
    setExtendedViewEnabled(stored == QStringLiteral("True"));
}

PanadapterApplet::~PanadapterApplet() = default;

void PanadapterApplet::addSlice(int sliceIndex)
{
    m_associatedSlices.insert(sliceIndex);
    if (m_activeSliceIndex == -1) {
        setActiveSliceIndex(sliceIndex);
    }
}

void PanadapterApplet::removeSlice(int sliceIndex)
{
    m_associatedSlices.remove(sliceIndex);
    if (m_activeSliceIndex == sliceIndex) {
        m_activeSliceIndex = m_associatedSlices.isEmpty() ? -1 : *m_associatedSlices.begin();
        emit activeSliceChanged(m_panId, m_activeSliceIndex);
    }
}

void PanadapterApplet::setActiveSliceIndex(int sliceIndex)
{
    if (m_activeSliceIndex == sliceIndex) { return; }
    m_activeSliceIndex = sliceIndex;
    // Bench-reported 2026-07-28 (Sub-Epic J): the newly-active slice's flag
    // must come to the front of THIS pan's stack -- see
    // SpectrumWidget::setFrontSliceIndex for why a plain raise() would not
    // hold. This is the single writer of m_activeSliceIndex from outside
    // (the addSlice seed path below also routes through here), so hanging
    // the z-order pin off it covers every caller, including
    // PanadapterStack::setActiveSliceOnHostingPan, without each one having
    // to remember to raise the flag itself.
    if (m_spectrum) {
        m_spectrum->setFrontSliceIndex(sliceIndex);
    }
    emit activeSliceChanged(m_panId, sliceIndex);
}

void PanadapterApplet::setCenterMhz(double mhz) { m_centerMhz = mhz; }
void PanadapterApplet::setBandwidthMhz(double bw) { m_bandwidthMhz = bw; }

// Phase 3F: any press inside this pan claims active-pan status. Watches the
// spectrum host and the overlay as well as the applet itself, because both sit
// on top of it and accept the press first.
//
// Ported from AetherSDR PanadapterApplet.cpp:628-630 [@6a142807]:
//     if (ev->type() == QEvent::MouseButtonPress)
//         emit activated(m_panId);
//     return QWidget::eventFilter(obj, ev);
//
// Observing, not intercepting: always falls through to the base implementation
// so the spectrum keeps its own click handling (tune, drag, zoom).
bool PanadapterApplet::eventFilter(QObject* obj, QEvent* ev)
{
    if (ev->type() == QEvent::MouseButtonPress) {
        emit activated(m_panId);
    }
    return QWidget::eventFilter(obj, ev);
}

void PanadapterApplet::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    repositionStatusOverlay();
}

QByteArrayList PanadapterApplet::statusOverlaySliceProperties()
{
    // Every SliceModel property updateStatusOverlay reads below. Keep the two
    // in step: a field painted with no entry here is a field that goes stale.
    //
    // streamIndex earns its place even though the overlay never paints it --
    // it is what the caller's chain resolution keys off, so a slice moving
    // between DDC streams changes the CH tag without touching any painted
    // property.
    //
    // sliceLetter is deliberately absent: the letter is derived from the
    // stable slice id, which cannot change for the life of the slice.
    return QByteArrayList{
        QByteArrayLiteral("frequency"),
        QByteArrayLiteral("dspMode"),
        QByteArrayLiteral("txSlice"),
        QByteArrayLiteral("diversityEnabled"),
        QByteArrayLiteral("psPaused"),
        QByteArrayLiteral("streamIndex"),
    };
}

void PanadapterApplet::updateStatusOverlay(SliceModel* slice, int chainIndex)
{
    if (!m_statusOverlay || !slice) { return; }
    // SliceModel::sliceLetter() now derives from the stable slice id, so the
    // local workaround this used to carry is gone. (It reported 'A' for every
    // slice while the letter was a stored member no one ever set.)
    m_statusOverlay->setSliceLetter(slice->sliceLetter());
    // SliceModel::frequency() returns double Hz (default 14225000.0 = 14.225 MHz).
    // Cast directly; no MHz->Hz conversion.
    m_statusOverlay->setFrequencyHz(static_cast<qint64>(slice->frequency()));
    m_statusOverlay->setMode(SliceModel::modeName(slice->dspMode()));
    // An unbound slice (chainIndex < 0) is on no chain at all. Holding the
    // last tag is honest about "unknown"; writing 0 would assert it is on
    // chain 0, which is the failure mode this parameter exists to stop.
    if (chainIndex >= 0) {
        m_statusOverlay->setChainIndex(chainIndex);
    }
    m_statusOverlay->setTxBound(slice->isTxSlice());
    m_statusOverlay->setDiversityActive(slice->diversityEnabled());
    m_statusOverlay->setPsPaused(slice->psPaused());
    // TX / DIV / PS pills all change the strip's width; re-anchor.
    repositionStatusOverlay();
}

QChar PanadapterApplet::statusSliceLetter() const
{
    return m_statusOverlay ? m_statusOverlay->sliceLetter() : QChar();
}

qint64 PanadapterApplet::statusFrequencyHz() const
{
    return m_statusOverlay ? m_statusOverlay->frequencyHz() : 0;
}

QString PanadapterApplet::statusMode() const
{
    return m_statusOverlay ? m_statusOverlay->mode() : QString();
}

int PanadapterApplet::statusChainIndex() const
{
    return m_statusOverlay ? m_statusOverlay->chainIndex() : 0;
}

// Phase 3F: WIDE pill forwarder. Kept separate from updateStatusOverlay
// because the two have different triggers and different sources: the slice
// fields refresh when the active slice changes, whereas the bypass state is
// a property of the chain feeding this pan and changes on band crossings,
// slice add/remove, wideband toggles and Filter Policy edits -- none of
// which need be the active slice, or any slice on this pan at all.
// Right-align the status strip, clear of the dBm scale strip.
//
// Must re-run whenever a pill lights or goes dark, not only on resize.
// SpectrumStatusOverlay::paintEvent grows minimumWidth() to fit the pills it
// just drew, and setGeometry clamps a narrower request up to that minimum --
// expanding the widget RIGHTWARD from its fixed x. So a strip positioned while
// only "CH 0" was showing crept back under the dBm range arrows the moment the
// TX or WIDE pill appeared. Re-anchoring from the current hint puts the right
// edge back where it belongs.
void PanadapterApplet::repositionStatusOverlay()
{
    if (!m_statusOverlay) { return; }
    const QSize hint = m_statusOverlay->sizeHint();
    // Clear of the dBm scale strip: its range up/down arrows sit at the top of
    // that column, and the status strip accepts mouse events for its own
    // badges, so any overlap makes the arrows both hard to read and hard to
    // hit.
    const int reserved = m_spectrum ? m_spectrum->reservedRightEdgeWidth() : 0;
    m_statusOverlay->setGeometry(width() - hint.width() - 8 - reserved, 8,
                                 hint.width(), hint.height());
}

void PanadapterApplet::setWideBpf(bool wide, const QString& reason)
{
    if (!m_statusOverlay) { return; }
    m_statusOverlay->setWideBpf(wide, reason);
    // The WIDE pill changes the strip's width; re-anchor its right edge.
    repositionStatusOverlay();
}

bool PanadapterApplet::wideBpf() const
{
    return m_statusOverlay && m_statusOverlay->wideBpf();
}

QString PanadapterApplet::wideReason() const
{
    return m_statusOverlay ? m_statusOverlay->wideReason() : QString();
}

// Phase 3F Sub-Epic F Task 13: operator-toggleable Extended view.
// Default is true so the SpectrumWidget zoom auto-derive (Task 7-10)
// gets to decide extendedMode based on bandwidth vs DDC sample rate.
// This toggle is permission, not the actual state: at normal zoom the
// wideband stream stays off even when permission is enabled.
void PanadapterApplet::setExtendedViewEnabled(bool on)
{
    const bool changed = (m_extendedViewEnabled != on);
    m_extendedViewEnabled = on;
    if (changed) {
        auto& s = AppSettings::instance();
        s.setValue(QStringLiteral("Pan_%1_ExtendedView").arg(m_panId),
                   on ? QStringLiteral("True") : QStringLiteral("False"));
    }
    if (m_spectrum) {
        m_spectrum->setExtendedViewAllowed(on);
    }
}

// Phase 3F Sub-Epic F Task 13: right-click context menu, originally a single
// checkable Extended view entry. Pops at the global cursor position.
void PanadapterApplet::contextMenuEvent(QContextMenuEvent* event)
{
    QMenu* menu = buildContextMenu(this);
    menu->exec(event->globalPos());
    menu->deleteLater();
}

// Task B5 (bottom-banner + pan-menu epic, design doc s8.5): "Add slice on
// this pan" / "Float this pan" used to live on the +PAN button's dropdown
// (Phase 3F Sub-Epic D Task 10, retired in Task B4) and resolved through
// m_panStack->activePanId() -- "active pan" meaning whichever pan happened
// to be active when the operator reached a button sitting nowhere near any
// pan. A control drawn ON a pan targets THAT pan, so both move here and
// carry this applet's own panId(). Neither action exists on AetherSDR's pan
// menu (design doc s8.5), so this is NereusSDR-original, not a further port.
//
// Split out of contextMenuEvent so a test can build the menu, find +
// trigger() these two actions by their operator-facing text, and assert the
// resulting signal without exec()-ing a real nested event loop -- mirrors
// AmpApplet::buildContextMenu() / buildContextMenuForTesting().
QMenu* PanadapterApplet::buildContextMenu(QObject* parent)
{
    auto* menu = new QMenu(qobject_cast<QWidget*>(parent));

    menu->addAction(tr("Add slice on this pan"), this, [this]() {
        emit addSliceRequested(panId());
    });
    menu->addAction(tr("Float this pan"), this, [this]() {
        emit floatRequested(panId());
    });
    menu->addSeparator();

    QAction* extAct = menu->addAction(tr("Extended view (wideband wings)"));
    extAct->setCheckable(true);
    extAct->setChecked(m_extendedViewEnabled);
    connect(extAct, &QAction::toggled, this, &PanadapterApplet::setExtendedViewEnabled);

    return menu;
}

// ↗ heisst „ablösen", ↙ heisst „zurueck in die Anordnung".
void PanadapterApplet::setFloatingIndicator(bool floating)
{
    m_floating = floating;
    if (!m_btnFloat) { return; }
    m_btnFloat->setText(floating ? QStringLiteral("\u2199")
                                 : QStringLiteral("\u2197"));
    m_btnFloat->setToolTip(floating
        ? QStringLiteral("Back into the layout")
        : QStringLiteral("Detach into its own window"));
}

} // namespace NereusSDR
