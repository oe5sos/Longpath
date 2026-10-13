// =================================================================
// src/core/CatProtocol.cpp  (Longpath)
// =================================================================
//
// Ported from Thetis sources:
//   Project Files/Source/Console/CAT/CATParser.cs   [v2.10.3.15]
//   Project Files/Source/Console/CAT/CATCommands.cs [v2.10.3.15]
//   Project Files/Source/Console/CAT/CATStructs.xml [v2.10.3.15]
// Original licences from the Thetis sources are included below.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Reimplemented in C++20/Qt6 for Longpath, AI-assisted
//                 via Anthropic Claude Code. Erster Gang: die zehn
//                 Befehle, die Fremdprogramme tatsaechlich sprechen
//                 (FA FB MD IF ID AI TX RX FR FT). Thetis' CAT ist
//                 18.081 Zeilen, davon 10.591 in CATCommands.cs mit
//                 dem vollen Kenwood-Satz plus allen ZZ-Erweiterungen;
//                 der Rest folgt in eigenen Gaengen.
// =================================================================
//
// --- From CATParser.cs ---
//=================================================================
// CATParser.cs
//=================================================================
// Copyright (C) 2012  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
//
// --- From CATCommands.cs ---
//=================================================================
// CATCommands.cs
//=================================================================
// Copyright (C) 2005  Bob Tracy
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
// You may contact the author via email at: k5kdn@arrl.net
//=================================================================
// Continual modifications Copyright (C) 2019-2026 Richard Samphire (MW0LGE)
//=================================================================

// =================================================================
// Der Kopf spiegelt CatProtocol.h.
// =================================================================

#include "CatProtocol.h"

#include <QChar>
#include <QLatin1String>

#include <array>
#include <cmath>

namespace Longpath {

namespace {

// Die Laengen je Befehl, aus Thetis CAT/CATStructs.xml [v2.10.3.15].
// nSet = Zeichen, die ein Setzen mitbringt; nGet = die ein Abfragen
// mitbringt (bei allen hier 0, also "AA;" ohne Nutzdaten); nAns =
// Zeichen der Antwort. -1 heisst: diese Richtung gibt es nicht.
//
//   AI: set=1  get=0  ans=1    auto information
//   FA: set=11 get=0  ans=11   vfo a frequency
//   FB: set=11 get=0  ans=11   vfo b frequency
//   FR: set=1  get=0  ans=1    selects rx vfo mem or call
//   FT: set=1  get=0  ans=1    selects tx vfo mem or call
//   ID: set=-1 get=0  ans=3    transceiver id number
//   IF: set=-1 get=0  ans=35   transceiver status
//   MD: set=1  get=0  ans=1    operating mode
//   RX: set=0  get=-1 ans=1    rx function status
//   TX: set=0  get=-1 ans=1    set tx mode
struct Laengen { int nSet; int nGet; int nAns; };

Laengen laengenFuer(QStringView praefix)
{
    if (praefix == QLatin1String("AI")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("FA")) { return {11,  0, 11}; }
    if (praefix == QLatin1String("FB")) { return {11,  0, 11}; }
    if (praefix == QLatin1String("FR")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("FT")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("ID")) { return {-1,  0,  3}; }
    if (praefix == QLatin1String("IF")) { return {-1,  0, 35}; }
    if (praefix == QLatin1String("MD")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("RX")) { return { 0, -1,  1}; }
    if (praefix == QLatin1String("TX")) { return { 0, -1,  1}; }
    return {-2, -2, -2};   // unbekannt
}

// Das 4-Zeichen-Schrittfeld des Statussatzes. Verbatim aus Thetis
// CAT/CATCommands.cs Step2String [v2.10.3.15]; die Hz-Werte stehen dort
// als Kommentar hinter jedem Code und sind hier die Zuordnung, weil
// Longpath den Schritt in Hz haelt und nicht als Listenindex.
//
// Die Tabelle ist NICHT die blosse Zehnerpotenz: Thetis hat sie am
// 2.10.3.9 (MW0LGE) fuer Sonderfaelle angepasst -- 50 Hz gibt "1000",
// 250 Hz "1001", 500 Hz "1010", 5 kHz "1011", 9 kHz "1100",
// 250 kHz "1101", 500 kHz "1110", und die auskommentierten Werte
// daneben zeigen, was dort vorher stand. Wer das auf
// "log10 aufrunden" vereinfacht, bricht genau diese sieben.
struct Schritt { int hz; const char* code; };
constexpr std::array<Schritt, 26> kSchritte{{
    {        1, "0000" },  // 10e0 (1Hz)
    {        2, "0000" },  // 10e0 (2Hz)
    {       10, "0001" },  // 10e1 (10Hz)
    {       25, "0001" },  // 10e1 (25Hz)
    {       50, "1000" },  // 10e1 (50Hz)      -- Sonderfall
    {      100, "0010" },  // 10e2 (100Hz)
    {      250, "1001" },  // 10e2 (250Hz)     -- Sonderfall
    {      500, "1010" },  // 10e2 (500Hz)     -- Sonderfall
    {     1000, "0011" },  // 10e3 (1kHz)
    {     2000, "0011" },  // 10e3 (2kHz)
    {     2500, "0011" },  // 10e3 (2.5kHz)
    {     5000, "1011" },  // 10e3 (5kHz)      -- Sonderfall
    {     6250, "0011" },  // 10e3 (6.25kHz)
    {     9000, "1100" },  // 10e3 (9kHz)      -- Sonderfall
    {    10000, "0100" },  // 10e4 (10kHz)
    {    12500, "0100" },  // 10e4 (12.5kHz)
    {    15000, "0100" },  // 10e4 (15kHz)
    {    20000, "0100" },  // 10e4 (20kHz)
    {    25000, "0100" },  // 10e4 (25kHz)
    {    30000, "0100" },  // 10e4 (30kHz)
    {    50000, "0100" },  // 10e4 (50kHz)
    {   100000, "0101" },  // 10e5 (100kHz)
    {   250000, "1101" },  // 10e5 (250kHz)    -- Sonderfall
    {   500000, "1110" },  // 10e5 (500kHz)    -- Sonderfall
    {  1000000, "0110" },  // 10e6 (1MHz)
    { 10000000, "0111" },  // 10e7 (10MHz)
}};

} // namespace

QString CatProtocol::mitNullen(quint64 wert, int stellen)
{
    return QStringLiteral("%1").arg(wert, stellen, 10, QLatin1Char('0'));
}

QString CatProtocol::schrittFeld(int stepHz)
{
    // Genau getroffen gewinnt. Thetis kennt nur die Listenindizes; ein
    // Schritt, den die Liste nicht hat, kommt bei ihm nie vor.
    for (const Schritt& s : kSchritte) {
        if (s.hz == stepHz) { return QString::fromLatin1(s.code); }
    }
    // Nicht in der Liste: den naechstkleineren nehmen, nicht den
    // naechstgelegenen. Ein zu grob gemeldeter Schritt laesst ein
    // Fremdprogramm gruber abstimmen als der Betreiber eingestellt hat;
    // zu fein gemeldet ist die harmlosere Richtung.
    //
    // From Thetis CAT/CATCommands.cs Step2String [v2.10.3.15]: ein
    // Index ausserhalb der Liste gibt dort "0000" ("some default").
    // Das waere hier 1 Hz fuer JEDEN unbekannten Schritt, auch fuer
    // 1 MHz -- darum der Rueckfall nach unten statt auf 1 Hz.
    const char* beste = kSchritte.front().code;
    for (const Schritt& s : kSchritte) {
        if (s.hz <= stepHz) { beste = s.code; } else { break; }
    }
    return QString::fromLatin1(beste);
}

QString CatProtocol::itFeld(int hz)
{
    // From Thetis CAT/CATCommands.cs IF() [v2.10.3.15]:
    //   if(ITValue < 0) incr = "-"+Abs(ITValue).PadLeft(5,'0');
    //   else            incr = "+"+Abs(ITValue).PadLeft(5,'0');
    // Also IMMER ein Vorzeichen, auch bei 0 -- "+00000".
    const int betrag = (hz < 0) ? -hz : hz;
    // Fuenf Stellen tragen 99999 Hz. Mehr RIT gibt es nicht, aber ein
    // ueberlaufender Wert wuerde den 35-Zeichen-Satz verschieben und
    // damit JEDES Feld dahinter -- darum hart gedeckelt.
    const int gedeckelt = (betrag > 99999) ? 99999 : betrag;
    return (hz < 0 ? QStringLiteral("-") : QStringLiteral("+"))
           + QStringLiteral("%1").arg(gedeckelt, 5, 10, QLatin1Char('0'));
}

QString CatProtocol::statussatz() const
{
    // From Thetis CAT/CATCommands.cs IF() [v2.10.3.15] -- Reihenfolge und
    // Feldbreiten verbatim, einschliesslich der Blindfelder:
    //   11 Freq A · 4 Schritt · 6 RIT/XIT · 1 RIT · 1 XIT · 3 Bank(000)
    //   · 1 TX · 1 Modus · 1 FR/FT(0) · 1 Scan(0) · 1 Split · 4 Bal(0000)
    //   = 35
    QString s;
    s += mitNullen(m_z.vfoAHz, 11);
    s += schrittFeld(m_z.stepHz);
    // Welcher Wert gezeigt wird, entscheidet Thetis so: erst RIT, sonst
    // XIT, sonst 0. Nicht beide zugleich.
    const int it = m_z.ritOn ? m_z.itHz : (m_z.xitOn ? m_z.itHz : 0);
    s += itFeld(it);
    s += m_z.ritOn ? QStringLiteral("1") : QStringLiteral("0");
    // Thetis setzt xit nur, wenn RIT AUS ist (`else if(console.XITOn)`).
    // Nachgebaut, nicht geglaettet: ein Fremdprogramm, das beide Flaggen
    // gesetzt sieht, weiss nicht, welchen Wert das IT-Feld traegt.
    s += (!m_z.ritOn && m_z.xitOn) ? QStringLiteral("1") : QStringLiteral("0");
    s += QStringLiteral("000");                                  // Speicherbank
    s += m_z.mox ? QStringLiteral("1") : QStringLiteral("0");
    // Modus. Thetis faellt auf "2" (USB) zurueck, wenn Mode2KString
    // seinen Fehler gibt -- also bei SAM, DRM, SPEC und allem anderen,
    // was Kenwood nicht kennt. Hier dasselbe: ein unmoeglicher Wert
    // wuerde den Satz verkuerzen.
    s += (m_z.modeK >= 1 && m_z.modeK <= 9)
             ? QString::number(m_z.modeK)
             : QStringLiteral("2");
    s += QStringLiteral("0");                                    // FR/FT (blind)
    s += QStringLiteral("0");                                    // Scan  (blind)
    s += m_z.split ? QStringLiteral("1") : QStringLiteral("0");
    s += QStringLiteral("0000");                                 // Balance (blind)
    return s;
}

QString CatProtocol::antwortAuf(const QString& befehl)
{
    // ── Zerlegen ─────────────────────────────────────────────────────────
    // From Thetis CAT/CATParser.cs:410-453 [v2.10.3.15] -- CheckFormat():
    //   * fuehrende ';' abwerfen (WriteLog schickt die)
    //   * ohne Abschluss ';' ab Stelle 2 ist es kein Befehl
    QString roh = befehl;
    while (roh.startsWith(QLatin1Char(';'))) { roh.remove(0, 1); }

    const int ende = roh.indexOf(QLatin1Char(';'));
    if (ende < 2) {
        // From CATParser.cs:116-123: kuerzer als "aa;" wird abgelehnt.
        return abgelehnt();
    }
    const QString praefix = roh.left(2).toUpper();
    const QString suffix  = roh.mid(2, ende - 2);

    const Laengen l = laengenFuer(praefix);
    if (l.nSet == -2) {
        // Unbekanntes Praefix. Thetis hat fuer jeden Kenwood-Code einen
        // Zweig und laesst die ungebauten LEER durchfallen -- die Antwort
        // ist dann "" und nicht "?;" (CATParser.cs:285-298: nur ein
        // schlechtes FORMAT gibt Error1).
        //
        // Longpath lehnt hier bewusst AB statt zu schweigen. Begruendung:
        // ein Fremdprogramm, das auf eine Antwort wartet, haengt bei
        // Stille bis in seinen Zeitablauf, und der Betreiber sieht ein
        // Programm, das "nicht geht". Ein `?;` sagt dagegen sofort
        // "diesen Befehl kenne ich nicht" -- WSJT-X und fldigi werten
        // das aus und gehen weiter.
        return abgelehnt();
    }

    const int laenge = suffix.size();
    const bool istSetzen   = (l.nSet >= 0 && laenge == l.nSet && l.nSet > 0);
    const bool istAbfragen = (l.nGet >= 0 && laenge == l.nGet);
    // TX; und RX; haben nSet=0: der Befehl SELBST ist die Tat, ohne
    // Nutzdaten. Er ist damit ein "Setzen" mit leerem Suffix.
    const bool istTat = (l.nSet == 0 && laenge == 0);

    QString daten;   // die Nutzdaten der Antwort, ohne Praefix und ';'

    if (praefix == QLatin1String("FA") || praefix == QLatin1String("FB")) {
        const bool a = (praefix == QLatin1String("FA"));
        if (istSetzen) {
            bool ok = false;
            const quint64 hz = suffix.toULongLong(&ok);
            if (!ok) { return abgelehnt(); }
            auto& haken = a ? m_h.setzeVfoA : m_h.setzeVfoB;
            if (!haken) { return abgelehnt(); }
            haken(hz);
            if (a) { m_z.vfoAHz = hz; } else { m_z.vfoBHz = hz; }
            return QString();      // Setzen wird nicht quittiert
        }
        if (istAbfragen) {
            // Der Haken ist das Kennzeichen dafuer, dass es diesen VFO
            // GIBT -- und er entscheidet darum auch die Abfrage, nicht
            // nur das Setzen.
            //
            // Ohne diese Pruefung meldete FA;/FB; den Vorgabewert des
            // Zustands, und der ist 0. Live am 2026-10-11 so gesehen:
            // Longpath antwortete auf FB; mit "FB00000000000;" -- eine
            // Frequenz fuer ein VFO B, das es hier nicht gibt (siehe
            // MainWindow: setzeVfoB bleibt absichtlich leer, weil
            // RadioModel keinen zweiten VFO kennt). Ein Fremdprogramm
            // liest das als "VFO B steht auf 0 Hz" und zeigt es an.
            //
            // Kein Prueffall hatte es gesehen: die unteren beiden
            // Schichten setzen in ihren Faellen immer beide Haken, und
            // der Verdrahtungsfall prueft nur das SETZEN von FB.
            const auto& haken = a ? m_h.setzeVfoA : m_h.setzeVfoB;
            if (!haken) { return abgelehnt(); }
            daten = mitNullen(a ? m_z.vfoAHz : m_z.vfoBHz, 11);
        } else { return abgelehnt(); }

    } else if (praefix == QLatin1String("MD")) {
        if (istSetzen) {
            bool ok = false;
            const int k = suffix.toInt(&ok);
            // From Thetis CAT/CATCommands.cs:573-580 [v2.10.3.15] --
            // MD(): `if(Convert.ToInt32(s) > 0 && <= 9)`, sonst Error1.
            if (!ok || k <= 0 || k > 9) { return abgelehnt(); }
            if (!m_h.setzeModeK) { return abgelehnt(); }
            m_h.setzeModeK(k);
            m_z.modeK = k;
            return QString();
        }
        if (istAbfragen) { daten = QString::number(m_z.modeK); }
        else { return abgelehnt(); }

    } else if (praefix == QLatin1String("IF")) {
        if (!istAbfragen) { return abgelehnt(); }
        daten = statussatz();

    } else if (praefix == QLatin1String("ID")) {
        if (!istAbfragen) { return abgelehnt(); }
        daten = mitNullen(static_cast<quint64>(m_z.rigId < 0 ? 0 : m_z.rigId), 3);

    } else if (praefix == QLatin1String("AI")) {
        // Selbstmeldung. Longpath nimmt den Befehl an und merkt sich die
        // Einstellung, SENDET aber (noch) nichts von selbst -- das
        // gehoert in den Transport, nicht in diese Schicht, und steht
        // als eigener Gang aus. Ein Fremdprogramm, das AI1; setzt und
        // dann auf Meldungen wartet, wuerde haengen; darum liest der
        // Server diese Flagge aus und fragt nach.
        if (istSetzen) {
            bool ok = false;
            const int v = suffix.toInt(&ok);
            if (!ok || v < 0 || v > 1) { return abgelehnt(); }
            m_selbstmeldung = (v == 1);
            return QString();
        }
        if (istAbfragen) { daten = m_selbstmeldung ? QStringLiteral("1")
                                                   : QStringLiteral("0"); }
        else { return abgelehnt(); }

    } else if (praefix == QLatin1String("FR") || praefix == QLatin1String("FT")) {
        // FR waehlt den EMPFANGS-VFO, FT den SENDE-VFO. 0 = VFO A,
        // 1 = VFO B. Longpath kennt kein Speicher-/Rufkanal-Ziel (2),
        // also wird das abgelehnt statt stillschweigend als B gedeutet.
        const bool senden = (praefix == QLatin1String("FT"));
        if (istSetzen) {
            bool ok = false;
            const int v = suffix.toInt(&ok);
            if (!ok || v < 0 || v > 1) { return abgelehnt(); }
            if (!senden) {
                // Empfangs-VFO umschalten kennt Longpath nicht als
                // eigenen Begriff -- A ist immer der Empfangsslice.
                // 0 ist damit erfuellbar, 1 nicht.
                return (v == 0) ? QString() : abgelehnt();
            }
            if (!m_h.setzeSplit) { return abgelehnt(); }
            m_h.setzeSplit(v == 1);
            m_z.split = (v == 1);
            return QString();
        }
        if (istAbfragen) {
            daten = senden ? (m_z.split ? QStringLiteral("1") : QStringLiteral("0"))
                           : QStringLiteral("0");
        } else { return abgelehnt(); }

    } else if (praefix == QLatin1String("TX") || praefix == QLatin1String("RX")) {
        const bool senden = (praefix == QLatin1String("TX"));
        if (!istTat) { return abgelehnt(); }
        if (!m_h.setzeMox) { return abgelehnt(); }
        m_h.setzeMox(senden);
        m_z.mox = senden;
        // From Thetis CAT/CATStructs.xml: TX und RX haben nAns=1 --
        // sie quittieren mit dem eigenen Praefix und einer "0".
        daten = QStringLiteral("0");

    } else {
        return abgelehnt();
    }

    // ── Antwort zusammensetzen ───────────────────────────────────────────
    // From Thetis CAT/CATParser.cs:390-401 [v2.10.3.15], verbatim die
    // Regel:
    //   if(rtncmd.Length == nAns && nAns > 0) rtncmd = prefix+rtncmd+";";
    //   else if(nAns == -1 || rtncmd == "")   rtncmd = "";
    //   else                                  rtncmd = Error3;   // "O;"
    if (daten.isEmpty()) { return QString(); }
    if (l.nAns > 0 && daten.size() == l.nAns) {
        return praefix + daten + QStringLiteral(";");
    }
    // Falsche Laenge. Thetis gibt hier "O;" (Error3, "processing
    // incomplete"). Das ist kein Formatfehler des Anrufers, sondern
    // einer bei UNS -- wenn der Statussatz je nicht 35 Zeichen hat,
    // soll das auffallen und nicht als gueltige Kurzantwort hinausgehen.
    return QStringLiteral("O;");
}

} // namespace Longpath
