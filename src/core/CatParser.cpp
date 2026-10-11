// =================================================================
// src/core/CatParser.cpp  (Longpath)
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

#include "CatParser.h"

#include "LogCategories.h"

#include <QtCore/QMetaObject>
#include <QtCore/QStringList>

#include <cmath>

namespace Longpath {

namespace {

// From Thetis CATParser.cs:56 [v2.10.3.15-5-g852bf0e] -- Error1.
const QString kError = QStringLiteral("?;");

// Thetis reports TS-2000 by default: CATCommands.cs:291-313
// [v2.10.3.15-5-g852bf0e] -- ID() switches on console.CATRigType and the
// default branch returns "019". Longpath has no rig-type selector yet, so
// the default is all there is.
const QString kRigId = QStringLiteral("019");

QString elevenDigits(qint64 hz)
{
    if (hz < 0) {
        hz = 0;
    }
    return QStringLiteral("%1").arg(hz, 11, 10, QLatin1Char('0'));
}

}  // namespace

CatParser::CatParser(QObject* radio)
    : m_radio(radio)
{
}

bool CatParser::takePttRefused()
{
    const bool was = m_pttRefused;
    m_pttRefused = false;
    return was;
}

// ---------------------------------------------------------------------------
// Mode digits
//
// From Thetis Mode2KString at CATCommands.cs:10180-10224
// [v2.10.3.15-5-g852bf0e]. Thetis maps DIGL/DIGU onto 1/2 when the
// operator set "DigU is USB" and onto 6/9 otherwise; Longpath has no such
// switch, so the plain FSK digits are used. Modes Kenwood has no digit for
// (DSB, SPEC, DRM, RADE) report USB, the same "nearest sideband a client
// can send back" rule as the rigctld bridge.
// ---------------------------------------------------------------------------

QString CatParser::kenwoodModeFromLongpath(const QString& longpathMode)
{
    if (longpathMode == QStringLiteral("LSB"))  { return QStringLiteral("1"); }
    if (longpathMode == QStringLiteral("USB"))  { return QStringLiteral("2"); }
    if (longpathMode == QStringLiteral("CWU"))  { return QStringLiteral("3"); }
    if (longpathMode == QStringLiteral("FM"))   { return QStringLiteral("4"); }
    if (longpathMode == QStringLiteral("AM"))   { return QStringLiteral("5"); }
    if (longpathMode == QStringLiteral("SAM"))  { return QStringLiteral("5"); }
    if (longpathMode == QStringLiteral("DIGL")) { return QStringLiteral("6"); }
    if (longpathMode == QStringLiteral("CWL"))  { return QStringLiteral("7"); }
    if (longpathMode == QStringLiteral("DIGU")) { return QStringLiteral("9"); }
    if (longpathMode == QStringLiteral("RADE-L")) { return QStringLiteral("1"); }
    return QStringLiteral("2");
}

QString CatParser::longpathModeFromKenwood(const QString& digit)
{
    if (digit == QStringLiteral("1")) { return QStringLiteral("LSB"); }
    if (digit == QStringLiteral("2")) { return QStringLiteral("USB"); }
    if (digit == QStringLiteral("3")) { return QStringLiteral("CWU"); }
    if (digit == QStringLiteral("4")) { return QStringLiteral("FM"); }
    if (digit == QStringLiteral("5")) { return QStringLiteral("AM"); }
    if (digit == QStringLiteral("6")) { return QStringLiteral("DIGL"); }
    if (digit == QStringLiteral("7")) { return QStringLiteral("CWL"); }
    if (digit == QStringLiteral("9")) { return QStringLiteral("DIGU"); }
    // 8 is FSK on a TS-2000 and has no Longpath counterpart; refused
    // rather than bent onto a neighbouring mode.
    return QString();
}

// From Thetis Step2String at CATCommands.cs:10386-10420
// [v2.10.3.15-5-g852bf0e] -- "[2.10.3.9]MW0LGE refactored, and tweaked for
// special cases to match original".  Thetis indexes a table by its own
// step-index; Longpath keeps the step in Hz (SliceModel::stepHz), so the
// same codes are selected by value. The tweaked cases are kept verbatim:
// 50 Hz, 250 Hz, 500 Hz, 5 kHz, 9 kHz, 250 kHz and 500 kHz each get their
// own code instead of their decade's.
QString CatParser::stepCodeFromHz(int stepHz)
{
    switch (stepHz) {
        case 50:      return QStringLiteral("1000");
        case 250:     return QStringLiteral("1001");
        case 500:     return QStringLiteral("1010");
        case 5000:    return QStringLiteral("1011");
        case 9000:    return QStringLiteral("1100");
        case 250000:  return QStringLiteral("1101");
        case 500000:  return QStringLiteral("1110");
        default: break;
    }
    if (stepHz >= 10000000) { return QStringLiteral("0111"); }
    if (stepHz >= 1000000)  { return QStringLiteral("0110"); }
    if (stepHz >= 100000)   { return QStringLiteral("0101"); }
    if (stepHz >= 10000)    { return QStringLiteral("0100"); }
    if (stepHz >= 1000)     { return QStringLiteral("0011"); }
    if (stepHz >= 100)      { return QStringLiteral("0010"); }
    if (stepHz >= 10)       { return QStringLiteral("0001"); }
    return QStringLiteral("0000");
}

// ---------------------------------------------------------------------------
// Dispatch
// ---------------------------------------------------------------------------

QString CatParser::handleCommand(const QString& command)
{
    QString cmd = command.trimmed();
    while (cmd.endsWith(QLatin1Char(';'))) {
        cmd.chop(1);
    }
    if (cmd.size() < 2) {
        return kError;
    }

    const QString name   = cmd.left(2).toUpper();
    const QString suffix = cmd.mid(2);

    if (name == QStringLiteral("ID")) {
        return suffix.isEmpty() ? handleId() : kError;
    }
    if (name == QStringLiteral("IF")) {
        return suffix.isEmpty() ? handleIf() : kError;
    }
    if (name == QStringLiteral("FA")) {
        return handleFrequency(suffix, 0, QStringLiteral("FA"));
    }
    if (name == QStringLiteral("FB")) {
        return handleFrequency(suffix, 1, QStringLiteral("FB"));
    }
    if (name == QStringLiteral("FR")) {
        return handleRxVfo(suffix);
    }
    if (name == QStringLiteral("FT")) {
        return handleTxVfo(suffix);
    }
    if (name == QStringLiteral("MD")) {
        return handleMode(suffix);
    }
    if (name == QStringLiteral("PS")) {
        return handlePowerStatus(suffix);
    }
    if (name == QStringLiteral("AI")) {
        return handleAutoInformation(suffix);
    }
    // From Thetis TX at CATCommands.cs:970-975 and RX at :822-827
    // [v2.10.3.15-5-g852bf0e]: both ignore their argument entirely and
    // just set console.CATPTT.
    if (name == QStringLiteral("TX")) {
        return handleKey(true);
    }
    if (name == QStringLiteral("RX")) {
        return handleKey(false);
    }

    qCDebug(lcCat) << "CatParser: no handler for" << command;
    return kError;
}

QString CatParser::handleId()
{
    return QStringLiteral("ID%1;").arg(kRigId);
}

// From Thetis IF() at CATCommands.cs:317-390 [v2.10.3.15-5-g852bf0e].
// 35 payload bytes, in this order and width:
//   11 VFO A frequency, 4 step code, 6 incremental tuning (sign + 5),
//   1 RIT, 1 XIT, 3 memory-bank dummy "000", 1 TX state, 1 mode digit,
//   1 FR/FT dummy, 1 scan dummy, 1 split, 4 balance dummy "0000".
QString CatParser::handleIf()
{
    qint64 hz = 0;
    QMetaObject::invokeMethod(m_radio, "vfoHz", Qt::DirectConnection,
                              Q_RETURN_ARG(qint64, hz),
                              Q_ARG(int, 0), Q_ARG(int, 0));

    int stepHz = 0;
    QMetaObject::invokeMethod(m_radio, "tuneStepHz", Qt::DirectConnection,
                              Q_RETURN_ARG(int, stepHz), Q_ARG(int, 0));

    bool ritOn = false;
    bool xitOn = false;
    int  ritHz = 0;
    int  xitHz = 0;
    QMetaObject::invokeMethod(m_radio, "ritEnable", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, ritOn));
    QMetaObject::invokeMethod(m_radio, "xitEnable", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, xitOn));
    QMetaObject::invokeMethod(m_radio, "ritOffset", Qt::DirectConnection,
                              Q_RETURN_ARG(int, ritHz));
    QMetaObject::invokeMethod(m_radio, "xitOffset", Qt::DirectConnection,
                              Q_RETURN_ARG(int, xitHz));

    // Thetis takes RIT first, then XIT, and only one of the two flags is
    // ever set in the answer (CATCommands.cs:333-338): the else-if is
    // deliberate upstream behaviour, not a slip, so it is kept.
    int itValue = 0;
    if (ritOn) {
        itValue = ritHz;
    } else if (xitOn) {
        itValue = xitHz;
    }
    const QString incr = QStringLiteral("%1%2")
        .arg(itValue < 0 ? QStringLiteral("-") : QStringLiteral("+"))
        .arg(std::abs(itValue), 5, 10, QLatin1Char('0'));

    bool mox = false;
    QMetaObject::invokeMethod(m_radio, "mox", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, mox));

    QString mode;
    QMetaObject::invokeMethod(m_radio, "mode", Qt::DirectConnection,
                              Q_RETURN_ARG(QString, mode), Q_ARG(int, 0));

    bool split = false;
    QMetaObject::invokeMethod(m_radio, "split", Qt::DirectConnection,
                              Q_RETURN_ARG(bool, split), Q_ARG(int, 0));

    QString payload;
    payload += elevenDigits(hz);                              // 11
    payload += stepCodeFromHz(stepHz);                        //  4
    payload += incr;                                          //  6
    payload += ritOn ? QStringLiteral("1") : QStringLiteral("0");   // 1
    payload += xitOn && !ritOn ? QStringLiteral("1")
                               : QStringLiteral("0");         //  1
    payload += QStringLiteral("000");                         //  3
    payload += mox ? QStringLiteral("1") : QStringLiteral("0");      // 1
    payload += kenwoodModeFromLongpath(mode);                 //  1
    payload += QStringLiteral("0");                           //  1 FR/FT
    payload += QStringLiteral("0");                           //  1 scan
    payload += split ? QStringLiteral("1") : QStringLiteral("0");    // 1
    payload += QStringLiteral("0000");                        //  4

    Q_ASSERT(payload.size() == 35);
    return QStringLiteral("IF%1;").arg(payload);
}

// From Thetis FA/FB at CATCommands.cs:189-218 -> ZZFA/ZZFB
// [v2.10.3.15-5-g852bf0e]: 11 digits on set, 11 digits on query.
//
// VFO B is slice 1 here, the same reading the rigctld bridge uses: in
// Longpath a second VFO IS a second slice (RadioModel::setVfoHz ignores
// chan != 0). With no second slice, FB reports zero and refuses the set
// rather than silently writing it to VFO A.
QString CatParser::handleFrequency(const QString& suffix, int sliceIndex,
                                   const QString& name)
{
    if (suffix.isEmpty()) {
        qint64 hz = 0;
        QMetaObject::invokeMethod(m_radio, "vfoHz", Qt::DirectConnection,
                                  Q_RETURN_ARG(qint64, hz),
                                  Q_ARG(int, sliceIndex), Q_ARG(int, 0));
        return QStringLiteral("%1%2;").arg(name, elevenDigits(hz));
    }

    if (suffix.size() != 11) {
        return kError;
    }
    bool ok = false;
    const qint64 hz = suffix.toLongLong(&ok);
    if (!ok || hz <= 0) {
        return kError;
    }
    QMetaObject::invokeMethod(m_radio, "setVfoHz", Qt::DirectConnection,
                              Q_ARG(int, sliceIndex), Q_ARG(int, 0),
                              Q_ARG(qint64, hz));

    // Read back: with no slice behind this index the set went nowhere,
    // and a silent "accepted" would be a lie. Same lesson as the TCI
    // _ex handlers (2026-10-11 bench run).
    qint64 applied = 0;
    QMetaObject::invokeMethod(m_radio, "vfoHz", Qt::DirectConnection,
                              Q_RETURN_ARG(qint64, applied),
                              Q_ARG(int, sliceIndex), Q_ARG(int, 0));
    return applied == hz ? QString() : kError;
}

// From Thetis MD at CATCommands.cs:566 [v2.10.3.15-5-g852bf0e].
QString CatParser::handleMode(const QString& suffix)
{
    if (suffix.isEmpty()) {
        QString mode;
        QMetaObject::invokeMethod(m_radio, "mode", Qt::DirectConnection,
                                  Q_RETURN_ARG(QString, mode), Q_ARG(int, 0));
        return QStringLiteral("MD%1;").arg(kenwoodModeFromLongpath(mode));
    }
    if (suffix.size() != 1) {
        return kError;
    }
    const QString longpathMode = longpathModeFromKenwood(suffix);
    if (longpathMode.isEmpty()) {
        return kError;
    }
    QMetaObject::invokeMethod(m_radio, "setMode", Qt::DirectConnection,
                              Q_ARG(int, 0), Q_ARG(QString, longpathMode));

    QString applied;
    QMetaObject::invokeMethod(m_radio, "mode", Qt::DirectConnection,
                              Q_RETURN_ARG(QString, applied), Q_ARG(int, 0));
    return applied == longpathMode ? QString() : kError;
}

// From Thetis PS at CATCommands.cs:734 -> ZZPS [v2.10.3.15-5-g852bf0e].
//
// Query answers 1: the program is running, or the client would not be
// talking to it. A set to 1 is accepted as a no-op, which is what loggers
// send on startup. PS0 is REFUSED: in Thetis it powers the radio down,
// and letting a logging program shut Longpath (or drop the radio link)
// mid-contest is not a side effect worth having.
QString CatParser::handlePowerStatus(const QString& suffix)
{
    if (suffix.isEmpty()) {
        bool on = true;
        QMetaObject::invokeMethod(m_radio, "powerOn", Qt::DirectConnection,
                                  Q_RETURN_ARG(bool, on));
        return QStringLiteral("PS%1;").arg(on ? 1 : 0);
    }
    if (suffix == QStringLiteral("1")) {
        return QString();
    }
    qCInfo(lcCat) << "CatParser: PS0 refused — a CAT client does not get to "
                     "power this station down";
    return kError;
}

// From Thetis AI at CATCommands.cs:105 -> ZZAI [v2.10.3.15-5-g852bf0e].
//
// AI1 asks the radio to push IF frames on every change. We do not push
// anything yet, so claiming to support it would leave a client waiting
// for updates that never come -- it is refused, and a client that gets
// "?;" falls back to polling, which works.
QString CatParser::handleAutoInformation(const QString& suffix)
{
    if (suffix.isEmpty()) {
        return QStringLiteral("AI0;");
    }
    if (suffix == QStringLiteral("0")) {
        return QString();
    }
    qCInfo(lcCat) << "CatParser: AI1 refused — Longpath does not push "
                     "unsolicited IF frames yet; poll instead";
    return kError;
}

// From Thetis FR at CATCommands.cs:223-235 [v2.10.3.15-5-g852bf0e] --
// "this is a dummy command to keep other software happy since the
// SDR-1000 always uses VFO A for rx". Same here: receive is slice 0.
QString CatParser::handleRxVfo(const QString& suffix)
{
    if (suffix.isEmpty()) {
        return QStringLiteral("FR0;");
    }
    if (suffix == QStringLiteral("0")) {
        return QString();
    }
    return kError;
}

// From Thetis FT at CATCommands.cs:237 [v2.10.3.15-5-g852bf0e] -- another
// "happiness" command upstream, where it drives split. Longpath has no
// split setter at all (RadioModel::split() is query-only, Phase 3F
// design §3), so FT0 is accepted and FT1 refused instead of pretending.
QString CatParser::handleTxVfo(const QString& suffix)
{
    if (suffix.isEmpty()) {
        return QStringLiteral("FT0;");
    }
    if (suffix == QStringLiteral("0")) {
        return QString();
    }
    qCInfo(lcCat) << "CatParser: FT1 refused — Longpath has no split; use XIT "
                     "or a second slice (\"Fake It\" on the client side)";
    return kError;
}

QString CatParser::handleKey(bool on)
{
    if (on && !m_pttAllowed) {
        m_pttRefused = true;
        qCWarning(lcCat) << "CatParser: refused TX — transmit over CAT is "
                            "turned off";
        return kError;
    }
    QMetaObject::invokeMethod(m_radio, "setMox", Qt::DirectConnection,
                              Q_ARG(bool, on));
    return QString();
}

}  // namespace Longpath
