// =================================================================
// src/gui/applets/eq/ClientEqEditorCanvas.cpp  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/gui/ClientEqEditorCanvas.cpp at 31b29583
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
// Modification history (Longpath):
//   2026-08-09 — Ported to NereusSDR by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 Longpath; include paths rebased onto
//                 core/strip/ and gui/applets/eq/. Behaviour unchanged.
// =================================================================

#include "gui/applets/eq/ClientEqEditorCanvas.h"
#include "gui/ScopedChildWidget.h"
#include "gui/applets/eq/EqFilterRing.h"
#include "gui/applets/eq/EqHost.h"
#include "gui/applets/eq/StripEqSimpleBar.h"
#include "core/strip/ClientEq.h"

#include <QMouseEvent>
#include <QWheelEvent>
#include <QContextMenuEvent>
#include <QMenu>
#include <QAction>
#include <QPointF>
#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {
constexpr float kHandleHitRadius = 8.0f;
}

ClientEqEditorCanvas::ClientEqEditorCanvas(QWidget* parent)
    : ClientEqCurveWidget(parent)
{
    setMinimumHeight(260);
    setCursor(Qt::CrossCursor);
    setMouseTracking(true);
}

void ClientEqEditorCanvas::setAudioEngine(EqHost* engine)
{
    m_audio = engine;
}

void ClientEqEditorCanvas::setSimpleMode(bool on)
{
    if (m_simpleMode == on) { return; }
    m_simpleMode = on;
    m_draggingBand = -1;
    m_draggingCutoff = CutoffEdge::None;
    setSelectedBand(-1);
    // Nur zeichnen, was sich auch anfassen laesst. Ein Punkt, der auf
    // einen Zug nicht reagiert, ist schlimmer als kein Punkt.
    quint32 mask = 0xffffffffu;
    if (on) {
        mask = 0u;
        for (const auto& sl : StripEqSimpleBar::bandSlots()) {
            if (sl.band >= 0 && sl.band < 32) {
                mask |= (1u << sl.band);
            }
        }
    }
    setHandleMask(mask);
    update();
}

namespace {

// Gehoert dieses Band zu den sechs benannten Reglern?
bool isSimpleBand(int idx)
{
    for (const auto& s : StripEqSimpleBar::bandSlots()) {
        if (s.band == idx) { return true; }
    }
    return false;
}

} // namespace

int ClientEqEditorCanvas::hitTestHandle(const QPointF& pos) const
{
    if (!m_eq) return -1;
    const int n = m_eq->activeBandCount();
    int best = -1;
    float bestDist = kHandleHitRadius * kHandleHitRadius;
    for (int i = 0; i < n; ++i) {
        if (m_simpleMode && !isSimpleBand(i)) { continue; }
        const auto bp = m_eq->band(i);
        const bool isSlope = (bp.type == ClientEq::FilterType::LowPass
                           || bp.type == ClientEq::FilterType::HighPass);
        const float handleDb = isSlope ? 0.0f : bp.gainDb;
        const QPointF c(freqToX(bp.freqHz), dbToY(handleDb));
        const float dx = static_cast<float>(pos.x() - c.x());
        const float dy = static_cast<float>(pos.y() - c.y());
        const float d2 = dx * dx + dy * dy;
        if (d2 < bestDist) {
            bestDist = d2;
            best = i;
        }
    }
    return best;
}

ClientEqEditorCanvas::CutoffEdge
ClientEqEditorCanvas::hitTestCutoffEdge(const QPointF& pos) const
{
    constexpr int kHitTol = 5;
    if (m_simpleMode) { return CutoffEdge::None; }
    // Don't intercept clicks in the bottom band-plan strip area.
    if (pos.y() > height() - kAudioBandStripPx) return CutoffEdge::None;

    const int lo = filterLowCutHz();
    const int hi = filterHighCutHz();
    int bestDist = kHitTol + 1;
    CutoffEdge best = CutoffEdge::None;
    if (lo > 0) {
        const int lx = static_cast<int>(freqToX(static_cast<float>(lo)));
        const int dx = static_cast<int>(std::abs(pos.x() - lx));
        if (dx <= kHitTol && dx < bestDist) { bestDist = dx; best = CutoffEdge::Low; }
    }
    if (hi > 0) {
        const int hx = static_cast<int>(freqToX(static_cast<float>(hi)));
        const int dx = static_cast<int>(std::abs(pos.x() - hx));
        if (dx <= kHitTol && dx < bestDist) { bestDist = dx; best = CutoffEdge::High; }
    }
    return best;
}

void ClientEqEditorCanvas::persist()
{
    if (m_audio) m_audio->saveClientEqSettings();
    emit bandsChanged();
    update();
}

void ClientEqEditorCanvas::mousePressEvent(QMouseEvent* ev)
{
    if (!m_eq) { QWidget::mousePressEvent(ev); return; }
    if (ev->button() != Qt::LeftButton) { QWidget::mousePressEvent(ev); return; }

    const int idx = hitTestHandle(ev->position());
    if (idx < 0) {
        // Cutoff-edge drag has lower priority than band handles, but
        // higher than clearing selection.  Start a cutoff drag if the
        // click lands on (or within ~5 px of) one of the dashed lines.
        const CutoffEdge edge = hitTestCutoffEdge(ev->position());
        if (edge != CutoffEdge::None) {
            m_draggingCutoff = edge;
            setCursor(Qt::SizeHorCursor);
            ev->accept();
            return;
        }
        // Clicking empty canvas clears selection so the icon row + param
        // row lose their highlight. Keeps the UI honest about where focus is.
        setSelectedBand(-1);
        QWidget::mousePressEvent(ev);
        return;
    }

    setSelectedBand(idx);

    const auto bp = m_eq->band(idx);
    // Frueher wurde ein Band hier eingeschaltet -- beim Druecken, vor
    // jeder Bewegung. Zehn Griffe liegen auf der Nulllinie, also war
    // jeder Klick auf die Linie schon ein Eingriff in den Ton. Jetzt
    // schaltet erst das wirkliche Ziehen ein (mouseMoveEvent).
    m_draggingBand = idx;
    m_dragMoved    = false;
    m_achse        = Achse::Offen;
    m_dragStart    = ev->position();
    m_dragStartFreqHz = bp.freqHz;
    m_dragStartGainDb = bp.gainDb;
    m_dragStartType   = bp.type;
    m_dragStartQ      = bp.q;
    setCursor(Qt::ClosedHandCursor);
    ev->accept();
}

void ClientEqEditorCanvas::mouseMoveEvent(QMouseEvent* ev)
{
    // Cutoff-edge drag — convert mouseX to Hz, clamp, and emit the
    // updated audio-domain low/high so the editor can write back to
    // TransmitModel (TX) or the active SliceModel (RX).  The display
    // updates locally via setFilterCutoffs() so the line tracks the
    // cursor without waiting for the radio's status echo.
    if (m_draggingCutoff != CutoffEdge::None) {
        constexpr int kMinHz = 20;
        constexpr int kMaxHz = 10000;
        constexpr int kMinSpan = 50;  // FlexLib's filter-width minimum
        const int newHz = static_cast<int>(std::round(
            xToFreq(static_cast<float>(ev->position().x()))));
        int lo = filterLowCutHz();
        int hi = filterHighCutHz();
        if (m_draggingCutoff == CutoffEdge::Low)
            lo = std::clamp(newHz, kMinHz, std::max(kMinHz, hi - kMinSpan));
        else
            hi = std::clamp(newHz, lo + kMinSpan, kMaxHz);
        setFilterCutoffs(lo, hi);  // local visual update
        emit cutoffsDragged(lo, hi);
        ev->accept();
        return;
    }

    if (m_draggingBand < 0 || !m_eq) {
        // Hover state — show size cursor over a cutoff line so the user
        // knows it's draggable, crosshair otherwise.
        const CutoffEdge edge = hitTestCutoffEdge(ev->position());
        setCursor(edge != CutoffEdge::None ? Qt::SizeHorCursor : Qt::CrossCursor);
        QWidget::mouseMoveEvent(ev); return;
    }

    // ── Eine Geste, eine Groesse ─────────────────────────────────────
    //
    // „das klicken der linie bewegt teilweise die komplette kurve, was
    //  ich oft nicht will."
    //
    // Er hat zweimal recht. Die zehn Griffe stehen ab Werk alle auf der
    // Nulllinie -- die Linie IST die Griffreihe, jeder Klick darauf
    // fasst einen an. Und ein Zug aenderte bis hierher Frequenz UND
    // Pegel zugleich, also rutschte das Band beim Anheben nebenbei zur
    // Seite. Bei Q = 0,707 ist ein Band rund zwei Oktaven breit, und
    // so sieht das aus wie „die ganze Kurve bewegt sich".
    //
    // Ab jetzt aendert ein Zug genau eine Groesse:
    //   ziehen        -- Pegel (bei Hoch-/Tiefpass: Grenzfrequenz,
    //                    die haben keinen Pegel)
    //   ⌥ + ziehen    -- Frequenz
    //   ⇧ + ziehen    -- Breite
    //
    // Und relativ zum Griffpunkt, nicht absolut zum Zeiger: wer den
    // Griff um drei Pixel verfehlt, soll das Band nicht um drei Pixel
    // springen sehen.
    const auto bp = m_eq->band(m_draggingBand);
    ClientEq::BandParams next = bp;
    const bool isSlope = (bp.type == ClientEq::FilterType::LowPass
                       || bp.type == ClientEq::FilterType::HighPass);

    if (!m_dragMoved) {
        const QPointF d = ev->position() - m_dragStart;
        if (d.manhattanLength() < 3.0) { ev->accept(); return; }
        m_dragMoved = true;

        // ── Die Richtung entscheidet, nicht eine Taste ───────────
        //
        // „man muss die punkte beim EQ auch horizontal verschieben
        //  können."
        //
        // Stimmt -- und vorher hiess es: „das klicken der linie
        // bewegt teilweise die komplette kurve, was ich oft nicht
        // will." Beides ist dieselbe Forderung von zwei Seiten:
        // waagrecht schieben JA, aber nicht nebenbei beim Anheben.
        //
        // Also entscheiden die ersten Pixel. Wer nach oben zieht,
        // zieht den Pegel; wer zur Seite zieht, zieht die Frequenz.
        // Schraeg gibt es nicht mehr, und deshalb kann auch nichts
        // mehr versehentlich mitwandern. ⌥ bleibt der Weg, beides
        // zugleich zu wollen.
        //
        // Die Schwelle liegt bei drei Pixeln (oben), die Richtung
        // entscheidet sich an dem Pixel danach -- das ist frueh
        // genug, dass es sich sofort anfuehlt, und spaet genug,
        // dass ein Zittern der Hand sie nicht bestimmt.
        m_achse = (std::abs(d.x()) > std::abs(d.y()))
                      ? Achse::Waagrecht : Achse::Senkrecht;
        // Jetzt, nicht beim Druecken: ein Band wird eingeschaltet,
        // weil daran gezogen wird.
        if (!bp.enabled) {
            next.enabled = true;
        }
    }

    // Verschiebung gegenueber dem Griffpunkt, in den Einheiten der
    // Achsen.
    const float dDb = yToDb(static_cast<float>(ev->position().y()))
                    - yToDb(static_cast<float>(m_dragStart.y()));
    const float fRatio =
        xToFreq(static_cast<float>(ev->position().x()))
        / std::max(1.0f, xToFreq(static_cast<float>(m_dragStart.x())));

    // Die Umschalttasten werden LAUFEND gelesen, nicht beim Druecken
    // festgehalten: wer mitten im Zug merkt, dass er doch die
    // Frequenz meint, soll ⌥ druecken koennen, statt loszulassen und
    // neu anzusetzen.
    const bool wantQ    = !m_simpleMode
                       && (ev->modifiers() & Qt::ShiftModifier) != 0;
    const bool wantFreq = !m_simpleMode
                       && (ev->modifiers() & Qt::AltModifier) != 0;

    if (wantQ) {
        // Shift-drag = vertical Q control. Map vertical delta to
        // exponential Q change so a full-height drag spans ~2 decades.
        const float dy = static_cast<float>(m_dragStart.y() - ev->position().y());
        const float hOver2 = std::max(1.0f, static_cast<float>(height()) * 0.5f);
        const float factor = std::pow(10.0f, dy / hOver2);
        next.q = std::clamp(m_dragStartQ * factor, 0.1f, 18.0f);
    } else if (wantFreq || isSlope || m_achse == Achse::Waagrecht) {
        // Hoch- und Tiefpass haben keinen Pegel: bei ihnen IST die
        // waagrechte Bewegung die Sache, auch ohne ⌥.
        next.freqHz = std::clamp(m_dragStartFreqHz * fRatio, 20.0f, 20000.0f);
    } else {
        next.gainDb = std::clamp(m_dragStartGainDb + dDb, -18.0f, 18.0f);
        // ── Eine Kerbe nach oben ist eine Nadel ─────────────────
        //
        // „diese form sollte auch nach oben gehen, nicht nur nach
        // unten" (2026-10-15).
        //
        // Die Kerbe ist eine eigene Filterart mit fester Form: sie
        // loescht ihre Frequenz, und ihr Pegel wird gar nicht
        // gelesen. Nach oben ziehen hat deshalb nichts getan.
        //
        // Das Gegenstueck zu einer schmalen Kerbe ist keine zweite
        // Filterart, sondern eine Glocke mit derselben hohen Guete
        // und umgekehrtem Vorzeichen. Also wird beim Ziehen ueber
        // null daraus genau das -- dieselbe Breite, dieselbe
        // Frequenz, nur nach oben. Wer wieder unter null zieht,
        // bekommt seine Kerbe zurueck.
        if (next.type == ClientEq::FilterType::Notch
            && next.gainDb > 0.5f) {
            next.type = ClientEq::FilterType::Peak;
        } else if (m_dragStartType == ClientEq::FilterType::Notch
                   && next.type == ClientEq::FilterType::Peak
                   && next.gainDb <= 0.5f) {
            next.type = ClientEq::FilterType::Notch;
        }
    }
    m_eq->setBand(m_draggingBand, next);
    emit bandsChanged();
    update();
    ev->accept();
}

void ClientEqEditorCanvas::mouseReleaseEvent(QMouseEvent* ev)
{
    if (m_draggingCutoff != CutoffEdge::None) {
        m_draggingCutoff = CutoffEdge::None;
        setCursor(Qt::CrossCursor);
        ev->accept();
        return;
    }
    if (m_draggingBand < 0) { QWidget::mouseReleaseEvent(ev); return; }
    m_draggingBand = -1;
    m_dragMoved = false;
    m_achse = Achse::Offen;
    setCursor(Qt::CrossCursor);
    persist();
    ev->accept();
}

// ── Das Rad ist die Breite ───────────────────────────────────────────
//
// „man sollte das auch minimaler erhöhen können, nicht sooo breit."
//
// Die Breite liess sich bisher nur mit ⇧ + ziehen ändern. Das steht
// zwar in der Zeile darüber, aber eine Taste, die man kennen muss,
// ist keine Bedienung -- und wer eine Glocke zu breit findet, greift
// als Erstes zum Rad. In jedem Entzerrer, den er kennt, tut das
// genau das hier.
//
// Nur über einem Griff, und nur in der Profi-Ansicht: in der
// einfachen ist die Breite absichtlich festgelegt.
void ClientEqEditorCanvas::wheelEvent(QWheelEvent* ev)
{
    if (!m_eq || m_simpleMode) { QWidget::wheelEvent(ev); return; }

    const int idx = hitTestHandle(ev->position());
    if (idx < 0) { QWidget::wheelEvent(ev); return; }

    const int stufen = ev->angleDelta().y() / 120;
    if (stufen == 0) { QWidget::wheelEvent(ev); return; }

    auto bp = m_eq->band(idx);
    // Je Rastung ein Achtel Oktave enger oder weiter -- fein genug,
    // um zu treffen, grob genug, um in drei Rastungen anzukommen.
    const float faktor = std::pow(1.25f, float(stufen));
    bp.q = std::clamp(bp.q * faktor, 0.3f, 18.0f);
    if (!bp.enabled) { bp.enabled = true; }
    m_eq->setBand(idx, bp);
    setSelectedBand(idx);
    emit bandsChanged();
    update();
    persist();
    ev->accept();
}

void ClientEqEditorCanvas::mouseDoubleClickEvent(QMouseEvent* ev)
{
    if (!m_eq || m_simpleMode) { QWidget::mouseDoubleClickEvent(ev); return; }
    if (ev->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(ev); return;
    }

    const int idx = hitTestHandle(ev->position());
    if (idx < 0) {
        // Empty canvas. Under the fixed-band model there is nothing to
        // create here, and inventing a band on a stray double-click is
        // worse than doing nothing.
        QWidget::mouseDoubleClickEvent(ev);
        return;
    }

    // The first click of the pair already started a drag and the release
    // already ended it; nothing to cancel. Belt and braces anyway, since
    // a platform that delivers the events in another order would
    // otherwise leave the band stuck to the cursor.
    m_draggingBand = -1;
    m_dragMoved    = false;
    setCursor(Qt::CrossCursor);

    setSelectedBand(idx);
    // Shift steps back round the ring, matching the icon row — five
    // types means going forward past the one you wanted is a four-click
    // journey otherwise.
    const int dir = (ev->modifiers() & Qt::ShiftModifier) ? -1 : 1;
    EqFilterRing::applyTo(*m_eq, idx,
                          EqFilterRing::step(m_eq->band(idx).type, dir));
    persist();
    ev->accept();
}

void ClientEqEditorCanvas::contextMenuEvent(QContextMenuEvent* ev)
{
    // In der einfachen Ansicht gibt es kein Menue. Wer es braucht,
    // schaltet auf Profi -- das ist ein Knopf, kein Umweg.
    if (m_simpleMode) { QWidget::contextMenuEvent(ev); return; }
    if (!m_eq) { QWidget::contextMenuEvent(ev); return; }

    const int idx = hitTestHandle(ev->pos());
    if (idx < 0) { QWidget::contextMenuEvent(ev); return; }

    const auto bp = m_eq->band(idx);
    ScopedChildWidget<QMenu> menuOwner(this);   // stirbt nicht mit dem Rahmen, siehe ScopedChildWidget.h
    QMenu& menu = *menuOwner.get();

    auto* typeLabel = menu.addAction(
        QString("Band %1 — %2").arg(idx + 1)
            .arg(QLatin1String(EqFilterRing::name(bp.type))));
    typeLabel->setEnabled(false);
    menu.addSeparator();

    auto* peakAct  = menu.addAction("Set type: Peak");
    auto* lsAct    = menu.addAction("Set type: Low Shelf");
    auto* hsAct    = menu.addAction("Set type: High Shelf");
    auto* lpAct    = menu.addAction("Set type: Low Pass");
    auto* hpAct    = menu.addAction("Set type: High Pass");
    auto* noAct    = menu.addAction("Set type: Notch");
    auto* bpAct    = menu.addAction("Set type: Band Pass");

    // Slope submenu — only meaningful for HP / LP bands.
    const bool isSlope = (bp.type == ClientEq::FilterType::LowPass
                       || bp.type == ClientEq::FilterType::HighPass);
    QMenu* slopeMenu = nullptr;
    QList<QAction*> slopeActions;
    if (isSlope) {
        menu.addSeparator();
        slopeMenu = menu.addMenu(QString("Slope  (%1 dB/oct)")
                                     .arg(bp.slopeDbPerOct));
        for (int s : {12, 24, 36, 48}) {
            auto* a = slopeMenu->addAction(QString("%1 dB/oct").arg(s));
            a->setCheckable(true);
            a->setChecked(bp.slopeDbPerOct == s);
            a->setData(s);
            slopeActions.append(a);
        }
    }

    menu.addSeparator();
    auto* enAct    = menu.addAction(bp.enabled ? "Bypass band" : "Enable band");
    auto* resetAct = menu.addAction("Reset to default");

    QAction* chosen = menu.exec(ev->globalPos());
    // Das Elternteil kann waehrend exec() gestorben sein — dann ist
    // auch das Menue weg und `this` eine Leiche. Siehe ScopedChildWidget.h.
    if (!menuOwner) { return; }
    if (!chosen) return;

    // Same path as the double-click and as the icon row. The menu used
    // to assign the type itself, which meant the three agreed only by
    // coincidence — and they had already stopped: the icon row's ring
    // and this canvas's differed.
    auto change = [&](ClientEq::FilterType t) {
        EqFilterRing::applyTo(*m_eq, idx, t);
    };
    if      (chosen == peakAct) change(ClientEq::FilterType::Peak);
    else if (chosen == lsAct)   change(ClientEq::FilterType::LowShelf);
    else if (chosen == hsAct)   change(ClientEq::FilterType::HighShelf);
    else if (chosen == lpAct)   change(ClientEq::FilterType::LowPass);
    else if (chosen == hpAct)   change(ClientEq::FilterType::HighPass);
    else if (chosen == noAct)   change(ClientEq::FilterType::Notch);
    else if (chosen == bpAct)   change(ClientEq::FilterType::BandPass);
    else if (chosen == enAct) {
        ClientEq::BandParams p = bp; p.enabled = !bp.enabled; m_eq->setBand(idx, p);
    }
    else if (chosen == resetAct) {
        // Restore this slot's factory preset — type, freq, Q all reset;
        // band is left disabled so the row returns to the quiet starting
        // state (matches the "disabled default" feel of fresh launch).
        m_eq->setBand(idx, ClientEq::defaultBand(idx));
    }
    else if (slopeActions.contains(chosen)) {
        ClientEq::BandParams p = bp;
        p.slopeDbPerOct = chosen->data().toInt();
        p.enabled = true;  // explicit edit = enable
        m_eq->setBand(idx, p);
    }
    persist();
    ev->accept();
}

} // namespace Longpath
