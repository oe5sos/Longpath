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
    // waere ehrlicher, aber bei vier Scheiben unlesbar; die
    // Beschriftung sagt, welche gemeint ist.
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(6);

        auto label = [&](const QString& t) {
            auto* l = new QLabel(t, body);
            l->setStyleSheet(QStringLiteral(
                "QLabel { color: %1; font-size: 10px; font-weight: bold; }")
                .arg(QLatin1String(Style::kTextScale)));
            return l;
        };

        auto box = [&](int lo, int hi) {
            auto* sb = new QSpinBox(body);
            sb->setRange(lo, hi);
            sb->setSingleStep(50);
            sb->setSuffix(QStringLiteral(" Hz"));
            // Nachgiebig statt fest: 92 Punkte sind die Wunschbreite,
            // 62 die Schmerzgrenze. Vorher war es setFixedWidth(92) —
            // damit hatte die Bedienzeile einen harten Boden von rund
            // 700 Punkten, und wer das Fenster kleiner zog, bekam
            // einen Rollbalken statt eines kleineren Inhalts.
            sb->setMinimumWidth(62);
            sb->setMaximumWidth(92);
            sb->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
            sb->setStyleSheet(Style::spinBoxStyle());
            sb->setKeyboardTracking(false);   // erst bei Enter/Verlassen
            return sb;
        };

        m_modeLbl = new QLabel(QStringLiteral("—"), body);
        m_modeLbl->setStyleSheet(QStringLiteral(
            "QLabel { color: %1; font-size: 11px; font-weight: bold; }")
            .arg(QLatin1String(Style::kTextPrimary)));
        row->addWidget(m_modeLbl);

        auto addShrinkableLabel = [&](const QString& t) {
            QLabel* l = label(t);
            m_shrinkableLabels.append(l);
            row->addWidget(l);
        };

        addShrinkableLabel(QStringLiteral("LOW"));
        m_lowBox = box(-SliceModel::kMaxFilterWidthHz,
                        SliceModel::kMaxFilterWidthHz);
        m_lowBox->setObjectName(QStringLiteral("bwFilterLow"));
        row->addWidget(m_lowBox);

        addShrinkableLabel(QStringLiteral("WIDTH"));
        m_widthBox = box(10, 2 * SliceModel::kMaxFilterWidthHz);
        m_widthBox->setObjectName(QStringLiteral("bwFilterWidth"));
        m_widthBox->setToolTip(QStringLiteral(
            "Type a width and the edges land where this mode wants them: "
            "CW centred on the sidetone, SSB anchored at the default low "
            "cut, AM symmetric around zero."));
        row->addWidget(m_widthBox);

        addShrinkableLabel(QStringLiteral("HIGH"));
        m_highBox = box(-SliceModel::kMaxFilterWidthHz,
                         SliceModel::kMaxFilterWidthHz);
        m_highBox->setObjectName(QStringLiteral("bwFilterHigh"));
        row->addWidget(m_highBox);

        // ── VAR1 und VAR2 ────────────────────────────────────────
        //
        // Der Grund: ohne sie ist die eigene Handeinstellung nach einem
        // Klick auf „2.4k" weg. VAR1 fuellt sich VON SELBST, sobald man
        // zieht (SliceModel::setFilterByHand); VAR2 ist der zweite
        // Platz, den man bewusst belegt — Rechtsklick.
        //
        // Ein Klick holt zurueck, ein Rechtsklick legt ab. Ein leerer
        // Platz sagt das auch, statt still nichts zu tun.
        for (int i = 0; i < SliceModel::kVarSlots; ++i) {
            QPushButton* b = styledButton(
                QStringLiteral("VAR %1").arg(i + 1), 56);
            b->setContextMenuPolicy(Qt::CustomContextMenu);
            m_varBtns.append(b);
            row->addWidget(b);

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

        m_resetBtn = styledButton(QStringLiteral("↺ Centre"), 78);
        m_resetBtn->setToolTip(QStringLiteral(
            "Put the passband back where this mode wants it — on the "
            "sidetone for CW, at the default low cut for SSB."));
        row->addWidget(m_resetBtn);

        row->addStretch(1);

        m_spanBtn = styledButton(QStringLiteral("10 kHz"), 62);
        m_spanBtn->setToolTip(QStringLiteral(
            "How much band the panes show. Click to cycle."));
        row->addWidget(m_spanBtn);

        m_ctrlRow = row;
        col->addLayout(row);

        // Zweite Reihe, zunaechst leer. Sie fuellt sich erst, wenn es
        // eng wird (resizeEvent) — breit bleibt alles wie bisher.
        m_ctrlRow2 = new QHBoxLayout;
        m_ctrlRow2->setSpacing(6);
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
        connect(m_lowBox, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updatingFromModel) { return; }
            if (SliceModel* s = activeSlice()) { s->setFilterLow(v); }
        });
        connect(m_highBox, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updatingFromModel) { return; }
            if (SliceModel* s = activeSlice()) { s->setFilterHigh(v); }
        });
        connect(m_widthBox, &QSpinBox::valueChanged, this, [this](int v) {
            if (m_updatingFromModel) { return; }
            // Die Regel je Betriebsart steckt im Modell, nicht hier.
            if (SliceModel* s = activeSlice()) { s->setFilterWidth(v); }
        });
        connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
            if (SliceModel* s = activeSlice()) { s->resetFilterCenter(); }
        });
        connect(m_spanBtn, &QPushButton::clicked, this, [this]() {
            // Vier Stufen reichen: eng genug fuer CW, weit genug fuer AM.
            static const int kSpans[] = {4000, 10000, 20000, 40000};
            int next = 0;
            for (int i = 0; i < 4; ++i) {
                if (kSpans[i] == m_spanHz) { next = (i + 1) % 4; break; }
            }
            m_spanHz = kSpans[next];
            m_spanBtn->setText(QStringLiteral("%1 kHz").arg(m_spanHz / 1000));
            for (BandwidthFilterPane* p : m_panes) { p->setSpan(m_spanHz); }
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
        if (SliceModel* s = sliceAt(sliceIndex)) { s->setFilterByHand(low, high); }
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
    pane->setLabel(QStringLiteral("RX%1").arg(i + 1));

    SliceModel* s = sliceAt(i);
    if (!s) {
        // Kein Empfaenger dahinter: die Flaeche steht da und sagt es.
        pane->setHasFrequency(false);
        return;
    }

    pane->setFilter(s->filterLow(), s->filterHigh());
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

void BandwidthFilterApplet::refreshNumbers()
{
    SliceModel* s = activeSlice();
    if (!s || !m_lowBox) { return; }

    m_updatingFromModel = true;

    const QSignalBlocker b1(m_lowBox);
    const QSignalBlocker b2(m_highBox);
    const QSignalBlocker b3(m_widthBox);

    m_lowBox->setValue(s->filterLow());
    m_highBox->setValue(s->filterHigh());
    m_widthBox->setValue(s->filterWidth());
    m_modeLbl->setText(SliceModel::modeName(s->dspMode()));

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
                // Eine Stuetzstelle je zwei Bildpunkte reicht; mehr
                // sieht man nicht, kostet aber jede Runde.
                const int pts = qBound(64, pane->width() / 2, 512);
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

    // Erste Stufe: die Wortmarken. Sie sind Beschriftung, keine
    // Information — die Einheit steht im Feld selbst ("2900 Hz"), und
    // die Reihenfolge tief/breit/hoch ist dieselbe wie im Bild
    // darueber.
    const bool roomy = width() >= 470;
    for (QLabel* l : m_shrinkableLabels) {
        if (l) { l->setVisible(roomy); }
    }

    // Zweite Stufe: die Knopfgruppe rutscht in eine eigene Reihe.
    //
    // Ohne sie blieb ein harter Boden von rund 600 Punkten, und
    // darunter schnitt ein Rollbalken den Inhalt ab, statt ihn zu
    // verkleinern — genau der Befund des Betreibers ("vor allem
    // verkleinert!"). Breit bleibt alles, wie es war: umgebrochen
    // wird erst unterhalb der Schwelle.
    const bool wrap = width() < 470;
    if (wrap == m_ctrlWrapped) { return; }
    m_ctrlWrapped = wrap;

    QList<QWidget*> movers;
    for (QPushButton* b : m_varBtns) { if (b) { movers.append(b); } }
    if (m_resetBtn) { movers.append(m_resetBtn); }
    if (m_spanBtn)  { movers.append(m_spanBtn); }

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