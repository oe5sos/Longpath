// no-port-check: NereusSDR-original. Kein Upstream-Port.

// SPDX-License-Identifier: GPL-3.0-or-later
#include "gui/chrome/TxSwitchBar.h"

#include "core/MoxController.h"
#include "core/PureSignal.h"
#include "gui/StyleConstants.h"
#include "gui/styles/ThemeQss.h"
#include "models/RadioModel.h"
#include "models/TransmitModel.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSignalBlocker>

namespace Longpath {

namespace {

struct Spec { const char* text; const char* tip; int rung; };

/// Faltreihenfolge: niedrige Sprosse faellt zuerst
/// (ChromeBarController.cpp:131). PS zuerst, MOX zuletzt.
Spec specFor(TxSwitch::Kind k)
{
    switch (k) {
    case TxSwitch::Kind::Ps:
        return {"PS", "PureSignal-Vorverzerrung ein- und ausschalten", 13};
    case TxSwitch::Kind::Tune:
        return {"TUNE", "Traeger mit Abstimmleistung senden", 14};
    case TxSwitch::Kind::Vox:
        return {"VOX", "Stimmgesteuertes Senden", 15};
    case TxSwitch::Kind::Mox:
        return {"MOX", "Senden ein- und ausschalten", 16};
    }
    return {"?", "", 13};
}

} // namespace

TxSwitch::TxSwitch(Kind kind, RadioModel* model, QWidget* parent)
    : QWidget(parent)
    , m_kind(kind)
    , m_model(model)
{
    const Spec s = specFor(kind);

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    lay->setSpacing(0);

    m_btn = new QPushButton(QString::fromLatin1(s.text), this);
    m_btn->setCheckable(true);
    m_btn->setToolTip(QString::fromLatin1(s.tip));
    m_btn->setCursor(Qt::PointingHandCursor);
    m_btn->setFocusPolicy(Qt::NoFocus);
    // MOX und TUNE tragen die Sendefarbe, VOX und PS die neutrale:
    // die Farbe sagt „hier geht Leistung raus", nicht „hier ist ein
    // Knopf".
    const bool isRf = (kind == Kind::Mox || kind == Kind::Tune);

    // ── Zuschnitt wie bei Zeus (2026-08-20) ──────────────────────────
    //
    // Vorlage ist der Bildschirm des Betreibers: dort stehen MOX, VOX,
    // TUNE und PS als GROSSER, GESPERRTER TEXT ohne Rahmen, mit einem
    // kleinen Punkt davor, der den Zustand traegt. Bei uns waren es
    // kleine umrandete Pillen zwischen zwanzig anderen kleinen
    // umrandeten Pillen — die vier wichtigsten Schalter der ganzen
    // Leiste sahen aus wie alles andere auch.
    //
    // Der Rahmen faellt weg, die Schrift wird groesser und gesperrt.
    // Angeschaltet traegt der TEXT die Farbe, nicht ein gefuellter
    // Kasten: bei MOX und TUNE die Sendefarbe, bei VOX und PS die
    // neutrale. Ein rot gefuellter Kasten unten links sieht aus wie
    // eine Warnung; roter Text sagt „hier geht Leistung raus".
    const QString onColour = QString::fromLatin1(isRf ? Style::kTxRed
                                                      : Style::kAccent);
    m_btn->setStyleSheet(Style::themed(QStringLiteral(
        "QPushButton { background: transparent; color: %1;"
        " border: none; padding: 2px 4px; font-weight: 600;"
        " font-size: 14px; letter-spacing: 2px; }"
        "QPushButton:hover { color: %2; }"
        "QPushButton:checked { color: %3; }")
        .arg(QString::fromLatin1(Style::kTextScale),
             QString::fromLatin1(Style::kTextPrimary),
             onColour)));

    // Der Punkt davor. Er traegt denselben Zustand ein zweites Mal —
    // Farbe UND Fuellung — damit man ihn auch im Augenwinkel sieht,
    // ohne den Text zu lesen. Zeus macht es genauso.
    m_dot = new QLabel(QStringLiteral("\u25CF"), this);
    m_dot->setFixedWidth(11);
    m_dot->setAlignment(Qt::AlignCenter);
    m_dotOnColour = onColour;
    lay->addWidget(m_dot);
    lay->addSpacing(1);
    lay->addWidget(m_btn);
    updateDot(false);

    wire();
}

// Der Zustandspunkt. Aus, heisst blass und klein; an, heisst in der
// Farbe des Schalters.
void TxSwitch::updateDot(bool on)
{
    if (!m_dot) { return; }
    m_dot->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; font-size: %2px; background: transparent; }")
        .arg(on ? m_dotOnColour
                : QString::fromLatin1(Style::kTextInactive))
        .arg(on ? 11 : 8));
}

int TxSwitch::rung() const { return specFor(m_kind).rung; }

void TxSwitch::wire()
{
    if (!m_model) { return; }

    // Jede Richtung getrennt, und die Rueckrichtung mit Sperre: das
    // Modell fuehrt den Knopf nach, ohne dass die Nachfuehrung wieder
    // als Bedienung zaehlt. Dasselbe Muster wie in der TxApplet
    // (TxApplet.cpp:1063), damit beide Flaechen sich gleich verhalten.
    auto pushBack = [this](QPushButton* b, bool on) {
        QSignalBlocker blk(b);
        m_updatingFromModel = true;
        b->setChecked(on);
        m_updatingFromModel = false;
        updateDot(on);
    };

    // Auch die eigene Bedienung faerbt den Punkt — sonst zoege er nur
    // nach, wenn das Modell antwortet, und bliebe bei einer abgelehnten
    // Umschaltung falsch stehen.
    connect(m_btn, &QPushButton::toggled, this,
            [this](bool on) { updateDot(on); });

    switch (m_kind) {
    case Kind::Mox: {
        MoxController* mox = m_model->moxController();
        if (!mox) { break; }
        connect(m_btn, &QPushButton::toggled, this, [this, mox](bool on) {
            if (m_updatingFromModel) { return; }
            mox->setMox(on);
        });
        // moxStateChanged feuert am ENDE des Zeitlaufs, also erst wenn
        // der Sender wirklich steht — der Knopf zeigt den bestaetigten
        // Zustand und nicht die Absicht.
        connect(mox, &MoxController::moxStateChanged, this,
                [this, pushBack](bool on) { pushBack(m_btn, on); });
        break;
    }
    case Kind::Tune: {
        MoxController* mox = m_model->moxController();
        connect(m_btn, &QPushButton::toggled, this, [this](bool on) {
            if (m_updatingFromModel) { return; }
            m_model->setTune(on);
        });
        if (mox) {
            connect(mox, &MoxController::manualMoxChanged, this,
                    [this, pushBack](bool isManual) {
                pushBack(m_btn, isManual);
                m_btn->setText(isManual ? QStringLiteral("TUNING")
                                        : QStringLiteral("TUNE"));
            });
        }
        break;
    }
    case Kind::Vox: {
        TransmitModel& tx = m_model->transmitModel();
        connect(m_btn, &QPushButton::toggled, this, [this, &tx](bool on) {
            if (m_updatingFromModel) { return; }
            tx.setVoxEnabled(on);
        });
        connect(&tx, &TransmitModel::voxEnabledChanged, this,
                [this, pushBack](bool on) { pushBack(m_btn, on); });
        pushBack(m_btn, tx.voxEnabled());
        break;
    }
    case Kind::Ps: {
        PureSignal* ps = m_model->pureSignal();
        if (!ps) {
            // PureSignal entsteht erst beim Verbinden. Ohne Radio bleibt
            // der Schalter da, aber unbedienbar — ein Knopf, der
            // verschwindet und wiederkommt, verschiebt seine Nachbarn.
            m_btn->setEnabled(false);
            break;
        }
        connect(m_btn, &QPushButton::toggled, this, [this, ps](bool on) {
            if (m_updatingFromModel) { return; }
            ps->setEnabled(on);
        });
        connect(ps, &PureSignal::enabledChanged, this,
                [this, pushBack](bool on) { pushBack(m_btn, on); });
        pushBack(m_btn, ps->isEnabled());
        break;
    }
    }
}

} // namespace Longpath
