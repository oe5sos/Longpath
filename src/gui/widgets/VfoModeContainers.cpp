// =================================================================
// src/gui/widgets/VfoModeContainers.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/console.cs, original licence from Thetis source is included below
//   Project Files/Source/Console/setup.designer.cs (upstream has no top-of-file header — project-level LICENSE applies)
//   Project Files/Source/Console/radio.cs, original licence from Thetis source is included below
//
// =================================================================
// Modification history (Longpath):
//   2026-04-17 — Reimplemented in C++20/Qt6 for NereusSDR by J.J. Boyd
//                 (KG4VCF), with AI-assisted transformation via Anthropic
//                 Claude Code.
//                 Structural pattern follows AetherSDR (ten9876/AetherSDR,
//                 GPLv3).
// =================================================================

//=================================================================
// console.cs
//=================================================================
// Thetis is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems 
// Copyright (C) 2010-2020  Doug Wigley
// Credit is given to Sizenko Alexander of Style-7 (http://www.styleseven.com/) for the Digital-7 font.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//
//=================================================================
// Modifications to support the Behringer Midi controllers
// by Chris Codella, W2PA, May 2017.  Indicated by //-W2PA comment lines. 
// Modifications for using the new database import function.  W2PA, 29 May 2017
// Support QSK, possible with Protocol-2 firmware v1.7 (Orion-MkI and Orion-MkII), and later.  W2PA, 5 April 2019 
// Modfied heavily - Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

// Migrated to VS2026 - 18/12/25 MW0LGE v2.10.3.12

//
// Upstream source 'Project Files/Source/Console/setup.designer.cs' has no top-of-file GPL header —
// project-level Thetis LICENSE applies.

//=================================================================
// radio.cs
//=================================================================
// PowerSDR is a C# implementation of a Software Defined Radio.
// Copyright (C) 2004-2009  FlexRadio Systems
// Copyright (C) 2010-2020  Doug Wigley
// Copyright (C) 2019-2026  Richard Samphire
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
// You may contact us via email at: sales@flex-radio.com.
// Paper mail may be sent to: 
//    FlexRadio Systems
//    8900 Marybank Dr.
//    Austin, TX 78750
//    USA
//=================================================================
//
//============================================================================================//
// Dual-Licensing Statement (Applies Only to Author's Contributions, Richard Samphire MW0LGE) //
// ------------------------------------------------------------------------------------------ //
// For any code originally written by Richard Samphire MW0LGE, or for any modifications       //
// made by him, the copyright holder for those portions (Richard Samphire) reserves the       //
// right to use, license, and distribute such code under different terms, including           //
// closed-source and proprietary licences, in addition to the GNU General Public License      //
// granted above. Nothing in this statement restricts any rights granted to recipients under  //
// the GNU GPL. Code contributed by others (not Richard Samphire) remains licensed under      //
// its original terms and is not affected by this dual-licensing statement in any way.        //
// Richard Samphire can be reached by email at :  mw0lge@grange-lane.co.uk                    //
//============================================================================================//

#include "VfoModeContainers.h"
#include "GuardedComboBox.h"
#include "ScrollableLabel.h"
#include "TriBtn.h"
#include "VfoStyles.h"
#include "../StyleConstants.h"
#include "core/DcsCode.h"
#include "models/SliceModel.h"
#include "core/WdspTypes.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSignalBlocker>

namespace Longpath {

// ── CTCSS tone list ────────────────────────────────────────────────────────
// Die 49 Toene von Thetis (2026-09-27; vorher 41 -- 69.3, 159.8, 165.5,
// 171.3, 177.3, 183.5, 189.9 und 199.5 fehlten, ein Thetis-Speicher mit
// einem davon fand seinen Ton im Menue nicht).
// From Thetis Console/console.cs:236-241 [@852bf0e] -- CTCSS_array
static const double kCtcssTones[] = {
     67.0,  69.3,  71.9,  74.4,  77.0,  79.7,  82.5,  85.4,  88.5,  91.5,
     94.8,  97.4, 100.0, 103.5, 107.2, 110.9, 114.8, 118.8, 123.0, 127.3,
    131.8, 136.5, 141.3, 146.2, 151.4, 156.7, 159.8, 162.2, 165.5, 167.9,
    171.3, 173.8, 177.3, 179.9, 183.5, 186.2, 189.9, 192.8, 199.5, 203.5,
    206.5, 210.7, 218.1, 225.7, 229.1, 233.6, 241.8, 250.3, 254.1
};
static constexpr int kCtcssCount = static_cast<int>(sizeof(kCtcssTones) / sizeof(kCtcssTones[0]));

QVector<double> FmOptContainer::ctcssTones()
{
    return QVector<double>(std::begin(kCtcssTones), std::end(kCtcssTones));
}

// ── RttyMarkShiftContainer step constants ─────────────────────────────────
// AetherSDR VfoWidget.cpp uses 25 Hz step for Mark and 5 Hz step for Shift.
// These are UX choices, not DSP constants — native Longpath values.
static constexpr int kMarkStep  = 25;
static constexpr int kShiftStep = 5;
static constexpr int kDigStep   = 10;

// ══════════════════════════════════════════════════════════════════════════
//  FmOptContainer
// ══════════════════════════════════════════════════════════════════════════

FmOptContainer::FmOptContainer(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void FmOptContainer::buildUi()
{
    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // ── Row 1: tone mode combo + tone value combo ─────────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(4);

        m_toneModeCmb = new GuardedComboBox(this);
        m_toneModeCmb->setObjectName("toneModeCmb");
        m_toneModeCmb->addItem(QStringLiteral("Off"),          QVariant(0));
        m_toneModeCmb->addItem(QStringLiteral("CTCSS Encode"), QVariant(1));
        m_toneModeCmb->addItem(QStringLiteral("CTCSS Decode"), QVariant(2));
        m_toneModeCmb->addItem(QStringLiteral("CTCSS Enc+Dec"),QVariant(3));
        // DCS nur als Decode: senden kann Longpath es nicht. WDSPs
        // `fmmod.c` speist einen Ton ein, aber kein Datenwort -- und ein
        // Menueeintrag, hinter dem nichts passiert, ist schlimmer als
        // keiner.
        m_toneModeCmb->addItem(QStringLiteral("DCS Decode"),   QVariant(4));

        m_toneValueCmb = new GuardedComboBox(this);
        m_toneValueCmb->setObjectName("toneValueCmb");
        for (int i = 0; i < kCtcssCount; ++i) {
            const QString text = QString::number(kCtcssTones[i], 'f', 1);
            m_toneValueCmb->addItem(text, QVariant(text));
        }

        m_toneModeCmb->setToolTip(QStringLiteral(
            "Tonmodus: Off / CTCSS Encode / Decode / Enc+Dec / DCS Decode"));
        m_toneValueCmb->setToolTip(QStringLiteral("CTCSS sub-audible tone frequency (Hz)"));
        row->addWidget(m_toneModeCmb, 1);
        row->addWidget(m_toneValueCmb, 1);

        // Tonleuchte: an, solange der eingestellte Subton anliegt.
        // kLiveGreen, nicht kGreenText -- ein anliegender Subton ist ein
        // echter Live-Zustand, keine ruhige Erfolgsmeldung (siehe die
        // Begruendung an der Konstante). Aus ist sie kBorderMuted, das
        // dokumentierte Gegenstueck bei LED-artigen Zustaenden: KEIN Rot,
        // denn "gerade kein Ton" ist der Normalfall zwischen zwei
        // Durchgaengen und kein Fehler.
        m_toneLamp = new QLabel(this);
        m_toneLamp->setObjectName("toneLamp");
        m_toneLamp->setFixedSize(10, 10);
        m_toneLamp->setToolTip(QStringLiteral(
            "Leuchtet, solange der eingestellte CTCSS-Subton anliegt"));
        row->addWidget(m_toneLamp, 0, Qt::AlignVCenter);
        vbox->addLayout(row);
    }

    // ── Row 2: offset label + spinbox ─────────────────────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(4);

        QLabel* offsetLbl = new QLabel(QStringLiteral("Offset:"), this);
        offsetLbl->setStyleSheet(kLabelStyle.toString());

        m_offsetKhzSpin = new QSpinBox(this);
        m_offsetKhzSpin->setObjectName("offsetKhzSpin");
        m_offsetKhzSpin->setRange(0, 10000);
        m_offsetKhzSpin->setSuffix(QStringLiteral(" kHz"));
        m_offsetKhzSpin->setSingleStep(50);

        row->addWidget(offsetLbl);
        row->addWidget(m_offsetKhzSpin, 1);
        vbox->addLayout(row);
    }

    // ── Row 3: TX direction buttons + Reverse toggle ──────────────────────
    {
        QHBoxLayout* row = new QHBoxLayout;
        row->setSpacing(4);

        m_txLowBtn   = new QPushButton(QStringLiteral("\u2212"), this);  // "−"
        m_simplexBtn = new QPushButton(QStringLiteral("Simplex"), this);
        m_txHighBtn  = new QPushButton(QStringLiteral("+"), this);
        m_revBtn     = new QPushButton(QStringLiteral("Rev"), this);

        m_txLowBtn->setObjectName("txLowBtn");
        m_txLowBtn->setToolTip(QStringLiteral("TX below RX (repeater Low offset)"));
        m_simplexBtn->setObjectName("simplexBtn");
        m_simplexBtn->setToolTip(QStringLiteral("Simplex — TX on same frequency as RX"));
        m_txHighBtn->setObjectName("txHighBtn");
        m_txHighBtn->setToolTip(QStringLiteral("TX above RX (repeater High offset)"));
        m_revBtn->setObjectName("revBtn");
        m_revBtn->setToolTip(QStringLiteral("Reverse — listen on the repeater output frequency"));

        for (QPushButton* btn : {m_txLowBtn, m_simplexBtn, m_txHighBtn, m_revBtn}) {
            btn->setCheckable(true);
            btn->setStyleSheet(kDspToggle.toString());
        }

        row->addWidget(m_txLowBtn);
        row->addWidget(m_simplexBtn);
        row->addWidget(m_txHighBtn);
        row->addWidget(m_revBtn);
        vbox->addLayout(row);
    }

    // ── Signal connections ────────────────────────────────────────────────

    // CTCSS Encode (und Enc+Dec) setzt den Ton beim Senden:
    // RadioModel::pushFmToneFromTxSlice (2026-09-27).
    //
    // Decode (und Enc+Dec) wirkt seit 2026-10-09 ebenfalls. Hier stand
    // vorher, die Einstellung werde nur gespeichert, "for when the tone
    // detector is implemented" -- er ist jetzt da: `CtcssDetector` samt
    // Abgriff auf das FM-Basisband, verdrahtet ueber
    // `RadioModel::pushCtcssSquelchForSlice` und
    // `RxChannel::setCtcssSquelch`.
    //
    // Thetis/WDSP haben dafuer weiterhin nichts: `wdsp/fmd.c` filtert den
    // Subton mit `snotch` nur aus dem Hoerbaren heraus, und WDSPs FMSQ
    // ist eine Rausch-, keine Tonsperre. Der Detektor ist darum
    // Longpath-eigen und nach TIA-603-D gebaut.

    connect(m_toneModeCmb,   QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (!m_slice) { return; }
        m_slice->setFmCtcssMode(m_toneModeCmb->itemData(idx).toInt());
    });

    connect(m_toneValueCmb, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int idx) {
        if (!m_slice || idx < 0) { return; }
        if (m_toneValueZeigtDcs) {
            // "023N" / "023I"
            const QString v = m_toneValueCmb->itemData(idx).toString();
            if (v.size() < 4) { return; }
            m_slice->setFmDcsCode(v.left(3).toInt());
            m_slice->setFmDcsPolarity(v.endsWith(QLatin1Char('I')) ? 1 : 0);
            return;
        }
        const double hz = m_toneValueCmb->itemData(idx).toString().toDouble();
        m_slice->setFmCtcssValueHz(hz);
    });

    connect(m_offsetKhzSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, [this](int kHz) {
        if (!m_slice) { return; }
        m_slice->setFmOffsetHz(kHz * 1000);
    });

    // Sendeablage: RadioModel::txFrequencyForSlice (2026-09-27).
    connect(m_txLowBtn, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setFmTxMode(FmTxMode::Low);
        m_txLowBtn->setChecked(true);
        m_simplexBtn->setChecked(false);
        m_txHighBtn->setChecked(false);
    });

    // Sendeablage: RadioModel::txFrequencyForSlice (2026-09-27).
    connect(m_simplexBtn, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setFmTxMode(FmTxMode::Simplex);
        m_txLowBtn->setChecked(false);
        m_simplexBtn->setChecked(true);
        m_txHighBtn->setChecked(false);
        // Simplex: no repeater offset. The offset spinbox retains its last
        // non-simplex value (Thetis console.cs:40412 chkFMTXSimplex_CheckedChanged
        // does not zero udFMOffset).
    });

    // Sendeablage: RadioModel::txFrequencyForSlice (2026-09-27).
    connect(m_txHighBtn, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setFmTxMode(FmTxMode::High);
        m_txLowBtn->setChecked(false);
        m_simplexBtn->setChecked(false);
        m_txHighBtn->setChecked(true);
    });

    // Reverse: RadioModel verschiebt die Empfangsfrequenz um die Ablage
    // und kehrt die Sendeablage um (Thetis console.cs:40442-40468).
    connect(m_revBtn, &QPushButton::toggled, this, [this](bool checked) {
        if (!m_slice) { return; }
        m_slice->setFmReverse(checked);
    });
}

void FmOptContainer::setSlice(SliceModel* s)
{
    if (m_slice) { disconnect(m_slice, nullptr, this, nullptr); }
    m_slice = s;
    if (s) {
        // Follow the model (2026-09-27): the FM applet, a memory recall and
        // a TX-mode change (which turns Reverse off) change these as well;
        // before, the flag only read the slice once.
        connect(s, &SliceModel::fmCtcssModeChanged,    this, [this](int)      { syncFromSlice(); });
        connect(s, &SliceModel::fmCtcssValueHzChanged, this, [this](double)   { syncFromSlice(); });
        connect(s, &SliceModel::fmCtcssToneDetectedChanged, this, [this](bool) { syncFromSlice(); });
        connect(s, &SliceModel::fmOffsetHzChanged,     this, [this](int)      { syncFromSlice(); });
        connect(s, &SliceModel::fmTxModeChanged,       this, [this](FmTxMode) { syncFromSlice(); });
        connect(s, &SliceModel::fmReverseChanged,      this, [this](bool)     { syncFromSlice(); });
    }
    syncFromSlice();
}

void FmOptContainer::syncFromSlice()
{
    if (!m_slice) { return; }

    const QSignalBlocker b1(m_toneModeCmb);
    const QSignalBlocker b2(m_toneValueCmb);
    const QSignalBlocker b3(m_offsetKhzSpin);
    const QSignalBlocker b4(m_revBtn);

    // Tone mode: find the index whose itemData == m_slice->fmCtcssMode()
    const int mode = m_slice->fmCtcssMode();
    for (int i = 0; i < m_toneModeCmb->count(); ++i) {
        if (m_toneModeCmb->itemData(i).toInt() == mode) {
            m_toneModeCmb->setCurrentIndex(i);
            break;
        }
    }

    // Der Wertewaehler zeigt je nach Modus zweierlei: bei CTCSS die
    // Subtoene in Hz, bei DCS die Codes als Oktalzahl mit Polaritaet
    // ("023 N", "023 I") -- so, wie Geraete sie im Menue fuehren. Die
    // Liste wird nur umgefuellt, wenn sie wirklich wechselt; ein
    // Neuaufbau bei jedem Aufruf wuerde die Auswahl staendig verwerfen.
    const bool dcsModus = (mode == 4);
    if (dcsModus != m_toneValueZeigtDcs) {
        m_toneValueZeigtDcs = dcsModus;
        m_toneValueCmb->clear();
        if (dcsModus) {
            for (int oktal : dcsStandardCodes()) {
                const QString basis = QStringLiteral("%1").arg(oktal, 3, 10, QLatin1Char('0'));
                m_toneValueCmb->addItem(basis + QStringLiteral(" N"),
                                        QStringLiteral("%1N").arg(basis));
                m_toneValueCmb->addItem(basis + QStringLiteral(" I"),
                                        QStringLiteral("%1I").arg(basis));
            }
            m_toneValueCmb->setToolTip(QStringLiteral(
                "DCS-Code als Oktalzahl, N = normal, I = invertiert"));
        } else {
            for (int i = 0; i < kCtcssCount; ++i) {
                const QString text = QString::number(kCtcssTones[i], 'f', 1);
                m_toneValueCmb->addItem(text, QVariant(text));
            }
            m_toneValueCmb->setToolTip(
                QStringLiteral("CTCSS sub-audible tone frequency (Hz)"));
        }
    }

    if (dcsModus) {
        const QString gesucht = QStringLiteral("%1%2")
            .arg(m_slice->fmDcsCode(), 3, 10, QLatin1Char('0'))
            .arg(m_slice->fmDcsPolarity() != 0 ? QLatin1Char('I') : QLatin1Char('N'));
        const int idx = m_toneValueCmb->findData(gesucht);
        if (idx >= 0) { m_toneValueCmb->setCurrentIndex(idx); }
    } else {
        const QString wantedTone = QString::number(m_slice->fmCtcssValueHz(), 'f', 1);
        const int toneIdx = m_toneValueCmb->findText(wantedTone);
        if (toneIdx >= 0) { m_toneValueCmb->setCurrentIndex(toneIdx); }
    }

    // Tonleuchte. Sichtbar nur, wo auch jemand auf einen Ton hoert
    // (Decode = 2, Enc+Dec = 3) -- in "Off" und "Encode" waere eine
    // dunkle Leuchte eine Behauptung ueber etwas, das gar nicht laeuft.
    if (m_toneLamp) {
        const bool hoertZu = (mode == 2 || mode == 3 || mode == 4);
        m_toneLamp->setVisible(hoertZu);
        if (hoertZu) {
            const bool tonDa = m_slice->fmCtcssToneDetected();
            m_toneLamp->setStyleSheet(QStringLiteral(
                "QLabel { background: %1; border-radius: 5px; }"
            ).arg(QLatin1String(tonDa ? Style::kLiveGreen : Style::kBorderMuted)));
            const QString was = dcsModus
                ? QStringLiteral("DCS-Code %1%2")
                      .arg(m_slice->fmDcsCode(), 3, 10, QLatin1Char('0'))
                      .arg(m_slice->fmDcsPolarity() != 0 ? QLatin1Char('I') : QLatin1Char('N'))
                : QStringLiteral("CTCSS-Subton %1 Hz")
                      .arg(QString::number(m_slice->fmCtcssValueHz(), 'f', 1));
            m_toneLamp->setToolTip(tonDa
                ? QStringLiteral("Der %1 liegt an").arg(was)
                : QStringLiteral("Warten auf den %1 -- er fehlt, der Kanal ist stumm")
                      .arg(was));
        }
    }

    // Offset: stored Hz → display kHz
    m_offsetKhzSpin->setValue(m_slice->fmOffsetHz() / 1000);

    // Direction buttons (mutually exclusive)
    const FmTxMode txMode = m_slice->fmTxMode();
    m_txLowBtn->setChecked(txMode == FmTxMode::Low);
    m_simplexBtn->setChecked(txMode == FmTxMode::Simplex);
    m_txHighBtn->setChecked(txMode == FmTxMode::High);

    // Reverse toggle. From Thetis console.cs:40414-40424 [@852bf0e] --
    // chkFMTXSimplex_CheckedChanged: chkFMTXRev.Enabled = false in Simplex.
    m_revBtn->setChecked(m_slice->fmReverse());
    m_revBtn->setEnabled(txMode != FmTxMode::Simplex);
}


// ══════════════════════════════════════════════════════════════════════════
//  DigOffsetContainer
// ══════════════════════════════════════════════════════════════════════════

DigOffsetContainer::DigOffsetContainer(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void DigOffsetContainer::buildUi()
{
    QHBoxLayout* row = new QHBoxLayout(this);
    row->setContentsMargins(4, 4, 4, 4);
    row->setSpacing(4);

    m_minusBtn   = new TriBtn(TriBtn::Left, this);
    m_offsetLabel = new ScrollableLabel(this);
    m_plusBtn    = new TriBtn(TriBtn::Right, this);

    m_minusBtn->setObjectName("minusBtn");
    m_minusBtn->setToolTip(QStringLiteral("Decrease DIG offset"));
    m_offsetLabel->setObjectName("offsetLabel");
    m_plusBtn->setObjectName("plusBtn");
    m_plusBtn->setToolTip(QStringLiteral("Increase DIG offset"));

    m_offsetLabel->setRange(-10000, 10000);
    m_offsetLabel->setStep(kDigStep);
    m_offsetLabel->setValue(0);
    m_offsetLabel->setFormat([](int v) -> QString {
        if (v > 0) { return QStringLiteral("+%1 Hz").arg(v); }
        return QStringLiteral("%1 Hz").arg(v);
    });

    row->addWidget(m_minusBtn);
    row->addWidget(m_offsetLabel, 1);
    row->addWidget(m_plusBtn);

    // ── Signal connections ────────────────────────────────────────────────

    connect(m_minusBtn, &QPushButton::clicked, this, [this]() {
        applyOffset(currentOffsetHz() - kDigStep);
    });

    connect(m_plusBtn, &QPushButton::clicked, this, [this]() {
        applyOffset(currentOffsetHz() + kDigStep);
    });

    connect(m_offsetLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        applyOffset(hz);
    });
}

int DigOffsetContainer::currentOffsetHz() const
{
    if (!m_slice) { return 0; }
    if (m_slice->dspMode() == DSPMode::DIGL) { return m_slice->diglOffsetHz(); }
    return m_slice->diguOffsetHz();  // DIGU or fallback
}

void DigOffsetContainer::applyOffset(int hz)
{
    if (!m_slice) { return; }
    if (m_slice->dspMode() == DSPMode::DIGL) {
        m_slice->setDiglOffsetHz(hz);
    } else {
        m_slice->setDiguOffsetHz(hz);
    }
}

void DigOffsetContainer::setSlice(SliceModel* s)
{
    m_slice = s;
    syncFromSlice();
}

void DigOffsetContainer::syncFromSlice()
{
    if (!m_slice) { return; }
    const QSignalBlocker b(m_offsetLabel);
    m_offsetLabel->setValue(currentOffsetHz());
}


// ══════════════════════════════════════════════════════════════════════════
//  RttyMarkShiftContainer
// ══════════════════════════════════════════════════════════════════════════

RttyMarkShiftContainer::RttyMarkShiftContainer(QWidget* parent)
    : QWidget(parent)
{
    buildUi();
}

void RttyMarkShiftContainer::buildUi()
{
    QVBoxLayout* vbox = new QVBoxLayout(this);
    vbox->setContentsMargins(4, 4, 4, 4);
    vbox->setSpacing(4);

    // ── Header row: "Mark" / "Shift" labels ──────────────────────────────
    {
        QHBoxLayout* hdr = new QHBoxLayout;
        hdr->setSpacing(4);

        QLabel* markHdr  = new QLabel(QStringLiteral("Mark"),  this);
        QLabel* shiftHdr = new QLabel(QStringLiteral("Shift"), this);
        markHdr->setStyleSheet(kLabelStyle.toString());
        shiftHdr->setStyleSheet(kLabelStyle.toString());
        markHdr->setAlignment(Qt::AlignCenter);
        shiftHdr->setAlignment(Qt::AlignCenter);

        hdr->addWidget(markHdr,  1);
        hdr->addWidget(shiftHdr, 1);
        vbox->addLayout(hdr);
    }

    // ── Control row: Mark sub-group + gap + Shift sub-group ──────────────
    {
        QHBoxLayout* ctrl = new QHBoxLayout;
        ctrl->setSpacing(4);

        // Mark sub-group
        m_markMinus = new TriBtn(TriBtn::Left, this);
        m_markLabel = new ScrollableLabel(this);
        m_markPlus  = new TriBtn(TriBtn::Right, this);

        m_markMinus->setObjectName("markMinus");
        m_markMinus->setToolTip(QStringLiteral("Decrease RTTY mark frequency"));
        m_markLabel->setObjectName("markLabel");
        m_markPlus->setObjectName("markPlus");
        m_markPlus->setToolTip(QStringLiteral("Increase RTTY mark frequency"));

        // From Thetis setup.designer.cs:40635 — RTTY mark default 2295 Hz
        m_markLabel->setRange(1000, 3500);
        m_markLabel->setStep(kMarkStep);
        m_markLabel->setValue(2295);

        ctrl->addWidget(m_markMinus);
        ctrl->addWidget(m_markLabel, 1);
        ctrl->addWidget(m_markPlus);

        ctrl->addSpacing(4);

        // Shift sub-group
        m_shiftMinus = new TriBtn(TriBtn::Left, this);
        m_shiftLabel = new ScrollableLabel(this);
        m_shiftPlus  = new TriBtn(TriBtn::Right, this);

        m_shiftMinus->setObjectName("shiftMinus");
        m_shiftMinus->setToolTip(QStringLiteral("Decrease RTTY shift spacing"));
        m_shiftLabel->setObjectName("shiftLabel");
        m_shiftPlus->setObjectName("shiftPlus");
        m_shiftPlus->setToolTip(QStringLiteral("Increase RTTY shift spacing"));

        // From Thetis radio.cs:2043-2044 — shift = mark(2295) − space(2125) = 170 Hz
        m_shiftLabel->setRange(50, 1000);
        m_shiftLabel->setStep(kShiftStep);
        m_shiftLabel->setValue(170);

        ctrl->addWidget(m_shiftMinus);
        ctrl->addWidget(m_shiftLabel, 1);
        ctrl->addWidget(m_shiftPlus);

        vbox->addLayout(ctrl);
    }

    // ── Signal connections ────────────────────────────────────────────────

    connect(m_markMinus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyMarkHz(m_slice->rttyMarkHz() - kMarkStep);
    });

    connect(m_markPlus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyMarkHz(m_slice->rttyMarkHz() + kMarkStep);
    });

    connect(m_markLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_slice) { return; }
        m_slice->setRttyMarkHz(hz);
    });

    connect(m_shiftMinus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyShiftHz(m_slice->rttyShiftHz() - kShiftStep);
    });

    connect(m_shiftPlus, &QPushButton::clicked, this, [this]() {
        if (!m_slice) { return; }
        m_slice->setRttyShiftHz(m_slice->rttyShiftHz() + kShiftStep);
    });

    connect(m_shiftLabel, &ScrollableLabel::valueChanged, this, [this](int hz) {
        if (!m_slice) { return; }
        m_slice->setRttyShiftHz(hz);
    });
}

void RttyMarkShiftContainer::setSlice(SliceModel* s)
{
    m_slice = s;
    syncFromSlice();
}

void RttyMarkShiftContainer::syncFromSlice()
{
    if (!m_slice) { return; }
    const QSignalBlocker b1(m_markLabel);
    const QSignalBlocker b2(m_shiftLabel);
    m_markLabel->setValue(m_slice->rttyMarkHz());
    m_shiftLabel->setValue(m_slice->rttyShiftHz());
}

// ── CW autotune (S2.10c) ──────────────────────────────────────────────────
//
// TODO (deferred — no WDSP API):
// CW autotune would require a pitch detection algorithm that identifies
// the received CW carrier frequency and applies a correction offset.
//
// Investigation of matchedCW.h (third_party/wdsp/src/matchedCW.h) shows
// that the "matched" module is a Gaussian partitioned-overlap-save filter
// with SetRXAMatchedRun/Freqs/Gain API — it is the APF "selection=1" type,
// not a CW tone detector or autotune controller.
//
// Implementing CW autotune natively would require:
//   1. A pitch detector (e.g., FFT peak-finder or autocorrelation) in the
//      audio-frequency range (~200–900 Hz for typical CW pitches)
//   2. A frequency error signal fed back to RxChannel::setShiftFrequency()
//      or slice->setRitHz() to center the tone on the APF frequency
//   3. A one-shot or polling mode (QTimer @ ~500ms per original plan)
//
// This is a substantial addition beyond the current phase scope.
// Deferred to a future phase with an explicit native design document.

}  // namespace Longpath
