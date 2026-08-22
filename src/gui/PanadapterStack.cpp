// no-port-check: AetherSDR-derived NereusSDR file. Pan layout manager
// (5-template QSplitter tree, active-pan tracking, float-pan signal) is
// adapted structurally from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559]. Registered in
// docs/attribution/aethersdr-reconciliation.md.

// SPDX-License-Identifier: GPL-3.0-or-later
//
// =================================================================
// src/gui/PanadapterStack.cpp  (NereusSDR)
// =================================================================
//
// Ported (structurally) from AetherSDR src/gui/PanadapterStack.{h,cpp}
// [@0cd4559].
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
// See PanadapterStack.h for full Modification history (NereusSDR).
// =================================================================

#include "gui/PanadapterStack.h"


#include "gui/PanadapterApplet.h"
#include "gui/SpectrumWidget.h"
#include "gui/StyleConstants.h"
#include "gui/PanFloatingWindow.h"
#include "core/AppSettings.h"
#include <QVBoxLayout>
#include <QSplitter>
#include <QTimer>
#include <QWindow>
#include <QSet>
#include <QStringList>

namespace Longpath {

PanadapterStack::PanadapterStack(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // ── Der Stapel malt seinen eigenen Grund ────────────────────────
    //
    // Gefunden am 2026-08-21 auf die Meldung „sobald ich den pandapter
    // veraendern moechte kommt eine kopie", mit Bildschirmfoto: das
    // Spektrum zweimal, leicht versetzt.
    //
    // Es sind NICHT zwei Widgets — das wurde gemessen: nach dem
    // Abloesen existiert genau ein SpectrumWidget, korrekt im
    // schwebenden Fenster. Was zurueckbleibt, ist ein sichtbarer,
    // LEERER QSplitter ueber die volle Flaeche (1125x647 gemessen).
    // Ein leerer Splitter malt nichts, und der Stapel darunter malte
    // bisher auch nichts. Eine Flaeche, die Platz belegt und die
    // niemand uebermalt, behaelt, was zuletzt darin stand — das alte
    // Spektrum. Daneben zeigt das schwebende Fenster das echte.
    //
    // Deshalb malt der Stapel jetzt selbst. Das kostet nichts, solange
    // ein Panadapter darin steht (er deckt ihn ohnehin ab), und es
    // beseitigt die Bedingung, unter der ueberhaupt etwas
    // stehenbleiben kann.
    m_rootSplitter = new QSplitter(Qt::Vertical, this);
    // Griffe, die man treffen kann — siehe die Notiz bei
    // Style::kSplitterHandlePx. Hier stand bisher gar nichts, also
    // Qts Vorgabe; genau deshalb liess sich der Panadapter nur durch
    // Abloesen vergroessern.
    m_rootSplitter->setHandleWidth(Style::kSplitterHandlePx);
    m_rootSplitter->setStyleSheet(Style::splitterStyle());
    layout->addWidget(m_rootSplitter);

    addPanadapter(QStringLiteral("pan-0"));
}

PanadapterStack::~PanadapterStack()
{
    dockAllFloatingPans();
}

PanadapterApplet* PanadapterStack::addPanadapter(const QString& panId)
{
    if (m_pans.contains(panId)) {
        return m_pans[panId];
    }
    auto* applet = new PanadapterApplet(panId, this);
    m_pans[panId] = applet;
    if (m_activePanId.isEmpty()) { setActivePan(panId); }
    if (m_pans.size() == 1) {
        m_rootSplitter->addWidget(applet);
    }
    emit countChanged(m_pans.size());
    return applet;
}

void PanadapterStack::removePanadapter(const QString& panId)
{
    auto* applet = m_pans.take(panId);
    if (!applet) { return; }
    applet->deleteLater();

    // Re-seat the active id if it named the pan just destroyed. activePanId()
    // feeds "Add slice on active pan" (Ctrl+R), "Float active pan..." and
    // rebuildFftRouting's last-resort pan resolution, so a stale id points all
    // three at a pan that no longer exists. Nothing would ever repair it:
    // setActivePan's only caller is guarded on m_activePanId.isEmpty(), so a
    // stale NON-empty id is permanent.
    //
    // Clearing on the last removal is what re-arms that guard, so the next
    // pan created becomes active. Untouched when some other pan was removed --
    // re-seating on every removal would move the operator's working pan out
    // from under them.
    //
    // m_pans is a QMap, so constBegin() is the lowest surviving pan id rather
    // than an arbitrary one -- "pan-0" before "pan-1". Deterministic on
    // purpose: which pan Ctrl+R targets after a close should not depend on
    // hash ordering.
    if (m_activePanId == panId) {
        setActivePan(m_pans.isEmpty() ? QString() : m_pans.constBegin().key());
    }

    emit countChanged(m_pans.size());
}

void PanadapterStack::removeAll() { /* TODO Task 5 */ }

void PanadapterStack::applyLayout(const QString& layoutId, const QStringList& panIds)
{
    dockAllFloatingPans();
    clearSplitters();

    // Retire orphan pans not referenced by the new layout. Without this,
    // switching from a layout that uses "pan-0" to one keyed on different ids
    // (e.g. "p0..p3" in 2x2 tests) would leak the prior pans into m_pans and
    // distort count(). NereusSDR-specific addition; AetherSDR's layout swap
    // assumes the caller passes the canonical id set.
    const QSet<QString> wanted(panIds.constBegin(), panIds.constEnd());
    const QList<QString> existing = m_pans.keys();
    for (const QString& id : existing) {
        if (!wanted.contains(id)) {
            removePanadapter(id);
        }
    }

    m_currentLayoutId = layoutId;

    if (layoutId == QStringLiteral("1") && !panIds.isEmpty()) {
        auto* applet = addPanadapter(panIds[0]);
        m_rootSplitter->setOrientation(Qt::Vertical);
        m_rootSplitter->addWidget(applet);
        applet->show();
    }
    else if (layoutId == QStringLiteral("2v") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* a = addPanadapter(panIds[0]);
        auto* b = addPanadapter(panIds[1]);
        m_rootSplitter->addWidget(a);
        m_rootSplitter->addWidget(b);
        a->show();
        b->show();
    }
    else if (layoutId == QStringLiteral("2h") && panIds.size() >= 2) {
        m_rootSplitter->setOrientation(Qt::Horizontal);
        auto* a = addPanadapter(panIds[0]);
        auto* b = addPanadapter(panIds[1]);
        m_rootSplitter->addWidget(a);
        m_rootSplitter->addWidget(b);
        a->show();
        b->show();
    }
    else if (layoutId == QStringLiteral("12h") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        auto* top = addPanadapter(panIds[0]);
        m_rootSplitter->addWidget(top);
        top->show();

        auto* bottomSplitter = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* bl = addPanadapter(panIds[1]);
        auto* br = addPanadapter(panIds[2]);
        bottomSplitter->addWidget(bl);
        bottomSplitter->addWidget(br);
        bl->show();
        br->show();
        m_rootSplitter->addWidget(bottomSplitter);

        m_rootSplitter->setStretchFactor(0, 2);  // wide top gets 2x weight
        m_rootSplitter->setStretchFactor(1, 1);
    }
    else if (layoutId == QStringLiteral("2x2") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();

        auto* bottomRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* bl = addPanadapter(panIds[2]);
        auto* br = addPanadapter(panIds[3]);
        bottomRow->addWidget(bl);
        bottomRow->addWidget(br);
        bl->show();
        br->show();

        m_rootSplitter->addWidget(topRow);
        m_rootSplitter->addWidget(bottomRow);
    }
    else if (layoutId == QStringLiteral("2h1") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        auto* tl = addPanadapter(panIds[0]);
        auto* tr = addPanadapter(panIds[1]);
        topRow->addWidget(tl);
        topRow->addWidget(tr);
        tl->show();
        tr->show();
        m_rootSplitter->addWidget(topRow);

        auto* bottom = addPanadapter(panIds[2]);
        m_rootSplitter->addWidget(bottom);
        bottom->show();
    }
    else if (layoutId == QStringLiteral("3v") && panIds.size() >= 3) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("4v") && panIds.size() >= 4) {
        m_rootSplitter->setOrientation(Qt::Vertical);
        for (int i = 0; i < 4; ++i) {
            auto* p = addPanadapter(panIds[i]);
            m_rootSplitter->addWidget(p);
            p->show();
        }
    }
    else if (layoutId == QStringLiteral("3h2") && panIds.size() >= 5) {
        m_rootSplitter->setOrientation(Qt::Vertical);

        auto* topRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 0; i < 3; ++i) {
            auto* p = addPanadapter(panIds[i]);
            topRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(topRow);

        auto* bottomRow = new QSplitter(Qt::Horizontal, m_rootSplitter);
        for (int i = 3; i < 5; ++i) {
            auto* p = addPanadapter(panIds[i]);
            bottomRow->addWidget(p);
            p->show();
        }
        m_rootSplitter->addWidget(bottomRow);
    }

}

PanadapterApplet* PanadapterStack::panadapter(const QString& id) const { return m_pans.value(id, nullptr); }
QList<PanadapterApplet*> PanadapterStack::allApplets() const { return m_pans.values(); }
SpectrumWidget* PanadapterStack::spectrum(const QString& panId) const
{
    PanadapterApplet* applet = m_pans.value(panId, nullptr);
    return applet ? applet->spectrumWidget() : nullptr;
}
void PanadapterStack::setActivePan(const QString& id) { if (m_activePanId != id) { m_activePanId = id; emit activePanChanged(id); } }

// The writer PanadapterApplet::activeSliceIndex() never had after its one-shot
// seed in addSlice. See the header for the bench defect this closes.
//
// The scan is over single-digit pan counts on a user action, and it asks each
// pan the one question that matters -- do you host this slice -- rather than
// resolving through SliceModel::panKey(). panKey is the authoritative binding
// for WHERE a slice belongs, but associatedSlices() is what
// MainWindow::sliceForPan actually reads back, so keying off the same set is
// what makes the pan's answer and this function's answer agree. Every pan that
// lists the slice is updated rather than the first one found: if two ever
// disagree, moving both is the state the operator can act on, where stopping
// at the first would leave a pan silently tuning something else.
void PanadapterStack::setActiveSliceOnHostingPan(int sliceId)
{
    const QList<PanadapterApplet*> pans = allApplets();
    for (PanadapterApplet* applet : pans) {
        if (!applet) { continue; }
        if (!applet->associatedSlices().contains(sliceId)) { continue; }
        applet->setActiveSliceIndex(sliceId);
    }
}

// Keeps associatedSlices() honest across a pan change. See the header.
void PanadapterStack::moveSliceToPan(int sliceId, const QString& destPanId)
{
    PanadapterApplet* dest = m_pans.value(destPanId, nullptr);
    if (!dest) { return; }

    const QList<PanadapterApplet*> pans = allApplets();
    for (PanadapterApplet* applet : pans) {
        if (!applet || applet == dest) { continue; }
        applet->removeSlice(sliceId);
    }
    dest->addSlice(sliceId);
}

// After moving a QRhiWidget between top-level windows, force a fresh initialize()
// cycle so Metal binds to the new NSView. The backing-store notification is sent
// before the actual reparent; sending it again here can make QRhiWidget remove a
// stale cleanup callback from the wrong QRhi during startup floating restore.
//   [From AetherSDR PanadapterStack.cpp:18-43 [@0cd4559], verbatim bis auf
//    den Makronamen. Bei der Portierung verlorengegangen und am 2026-08-22
//    rueckportiert — ohne diesen Zyklus blieb das Fenster nach dem
//    Umhaengen schwarz.]
static void refreshAfterReparent(Longpath::SpectrumWidget* sw)
{
    if (!sw) { return; }
#if defined(Q_OS_MAC) && defined(NEREUS_GPU_SPECTRUM)
    const bool wasVisible = sw->isVisible();
    sw->hide();
    sw->resetGpuResources();
    if (QWindow* windowHandle = sw->windowHandle()) {
        windowHandle->destroy();
    }
    // Re-realize the native leaf with its ancestor isolation intact — the helper
    // reasserts WA_NativeWindow *and* WA_DontCreateNativeAncestors as a pair, so a
    // reparent can't promote ancestors to native (redundant backing stores, #4339).
    sw->applyNativeWindowIsolationPolicy();
    if (wasVisible) {
        sw->show();
    }
    QTimer::singleShot(50, sw, [sw]() { sw->update(); });
#else
    sw->resetGpuResources();
#endif
}

void PanadapterStack::floatPanadapter(const QString& panId)
{
    // Sub-Epic D Task 8: detach the pan into a top-level PanFloatingWindow
    // (multi-monitor). The applet is reparented into the window's layout, so
    // ownership transfers to the window for the duration of the float. On
    // dockRequested (close or explicit redock), the layout is rebuilt from
    // m_currentLayoutId so the applet is reparented back under the stack.
    auto* applet = m_pans.value(panId, nullptr);
    if (!applet || m_floating.contains(panId)) { return; }

    // Reparent while hidden, and re-show only once the new window is up.
    //
    // The applet hosts a SpectrumWidget, which is a QRhiWidget: its render
    // context belongs to the window it lives in. Reparenting a live, visible
    // one straight into a not-yet-shown window left Qt unable to re-acquire an
    // RHI for it, and the pan froze while logging "QRhiWidget: No QRhi" once
    // per frame forever. Hiding first lets the old context tear down cleanly
    // (releaseResources clears m_rhiInitialized), so the show below drives a
    // fresh initialize() against the floating window's own surface.
    // Wo stand der Panadapter? Das Fenster geht genau dort auf —
    // siehe die Begruendung in MainWindow::detachApplet (2026-08-20):
    // ein Fenster, das an fremder Stelle aufgeht, liest sich als NEUES
    // Fenster; geht es dort auf, wo die Flaeche stand, liest sich
    // dieselbe Geste als „aufheben".
    //
    // Vor dem Verstecken greifen: ein verstecktes Widget hat keine
    // Lage auf dem Schirm mehr.
    QRect pickedUpAt;
    if (applet->isVisible()) {
        pickedUpAt = QRect(applet->mapToGlobal(QPoint(0, 0)), applet->size());
    }

    applet->hide();

    // From AetherSDR PanadapterStack.cpp:793-801 [@0cd4559]: GPU-Schutz
    // VOR dem Umhaengen — Rueckruf abmelden (#2495), Ressourcen
    // freigeben, damit initialize() gegen das neue Fenster neu bindet.
    if (SpectrumWidget* sw = applet->spectrumWidget()) {
        sw->hide();
        sw->prepareForTopLevelChange();
        sw->resetGpuResources();
    }

    // Mit Elternfenster: ein Qt::Tool ohne Elternteil ist kein
    // Hilfsfenster, sondern ein weiteres Hauptfenster — und faellt in
    // die Vollbildflaeche zurueck, die es gerade vermeiden soll.
    auto* floater = new PanFloatingWindow(applet, window());
    m_floating[panId] = floater;
    m_floatEver.append(floater);

    connect(floater, &PanFloatingWindow::dockRequested, this, [this, panId]() {
        auto* taken = m_floating.take(panId);
        if (!taken) { return; }
        emit panFloatStateChanged(panId, false);
        // From AetherSDR PanadapterStack.cpp:840-852 [@0cd4559]: erst den
        // GPU-Schutz, DANN das Umhaengen — sonst bricht der doppelte
        // NSView-Lebenszyklus die NSResponder-Kette (#1344).
        if (PanadapterApplet* fa = m_pans.value(panId, nullptr)) {
            if (SpectrumWidget* sw = fa->spectrumWidget()) {
                sw->hide();
                sw->prepareForTopLevelChange();
                sw->resetGpuResources();
            }
        }
        if (PanadapterApplet* a = m_pans.value(panId, nullptr)) {
            a->setFloatingIndicator(false);
        }
        // Re-attach all currently-known applets to the current layout. The
        // applyLayout path detaches every applet from its parent first
        // (clearSplitters reparents back to `this` and hides) and then
        // re-adds the ones listed in m_pans.keys(), so the floated applet
        // is pulled out of the window's layout cleanly before we delete it.
        applyLayout(m_currentLayoutId, m_pans.keys());
        taken->deleteLater();
        if (PanadapterApplet* fa = m_pans.value(panId, nullptr)) {
            if (SpectrumWidget* sw = fa->spectrumWidget()) {
                QTimer::singleShot(0, sw, [sw]() {
                    refreshAfterReparent(sw);
                    sw->show();
                });
            }
        }
    });

    floater->show();
    // Now that the floating window exists and is mapped, bring the pan back up
    // so its QRhiWidget initializes against that window's surface.
    applet->show();
    if (SpectrumWidget* sw = applet->spectrumWidget()) {
        // From AetherSDR PanadapterStack.cpp:823-830 [@0cd4559]: Metal erst
        // an die neue NSView binden lassen, dann zeigen.
        QTimer::singleShot(0, sw, [sw]() {
            refreshAfterReparent(sw);
            sw->show();
        });
    }
    // Erst JETZT die Groesse setzen: solange das Applet versteckt war,
    // verlangte die Anordnung fast nichts, und jede vorher gesetzte
    // Zahl wird von der Mindestgroesse des Inhalts ueberschrieben.
    floater->applyDefaultSize();
    if (pickedUpAt.isValid()) {
        floater->move(pickedUpAt.topLeft());
    }

    applet->setFloatingIndicator(true);
    emit panFloatStateChanged(panId, true);

    // Re-establish the pans that stayed behind.
    //
    // Pulling a widget out of a QSplitter costs its siblings their render
    // context too: floating one pan turned the OTHER pan black and logged
    // "QRhiWidget: No QRhi" once per frame. A hide/show cycle makes Qt tear
    // the context down cleanly (releaseResources clears m_rhiInitialized) and
    // build a fresh one against the main window.
    //
    // Deliberately NOT applyLayout() here: its clearSplitters() reparents
    // EVERY applet in m_pans back under the stack, floated ones included --
    // which is exactly what the dockRequested handler above relies on, and
    // exactly wrong while a float is being set up. Doing that emptied the
    // floating window and the main window at once.
    for (auto it = m_pans.cbegin(); it != m_pans.cend(); ++it) {
        if (it.key() == panId || m_floating.contains(it.key())) { continue; }
        if (auto* other = it.value()) {
            other->hide();
            other->show();
        }
    }

    // ── UND ALLES ANDERE IM HAUPTFENSTER ────────────────────────────
    //
    // Die Runde darueber heilt nur die anderen PANADAPTER. Im
    // Hauptfenster haengen aber noch mehr QRhiWidgets: jedes Instrument
    // im Applet-Streifen ist eines (MeterWidget leitet von QRhiWidget
    // ab). Die verlieren ihren Kontext genauso.
    //
    // GESEHEN AM 2026-08-20 beim Selbsttest an der laufenden Anwendung:
    // Panadapter ablösen — und das ganze Hauptfenster war schwarz, mit
    // 79 Meldungen „QRhiWidget: No QRhi", eine je Bild. Beim
    // Zurueckdocken kam alles wieder. Der Fehler lag seit Phase 3F da;
    // er faellt nur niemandem auf, solange niemand ablöst — und
    // ablösen konnte man bis heute nur ueber ein Rechtsklick-Menue,
    // das der Betreiber nie gefunden hat.
    //
    // Dieselbe Kur, groesserer Kreis: dieselbe Hide/Show-Runde fuer
    // jedes QRhiWidget, das noch im Hauptfenster steht.
    // ── EIN BEKANNTER FEHLER, DER HIER NICHT GELOEST IST ────────────
    //
    // Solange ein Panadapter abgeloest ist, bleibt das HAUPTFENSTER
    // schwarz, und Qt meldet „QRhiWidget: No QRhi" einmal je Bild. Beim
    // Zurueckdocken kommt alles wieder.
    //
    // GEMESSEN AM 2026-08-20 an der laufenden Anwendung. Zwei Kuren
    // versucht und beide VERWORFEN, weil sie nicht wirkten:
    //
    //   1. dieselbe Hide/Show-Runde wie oben, aber ueber alle
    //      QRhiWidgets des Hauptfensters — 79 Meldungen blieben 79.
    //   2. dasselbe, um einen Durchlauf der Ereignisschleife verzoegert
    //      — 34 statt 79, also nur weniger Zeit zum Melden, nicht
    //      geheilt.
    //
    // Nicht auf meine Aenderungen zurueckzufuehren: der Weg zum
    // Ablösen war bis heute UEBERHAUPT NICHT ERREICHBAR. Das
    // Rechtsklick-Menue des Applets (Task B5) kommt nicht durch — der
    // Spektrumbereich faengt den Klick ab und zeigt sein eigenes Menue
    // — und einen Knopf gab es nicht. Der Fehler sitzt seit Phase 3F
    // im Baum und ist nur nie jemandem begegnet.
    //
    // Was es braucht, ist eine echte Diagnose, welches QRhiWidget den
    // Kontext verliert und warum das Umhaengen des Applets die
    // Oberflaeche des Hauptfensters mitnimmt. Das ist ein eigener
    // Schritt, kein Anhaengsel an diesen hier — und Raten macht es
    // schlimmer, siehe die zwei verworfenen Kuren.
}
// Der umgekehrte Weg zu floatPanadapter. Er tut genau das, was der
// dockRequested-Empfaenger dort tut — nur von aussen aufrufbar, damit
// der Kopf der Kachel EINEN Schalter fuer beide Richtungen haben kann.
//
// Ueber das Fenster und nicht ueber die Anordnung: PanFloatingWindow
// meldet dockRequested, wenn es geschlossen wird, und dort haengt die
// ganze Wiederherstellung (applyLayout, Aufraeumen des Fensters, die
// Hide/Show-Runde fuer die QRhi-Kontexte der Nachbarn). Das hier zu
// wiederholen waere ein zweiter Weg, der beim naechsten Umbau
// auseinanderlaeuft.
// ── Alle abgeloesten Panadapter zurueckholen ─────────────────────────
//
// Gebraucht beim Beenden. Am 2026-08-21 hat eine Probe im echten
// Hauptfenster einen SIGSEGV geliefert: Panadapter abloesen, Fenster
// schliessen — Absturz beim Abbau.
//
// Die Ursache ist dieselbe Familie wie der Absturz von heute
// Vormittag (c8d8161a, 1c781bae): ein Fenster, das das Schliessen
// ueberlebt. Dort waren es die schwebenden APPLET-Fenster
// (m_floatingApplets im MainWindow); PanFloatingWindow ist eine
// ANDERE Sammlung, und die habe ich uebersehen. Ein abgeloester
// Panadapter gehoert beim Beenden dem schwebenden Fenster, das
// Applet steht aber weiter in m_pans — beim Abbau greift einer ins
// Leere.
//
// Reihenfolge wie im dockRequested-Weg: erst aus m_floating nehmen,
// dann applyLayout (das holt das Applet aus dem Fenster zurueck unter
// den Stapel), erst dann das Fenster loeschen. Andersherum naehme das
// Fenster das Applet mit ins Grab.
void PanadapterStack::setShuttingDown(bool on)
{
    for (PanFloatingWindow* fw : m_floating) {
        if (fw) { fw->setShuttingDown(on); }
    }
    // Auch die schon Herausgenommenen — sonst bittet eines davon
    // mitten im Abbau noch ums Zurueckhaengen.
    for (QPointer<PanFloatingWindow>& w : m_floatEver) {
        if (w) { w->setShuttingDown(on); }
    }
}

void PanadapterStack::shutDownFloating()
{
    setShuttingDown(true);
    // KEIN vorzeitiges Umkehren bei leerer Liste: genau dann liegt der
    // Fall vor, fuer den das Netz unten da ist — wer zurueckdockt und
    // sofort beendet, hat m_floating geleert, aber das Fenster lebt
    // noch (deleteLater kommt beim Beenden nicht mehr an). Gemessen am
    // 2026-08-22: mit dem alten "return" blieb es stehen, obwohl das
    // Netz eingebaut war.

    // Beim Beenden wird NICHT zurueckgehaengt. Das ist der Kern, und er
    // hat mich heute zwei Abstuerze gekostet:
    //
    //   Anlauf 1: stilllegen, DANN applyLayout()  -> die Anordnung
    //             haengt ein Widget zurueck, dessen native View gerade
    //             abgerissen wurde. Absturz.
    //   Anlauf 2: umhaengen, DANN stilllegen      -> das
    //             QRhi-Abmeldeereignis geht ZWEIMAL raus. Beim zweiten
    //             Mal ist der QRhi tot. Derselbe Absturz, andere Tuer:
    //               QHash<void const*, function<void(QRhi*)>>::removeImpl
    //
    // AetherSDR sagt es woertlich (SpectrumWidget.cpp:2278-2280
    // [@0cd4559]): "The send must happen exactly once, before the
    // reparent." Beim Beenden gibt es kein Danach, in dem ein
    // zurueckgehaengtes Applet noch gebraucht wuerde — also faellt das
    // Umhaengen weg, prepareForShutdown() sendet das eine Ereignis, und
    // das Fenster nimmt sein Applet mit ins Grab.
    //
    // Der Eintrag in m_pans muss dabei WEG: das Applet gehoert dem
    // Fenster und stirbt mit ihm. Bliebe der Zeiger stehen, liefe der
    // restliche Abbau in eine Leiche.
    const QStringList ids = m_floating.keys();
    for (const QString& id : ids) {
        PanFloatingWindow* w = m_floating.take(id);
        if (!w) { continue; }
        for (SpectrumWidget* sw : w->findChildren<SpectrumWidget*>()) {
            sw->prepareForShutdown();
        }
        m_pans.remove(id);
        // Sofort, nicht nachgereicht: beim Beenden laeuft keine Runde
        // mehr, in der ein deleteLater ankaeme — genau daran hing der
        // Geist am Schreibtisch, den der Betreiber fotografiert hat.
        delete w;
    }

    // Und die zweite Tuer zu: was der Zurueckdocken-Weg schon aus
    // m_floating genommen, aber nur per deleteLater zum Loeschen
    // vorgemerkt hat, steht sonst weiter am Schreibtisch. Siehe
    // m_floatEver.
    for (QPointer<PanFloatingWindow>& w : m_floatEver) {
        if (w) {
            w->setShuttingDown(true);
            for (SpectrumWidget* sw : w->findChildren<SpectrumWidget*>()) {
                sw->prepareForShutdown();
            }
            delete w.data();
        }
    }
    m_floatEver.clear();
}

void PanadapterStack::dockPanadapter(const QString& panId)
{
    PanFloatingWindow* floater = m_floating.value(panId, nullptr);
    if (!floater) { return; }
    floater->requestDock();
}

void PanadapterStack::rebuildSplitters(const QString&, const QStringList&) {}

void PanadapterStack::dockAllFloatingPans()
{
    while (!m_floating.isEmpty()) {
        auto it = m_floating.begin();
        PanFloatingWindow* floater = it.value();
        m_floating.erase(it);
        if (!floater) { continue; }

        QObject::disconnect(floater, nullptr, this, nullptr);
        if (PanadapterApplet* applet = floater->applet()) {
            applet->hide();
            applet->setParent(this);
        }
        floater->close();
        delete floater;
    }
}

void PanadapterStack::clearSplitters()
{
    // Detach all applets from the current splitter tree but do not delete them
    // (they live in m_pans and may be re-attached by the new layout).
    for (auto* applet : m_pans.values()) {
        applet->setParent(this);
        applet->hide();
    }
    // Tear down the splitter tree and rebuild from scratch.
    if (m_rootSplitter) {
        layout()->removeWidget(m_rootSplitter);
        m_rootSplitter->deleteLater();
    }
    m_rootSplitter = new QSplitter(Qt::Vertical, this);
    layout()->addWidget(m_rootSplitter);
}

// Phase 3F Sub-Epic D Task 6: persist splitter geometry across launches.
// Storage layout: PanLayoutId + PanSplitter0Sizes (root) + PanSplitter1Sizes /
// PanSplitter2Sizes (nested splitters for 12h / 2x2). Sub-Epic D ships only
// root-splitter persistence; nested splitter persistence may be wired in
// Sub-Epic H polish if bench feedback demands it.
void PanadapterStack::saveSplitterState()
{
    if (!m_rootSplitter) { return; }
    auto& s = AppSettings::instance();
    QStringList parts;
    const QList<int> sizes = m_rootSplitter->sizes();
    for (int sz : sizes) {
        parts << QString::number(sz);
    }
    s.setValue(QStringLiteral("PanSplitter0Sizes"), parts.join(QStringLiteral(",")));
    s.setValue(QStringLiteral("PanLayoutId"), m_currentLayoutId);
}

void PanadapterStack::restoreSplitterState()
{
    if (!m_rootSplitter) { return; }
    auto& s = AppSettings::instance();
    const QString raw = s.value(QStringLiteral("PanSplitter0Sizes"), QString()).toString();
    if (raw.isEmpty()) { return; }
    QList<int> sizes;
    const QStringList parts = raw.split(QStringLiteral(","), Qt::SkipEmptyParts);
    for (const QString& part : parts) {
        sizes << part.toInt();
    }
    if (!sizes.isEmpty()) {
        m_rootSplitter->setSizes(sizes);
    }
}

QList<int> PanadapterStack::rootSplitterSizes() const
{
    if (!m_rootSplitter) { return {}; }
    return m_rootSplitter->sizes();
}

void PanadapterStack::rootSplitterSetSizesForTest(const QList<int>& sizes)
{
    if (m_rootSplitter) {
        m_rootSplitter->setSizes(sizes);
    }
}

} // namespace Longpath
