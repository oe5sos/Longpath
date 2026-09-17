// =================================================================
// src/gui/applets/BandwidthFilterApplet.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung steht im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-20 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "BandwidthFilterApplet.h"

#include "gui/StyleConstants.h"
#include "gui/widgets/BandwidthFilterPane.h"
#include "models/RadioModel.h"
#include "models/SliceModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QResizeEvent>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Die Farben der Empfaenger, wie in der Vorlage: der erste blau, der
// zweite gruen. Danach zwei weitere, damit vier Scheiben nicht in
// derselben Farbe stehen.
QColor accentFor(int index)
{
    switch (index) {
    case 0:  return QColor(Style::kAccent);      // blau
    case 1:  return QColor(Style::kGreenText);   // gruen
    case 2:  return QColor(Style::kAmberText);   // bernstein
    default: return QColor(Style::kTextSecondary);
    }
}

// ── LOW und HIGH sind AUDIO-Begriffe ────────────────────────────────
//
// Der Betreiber am 2026-09-17, auf 40 m LSB: "100 - 3000 ergibt
// 2900?!?!?" — und davor: "hört sich auf 40 meter katastrophal an".
//
// Was passiert war: die Felder zeigten seit dem 2026-09-03 die
// BETRAEGE der inneren Kanten (filterLow, filterHigh), und die sind
// bei LSB beide negativ: filterLow = -2950 ist die FERNE Kante
// (Audio-Hochschnitt), filterHigh = -150 die NAHE (Audio-Tiefschnitt).
// Im Feld "LOW" stand darum 2950 und in "HIGH" 150 — genau verkehrt
// zu dem, was jeder Funker unter Low Cut und High Cut versteht. Wer
// dann "LOW 100" tippte, setzte in Wahrheit die FERNE Kante auf -100,
// und WIDTH 3000 zaehlte von dort nach OBEN: -100 … +2900, quer ueber
// den Traeger auf das falsche Seitenband. Bei USB fiel das nicht auf,
// weil dort innere Zaehlrichtung und Audiorichtung zusammenfallen —
// "20 meter passt".
//
// Deshalb hier die eine Uebersetzung, an der alles andere haengt:
//
//   LOW  = die Kante NAHE am Traeger   (Audio-Tiefschnitt)
//   HIGH = die Kante FERN vom Traeger  (Audio-Hochschnitt)
//
// fuer beide Seitenbaender. Bei USB ist das filterLow/filterHigh, bei
// LSB umgekehrt |filterHigh|/|filterLow|. Zweiseitige Betriebsarten
// (AM/SAM/FM/DSB/SPEC/DRM) behalten LOW = negative, HIGH = positive
// Kante — dort gibt es kein "nah" und "fern".
//
// Das Vorzeichen bleibt weiterhin unsichtbar ("minus darf nie",
// 2026-09-03) — es steckt jetzt in der Betriebsart, nicht im Feld.
enum class Sideband { Lower, Upper, Both };

Sideband sidebandOf(DSPMode mode)
{
    switch (mode) {
    case DSPMode::LSB:
    case DSPMode::CWL:
    case DSPMode::DIGL:
    case DSPMode::RADE_L:
        return Sideband::Lower;
    case DSPMode::USB:
    case DSPMode::CWU:
    case DSPMode::DIGU:
    case DSPMode::RADE_U:
        return Sideband::Upper;
    default:
        return Sideband::Both;
    }
}

/// Die nahe Kante (Audio-Tiefschnitt) als Betrag.
int nearEdgeHz(const SliceModel* s)
{
    switch (sidebandOf(s->dspMode())) {
    case Sideband::Lower: return qAbs(s->filterHigh());
    case Sideband::Upper: return qAbs(s->filterLow());
    default:              return qAbs(s->filterLow());
    }
}

/// Die ferne Kante (Audio-Hochschnitt) als Betrag.
int farEdgeHz(const SliceModel* s)
{
    switch (sidebandOf(s->dspMode())) {
    case Sideband::Lower: return qAbs(s->filterLow());
    case Sideband::Upper: return qAbs(s->filterHigh());
    default:              return qAbs(s->filterHigh());
    }
}

} // namespace

BandwidthFilterApplet::BandwidthFilterApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();

    if (m_model) {
        // Kommt eine Scheibe dazu oder faellt eine weg, aendert sich die
        // Zahl der Flaechen.
        connect(m_model, &RadioModel::sliceAdded,
                this, [this](int) { rebuildPanes(); });
        connect(m_model, &RadioModel::sliceRemoved,
                this, [this](int) { rebuildPanes(); });
    }

    rebuildPanes();
}

void BandwidthFilterApplet::buildUI()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* body = new QWidget(this);
    auto* col = new QVBoxLayout(body);
    col->setContentsMargins(5, 4, 5, 5);
    col->setSpacing(5);

    // Die Flaechen. Nebeneinander, gleich breit — man vergleicht sie.
    m_paneRow = new QHBoxLayout;
    m_paneRow->setSpacing(4);
    col->addLayout(m_paneRow, 1);

    // ── Die Zahlen ───────────────────────────────────────────────────
    //
    // Sie gelten fuer die AKTIVE Scheibe. Eine Zeile je Empfaenger
    // waere ehrlicher, aber bei vier Scheiben unlesbar; die Kapsel
    // in der Flaeche ("RX1 · LSB") sagt, welche gemeint ist.
    //
    // Glas & Tiefe (2026-09-17, Stilblatt 3): jede Bedienung steht in
    // einer benannten Gruppe — eine Versalzeile UEBER dem Feld, nicht
    // ein fettes Wort daneben (Hausstil Regel 1). Die Zahlen sitzen in
    // Glasfeldern ohne Pfeile, Monospace (Regel 4). Die Betriebsart
    // stand hier links als Label; sie steht jetzt in der Kapsel der
    // Flaeche, wo sie zur Kurve gehoert.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(22);

        auto caps = [&](const QString& t) {
            auto* l = new QLabel(t, body);
            l->setFont(Style::capsFont(body->font(), Style::kFontCaption));
            l->setStyleSheet(QStringLiteral("QLabel { color: %1; }")
                .arg(QLatin1String(Style::kTextScale)));
            return l;
        };

        // Eine Gruppe: Versalzeile, darunter das Ding selbst.
        auto group = [&](const QString& title, QWidget* content) {
            auto* w = new QWidget(body);
            auto* v = new QVBoxLayout(w);
            v->setContentsMargins(0, 0, 0, 0);
            v->setSpacing(3);
            v->addWidget(caps(title));
            v->addWidget(content);
            return w;
        };

        auto box = [&](int lo, int hi) {
            auto* sb = new QSpinBox(body);
            sb->setRange(lo, hi);
            sb->setSingleStep(50);
            sb->setSuffix(QStringLiteral(" Hz"));
            sb->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            sb->setButtonSymbols(QAbstractSpinBox::NoButtons);
            // Nachgiebig statt fest: 92 Punkte sind die Wunschbreite,
            // 62 die Schmerzgrenze (siehe Umbruch in resizeEvent).
            sb->setMinimumWidth(62);
            sb->setMaximumWidth(92);
            sb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            sb->setFont(Style::monoFont(body->font(), Style::kFontBody));
            sb->setStyleSheet(Style::glassFieldStyle());
            sb->setKeyboardTracking(false);   // erst bei Enter/Verlassen
            return sb;
        };

        // Betreiber 2026-09-03, mit Nachdruck: "minus darf nie!!!!!" —
        // LOW/HIGH sind intern vorzeichenbehaftete Versatzwerte (LSB legt
        // beide Kanten unterhalb des Traegers), aber diese Felder zeigen
        // und nehmen nur noch den BETRAG entgegen. Die Spanne beginnt
        // darum bei 0 statt bei -kMaxFilterWidthHz — negativ eintippen
        // geht am Feld selbst schon nicht mehr. Das Vorzeichen bleibt
        // intern erhalten (siehe die beiden valueChanged-Anschluesse
        // unten) und wird beim Zurueckschreiben ins Modell wieder
        // angelegt.
        m_lowBox = box(0, SliceModel::kMaxFilterWidthHz);
        m_lowBox->setObjectName(QStringLiteral("bwFilterLow"));
        row->addWidget(group(QStringLiteral("Low"), m_lowBox));

        // Betreiber 2026-09-03: "bandbreite solle von 50-3000 sein" /
        // "Ende zwischen 2700 bis 3000 standard, maximum 8000 (10000)
        // sollte einstellbar sein" — Untergrenze von 10 auf 50 Hz an.
        // Obergrenze bleibt bei 2*kMaxFilterWidthHz: kMaxFilterWidthHz
        // (10000) deckelt laut SliceModel.h/constrainFilter() JEDE
        // KANTE fuer sich, nicht die Breite direkt — FM sitzt
        // symmetrisch um Null (resetFilter(): 12000 Hz Breite = je
        // 6000 Hz Kante, weit unter dem Kantendeckel). Ein Deckel von
        // genau 10000 auf DIESES Feld haette FMs eigenen, gueltigen
        // Vorgabewert schon abgeschnitten. 2*kMaxFilterWidthHz deckt
        // den breitesten ueberhaupt erreichbaren Fall (beide Kanten am
        // Anschlag) und liegt fuer SSB/CW in der Praxis laengst
        // innerhalb der 10000, die der Betreiber nannte.
        m_widthBox = box(50, 2 * SliceModel::kMaxFilterWidthHz);
        m_widthBox->setObjectName(QStringLiteral("bwFilterWidth"));
        m_widthBox->setToolTip(QStringLiteral(
            "Type a width: SSB keeps the edge you set last and moves the "
            "other (LOW 100 + 3000 = HIGH 3100, on either sideband), CW "
            "stays centred on the sidetone, AM symmetric around zero."));
        row->addWidget(group(QStringLiteral("Width"), m_widthBox));

        m_highBox = box(0, SliceModel::kMaxFilterWidthHz);
        m_highBox->setObjectName(QStringLiteral("bwFilterHigh"));
        row->addWidget(group(QStringLiteral("High"), m_highBox));

        // ── VAR1 und VAR2 ────────────────────────────────────────
        //
        // Der Grund: ohne sie ist die eigene Handeinstellung nach einem
        // Klick auf „2.4k" weg. VAR1 fuellt sich VON SELBST, sobald man
        // zieht (SliceModel::setFilterByHand); VAR2 ist der zweite
        // Platz, den man bewusst belegt — Rechtsklick.
        //
        // Ein Klick holt zurueck, ein Rechtsklick legt ab. Ein leerer
        // Platz sagt das auch, statt still nichts zu tun.
        auto* memRow = new QWidget(body);
        auto* memLay = new QHBoxLayout(memRow);
        memLay->setContentsMargins(0, 0, 0, 0);
        memLay->setSpacing(5);
        for (int i = 0; i < SliceModel::kVarSlots; ++i) {
            QPushButton* b = styledButton(
                QStringLiteral("VAR %1").arg(i + 1), 56, 26);
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            m_varBtns.append(b);
            memLay->addWidget(b);

            connect(b, &QPushButton::clicked, this, [this, i]() {
                if (SliceModel* s = activeSlice()) { s->recallVarFilter(i); }
                refreshVarButtons();
            });
            connect(b, &QPushButton::customContextMenuRequested, this,
                    [this, i](const QPoint&) {
                if (SliceModel* s = activeSlice()) { s->storeVarFilter(i); }
                refreshVarButtons();
            });
        }

        m_resetBtn = styledButton(QStringLiteral("↺ Centre"), 78, 26);
        m_resetBtn->setToolTip(QStringLiteral(
            "Put the passband back where this mode wants it — on the "
            "sidetone for CW, at the default low cut for SSB."));
        memLay->addWidget(m_resetBtn);
        m_memoryGroup = group(QStringLiteral("Memory"), memRow);
        m_memoryGroup->setObjectName(QStringLiteral("bwFilterMemoryGroup"));
        row->addWidget(m_memoryGroup);

        row->addStretch(1);

        m_spanBtn = styledButton(QStringLiteral("AUTO"), 62, 26);
        m_spanBtn->setToolTip(QStringLiteral(
            "How much band the panes show. Click to cycle."));
        m_spanGroup = group(QStringLiteral("Span"), m_spanBtn);
        m_spanGroup->setObjectName(QStringLiteral("bwFilterSpanGroup"));
        row->addWidget(m_spanGroup);

        m_ctrlRow = row;
        col->addLayout(row);

        // Zweite Reihe, zunaechst leer. Sie fuellt sich erst, wenn es
        // eng wird (resizeEvent) — breit bleibt alles wie bisher.
        m_ctrlRow2 = new QHBoxLayout;
        m_ctrlRow2->setSpacing(22);
        col->addLayout(m_ctrlRow2);
    }

    {
        // Ausdruecklich erlauben, schmal zu werden.
        //
        // minimumSizeHint() allein reicht NICHT: bei einem Widget mit
        // Anordnung erzwingt Qt zusaetzlich die Untergrenze der
        // Anordnung selbst, und die kommt aus den Kindern. Gemessen am
        // 2026-08-22: resize(360) liess das Applet bei 608 stehen,
        // also feuerte resizeEvent nie unter der Umbruchschwelle, also
        // brach nie etwas um. Ein ausdruecklich gesetztes Mindestmass
        // sticht die Anordnung.
        setMinimumWidth(300);

        // ── Verdrahtung ──────────────────────────────────────────────
        // v kommt jetzt immer als Betrag an (siehe box()-Aufruf oben,
        // Spanne beginnt bei 0) -- das Vorzeichen, das diese Betriebsart
        // fuer diese Kante vorsieht, bleibt erhalten: negativ, wenn die
        // Kante gerade negativ stand (der Normalfall bei LSB), sonst
        // positiv. Nur wenn die Kante zufaellig exakt auf 0 stand (keine
        // Seite erkennbar), gilt das Vorzeichen der jeweils ANDEREN
        // Kante als naechstbeste Auskunft ueber die Seite dieser
        // Betriebsart -- beide Kanten liegen bei jeder Betriebsart hier
        // auf derselben Seite oder symmetrisch, nie gemischt.
        connect(m_lowBox, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updatingFromModel) { return; }
            SliceModel* s = activeSlice();
            if (!s) { return; }
            m_lastEditedEdge = LastEditedEdge::Low;
            switch (sidebandOf(s->dspMode())) {
            case Sideband::Lower: s->setFilterHigh(-v); break;   // nahe Kante
            case Sideband::Upper: s->setFilterLow(v);   break;   // nahe Kante
            default: {
                // Zweiseitig: LOW ist die negative Kante. Nur wenn sie
                // zufaellig exakt auf 0 stand, entscheidet die andere
                // Kante ueber die Seite.
                const int prev = s->filterLow();
                const bool negative = prev != 0 ? (prev < 0) : (s->filterHigh() <= 0);
                s->setFilterLow(negative ? -v : v);
                break;
            }
            }
        });
        connect(m_highBox, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updatingFromModel) { return; }
            SliceModel* s = activeSlice();
            if (!s) { return; }
            m_lastEditedEdge = LastEditedEdge::High;
            switch (sidebandOf(s->dspMode())) {
            case Sideband::Lower: s->setFilterLow(-v);  break;   // ferne Kante
            case Sideband::Upper: s->setFilterHigh(v);  break;   // ferne Kante
            default: {
                const int prev = s->filterHigh();
                const bool negative = prev != 0 ? (prev < 0) : (s->filterLow() < 0);
                s->setFilterHigh(negative ? -v : v);
                break;
            }
            }
        });
        connect(m_widthBox, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updatingFromModel) { return; }
            SliceModel* s = activeSlice();
            if (!s) { return; }
            // Betreiber 2026-09-03: "ich muss beide Werte frei eingeben
            // koennen" — die Kante, die zuletzt von Hand gesetzt wurde,
            // bleibt stehen, die andere folgt der Breite. Seit dem
            // 2026-09-17 in AUDIO-Begriffen (siehe sidebandOf): LOW ist
            // die nahe Kante, HIGH die ferne, bei beiden Seitenbaendern.
            //
            //   LOW zuletzt:  HIGH = LOW + WIDTH      (100 + 3000 = 3100)
            //   HIGH zuletzt: LOW  = HIGH - WIDTH; reicht das unter den
            //                 Traeger, bleibt LOW bei 0 und HIGH wird
            //                 die Breite — nie ueber den Traeger hinweg.
            //
            // NUR fuer LSB/USB/RADE_L/RADE_U. CWL/CWU/DIGL/DIGU halten
            // die Mitte (Mithoerton bzw. Click-Tune-Versatz,
            // SliceModel::widthToEdges), die zweiseitigen Betriebsarten
            // ebenso — dort ist "welche Kante zuletzt" keine Frage.
            switch (s->dspMode()) {
            case DSPMode::LSB:
            case DSPMode::RADE_L:
            case DSPMode::USB:
            case DSPMode::RADE_U: {
                int nearHz = nearEdgeHz(s);
                int farHz  = farEdgeHz(s);
                if (m_lastEditedEdge == LastEditedEdge::Low) {
                    farHz = nearHz + v;
                } else {
                    nearHz = farHz - v;
                    if (nearHz < 0) { nearHz = 0; farHz = v; }
                }
                if (sidebandOf(s->dspMode()) == Sideband::Lower) {
                    s->setFilter(-farHz, -nearHz);
                } else {
                    s->setFilter(nearHz, farHz);
                }
                break;
            }
            default:
                s->setFilterWidth(v);
                break;
            }
        });
        connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
            // resetFilter, nicht resetFilterCenter: hier stand bis zum
            // 2026-08-23 nur das Zentrieren, und genau daran ist der
            // Betreiber auf 40 m haengengeblieben — seine Bandbreite
            // begann bei 2000 Hz, Sprache war praktisch weg, und der
            // Knopf, der danach aussieht, als hole er einen da heraus,
            // tat es nicht. Ein Knopf mit der Aufschrift "zurueck"
            // muss den ganzen Durchlass zuruecknehmen.
            if (SliceModel* s = activeSlice()) { s->resetFilter(); }
        });
        connect(m_spanBtn, &QPushButton::clicked, this, [this]() {
            // ── AUTO zuerst, dann die festen Stufen ─────────────────
            //
            // Der Betreiber am 2026-08-22 nach der Zeus-Vorfuehrung:
            // "der bandfilter sollte auch genau den bereich zeigen,
            // den man ausgewählt hat" — und gleich dazu: "kann auch
            // danach ein größerer bereich sein".
            //
            // Also beides, in dieser Reihenfolge: AUTO ist die
            // Vorgabe und richtet sich nach der GEWAEHLTEN Breite;
            // wer mehr Umgebung will, klickt weiter.
            static const int kSpans[] = {4000, 10000, 20000, 40000};
            if (m_spanAuto) {
                m_spanAuto = false;
                m_spanHz = kSpans[0];
            } else {
                int next = -1;
                for (int i = 0; i < 4; ++i) {
                    if (kSpans[i] == m_spanHz) { next = i + 1; break; }
                }
                if (next < 0 || next >= 4) { m_spanAuto = true; }
                else { m_spanHz = kSpans[next]; }
            }
            m_spanBtn->setText(m_spanAuto
                ? QStringLiteral("AUTO")
                : QStringLiteral("%1 kHz").arg(m_spanHz / 1000));
            for (int i = 0; i < m_panes.size(); ++i) { refreshPane(i); }
        });
    }

    root->addWidget(body);
}

SliceModel* BandwidthFilterApplet::sliceAt(int i) const
{
    if (!m_model) { return nullptr; }
    const QList<SliceModel*> list = m_model->slices();
    return (i >= 0 && i < list.size()) ? list.at(i) : nullptr;
}

SliceModel* BandwidthFilterApplet::activeSlice() const
{
    if (!m_model) { return nullptr; }
    if (SliceModel* s = m_model->activeSlice()) { return s; }
    return sliceAt(0);
}

void BandwidthFilterApplet::rebuildPanes()
{
    if (!m_model || !m_paneRow) { return; }

    // Mindestens EINE Flaeche, auch ohne Empfaenger. Eine leere Kachel
    // sieht aus wie ein Fehler; eine Flaeche, auf der „no radio" steht,
    // sagt, woran es liegt. Dieselbe Regel wie beim Panadapter-Kopf.
    const int want = std::max<int>(1, static_cast<int>(m_model->slices().size()));

    // Zu viele: die ueberzaehligen weg.
    while (m_panes.size() > want) {
        BandwidthFilterPane* p = m_panes.takeLast();
        m_sliceIndices.removeLast();
        m_paneRow->removeWidget(p);
        p->deleteLater();
    }

    // Zu wenige: nachlegen. Die vorhandenen bleiben stehen — eine
    // Flaeche, die beim Hinzufuegen einer Scheibe kurz verschwindet,
    // flackert.
    while (m_panes.size() < want) {
        const int i = m_panes.size();
        auto* pane = new BandwidthFilterPane(this);
        pane->setAccent(accentFor(i));
        pane->setSpan(m_spanHz);
        m_paneRow->addWidget(pane, 1);
        m_panes.append(pane);
        m_sliceIndices.append(i);
        wirePane(pane, i);
    }

    // ── IMMER neu verdrahten, nicht nur bei neuer Flaechenzahl ──────
    //
    // Der Fehler, den der Betreiber am 2026-08-22 fotografiert hat:
    // sein Bandfilter stand auf 14,22 MHz, waehrend das Geraet auf
    // 7,1156 empfing.
    //
    // Ursache: ohne Geraet gibt es keine Scheibe, aber trotzdem EINE
    // Flaeche (max(1, ...) oben — damit das Applet nicht leer
    // dasteht). wirePane() lief dafuer, fand keine Scheibe und knuepfte
    // KEINE Verbindung. Kam das Geraet dazu, blieb die gewuenschte
    // Flaechenzahl bei 1, beide Schleifen taten nichts — und
    // nachverdrahtet wurde nie. Die Achse blieb auf dem Vorgabewert
    // stehen (14,225 MHz, die 20-m-Vorgabe) und bewegte sich nie
    // wieder.
    //
    // Gemessen: nach setFrequency(7,1156 MHz) stand die Flaeche auf
    // 14,225; ein erzwungenes syncFromModel() lieferte sofort den
    // richtigen Wert. Also war nicht die Auffrischung kaputt, sondern
    // der Signalweg gar nicht vorhanden.
    for (const QMetaObject::Connection& c : m_paneConns) { disconnect(c); }
    m_paneConns.clear();
    for (int i = 0; i < m_panes.size(); ++i) { wirePane(m_panes[i], i); }

    for (int i = 0; i < m_panes.size(); ++i) { refreshPane(i); }
    refreshNumbers();
}

void BandwidthFilterApplet::wirePane(BandwidthFilterPane* pane, int sliceIndex)
{
    connect(pane, &BandwidthFilterPane::filterChanged, this,
            [this, sliceIndex](int low, int high) {
        // Nicht begrenzen — das tut setFilter. Die Flaeche meldet
        // Wunschwerte.
        if (SliceModel* s = sliceAt(sliceIndex)) {
            // Ziehen an der Flaeche ist ein Bedienereingriff genau wie
            // ein Zahlenfeld -- ohne diese Zeile merkt sich
            // m_lastEditedEdge das Ziehen nicht, und ein anschliessender
            // WIDTH-Eintrag verwirft die gerade gezogene Kante
            // stillschweigend (dieselbe Beschwerde, die diese ganze
            // Kennung ueberhaupt erst ausgeloest hat, jetzt ueber den
            // Flaechen- statt den Zahlenfeld-Weg). Welche Kante sich
            // staerker bewegt hat, gilt als die gezogene; bei
            // gleichzeitiger Verschiebung beider (Mitte ziehen) bleibt
            // die vorige Kennung stehen, weil keine Seite ausgezeichnet
            // ist.
            const int prevLow = s->filterLow();
            const int prevHigh = s->filterHigh();
            const int lowDelta = qAbs(low - prevLow);
            const int highDelta = qAbs(high - prevHigh);
            // In FELD-Begriffen (LOW = nahe Kante): bei LSB ist die
            // innere Low-Kante die ferne, also das HIGH-Feld.
            const bool lower = sidebandOf(s->dspMode()) == Sideband::Lower;
            if (lowDelta > highDelta) {
                m_lastEditedEdge = lower ? LastEditedEdge::High : LastEditedEdge::Low;
            } else if (highDelta > lowDelta) {
                m_lastEditedEdge = lower ? LastEditedEdge::Low : LastEditedEdge::High;
            }
            s->setFilterByHand(low, high);
        }
    });

    connect(pane, &BandwidthFilterPane::filterCentreChanged, this,
            [this, sliceIndex](int centre) {
        // Eigener Weg, weil hier die BREITE erhalten bleiben muss:
        // setFilterCenter begrenzt mit filterShift und laesst die andere
        // Kante mitwandern, wenn eine anstoesst.
        if (SliceModel* s = sliceAt(sliceIndex)) { s->setFilterCenter(centre); }
    });

    if (SliceModel* s = sliceAt(sliceIndex)) {
        m_paneConns.append(connect(s, &SliceModel::filterChanged, this,
                [this, sliceIndex](int, int) {
            refreshPane(sliceIndex);
            refreshNumbers();
        }));
        m_paneConns.append(connect(s, &SliceModel::frequencyChanged, this,
                [this, sliceIndex](double) { refreshPane(sliceIndex); }));
        m_paneConns.append(connect(s, &SliceModel::dspModeChanged, this,
                [this, sliceIndex](DSPMode) {
            refreshPane(sliceIndex);
            refreshNumbers();
        }));
    }
}

void BandwidthFilterApplet::refreshPane(int i)
{
    if (i < 0 || i >= m_panes.size()) { return; }
    BandwidthFilterPane* pane = m_panes.at(i);

    SliceModel* s = sliceAt(i);
    if (!s) {
        // Kein Empfaenger dahinter: die Flaeche steht da und sagt es.
        pane->setLabel(QStringLiteral("RX%1").arg(i + 1));
        pane->setHasFrequency(false);
        return;
    }
    // Die Kapsel traegt die Betriebsart mit: sie gehoert zur Kurve,
    // nicht in die Bedienzeile (Glas & Tiefe, 2026-09-17).
    pane->setLabel(QStringLiteral("RX%1 · %2").arg(i + 1)
                       .arg(SliceModel::modeName(s->dspMode())));

    pane->setFilter(s->filterLow(), s->filterHigh());

    // ── AUTO: die Spanne folgt der gewaehlten Breite ────────────────
    //
    // Zeus zeigt bei 2,9 kHz Filter rund 10 kHz Fenster — der Durchlass
    // fuellt also etwa ein Drittel und hat Umgebung, in der man sieht,
    // ob das Signal daneben liegt. Faktor 3,4, untere Grenze 2 kHz
    // (sonst wird CW zur Briefmarke), obere 40 kHz wie die feste
    // Stufenreihe.
    if (m_spanAuto) {
        // ── STUFIG, nicht stufenlos ─────────────────────────────────
        //
        // Der Betreiber am 2026-08-23: "das zittern beim abdrehen des
        // filters aus dem menü ist nervig."
        //
        // Die erste Fassung rechnete Breite mal 3,4 — stufenlos. Damit
        // verschob sich bei JEDEM Schritt der Massstab, und wer die
        // Breite durchdreht, sieht das ganze Bild wandern. Genau das
        // Zittern.
        //
        // Die Vorlage macht es anders, und man sieht es auf seinen
        // Bildern: bei OpenHPSDR blieb die Achse auf 14.158-14.168
        // stehen, WAEHREND der Filter von 2,4 auf 3,3 kHz wechselte.
        // Die Spanne haengt dort an der Groessenklasse, nicht am
        // genauen Wert.
        //
        // Also Stufen. Alle ueblichen Sprechfilter (2,7 / 2,9 / 3,2 /
        // 3,3 / 3,5) fallen in dieselbe — beim Durchdrehen bewegt sich
        // nichts.
        const int wHz = qAbs(s->filterHigh() - s->filterLow());
        int span = 40000;
        if      (wHz <=   700) { span =  3000; }   // CW eng
        else if (wHz <=  1600) { span =  6000; }   // CW weit, RTTY
        else if (wHz <=  4000) { span = 10000; }   // SSB, alle Breiten
        else if (wHz <=  7000) { span = 16000; }   // eSSB
        else if (wHz <= 12000) { span = 25000; }   // AM
        pane->setSpan(span);
    } else {
        pane->setSpan(m_spanHz);
    }
    pane->setVfoFrequency(s->frequency());

    // Ohne Frequenz keine Achse. Eine erfundene waere eine Behauptung.
    pane->setHasFrequency(s->frequency() > 0.0);
}

// Ein leerer Platz sagt „leer" und ist blass; ein belegter zeigt seine
// Breite. Ein Knopf, der nur „VAR 1" sagt, laesst offen, ob ein Klick
// etwas tut.
void BandwidthFilterApplet::refreshVarButtons()
{
    SliceModel* s = activeSlice();
    for (int i = 0; i < m_varBtns.size(); ++i) {
        QPushButton* b = m_varBtns.at(i);
        const bool filled = s && s->hasVarFilter(i);
        if (filled) {
            const QPair<int,int> v = s->varFilter(i);
            const int bw = v.second - v.first;
            b->setText(bw >= 1000
                ? QStringLiteral("%1k").arg(bw / 1000.0, 0, 'f', 1)
                : QStringLiteral("%1").arg(bw));
            b->setToolTip(QStringLiteral(
                "VAR %1: %2 Hz … %3 Hz — Klick holt zurück, "
                "Rechtsklick überschreibt.").arg(i + 1)
                .arg(v.first).arg(v.second));
        } else {
            b->setText(QStringLiteral("VAR %1").arg(i + 1));
            b->setToolTip(QStringLiteral(
                "Noch leer. VAR 1 füllt sich von selbst, sobald du am "
                "Filter ziehst; Rechtsklick legt die jetzige "
                "Einstellung ab."));
        }
        b->setEnabled(true);
        b->setProperty("varFilled", filled);
    }
}

// Welche FELD-Kante als Anker gilt, solange der Bedienende noch keine
// selbst gesetzt hat: die nahe (LOW). So rechnet auch
// SliceModel::widthToEdges — LSB verankert high=-defaultLowCut(), USB
// low=defaultLowCut(), beides die nahe Kante. Fuer alles andere
// (CW/DIG/AM/FM/...) fragt der WIDTH-Anschluss den Wert nicht ab.
BandwidthFilterApplet::LastEditedEdge
BandwidthFilterApplet::naturalAnchorEdge(DSPMode mode)
{
    Q_UNUSED(mode);
    return LastEditedEdge::Low;
}

void BandwidthFilterApplet::refreshNumbers()
{
    SliceModel* s = activeSlice();
    if (!s || !m_lowBox) { return; }

    // Scheibe oder Betriebsart seit dem letzten Mal gewechselt? Dann ist
    // m_lastEditedEdge die Auskunft einer ANDEREN Scheibe/Betriebsart und
    // keine verlaessliche Angabe mehr fuer diese hier -- zurueck auf den
    // natuerlichen Anker dieser Betriebsart. Ohne das wuerde die erste
    // WIDTH-Eingabe nach einem Wechsel (oder die allererste der ganzen
    // Sitzung) manchmal die falsche Kante festhalten, siehe
    // naturalAnchorEdge().
    const int modeInt = static_cast<int>(s->dspMode());
    if (s != m_lastSyncedSlice || modeInt != m_lastSyncedModeInt) {
        m_lastEditedEdge = naturalAnchorEdge(s->dspMode());
        m_lastSyncedSlice = s;
        m_lastSyncedModeInt = modeInt;
    }

    m_updatingFromModel = true;

    const QSignalBlocker b1(m_lowBox);
    const QSignalBlocker b2(m_highBox);
    const QSignalBlocker b3(m_widthBox);

    // Audio-Begriffe, siehe sidebandOf(): LOW = nahe Kante, HIGH = ferne
    // Kante — bei LSB also |filterHigh| und |filterLow|. Betraege, weil
    // "minus darf nie!!!!!" (2026-09-03) fuer jede Anzeige dieser Kanten
    // gilt, nicht nur die schwebende Beschriftung im Bild.
    m_lowBox->setValue(nearEdgeHz(s));
    m_highBox->setValue(farEdgeHz(s));
    // qAbs() hier ebenso: SliceModel::filterWidth() ist ein einfaches
    // filterHigh()-filterLow(), das negativ wird, sobald LOW zahlenmaessig
    // kleiner als HIGH steht (z.B. LOW=50, HIGH=150 bei LSB -- ungewoehnlich,
    // aber nach "er muss das machen, was ich eingebe" gueltig, seit die
    // beiden Kanten oben unabhaengig voneinander stehen bleiben). Ohne
    // qAbs() kappt QSpinBox::setValue() den negativen Wert stillschweigend
    // auf das Feldminimum (50) -- eine Zahl, die nichts Echtes mehr zeigt.
    m_widthBox->setValue(qAbs(s->filterWidth()));

    m_updatingFromModel = false;
    refreshVarButtons();
}

void BandwidthFilterApplet::syncFromModel()
{
    rebuildPanes();
}

// ── Eng wird weniger, nicht abgeschnitten ───────────────────────────
//
// Der Betreiber am 2026-08-22: "weiters verändert sich das fenster
// bandfilter und der inhalt nicht automatisch, sobald ich die größe
// verändere" — und nachgeschoben: "vor allem verkleinert!"
//
// Genau so war es. Die Bedienzeile stand in EINER Reihe: drei
// Wortmarken, drei Zahlenfelder mit fester Breite, dazu VAR1, VAR2
// und Zentrieren. Zusammen ein harter Boden von rund 700 Punkten.
// Wurde das Fenster schmaler, sprang ein Rollbalken an und schnitt
// den Rest ab — der Inhalt passte sich nicht an, er verschwand.
//
// Zwei Massnahmen, in dieser Reihenfolge: die Zahlenfelder duerfen
// jetzt bis 62 Punkte schrumpfen (oben), und darunter fallen die
// Wortmarken weg. Sie sind Beschriftungen, keine Information — die
// Einheit steht im Feld selbst ("2900 Hz"), und die Reihenfolge
// tief/breit/hoch ist dieselbe wie im Bild darueber.
void BandwidthFilterApplet::setSpectrumSource(SpectrumSource src)
{
    m_spectrumSource = std::move(src);
    if (!m_traceTimer) {
        // 20 Hz. Schneller waere Verschwendung: die Kurve dient dem
        // Augenmass beim Kantenziehen, nicht der Signalsuche — dafuer
        // ist der Panadapter da.
        m_traceTimer = new QTimer(this);
        m_traceTimer->setInterval(50);
        connect(m_traceTimer, &QTimer::timeout, this, [this]() {
            if (!m_spectrumSource) { return; }
            for (int i = 0; i < m_panes.size(); ++i) {
                BandwidthFilterPane* pane = m_panes.at(i);
                SliceModel* s = sliceAt(i);
                if (!pane || !s || !pane->isVisible()) { continue; }
                const double f  = s->frequency();
                if (f <= 0.0) { pane->setTrace({}); continue; }
                const double half = pane->spanHz() / 2.0;
                // ── Eine Stuetzstelle je ZWOELF Bildpunkte ────────
                //
                // Hier stand "je zwei Bildpunkte" — und genau daraus
                // kam die Unruhe, die der Betreiber mit "extrem
                // unruhig" beschrieben hat.
                //
                // Auf seinen OpenHPSDR-Bildern vom 2026-08-23 zaehlt
                // man im Rauschbereich Zacken von rund zwoelf
                // Bildpunkten Abstand: die Vorlage tastet GROB ab und
                // verbindet die Punkte gerade. Bei uns lagen die
                // Punkte sechsmal dichter, also lag zwischen zwei
                // Zacken kaum ein Pixel — aus der Kurve wurde ein
                // flimmerndes Band.
                //
                // Die Spitzen gehen dabei nicht verloren: die Quelle
                // nimmt je Eimer das MAXIMUM, ein Traeger bleibt also
                // in voller Hoehe stehen, nur eben als eine Zacke
                // statt als sechs.
                //
                // ── Zwoelf BILDSCHIRMpunkte, nicht zwoelf Qt-Punkte ──
                //
                // Der Betreiber am 2026-09-17: "der bandfilter könnte
                // noch genauer und besser sein."
                //
                // Die zwoelf waren von seinen OpenHPSDR-Bildern
                // abgezaehlt — Bildschirmpunkte. `pane->width()` zaehlt
                // aber Qt-Punkte, und auf seinem Retina-Schirm ist
                // jeder davon zwei Bildschirmpunkte. Unsere
                // Stuetzstellen lagen also doppelt so weit auseinander
                // wie die der Vorlage: 51 Stueck auf 620 Qt-Punkten,
                // eine je 196 Hz bei 10 kHz Spanne — ein Sprechsignal
                // von 2,8 kHz bestand aus vierzehn Punkten. Jetzt in
                // Bildschirmpunkten gerechnet: derselbe Abstand wie in
                // der Vorlage, auf Retina doppelt so viele Stellen,
                // auf einem 1:1-Schirm unveraendert. Dieselbe
                // Korrektur, die der Panadapter am 2026-08-26 fuer
                // seine Abtastbreite bekommen hat (displayWidth in
                // Geraetepunkten, SpectrumWidget::pushSpectrum).
                //
                // Untergrenze 64 statt 48, Obergrenze 400 statt 200 —
                // die alten Deckel waren fuer die halbe Dichte gesetzt.
                const qreal dpr = pane->devicePixelRatioF();
                const int pts = qBound(
                    64, static_cast<int>(std::lround(pane->width() * dpr / 12.0)),
                    400);
                pane->setTrace(
                    m_spectrumSource(i, f - half, f + half, pts));
            }
        });
    }
    m_traceTimer->start();
}

QSize BandwidthFilterApplet::minimumSizeHint() const
{
    const QSize base = AppletWidget::minimumSizeHint();
    // 300 Punkte: darunter wird selbst die umgebrochene Fassung eng.
    return QSize(qMin(300, base.width()), base.height());
}

void BandwidthFilterApplet::resizeEvent(QResizeEvent* event)
{
    AppletWidget::resizeEvent(event);
    if (!m_ctrlRow || !m_ctrlRow2) { return; }

    // Eng: die Knopfgruppen (Memory, Span) rutschen in eine eigene
    // Reihe. Ohne das blieb ein harter Boden von rund 600 Punkten, und
    // darunter schnitt ein Rollbalken den Inhalt ab, statt ihn zu
    // verkleinern — genau der Befund des Betreibers ("vor allem
    // verkleinert!"). Breit bleibt alles, wie es war: umgebrochen
    // wird erst unterhalb der Schwelle. Die Versalzeilen ueber den
    // Feldern bleiben immer — sie sind neun Punkte hoch und kosten
    // keine Breite.
    const bool wrap = width() < 470;
    if (wrap == m_ctrlWrapped) { return; }
    m_ctrlWrapped = wrap;

    QList<QWidget*> movers;
    if (m_memoryGroup) { movers.append(m_memoryGroup); }
    if (m_spanGroup)   { movers.append(m_spanGroup); }

    for (QWidget* wgt : movers) {
        if (wrap) {
            m_ctrlRow->removeWidget(wgt);
            m_ctrlRow2->addWidget(wgt);
        } else {
            m_ctrlRow2->removeWidget(wgt);
            m_ctrlRow->addWidget(wgt);
        }
    }
    if (wrap) { m_ctrlRow2->addStretch(1); }
    updateGeometry();
}


} // namespace Longpath