// no-port-check: Die drei Verweise auf RXA.c in dieser Datei meinen
// WDSP (TAPR), nicht Thetis -- third_party/wdsp/src/RXA.c:260/261/673,
// die Vorgabekurve default_G und der eqp-Einstiegspunkt. Der Pruefer
// erkennt den Dateinamen, nicht den Baum: RXA.c steht in Thetis'
// mitgeliefertem wdsp-Ordner genauso. Eine Zeile in
// THETIS-PROVENANCE.md waere hier falsch und wuerde die Spur
// verfaelschen; WDSP hat seine eigene Herkunft und seinen eigenen
// Pruefer (audit-wdsp-headers.py, gruen).
// src/gui/applets/EqApplet.cpp
//
// Layout from AetherSDR EqApplet.cpp (ten9876/AetherSDR).
// All controls NYI — wired in Phase 3I-3 (TX Processing).

// =================================================================
// src/gui/applets/EqApplet.cpp  (Longpath)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3; see LICENSE
//       and About dialog for the live contributor list)
//
//   This file is a port or structural derivative of AetherSDR source.
//   AetherSDR is licensed under the GNU General Public License v3.
//   Longpath is also GPLv3. Attribution follows GPLv3 §5 requirements.
//
// =================================================================
// Modification history (Longpath):
//   2026-04-16 — Ported/adapted in C++20/Qt6 for NereusSDR by
//                 J.J. Boyd (KG4VCF), with AI-assisted transformation
//                 via Anthropic Claude Code.
//                 Layout mirrors AetherSDR `src/gui/EqApplet.{h,cpp}`.
// =================================================================

#include "EqApplet.h"
#include "gui/styles/ThemeQss.h"
#include "NyiOverlay.h"
#include "gui/ComboStyle.h"
#include "gui/StyleConstants.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QComboBox>
#include <QLabel>
#include "models/RadioModel.h"
#include "models/SliceModel.h"
#include "models/TransmitModel.h"

#include <QPainter>
#include <QPen>
#include <QSignalBlocker>
#include <algorithm>
#include <cmath>

namespace Longpath {

// ── ResetButton: 22×22 button that paints a 3/4-circle undo arrow ────────────
// Matches AetherSDR EqApplet.cpp ResetButton exactly.

class ResetButton : public QPushButton {
public:
    explicit ResetButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setFixedSize(22, 22);
        setCursor(Qt::PointingHandCursor);
        setToolTip(QStringLiteral("Reset all bands to 0 dB"));
        setStyleSheet(Style::themed(
            "QPushButton { background-color: #1a2a3a; border: 1px solid #205070; "
            "border-radius: 6px; }"
            "QPushButton:hover { background-color: #203040; }"
            "QPushButton:pressed { background-color: #4a7ba8; }"));
    }

protected:
    void paintEvent(QPaintEvent* e) override {
        QPushButton::paintEvent(e);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);

        const int cx = width() / 2;
        const int cy = height() / 2;
        const int r  = 6;

        // 3/4 arc: gap at the bottom (6 o'clock), mirrored for "undo" look.
        // Qt angles: 0 = 3 o'clock, counter-clockwise positive, units 1/16 deg.
        // Start at 225 deg (7:30 position), sweep -270 deg (CW) to -45 deg (4:30).
        QPen pen(QColor(0xc8, 0xd8, 0xe8), 1.5);
        p.setPen(pen);
        p.drawArc(cx - r, cy - r, r * 2, r * 2, 225 * 16, -270 * 16);

        // Arrowhead at the end of the arc (225 deg = lower-left).
        const double angle = 225.0 * M_PI / 180.0;
        const double ex = cx + r * std::cos(angle);
        const double ey = cy - r * std::sin(angle);
        p.drawLine(QPointF(ex, ey), QPointF(ex - 4, ey - 1));
        p.drawLine(QPointF(ex, ey), QPointF(ex + 1, ey - 4));
    }
};


// ── Style constants (from AetherSDR EqApplet.cpp + StyleConstants.h) ─────────

static const QString kBtnBase =
    "QPushButton { background-color: #1a2a3a; color: #c8d8e8; "
    "border: 1px solid #205070; border-radius: 6px; font-size: 11px; "
    "font-weight: bold; padding: 2px 4px; }"
    "QPushButton:hover { background-color: #203040; }";

static const QString kGreenActive =
    "QPushButton:checked { background-color: #006040; color: #00ff88; "
    "border: 1px solid " + QString::fromLatin1(Style::kGreenBorder) + "; }";

static const QString kBlueActive =
    "QPushButton:checked { background-color: #4a7ba8; color: #ffffff; "
    "border: 1px solid #4a7ba8; }";

// 10-band frequency labels: 32/63/125/250/500/1k/2k/4k/8k/16k Hz.
static constexpr int kEqBandCount = 10;
static constexpr const char* kBandLabels[kEqBandCount] = {
    "32", "63", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"
};

// Die Vorwahlen, 2026-10-11. Nur zwei, und beide sind belegt:
//
//   "Flach"  — alles 0 dB. WDSPs eigene Vorgabe beim Anlegen
//              (third_party/wdsp/src/RXA.c:261 default_G, und TXA.c:113
//              auf der Sendeseite dasselbe).
//   "Sprache"— die Kurve, die in WDSPs Quelltext direkt ueber der flachen
//              steht, dort auskommentiert (RXA.c:260):
//                //double default_G[11] = {0.0, -12.0, -12.0, -12.0, -1.0,
//                //                        +1.0, +4.0, +9.0, +12.0, -10.0, -10.0};
//              Tiefen weg, 1-8 kHz angehoben, die obersten zwei gedaempft.
//
// "Music" stand hier bis heute als dritter Eintrag und ist entfernt: es gab
// dafuer keine Quelle -- weder Thetis (eqform.cs kennt ueberhaupt keine
// Vorwahlen) noch AetherSDR (EqApplet.cpp:277 hat nur die Schieber). Eine
// Vorwahl, deren Werte niemand nachrechnen kann, ist weniger wert als keine.
// Eigene DSP-Kurven zu erfinden verbietet CLAUDE.md ausdruecklich.
static constexpr std::array<int, 11> kKurveFlach{
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}};
static constexpr std::array<int, 11> kKurveSprache{
    {0, -12, -12, -12, -1, 1, 4, 9, 12, -10, -10}};

// Reihenfolge der Combo-Eintraege.
enum class Vorwahl : int { Flach = 0, Sprache = 1, Eigen = 2 };


// ── EqApplet ──────────────────────────────────────────────────────────────────

EqApplet::EqApplet(RadioModel* model, QWidget* parent)
    : AppletWidget(model, parent)
{
    buildUI();
    wireControls();
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
}

void EqApplet::buildUI()
{
    // Outer layout: zero margins (title bar flush to edges)
    // Body: padded content — matches AetherSDR EqApplet.cpp outer/vbox pattern
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto* body = new QWidget(this);
    auto* vbox = new QVBoxLayout(body);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);
    outer->addWidget(body);

    // ── Control row: ON | Reset | RX | TX ────────────────────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        // Control 1: ON — green checkable 36×22
        m_onBtn = new QPushButton(QStringLiteral("ON"), this);
        m_onBtn->setCheckable(true);
        m_onBtn->setFixedSize(36, 22);
        m_onBtn->setStyleSheet(kBtnBase + kGreenActive);
        m_onBtn->setToolTip(QStringLiteral("Enable equalizer"));
        row->addWidget(m_onBtn);

        // Control 4: Reset — 22×22, paints 3/4-circle undo icon (pen #c8d8e8, 1.5px)
        auto* resetBtn = new ResetButton(this);
        m_resetBtn = resetBtn;
        row->addWidget(m_resetBtn);

        // Control 2: RX — blue checkable 36×22
        m_rxBtn = new QPushButton(QStringLiteral("RX"), this);
        m_rxBtn->setCheckable(true);
        m_rxBtn->setFixedSize(36, 22);
        m_rxBtn->setStyleSheet(kBtnBase + kBlueActive);
        m_rxBtn->setToolTip(QStringLiteral("Show receive equalizer bands"));
        row->addWidget(m_rxBtn);

        // Control 3: TX — blue checkable 36×22, starts checked
        m_txBtn = new QPushButton(QStringLiteral("TX"), this);
        m_txBtn->setCheckable(true);
        m_txBtn->setChecked(true);
        m_txBtn->setFixedSize(36, 22);
        m_txBtn->setStyleSheet(kBtnBase + kBlueActive);
        m_txBtn->setToolTip(QStringLiteral("Show transmit equalizer bands"));
        row->addWidget(m_txBtn);

        row->addStretch();

        vbox->addLayout(row);
    }

    // ── Horizontal divider ────────────────────────────────────────────────────
    {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(Style::themed(QStringLiteral("QFrame { color: #203040; }")));
        vbox->addWidget(line);
    }

    // ── Band label row (controls 5): 32/63/125/250/500/1k/2k/4k/8k/16k ──────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        // Left spacer aligns with dB scale (fixedWidth 20)
        auto* spacerL = new QWidget(this);
        spacerL->setFixedWidth(16);
        row->addWidget(spacerL);

        for (int i = 0; i < kEqBandCount; ++i) {
            auto* lbl = new QLabel(QString::fromLatin1(kBandLabels[i]), this);
            lbl->setAlignment(Qt::AlignCenter);
            lbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #8090a0; font-size: 9px; }")));
            row->addWidget(lbl, 1);
        }

        // Right spacer aligns with right dB scale (fixedWidth 20)
        auto* spacerR = new QWidget(this);
        spacerR->setFixedWidth(16);
        row->addWidget(spacerR);

        vbox->addLayout(row);
    }

    // ── Slider area: left dB scale (control 6) | 10 sliders (controls 7-16) | right dB scale ──
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(0);

        // Left dB scale: +10 (#607080, 9px) / 0 (#8090a0, 9px) / -10 (#607080, 9px)
        {
            auto* col = new QVBoxLayout;
            col->setSpacing(0);

            auto* topLbl = new QLabel(QStringLiteral("+10"), this);
            topLbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #607080; font-size: 9px; }")));
            topLbl->setAlignment(Qt::AlignRight | Qt::AlignTop);
            topLbl->setFixedWidth(16);
            col->addWidget(topLbl);

            col->addStretch();

            auto* midLbl = new QLabel(QStringLiteral("0"), this);
            midLbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #8090a0; font-size: 9px; }")));
            midLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            midLbl->setFixedWidth(16);
            col->addWidget(midLbl);

            col->addStretch();

            auto* botLbl = new QLabel(QStringLiteral("-10"), this);
            botLbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #607080; font-size: 9px; }")));
            botLbl->setAlignment(Qt::AlignRight | Qt::AlignBottom);
            botLbl->setFixedWidth(16);
            col->addWidget(botLbl);

            row->addLayout(col);
        }

        // Controls 7–16: 10 vertical band sliders
        for (int i = 0; i < kEqBandCount; ++i) {
            auto* col = new QVBoxLayout;
            col->setSpacing(1);
            col->setContentsMargins(0, 0, 0, 0);

            auto* slider = new QSlider(Qt::Vertical, this);
            slider->setRange(-10, 10);
            slider->setValue(0);
            slider->setTickPosition(QSlider::NoTicks);
            slider->setStyleSheet(Longpath::Style::sliderVStyle());
            slider->setFixedHeight(100);
            slider->setToolTip(
                QStringLiteral("%1 Hz band, \u221210 to +10 dB").arg(
                    QString::fromLatin1(kBandLabels[i])));
            m_sliders[i] = slider;
            col->addWidget(slider, 0, Qt::AlignHCenter);

            // Value label under each slider (#c8d8e8, 9px)
            auto* valLbl = new QLabel(QStringLiteral("0"), this);
            valLbl->setAlignment(Qt::AlignCenter);
            valLbl->setStyleSheet(Style::themed(
                QStringLiteral("QLabel { color: #c8d8e8; font-size: 9px; }")));
            m_valueLabels[i] = valLbl;
            col->addWidget(valLbl);

            row->addLayout(col, 1);
        }

        // Right dB scale: mirror of left
        {
            auto* col = new QVBoxLayout;
            col->setSpacing(0);

            auto* topLbl = new QLabel(QStringLiteral("+10"), this);
            topLbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #607080; font-size: 9px; }")));
            topLbl->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            topLbl->setFixedWidth(16);
            col->addWidget(topLbl);

            col->addStretch();

            auto* midLbl = new QLabel(QStringLiteral("0"), this);
            midLbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #8090a0; font-size: 9px; }")));
            midLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            midLbl->setFixedWidth(16);
            col->addWidget(midLbl);

            col->addStretch();

            auto* botLbl = new QLabel(QStringLiteral("-10"), this);
            botLbl->setStyleSheet(Style::themed(QStringLiteral("QLabel { color: #607080; font-size: 9px; }")));
            botLbl->setAlignment(Qt::AlignLeft | Qt::AlignBottom);
            botLbl->setFixedWidth(16);
            col->addWidget(botLbl);

            row->addLayout(col);
        }

        vbox->addLayout(row);
    }

    // ── Horizontal divider ────────────────────────────────────────────────────
    {
        auto* line = new QFrame(this);
        line->setFrameShape(QFrame::HLine);
        line->setFrameShadow(QFrame::Plain);
        line->setStyleSheet(Style::themed(QStringLiteral("QFrame { color: #203040; }")));
        vbox->addWidget(line);
    }

    // ── Preset row: "Preset" label + combo (control 17) ─────────────────────
    {
        auto* row = new QHBoxLayout;
        row->setSpacing(4);

        auto* presetLbl = new QLabel(QStringLiteral("Preset"), this);
        presetLbl->setStyleSheet(Style::themed(
            QStringLiteral("QLabel { color: #8090a0; font-size: 11px; }")));
        row->addWidget(presetLbl);

        // Control 17: Preset combo — Flat / Voice / Music / Custom
        m_presetCombo = new QComboBox(this);
        // Reihenfolge = enum Vorwahl. "Music" ist weg, siehe die Notiz bei
        // kKurveFlach -- es gab keine Quelle dafuer.
        m_presetCombo->addItem(QStringLiteral("Flach"));
        m_presetCombo->addItem(QStringLiteral("Sprache"));
        m_presetCombo->addItem(QStringLiteral("Eigen"));
        m_presetCombo->setToolTip(QStringLiteral("Equalizer preset"));
        applyComboStyle(m_presetCombo);
        row->addWidget(m_presetCombo, 1);

        vbox->addLayout(row);
    }
}

// ── Verdrahtung (2026-10-11) ─────────────────────────────────────────────────
//
// Dieses Feld stand seit der Skelettphase (3-UI) vollstaendig da und tat
// nichts: zehn Schieber, ON, Reset, RX/TX und eine Vorwahl, alle sechs mit
// NyiOverlay markiert. Beide Haelften gehen jetzt dorthin, wo die Kurve
// wirklich liegt:
//
//   RX -> SliceModel::rxEq* -> RadioModel -> RxChannel::setRxEqGraph10
//         (WDSP eqp, RXA.c:673 -- neu am 2026-10-11, Longpath hatte den
//          Empfangs-Equalizer nicht)
//   TX -> TransmitModel::txEq* -> TxChannel::setTxEqGraph10
//         (schon seit 3M-3a-i vorhanden; der grosse Editor dazu ist
//          TxEqDialog, dieses Feld ist die Schnellbedienung davor)
//
// Die Schieber gehen -10..+10, die DSP-Grenzen sind -12..+15 (Thetis
// eqform.cs:1871-1872 tbRXEQ1, :1946-1947 tbRXEQPreamp [v2.10.3.15]). Der
// Bereich bleibt, wie die Vorlage ihn hatte (AetherSDR EqApplet.cpp:277) --
// ihn zu weiten ist eine Sache der Gestaltung und gehoert dem Betreiber.
// Die Modelle klammern selbst, es kann also nichts Falsches hinausgehen.

void EqApplet::wireControls()
{
    if (!m_model) { return; }

    // ── Seitenwahl RX / TX, gegenseitig ausschliessend ───────────────────────
    connect(m_rxBtn, &QPushButton::clicked, this, [this]() {
        setSeite(Seite::Empfang);
    });
    connect(m_txBtn, &QPushButton::clicked, this, [this]() {
        setSeite(Seite::Senden);
    });

    // ── ON ───────────────────────────────────────────────────────────────────
    connect(m_onBtn, &QPushButton::clicked, this, [this](bool on) {
        if (m_updatingFromModel || !m_model) { return; }
        if (m_seite == Seite::Empfang) {
            if (m_slice) { m_slice->setRxEqEnabled(on); }
        } else {
            m_model->transmitModel().setTxEqEnabled(on);
        }
    });

    // ── Die zehn Baender ─────────────────────────────────────────────────────
    for (int i = 0; i < kEqBandCount; ++i) {
        connect(m_sliders[i], &QSlider::valueChanged, this, [this, i](int dB) {
            if (m_updatingFromModel || !m_model) { return; }
            if (m_seite == Seite::Empfang) {
                if (m_slice) { m_slice->setRxEqBandDb(i, dB); }
            } else {
                m_model->transmitModel().setTxEqBand(i, dB);
            }
            m_valueLabels[i]->setText(QString::number(dB));
            // Von Hand gezogen heisst "Eigen" -- sonst behauptete die Combo
            // weiter "Flach", waehrend die Kurve es nicht mehr ist.
            QSignalBlocker b(m_presetCombo);
            m_presetCombo->setCurrentIndex(static_cast<int>(Vorwahl::Eigen));
        });
    }

    // ── Reset: zurueck auf flach ─────────────────────────────────────────────
    connect(m_resetBtn, &QPushButton::clicked, this, [this]() {
        kurveSetzen(kKurveFlach);
        QSignalBlocker b(m_presetCombo);
        m_presetCombo->setCurrentIndex(static_cast<int>(Vorwahl::Flach));
    });

    // ── Vorwahl ──────────────────────────────────────────────────────────────
    connect(m_presetCombo, &QComboBox::currentIndexChanged, this, [this](int idx) {
        if (m_updatingFromModel) { return; }
        switch (static_cast<Vorwahl>(idx)) {
        case Vorwahl::Flach:   kurveSetzen(kKurveFlach);   break;
        case Vorwahl::Sprache: kurveSetzen(kKurveSprache); break;
        case Vorwahl::Eigen:   /* nichts -- das ist der Zustand, nicht ein Befehl */ break;
        }
    });

    // ── Modell -> Oberflaeche, Sendeseite ────────────────────────────────────
    TransmitModel& tx = m_model->transmitModel();
    connect(&tx, &TransmitModel::txEqEnabledChanged, this, [this](bool) {
        if (m_seite == Seite::Senden) { syncSeite(); }
    });
    connect(&tx, &TransmitModel::txEqBandChanged, this, [this](int, int) {
        if (m_seite == Seite::Senden) { syncSeite(); }
    });
    connect(&tx, &TransmitModel::txEqPreampChanged, this, [this](int) {
        if (m_seite == Seite::Senden) { syncSeite(); }
    });

    // ── Modell -> Oberflaeche, Empfangsseite (haengt am aktiven Slice) ───────
    connect(m_model, &RadioModel::activeSliceChanged, this, [this](int) {
        bindSlice(m_model ? m_model->activeSlice() : nullptr);
    });
    bindSlice(m_model->activeSlice());

    syncSeite();
}

void EqApplet::bindSlice(SliceModel* s)
{
    if (m_slice) { disconnect(m_slice, nullptr, this, nullptr); }
    m_slice = s;
    if (s) {
        connect(s, &SliceModel::rxEqEnabledChanged, this, [this](bool) {
            if (m_seite == Seite::Empfang) { syncSeite(); }
        });
        connect(s, &SliceModel::rxEqBandDbChanged, this, [this](int, int) {
            if (m_seite == Seite::Empfang) { syncSeite(); }
        });
        connect(s, &SliceModel::rxEqPreampDbChanged, this, [this](int) {
            if (m_seite == Seite::Empfang) { syncSeite(); }
        });
    }
    if (m_seite == Seite::Empfang) { syncSeite(); }
}

void EqApplet::setSeite(Seite seite)
{
    m_seite = seite;
    // Die zwei Knoepfe sind nicht in einer QButtonGroup; der Zustand wird
    // hier von Hand gehalten, damit ein zweiter Klick auf den schon
    // gewaehlten Knopf ihn nicht ausschaltet und beide Seiten dunkel laesst.
    {
        QSignalBlocker br(m_rxBtn);
        QSignalBlocker bt(m_txBtn);
        m_rxBtn->setChecked(seite == Seite::Empfang);
        m_txBtn->setChecked(seite == Seite::Senden);
    }
    syncSeite();
}

void EqApplet::kurveSetzen(const std::array<int, 11>& kurve)
{
    if (!m_model) { return; }
    // kurve[0] ist die Vorverstaerkung, kurve[1..10] die Baender -- dieselbe
    // Form, die WDSPs SetRXAGrphEQ10 / SetTXAGrphEQ10 erwartet.
    if (m_seite == Seite::Empfang) {
        if (!m_slice) { return; }
        m_slice->setRxEqPreampDb(kurve[0]);
        for (int i = 0; i < kEqBandCount; ++i) {
            m_slice->setRxEqBandDb(i, kurve[static_cast<std::size_t>(i) + 1]);
        }
    } else {
        TransmitModel& tx = m_model->transmitModel();
        tx.setTxEqPreamp(kurve[0]);
        for (int i = 0; i < kEqBandCount; ++i) {
            tx.setTxEqBand(i, kurve[static_cast<std::size_t>(i) + 1]);
        }
    }
    syncSeite();
}

void EqApplet::syncSeite()
{
    if (!m_model) { return; }

    bool on = false;
    std::array<int, kEqBandCount> baender{};
    if (m_seite == Seite::Empfang) {
        if (!m_slice) {
            // Ohne Slice gibt es keine Empfangskurve -- die Schieber dann
            // nicht auf 0 luegen lassen, sondern sperren.
            m_updatingFromModel = true;
            for (int i = 0; i < kEqBandCount; ++i) { m_sliders[i]->setEnabled(false); }
            m_onBtn->setEnabled(false);
            m_updatingFromModel = false;
            return;
        }
        on = m_slice->rxEqEnabled();
        for (int i = 0; i < kEqBandCount; ++i) { baender[static_cast<std::size_t>(i)] = m_slice->rxEqBandDb(i); }
    } else {
        const TransmitModel& tx = m_model->transmitModel();
        on = tx.txEqEnabled();
        for (int i = 0; i < kEqBandCount; ++i) { baender[static_cast<std::size_t>(i)] = tx.txEqBand(i); }
    }

    m_updatingFromModel = true;
    m_onBtn->setEnabled(true);
    {
        QSignalBlocker b(m_onBtn);
        m_onBtn->setChecked(on);
    }
    for (int i = 0; i < kEqBandCount; ++i) {
        const int dB = baender[static_cast<std::size_t>(i)];
        m_sliders[i]->setEnabled(true);
        QSignalBlocker b(m_sliders[i]);
        // Die Schieber gehen -10..+10, das Modell bis -12..+15. Ein Wert
        // ausserhalb wird fuer die ANZEIGE geklammert; das Modell behaelt
        // seinen echten Wert, damit ein Blick auf das Feld ihn nicht
        // heimlich zurechtzieht.
        m_sliders[i]->setValue(std::clamp(dB, m_sliders[i]->minimum(), m_sliders[i]->maximum()));
        m_valueLabels[i]->setText(QString::number(dB));
    }
    m_updatingFromModel = false;
}

void EqApplet::syncFromModel()
{
    bindSlice(m_model ? m_model->activeSlice() : nullptr);
    syncSeite();
}

} // namespace Longpath
