// =================================================================
// src/core/CatParser.h  (Longpath)
// =================================================================
//
// Ported from Thetis source:
//   Project Files/Source/Console/CAT/CATCommands.cs,
//   Project Files/Source/Console/CAT/CATParser.cs
//   [v2.10.3.15-5-g852bf0e], original licence from the Thetis sources is
//   included below
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Ported in C++20/Qt6 for Longpath, with AI-assisted
//                transformation via Anthropic Claude Code. Phase 3K,
//                second step: the Kenwood-style CAT command set that the
//                rigctld bridge does not cover. Scope is the core set a
//                logger or contest program needs — ID, IF, FA, FB, FR,
//                FT, MD, PS, TX, RX, AI — not the whole 10,591-line
//                CATCommands.cs. Everything else answers "?;", which is
//                Thetis's own answer for a command it does not know
//                (CATParser.cs:56, Error1).
// =================================================================

/*  CATCommands.cs / CATParser.cs

This code/file can be found on GitHub : https://github.com/ramdor/Thetis

Copyright (C) 2012  Bob Tracy

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

You may contact the author via email at: k5kdn@arrl.net
*/
// =================================================================

#pragma once

#include <QtCore/QObject>
#include <QtCore/QString>

namespace Longpath {

// CatParser — one Kenwood-style CAT command in, one answer out.
//
// Transport-free on purpose: CatServer feeds it lines, TciProtocol's
// run_cat_ex feeds it single commands, and the tests feed it strings.
// The radio is a plain QObject* reached through the same Q_INVOKABLE
// shims TciProtocol and RigctldServer use.
class CatParser {
public:
    explicit CatParser(QObject* radio);

    /// Takes ONE command with or without its trailing ';' and returns the
    /// answer including ';', or an empty string when the command is a
    /// setter that Thetis answers with silence. Unknown or malformed
    /// commands return "?;" (Thetis CATParser.cs:56 -- Error1).
    QString handleCommand(const QString& command);

    /// Transmit gate, same contract as RigctldServer: closed by default,
    /// unkeying always allowed. TX while closed answers "?;".
    void setPttAllowed(bool on) { m_pttAllowed = on; }
    bool pttAllowed() const     { return m_pttAllowed; }

    /// True when the last command refused to key the radio because the
    /// gate was closed. CatServer reads it to log and signal once.
    bool takePttRefused();

    /// Kenwood mode digit <-> Longpath mode name.
    /// From Thetis Mode2KString at CATCommands.cs:10180 [v2.10.3.15-5-g852bf0e].
    static QString kenwoodModeFromLongpath(const QString& longpathMode);
    static QString longpathModeFromKenwood(const QString& digit);

    /// From Thetis Step2String at CATCommands.cs:10386
    /// [v2.10.3.15-5-g852bf0e] -- the 4-character step code IF carries.
    static QString stepCodeFromHz(int stepHz);

    /// Kenwood S-meter reading (0..30) from a dBm signal level.
    /// From Thetis SM at CATCommands.cs:896-931 [v2.10.3.15-5-g852bf0e],
    /// including its clamps (-140..-10 dBm) and the two-piece scale:
    /// below S9 it is sx * 1.6667, above it 15 + (dBm + 73).
    static int kenwoodSMeterFromDbm(double dbm);

private:
    QString handleId();
    QString handleIf();
    QString handleFrequency(const QString& suffix, int sliceIndex, const QString& name);
    QString handleMode(const QString& suffix);
    QString handlePowerStatus(const QString& suffix);
    QString handleAutoInformation(const QString& suffix);
    QString handleRxVfo(const QString& suffix);
    QString handleTxVfo(const QString& suffix);
    QString handleKey(bool on);
    QString handleSignalMeter(const QString& suffix);
    QString handleRitEnable(const QString& suffix);
    QString handleXitEnable(const QString& suffix);
    QString handleClearIt();
    QString handleStepIt(int direction);

    QObject* m_radio{nullptr};
    bool     m_pttAllowed{false};
    bool     m_pttRefused{false};
};

}  // namespace Longpath
