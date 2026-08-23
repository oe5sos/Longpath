// =================================================================
// src/gui/applets/TxMeterApplet.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Begruendung steht in der Kopfdatei.
//
// =================================================================
// Modification history (Longpath):
//   2026-08-23 — Angelegt fuer Longpath von Martin Fischer,
//                KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "gui/applets/TxMeterApplet.h"

#include "core/AppSettings.h"
#include "gui/StyleConstants.h"
#include "gui/meters/MeterPoller.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QActionGroup>
#include <QContextMenuEvent>
#include <QLabel>
#include <QMenu>
#include <QVBoxLayout>

namespace Longpath {

TxMeterApplet::TxMeterApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
    , m_tuneBinding(MeterBinding::TxSwr)
    , m_txBinding(MeterBinding::TxPower)
{
    auto* lay = new QVBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 2);
    lay->setSpacing(1);

    m_bar = new BarInstrument(this);
    // ── Knapp, weil eine Zeile im Panel kurz ist ────────────────────
    //
    // Ohne das legt sich die Fusszeile ("Spitze — · Grenze 2.50") ueber
    // die Skalenzahlen. Auf dem Bildschirmfoto des Betreibers vom
    // 2026-08-23 steht dort "Spitze — 1.5Grenze 2.50 2 2.5 3"
    // uebereinander.
    //
    // Derselbe Fehler wie bei den Zusatzzeilen im Frequenz-Widget,
    // und ich habe ihn dort behoben und hier vergessen — das Applet
    // hat seinen eigenen Balken. Ein Fehler, der zweimal in
    // verschiedenen Dateien wohnt, wird beim ersten Mal nur halb
    // behoben.
    m_bar->setCompact(true);
    lay->addWidget(m_bar);

    // Eine kleine Zeile, die sagt, WELCHE Groesse gerade steht. Ohne
    // sie waere eine Anzeige, die selbstaendig umschaltet, eine Falle:
    // man liest eine Zahl und weiss nicht, wovon.
    m_state = new QLabel(this);
    m_state->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_state->setStyleSheet(
        QStringLiteral("QLabel { color: %1; font-size: 9px; padding-right: 6px; }")
            .arg(Style::role("text-scale", Style::kTextScale)));
    lay->addWidget(m_state);

    if (m_model) {
        connect(&m_model->transmitModel(), &TransmitModel::moxChanged,
                this, [this](bool) { chooseBinding(); });
        connect(&m_model->transmitModel(), &TransmitModel::tuneChanged,
                this, [this](bool) { chooseBinding(); });
    }

    applyBinding(m_tuneBinding);
    chooseBinding();
}

void TxMeterApplet::setBindings(int whileTuning, int whileTransmitting)
{
    m_tuneBinding = whileTuning;
    m_txBinding   = whileTransmitting;
    chooseBinding();
    saveState();
}

void TxMeterApplet::chooseBinding()
{
    if (!m_model) { return; }
    const TransmitModel& tx = m_model->transmitModel();
    const bool tuning = tx.isTune();
    const bool sending = tx.isMox();

    if (tuning) {
        m_wasActive = true;
        applyBinding(m_tuneBinding);
        return;
    }
    if (sending) {
        m_wasActive = true;
        applyBinding(m_txBinding);
        return;
    }

    // ── Ruhe: stehen lassen, was zuletzt galt ───────────────────────
    //
    // Begruendung in der Kopfdatei. Kurz: wer nach dem Loslassen der
    // Taste hinsieht, will sehen, was er gerade gemacht hat — nicht
    // eine Groesse, die seit einer Minute nicht mehr gemessen wurde.
    if (!m_wasActive && m_current < 0) {
        applyBinding(m_tuneBinding);
    }
    if (m_state) {
        m_state->setText(m_current == m_tuneBinding
                             ? tr("SWR  ·  Ruhe")
                             : tr("Leistung  ·  Ruhe"));
    }
}

void TxMeterApplet::applyBinding(int bindingId)
{
    if (m_current == bindingId) { return; }
    m_current = bindingId;
    if (m_bar) {
        m_bar->setPrimary(bindingId);
        // ── Wert UND Spitze loeschen, nicht nur die Spitze ──────────
        //
        // Hier stand zuerst nur resetPeak(). Das genuegt nicht, und
        // die Pruefung hat es sofort gezeigt: resetPeak setzt die
        // Spitze auf den AKTUELLEN Wert, und der ist beim Umschalten
        // noch der alte — nach dem Senden also 100 Watt, jetzt gelesen
        // auf einer SWR-Skala, die bis 3 geht. Der Balken stand am
        // Anschlag und behauptete eine katastrophale Fehlanpassung.
        //
        // clearValue() nimmt beides weg. Die Flaeche steht dann leer,
        // bis der erste Messwert der neuen Groesse kommt — das ist
        // ehrlich, denn gemessen wurde sie ja noch nicht.
        m_bar->clearValue();
    }
    if (m_state) {
        m_state->setText(bindingId == m_tuneBinding ? tr("SWR  ·  Abstimmen")
                                                    : tr("Leistung  ·  Senden"));
    }
}

void TxMeterApplet::onReading(int bindingId, double value)
{
    if (m_bar) { m_bar->onReading(bindingId, value); }
}

void TxMeterApplet::syncFromModel()
{
    chooseBinding();
}

namespace {
QString tubeKey() { return QStringLiteral("TxMeterApplet_BarTube"); }
QString segKey()  { return QStringLiteral("TxMeterApplet_BarSegments"); }
QString swapKey() { return QStringLiteral("TxMeterApplet_Swapped"); }
} // namespace

void TxMeterApplet::saveState() const
{
    auto& st = AppSettings::instance();
    st.setValue(tubeKey(), (m_bar && m_bar->isTube()) ? QStringLiteral("True")
                                                      : QStringLiteral("False"));
    st.setValue(segKey(), (m_bar && m_bar->isSegmented())
                              ? QStringLiteral("True") : QStringLiteral("False"));
    st.setValue(swapKey(), m_tuneBinding == MeterBinding::TxPower
                               ? QStringLiteral("True") : QStringLiteral("False"));
}

void TxMeterApplet::restoreState()
{
    auto& st = AppSettings::instance();
    if (m_bar) {
        m_bar->setTube(st.value(tubeKey(), QStringLiteral("False")).toString()
                       == QStringLiteral("True"));
        m_bar->setSegmented(
            st.value(segKey(), QStringLiteral("False")).toString()
            == QStringLiteral("True"));
    }
    const bool swapped =
        st.value(swapKey(), QStringLiteral("False")).toString()
        == QStringLiteral("True");
    m_tuneBinding = swapped ? MeterBinding::TxPower : MeterBinding::TxSwr;
    m_txBinding   = swapped ? MeterBinding::TxSwr   : MeterBinding::TxPower;
    m_current = -1;   // erzwingt ein Anwenden
    chooseBinding();
}

void TxMeterApplet::contextMenuEvent(QContextMenuEvent* ev)
{
    QMenu menu(this);

    // ── Die Zuordnung umdrehen ──────────────────────────────────────
    //
    // Sie steht hier, weil ich die Woerter des Betreibers deuten
    // musste: "Stehwelle" und "SWR" sind dasselbe Wort, und welche
    // Groesse er bei welchem Zustand sehen will, ergab sich nur aus
    // dem Zusammenhang. Ein Menuepunkt kostet nichts und macht die
    // Deutung widerrufbar.
    auto* which = menu.addMenu(tr("Zuordnung"));
    auto* group = new QActionGroup(&menu);
    group->setExclusive(true);
    const struct { const char* label; bool swapped; } kOrder[] = {
        {QT_TR_NOOP("Abstimmen: SWR  ·  Senden: Leistung"), false},
        {QT_TR_NOOP("Abstimmen: Leistung  ·  Senden: SWR"), true},
    };
    for (const auto& o : kOrder) {
        QAction* a = which->addAction(tr(o.label));
        a->setCheckable(true);
        a->setChecked((m_tuneBinding == MeterBinding::TxPower) == o.swapped);
        group->addAction(a);
        const bool sw = o.swapped;
        connect(a, &QAction::triggered, this, [this, sw]() {
            m_current = -1;
            setBindings(sw ? MeterBinding::TxPower : MeterBinding::TxSwr,
                        sw ? MeterBinding::TxSwr   : MeterBinding::TxPower);
        });
    }

    menu.addSeparator();
    QAction* seg = menu.addAction(tr("Segmente"));
    seg->setCheckable(true);
    seg->setChecked(m_bar && m_bar->isSegmented());
    connect(seg, &QAction::triggered, this, [this](bool on) {
        if (m_bar) { m_bar->setSegmented(on); }
        saveState();
    });
    QAction* tube = menu.addAction(tr("Roehre (3D)"));
    tube->setCheckable(true);
    tube->setChecked(m_bar && m_bar->isTube());
    connect(tube, &QAction::triggered, this, [this](bool on) {
        if (m_bar) { m_bar->setTube(on); }
        saveState();
    });

    menu.exec(ev->globalPos());
    ev->accept();
}

} // namespace Longpath
