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
// Der vollstaendige Block steht hier UND in CatProtocol.h, nicht nur
// dort: verify-thetis-headers.py prueft Datei fuer Datei, und beide
// stehen in THETIS-PROVENANCE.md. Ein Verweis "siehe .h" genuegt nicht
// -- zu Recht, denn wer nur die .cpp vor sich hat, soll die Herkunft
// sehen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-11 — Reimplemented in C++20/Qt6 for Longpath, AI-assisted
//                 via Anthropic Claude Code. Erster Gang: die zehn
//                 Befehle, die Fremdprogramme tatsaechlich sprechen.
//   2026-10-13 — Nachleser (Ruecklesen nach jedem Setzen) und sechs
//                 weitere Befehle (PS SM RT XT RC RU RD) aus der
//                 zweiten CAT-Fassung dieses Projekts uebernommen;
//                 siehe docs/architecture/2026-10-12-cat-zwei-fassungen.md.
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

#include "CatProtocol.h"

#include "CatModeMap.h"

#include <QChar>
#include <QLatin1String>

#include <algorithm>
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
    // Die sechs aus der zweiten Fassung (2026-10-13). Laengen aus
    // Thetis CAT/CATStructs.xml:
    //   PS: set=1 get=0 ans=1   power on/off
    //   SM: set=1 get=-1 ans=5  s-meter (set traegt den Kanal 0 oder 2)
    //   RT: set=1 get=0 ans=1   rit on/off
    //   XT: set=1 get=0 ans=1   xit on/off
    //   RC: set=0 get=-1 ans=-1 clear rit/xit  (quittiert NICHT)
    //   RU: set=0 get=-1 ans=-1 rit/xit up
    //   RD: set=0 get=-1 ans=-1 rit/xit down
    if (praefix == QLatin1String("PS")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("SM")) { return { 1, -1,  5}; }
    if (praefix == QLatin1String("RT")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("XT")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("RC")) { return { 0, -1, -1}; }
    if (praefix == QLatin1String("RU")) { return { 0, -1, -1}; }
    if (praefix == QLatin1String("RD")) { return { 0, -1, -1}; }
    return {-2, -2, -2};   // unbekannt
}

// Der ZZ-Satz, erster Gang (2026-10-13).
//
// Laengen verbatim aus Thetis CAT/CATStructs.xml [v2.10.3.15]. Die Datei
// hat 318 Befehle, davon 239 mit ZZ-Praefix und 201 davon aktiv -- der
// hier gebaute Satz sind die 22, die ein Logbuch- oder Contestprogramm
// braucht, um abzustimmen, den Modus zu setzen, den Zustand zu lesen, zu
// tasten und die Ablage zu stellen. Das ist der Satz, auf den sich N1MM+
// (Geraetetyp Flex5000 ueber TCP) und DXLab Commander (Geraetetyp Thetis)
// stuetzen.
//
// NICHT geprueft, weil es hier kein N1MM+ und kein DXLab gibt (beide
// Windows): WELCHE dieser Befehle die zwei Programme tatsaechlich
// senden. Die Auswahl folgt den Beschreibungen in CATStructs.xml und
// dem, was ein Logbuch tut -- der Beleg dafuer ist ein Lauf mit dem
// echten Programm, nicht dieser Prueffall.
Laengen zzLaengenFuer(QStringView praefix)
{
    //                                           set  get  ans
    if (praefix == QLatin1String("ZZFA")) { return {11,  0, 11}; }
    if (praefix == QLatin1String("ZZFB")) { return {11,  0, 11}; }
    if (praefix == QLatin1String("ZZFT")) { return {11,  0, 11}; }
    if (praefix == QLatin1String("ZZMD")) { return { 2,  0,  2}; }
    if (praefix == QLatin1String("ZZIF")) { return {-1,  0, 36}; }
    if (praefix == QLatin1String("ZZSP")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("ZZTX")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("ZZRT")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("ZZXS")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("ZZRF")) { return { 5,  0,  5}; }
    if (praefix == QLatin1String("ZZXF")) { return { 5,  0,  5}; }
    if (praefix == QLatin1String("ZZRU")) { return { 5,  0,  0}; }
    if (praefix == QLatin1String("ZZRD")) { return { 5,  0,  0}; }
    if (praefix == QLatin1String("ZZXU")) { return { 5,  0,  0}; }
    if (praefix == QLatin1String("ZZXD")) { return { 5,  0,  0}; }
    if (praefix == QLatin1String("ZZXC")) { return { 0, -1, -1}; }
    if (praefix == QLatin1String("ZZFL")) { return { 5,  0,  5}; }
    if (praefix == QLatin1String("ZZFH")) { return { 5,  0,  5}; }
    if (praefix == QLatin1String("ZZSO")) { return { 1,  0,  1}; }
    if (praefix == QLatin1String("ZZID")) { return {-1,  0, -1}; }
    // ZZZM und ZZZV tragen eine Antwort VERAENDERLICHER Laenge. Thetis
    // loest das, indem der Handler Praefix und ';' selbst anhaengt und
    // den Zusammenbau umgeht (CATCommands.cs:8642-8663). Hier wird
    // stattdessen die Laenge beim Antworten nachgetragen (Form::nAns),
    // damit es nur EINE Stelle gibt, die eine Antwort zusammensetzt.
    if (praefix == QLatin1String("ZZZM")) { return { 0,  0,  0}; }
    if (praefix == QLatin1String("ZZZV")) { return { 0,  0,  0}; }
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

int CatProtocol::sMeterAusDbm(double dbm)
{
    // From Thetis CAT/CATCommands.cs:896-931 [v2.10.3.15] -- SM.
    // Verbatim uebernommen aus der zweiten CAT-Fassung dieses Projekts
    // (Zweig fix/discovery-quietpolls-thetis-parity), die sie gegen
    // hamlibs TS-2000-Treiber gefahren hat.
    //
    // Der Knick bei S9 ist der Punkt: darunter eine Gerade durch
    // (dBm+127)/6, darueber 15 + (dBm+73). Wer das durch eine einzige
    // Gerade ersetzt, trifft genau in dem Bereich daneben, in dem ein
    // Contestprogramm Rapporte bildet.
    const double num = std::max(-140.0, std::min(-10.0, dbm));
    double sx = (num + 127.0) / 6.0;
    if (sx < 0.0) { sx = 0.0; }
    int sm = 0;
    if (sx <= 9.0) {
        sm = std::abs(static_cast<int>(sx * 1.6667));
    } else {
        const double overS9 = num + 73.0;
        sm = 15 + static_cast<int>(overS9);
    }
    return std::max(0, std::min(30, sm));
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
    const int it = aktiveAblage(m_z);
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

int CatProtocol::aktiveAblage(const Zustand& z)
{
    // From Thetis CAT/CATCommands.cs IF() und ZZIF() [v2.10.3.15]:
    //   if(RITOn) ITValue = RITValue; else if(XITOn) ITValue = XITValue;
    // Nicht beide zugleich -- siehe die Notiz im Statussatz.
    if (z.ritOn) { return z.ritHz; }
    if (z.xitOn) { return z.xitHz; }
    return 0;
}

QString CatProtocol::vorzeichenFeld4(int hz)
{
    // From Thetis CAT/CATCommands.cs:5904-5940 [v2.10.3.15] -- ZZRF:
    //   sign + AddLeadingZeros(Math.Abs(x)).Substring(1)
    // Also: Vorzeichen, dann vier Stellen mit fuehrenden Nullen.
    //
    // Geklammert auf +-9999 -- siehe die Begruendung am Kopf der
    // Deklaration (Thetis klammert weiter und verfaelscht dann die
    // Antwort).
    const int geklammert = std::clamp(hz, -9999, 9999);
    const QString v = (geklammert < 0) ? QStringLiteral("-") : QStringLiteral("+");
    return v + QStringLiteral("%1").arg(std::abs(geklammert), 4, 10, QLatin1Char('0'));
}

QString CatProtocol::zzStatussatz() const
{
    // From Thetis CAT/CATCommands.cs:3386-3441 [v2.10.3.15] -- ZZIF():
    //   11 Freq A · 4 Schritt · 6 RIT/XIT · 1 RIT · 1 XIT · 3 Bank(000)
    //   · 1 TX · 2 MODUS · 1 FR/FT(0) · 1 Scan(0) · 1 Split · 4 Bal(0000)
    //   = 36
    //
    // Gegenueber statussatz() (35) ist genau EIN Feld anders: der Modus
    // steht zweistellig. Alles andere liegt Zeichen fuer Zeichen gleich.
    // Darum ist der Satz hier NICHT aus dem anderen abgeleitet: eine
    // gemeinsame Hilfsfunktion mit einem Schalter fuer die Modusbreite
    // haette genau die Stelle versteckt, auf die es ankommt -- wer die
    // 35 auf 36 verschiebt, verschiebt jedes Feld hinter dem Modus.
    QString s;
    s += mitNullen(m_z.vfoAHz, 11);
    s += schrittFeld(m_z.stepHz);
    const int it = aktiveAblage(m_z);
    s += itFeld(it);
    s += m_z.ritOn ? QStringLiteral("1") : QStringLiteral("0");
    s += (!m_z.ritOn && m_z.xitOn) ? QStringLiteral("1") : QStringLiteral("0");
    s += QStringLiteral("000");                                  // Speicherbank
    s += m_z.mox ? QStringLiteral("1") : QStringLiteral("0");
    // Der Modus zweistellig. Ein Wert ausserhalb 0..11 wuerde den Satz
    // verkuerzen oder verlaengern; dann lieber USB melden als ein Feld
    // verschieben. catZzZahl() kann das gar nicht liefern -- die
    // Pruefung gilt dem Fall, dass jemand modeZZ von Hand setzt.
    s += catZzZahlGueltig(m_z.modeZZ)
             ? QStringLiteral("%1").arg(m_z.modeZZ, 2, 10, QLatin1Char('0'))
             : QStringLiteral("01");
    s += QStringLiteral("0");                                    // FR/FT (blind)
    s += QStringLiteral("0");                                    // Scan  (blind)
    s += m_z.split ? QStringLiteral("1") : QStringLiteral("0");
    s += QStringLiteral("0000");                                 // Balance (blind)
    return s;
}

bool CatProtocol::hatGewirkt(const std::function<bool(const Zustand&)>& pruefung)
{
    if (!m_nachleser) {
        // Kein Nachleser angehaengt: wie vorher blind quittieren. Das
        // ist dann die Entscheidung des Aufrufers (und die Pruefstaende
        // der Protokollschicht tun es absichtlich, weil sie keinen
        // Zustand hinter sich haben).
        return true;
    }
    m_z = m_nachleser();
    return pruefung(m_z);
}

// ---------------------------------------------------------------------------
// Die ZZ-Haelfte
// ---------------------------------------------------------------------------
//
// From Thetis CAT/CATCommands.cs [v2.10.3.15], je Befehl die Fundstelle
// am Zweig. Die Regeln, die fuer ALLE gelten und darum nicht bei jedem
// wiederholt stehen:
//
//   * Der HAKEN ist das Kennzeichen, dass eine Sache existiert -- er
//     entscheidet auch die ABFRAGE, nicht nur das Setzen. Ohne diese
//     Regel meldete Longpath am 2026-10-11 live "FB00000000000;" fuer
//     ein VFO B, das es nicht gibt.
//   * Nach jedem Setzen wird NACHGELESEN. Ein blind quittiertes Setzen
//     bringt ein Contestprogramm dazu, auf der alten Frequenz zu senden.
//
std::optional<QString> CatProtocol::zzDaten(const QString& praefix,
                                            const QString& suffix,
                                            Form& f)
{
    const std::optional<QString> nein = std::nullopt;

    // ── Frequenzen: ZZFA, ZZFB ───────────────────────────────────────────
    // From CATCommands.cs:2653-2700 (ZZFA), :2702-2750 (ZZFB).
    // Elf Stellen in Hz, wie FA/FB. Thetis rechnet sie ueber einen
    // eingeschobenen Dezimalpunkt in seine MHz-Zahl -- das Drahtformat
    // ist dasselbe.
    //
    // NICHT uebernommen: Thetis' RTTY-Versatz (RttyOffsetEnabledA, in
    // DIGU/DIGL wird der Offset auf die gemeldete Frequenz
    // aufgeschlagen). Longpath hat diese Einstellung nicht; wer sie
    // baut, gehoert hierher und nicht in die Verdrahtung.
    if (praefix == QLatin1String("ZZFA") || praefix == QLatin1String("ZZFB")) {
        const bool a = (praefix == QLatin1String("ZZFA"));
        auto& haken = a ? m_h.setzeVfoA : m_h.setzeVfoB;
        if (!haken) { return nein; }
        if (f.setzen) {
            bool ok = false;
            const quint64 hz = suffix.toULongLong(&ok);
            if (!ok) { return nein; }
            haken(hz);
            if (a) { m_z.vfoAHz = hz; } else { m_z.vfoBHz = hz; }
            if (!hatGewirkt([a, hz](const Zustand& z) {
                    return (a ? z.vfoAHz : z.vfoBHz) == hz;
                })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) { return mitNullen(a ? m_z.vfoAHz : m_z.vfoBHz, 11); }
        return nein;
    }

    // ── ZZFT: die SENDEfrequenz ──────────────────────────────────────────
    // From CATCommands.cs:2753-2800.
    //
    // Achtung, Namensfalle: Kenwoods FT waehlt das Sende-VFO (eine
    // Stelle), ZZFT ist die Sendefrequenz selbst (elf Stellen). Gleiches
    // Wort, verschiedene Sache.
    //
    // Longpath hat keinen Split (RadioModel::split() gibt bewusst immer
    // false, Phase-3F-Entwurf §3). Die ABFRAGE ist darum beantwortbar --
    // ohne Split ist die Sendefrequenz die von VFO A -- das SETZEN aber
    // nicht: wer ZZFT setzt, will senden und empfangen trennen. Das
    // stillschweigend als "stimme VFO A ab" auszufuehren wuerde die
    // Empfangsfrequenz des Betreibers mitten im Betrieb verschieben.
    // Darum `?;` -- ein Programm, das das liest, schaltet auf einen
    // zweiten Empfaenger um oder laesst Split aus.
    if (praefix == QLatin1String("ZZFT")) {
        if (!m_h.setzeVfoA) { return nein; }
        if (f.abfragen) { return mitNullen(m_z.vfoAHz, 11); }
        return nein;
    }

    // ── ZZMD: der Modus, zweistellig ─────────────────────────────────────
    // From CATCommands.cs:3949-3969. Bereich 0..11, ausserhalb `?;`.
    if (praefix == QLatin1String("ZZMD")) {
        if (!m_h.setzeModeZZ) { return nein; }
        if (f.setzen) {
            bool ok = false;
            const int z = suffix.toInt(&ok);
            if (!ok || !catZzZahlGueltig(z)) { return nein; }
            m_h.setzeModeZZ(z);
            m_z.modeZZ = z;
            if (!hatGewirkt([z](const Zustand& s) { return s.modeZZ == z; })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            return QStringLiteral("%1").arg(m_z.modeZZ, 2, 10, QLatin1Char('0'));
        }
        return nein;
    }

    // ── ZZIF: der erweiterte Statussatz ──────────────────────────────────
    if (praefix == QLatin1String("ZZIF")) {
        if (!f.abfragen) { return nein; }
        return zzStatussatz();
    }

    // ── ZZSP: Split ──────────────────────────────────────────────────────
    // From CATCommands.cs:6353-6390. Haken fehlt in Longpath mit Absicht
    // (kein Split), also auch keine Antwort -- eine gemeldete "0" waere
    // eine Aussage ueber etwas, das es nicht gibt.
    if (praefix == QLatin1String("ZZSP")) {
        if (!m_h.setzeSplit) { return nein; }
        if (f.setzen) {
            if (suffix != QLatin1String("0") && suffix != QLatin1String("1")) {
                return nein;
            }
            const bool an = (suffix == QLatin1String("1"));
            m_h.setzeSplit(an);
            m_z.split = an;
            if (!hatGewirkt([an](const Zustand& z) { return z.split == an; })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            return m_z.split ? QStringLiteral("1") : QStringLiteral("0");
        }
        return nein;
    }

    // ── ZZTX: tasten ─────────────────────────────────────────────────────
    // From CATCommands.cs:6961-6985. Die Abfrage meldet 1, wenn CAT-PTT
    // ODER der Sendeschalter am Geraet an ist -- also der tatsaechliche
    // Sendezustand, nicht nur der von CAT verursachte.
    if (praefix == QLatin1String("ZZTX")) {
        if (!m_h.setzeMox) { return nein; }
        if (f.setzen) {
            if (suffix != QLatin1String("0") && suffix != QLatin1String("1")) {
                return nein;
            }
            const bool an = (suffix == QLatin1String("1"));
            m_h.setzeMox(an);
            m_z.mox = an;
            if (!hatGewirkt([an](const Zustand& z) { return z.mox == an; })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            return m_z.mox ? QStringLiteral("1") : QStringLiteral("0");
        }
        return nein;
    }

    // ── ZZRT / ZZXS: RIT bzw. XIT ein ────────────────────────────────────
    // From CATCommands.cs:6082-6105 (ZZRT). ZZXS ist die XIT-Entsprechung.
    if (praefix == QLatin1String("ZZRT") || praefix == QLatin1String("ZZXS")) {
        const bool rit = (praefix == QLatin1String("ZZRT"));
        auto& haken = rit ? m_h.setzeRit : m_h.setzeXit;
        if (!haken) { return nein; }
        if (f.setzen) {
            if (suffix != QLatin1String("0") && suffix != QLatin1String("1")) {
                return nein;
            }
            const bool an = (suffix == QLatin1String("1"));
            haken(an);
            if (rit) { m_z.ritOn = an; } else { m_z.xitOn = an; }
            if (!hatGewirkt([rit, an](const Zustand& z) {
                    return (rit ? z.ritOn : z.xitOn) == an;
                })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            return (rit ? m_z.ritOn : m_z.xitOn) ? QStringLiteral("1")
                                                 : QStringLiteral("0");
        }
        return nein;
    }

    // ── ZZRF / ZZXF: die Ablagewerte, getrennt ───────────────────────────
    // From CATCommands.cs:5904-5940 (ZZRF). Fuenf Zeichen: Vorzeichen +
    // vier Stellen. Das Setzen wirkt UNABHAENGIG davon, ob die Ablage
    // gerade an ist -- so auch bei Thetis (W2PA: "Want to be able to
    // change RIT value even if it's off").
    if (praefix == QLatin1String("ZZRF") || praefix == QLatin1String("ZZXF")) {
        const bool rit = (praefix == QLatin1String("ZZRF"));
        auto& haken = rit ? m_h.setzeRitHz : m_h.setzeXitHz;
        if (!haken) { return nein; }
        if (f.setzen) {
            bool ok = false;
            const int roh = suffix.toInt(&ok);
            if (!ok) { return nein; }
            const int hz = std::clamp(roh, -9999, 9999);
            haken(hz);
            if (rit) { m_z.ritHz = hz; } else { m_z.xitHz = hz; }
            if (!hatGewirkt([rit, hz](const Zustand& z) {
                    return (rit ? z.ritHz : z.xitHz) == hz;
                })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            return vorzeichenFeld4(rit ? m_z.ritHz : m_z.xitHz);
        }
        return nein;
    }

    // ── ZZRU/ZZRD und ZZXU/ZZXD: Ablage schieben ─────────────────────────
    // From CATCommands.cs:5960-5990 (ZZRU).
    //
    // Der Schritt ist FEST ZEHN HERTZ und nicht der Abstimmschritt --
    // anders als bei Kenwoods RU/RD. In Thetis stehen die
    // modusabhaengigen Schritte (10 fuer CW, 50 fuer SSB) direkt
    // darueber auskommentiert, mit W2PAs Notiz "Changed to operate in
    // all modes". Wer das hier auf stepHz umstellt, bricht genau diese
    // Entscheidung.
    //
    // Mit fuenf Zeichen ist es ein SETZEN und geht an ZZRF/ZZXF; mit
    // leerem Rest ist es das Schieben.
    if (praefix == QLatin1String("ZZRU") || praefix == QLatin1String("ZZRD")
        || praefix == QLatin1String("ZZXU") || praefix == QLatin1String("ZZXD")) {
        const bool rit = praefix.startsWith(QLatin1String("ZZR"));
        const bool hoch = praefix.endsWith(QLatin1Char('U'));
        auto& haken = rit ? m_h.setzeRitHz : m_h.setzeXitHz;
        if (!haken) { return nein; }
        if (f.setzen) {
            bool ok = false;
            const int roh = suffix.toInt(&ok);
            if (!ok) { return nein; }
            const int hz = std::clamp(roh, -9999, 9999);
            haken(hz);
            if (rit) { m_z.ritHz = hz; } else { m_z.xitHz = hz; }
            if (!hatGewirkt([rit, hz](const Zustand& z) {
                    return (rit ? z.ritHz : z.xitHz) == hz;
                })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            const int ziel = std::clamp((rit ? m_z.ritHz : m_z.xitHz)
                                            + (hoch ? 10 : -10),
                                        -9999, 9999);
            haken(ziel);
            if (rit) { m_z.ritHz = ziel; } else { m_z.xitHz = ziel; }
            if (!hatGewirkt([rit, ziel](const Zustand& z) {
                    return (rit ? z.ritHz : z.xitHz) == ziel;
                })) {
                return nein;
            }
            return QString();      // nAns = 0: antwortet nie
        }
        return nein;
    }

    // ── ZZXC: XIT auf null ───────────────────────────────────────────────
    if (praefix == QLatin1String("ZZXC")) {
        if (!f.tat) { return nein; }
        if (!m_h.setzeXitHz) { return nein; }
        m_h.setzeXitHz(0);
        m_z.xitHz = 0;
        if (!hatGewirkt([](const Zustand& z) { return z.xitHz == 0; })) {
            return nein;
        }
        return QString();
    }

    // ── ZZFL / ZZFH: die Filterkanten ────────────────────────────────────
    // From CATCommands.cs ZZFL/ZZFH. Fuenf Zeichen wie bei den Ablagen.
    // Thetis klammert auf +-10000 und kann in der Antwort nur vier
    // Stellen fuehren -- aus 10000 wird dort "+0000". Hier +-9999, siehe
    // vorzeichenFeld4().
    if (praefix == QLatin1String("ZZFL") || praefix == QLatin1String("ZZFH")) {
        const bool tief = (praefix == QLatin1String("ZZFL"));
        auto& haken = tief ? m_h.setzeFilterLow : m_h.setzeFilterHigh;
        if (!haken) { return nein; }
        if (f.setzen) {
            bool ok = false;
            const int roh = suffix.toInt(&ok);
            if (!ok) { return nein; }
            const int hz = std::clamp(roh, -9999, 9999);
            haken(hz);
            if (tief) { m_z.filterLowHz = hz; } else { m_z.filterHighHz = hz; }
            if (!hatGewirkt([tief, hz](const Zustand& z) {
                    return (tief ? z.filterLowHz : z.filterHighHz) == hz;
                })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) {
            return vorzeichenFeld4(tief ? m_z.filterLowHz : m_z.filterHighHz);
        }
        return nein;
    }

    // ── ZZSO: Rauschsperre ───────────────────────────────────────────────
    // From CATCommands.cs ZZSO -- 0 aus, 1 an, 2 Stimmsperre
    // ("[2.10.3.5]MW0LGE 2 is vsql").
    if (praefix == QLatin1String("ZZSO")) {
        if (!m_h.setzeSquelch) { return nein; }
        if (f.setzen) {
            bool ok = false;
            const int w = suffix.toInt(&ok);
            if (!ok || w < 0 || w > 2) { return nein; }
            m_h.setzeSquelch(w);
            m_z.squelch = w;
            if (!hatGewirkt([w](const Zustand& z) { return z.squelch == w; })) {
                return nein;
            }
            return QString();
        }
        if (f.abfragen) { return QString::number(m_z.squelch); }
        return nein;
    }

    // ── ZZID: "sprich PowerSDR mit mir" ──────────────────────────────────
    // From CATCommands.cs:3369-3384. Thetis stellt damit seinen eigenen
    // CAT-Geraetetyp auf "PowerSDR" um und antwortet NICHT.
    //
    // Longpath hat nur EINEN Dialekt, also ist hier nichts umzustellen.
    // Angenommen wird der Befehl trotzdem, und das ist der Zweck dieses
    // Zweiges: DXLab Commander und N1MM+ schicken ihn beim Verbinden.
    // Ein `?;` darauf kann ein Programm als "falsches Geraet" lesen und
    // die Verbindung gleich wieder aufgeben.
    if (praefix == QLatin1String("ZZID")) {
        if (!f.abfragen) { return nein; }
        return QString();
    }

    // ── ZZZM / ZZZV: Gerätebezeichnung und Fassung ───────────────────────
    // From CATCommands.cs:8642-8663. Antwort VERAENDERLICHER Laenge;
    // Thetis haengt Praefix und ';' im Handler selbst an. Hier wird
    // stattdessen f.nAns nachgetragen, damit der Zusammenbau an EINER
    // Stelle bleibt.
    //
    // Ein ';' im Text wuerde die Antwort mitten durchschneiden -- Thetis
    // entfernt es bei ZZZV ausdruecklich (`.Replace(";", "")`), bei ZZZM
    // nicht. Hier bei beiden.
    if (praefix == QLatin1String("ZZZM") || praefix == QLatin1String("ZZZV")) {
        if (!f.abfragen) { return nein; }
        QString t = (praefix == QLatin1String("ZZZM")) ? m_z.modell : m_z.fassung;
        t.remove(QLatin1Char(';'));
        if (t.isEmpty()) { return nein; }
        f.nAns = t.size();
        return t;
    }

    return nein;
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
    // Das Befehlswort ist ZWEI Zeichen lang -- ausser bei den
    // ZZ-Erweiterungen, da sind es VIER.
    //
    // From Thetis CAT/CATParser.cs:108-215 [v2.10.3.15]: dort steht die
    // Fallunterscheidung als zwei getrennte Schalter, erst auf das
    // zweistellige Praefix und, wenn das "ZZ" ist, auf das vierstellige.
    // Ohne diese Stufe traefe "ZZFA00014250000;" auf das Praefix "ZZ"
    // und waere unbekannt -- und ALLE 239 ZZ-Befehle gaeben `?;`.
    const bool istZz = roh.startsWith(QLatin1String("ZZ"), Qt::CaseInsensitive);
    const int  wortlaenge = istZz ? 4 : 2;
    if (ende < wortlaenge) {
        // "ZZ;" oder "ZZF;" ist kein Befehl.
        return abgelehnt();
    }
    const QString praefix = roh.left(wortlaenge).toUpper();
    const QString suffix  = roh.mid(wortlaenge, ende - wortlaenge);

    const Laengen l = istZz ? zzLaengenFuer(praefix) : laengenFuer(praefix);
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
    // Die erwartete Antwortlaenge liegt in der Form, damit ZZZM; und
    // ZZZV; sie nachtragen koennen (veraenderliche Laenge).
    Form form{istSetzen, istAbfragen, istTat, l.nAns};

    if (istZz) {
        const std::optional<QString> d = zzDaten(praefix, suffix, form);
        if (!d.has_value()) { return abgelehnt(); }
        daten = *d;

    } else if (praefix == QLatin1String("FA") || praefix == QLatin1String("FB")) {
        const bool a = (praefix == QLatin1String("FA"));
        if (istSetzen) {
            bool ok = false;
            const quint64 hz = suffix.toULongLong(&ok);
            if (!ok) { return abgelehnt(); }
            auto& haken = a ? m_h.setzeVfoA : m_h.setzeVfoB;
            if (!haken) { return abgelehnt(); }
            haken(hz);
            if (a) { m_z.vfoAHz = hz; } else { m_z.vfoBHz = hz; }
            // Nachlesen, nicht blind quittieren: ohne gebundene Scheibe
            // laeuft der Haken ins Leere, und ein Fremdprogramm sendete
            // auf der alten Frequenz weiter. Siehe setzeNachleser().
            if (!hatGewirkt([a, hz](const Zustand& z) {
                    return (a ? z.vfoAHz : z.vfoBHz) == hz;
                })) {
                return abgelehnt();
            }
            return QString();      // gewirkt; Kenwood quittiert nicht
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
            if (!hatGewirkt([k](const Zustand& z) { return z.modeK == k; })) {
                return abgelehnt();
            }
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
            if (!hatGewirkt([v](const Zustand& z) { return z.split == (v == 1); })) {
                return abgelehnt();
            }
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
        // Beim Tasten ist das Nachlesen am wichtigsten: ein TX;, das
        // quittiert wird, ohne dass der Sender angeht, laesst ein
        // Contestprogramm ins Leere rufen -- und beim RX; waere der
        // umgekehrte Fall gefaehrlich (Sender bleibt an, Programm
        // glaubt, er sei aus).
        if (!hatGewirkt([senden](const Zustand& z) { return z.mox == senden; })) {
            return abgelehnt();
        }
        // From Thetis CAT/CATStructs.xml: TX und RX haben nAns=1 --
        // sie quittieren mit dem eigenen Praefix und einer "0".
        daten = QStringLiteral("0");

    } else if (praefix == QLatin1String("PS")) {
        if (istAbfragen) {
            daten = m_z.eingeschaltet ? QStringLiteral("1") : QStringLiteral("0");
        } else if (istSetzen) {
            // PS1 ist ein No-Op (laeuft schon). PS0 wird ABGELEHNT: ein
            // CAT-Anrufer darf diese Station nicht abschalten.
            // Uebernommen aus der zweiten Fassung, mit ihrer Begruendung
            // -- "a CAT client does not get to power this station down".
            if (suffix == QStringLiteral("1")) { return QString(); }
            return abgelehnt();
        } else { return abgelehnt(); }

    } else if (praefix == QLatin1String("SM")) {
        // SM0; und SM2; fragen den Pegel (0 = Hauptempfaenger, 2 =
        // Zweitempfaenger bei Kenwood). Alles andere ist kein SM.
        if (!(suffix == QStringLiteral("0") || suffix == QStringLiteral("2"))) {
            return abgelehnt();
        }
        daten = mitNullen(static_cast<quint64>(sMeterAusDbm(m_z.signalDbm)), 5);

    } else if (praefix == QLatin1String("RT") || praefix == QLatin1String("XT")) {
        // RIT und XIT. Longpath hat keinen Split, und der dokumentierte
        // Ersatz ist XIT (Phase-3F-Entwurf §3) -- ein Contestprogramm,
        // das nach Split greift, findet hier etwas, das wirkt.
        const bool rit = (praefix == QLatin1String("RT"));
        if (istAbfragen) {
            const bool an = rit ? m_z.ritOn : m_z.xitOn;
            daten = an ? QStringLiteral("1") : QStringLiteral("0");
        } else if (istSetzen) {
            if (suffix != QStringLiteral("0") && suffix != QStringLiteral("1")) {
                return abgelehnt();
            }
            const bool wunsch = (suffix == QStringLiteral("1"));
            auto& haken = rit ? m_h.setzeRit : m_h.setzeXit;
            if (!haken) { return abgelehnt(); }
            haken(wunsch);
            if (rit) { m_z.ritOn = wunsch; } else { m_z.xitOn = wunsch; }
            // Nachlesen -- genau hier hat die zweite Fassung am Draht
            // gesehen, dass RT1 still quittierte und das folgende RT
            // weiter RT0 sagte.
            if (!hatGewirkt([rit, wunsch](const Zustand& z) {
                    return (rit ? z.ritOn : z.xitOn) == wunsch;
                })) {
                return abgelehnt();
            }
            return QString();
        } else { return abgelehnt(); }

    } else if (praefix == QLatin1String("RC")) {
        // Ablage auf null. Ist keine der beiden an, gibt es nichts zu
        // loeschen -- und ein stilles "erledigt" sagte dem Programm, die
        // Ablage sei weg, wo keine war.
        if (!istTat) { return abgelehnt(); }
        if (!m_z.ritOn && !m_z.xitOn) { return abgelehnt(); }
        // Welche der beiden Ablagen gemeint ist, entscheidet die Flagge
        // -- wie bei Thetis. Der Haken dafuer muss haengen.
        const bool aufRit = m_z.ritOn;
        auto& haken = aufRit ? m_h.setzeRitHz : m_h.setzeXitHz;
        if (!haken) { return abgelehnt(); }
        haken(0);
        if (aufRit) { m_z.ritHz = 0; } else { m_z.xitHz = 0; }
        if (!hatGewirkt([](const Zustand& z) { return aktiveAblage(z) == 0; })) {
            return abgelehnt();
        }
        return QString();

    } else if (praefix == QLatin1String("RU") || praefix == QLatin1String("RD")) {
        // Ablage um einen Abstimmschritt hoch oder runter.
        if (!istTat) { return abgelehnt(); }
        if (!m_z.ritOn && !m_z.xitOn) { return abgelehnt(); }
        const bool aufRit = m_z.ritOn;
        auto& haken = aufRit ? m_h.setzeRitHz : m_h.setzeXitHz;
        if (!haken) { return abgelehnt(); }
        // Kenwoods eigene Vorgabe, wenn der Schritt unbekannt ist.
        const int schritt = (m_z.stepHz > 0) ? m_z.stepHz : 10;
        const int ziel = aktiveAblage(m_z)
                         + ((praefix == QLatin1String("RU")) ? schritt : -schritt);
        haken(ziel);
        if (aufRit) { m_z.ritHz = ziel; } else { m_z.xitHz = ziel; }
        if (!hatGewirkt([ziel](const Zustand& z) { return aktiveAblage(z) == ziel; })) {
            return abgelehnt();
        }
        return QString();

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
    if (form.nAns > 0 && daten.size() == form.nAns) {
        return praefix + daten + QStringLiteral(";");
    }
    // Falsche Laenge. Thetis gibt hier "O;" (Error3, "processing
    // incomplete"). Das ist kein Formatfehler des Anrufers, sondern
    // einer bei UNS -- wenn der Statussatz je nicht 35 Zeichen hat,
    // soll das auffallen und nicht als gueltige Kurzantwort hinausgehen.
    return QStringLiteral("O;");
}

} // namespace Longpath
