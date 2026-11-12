// =================================================================
// src/core/Ts590Cat.cpp  (Longpath)
// =================================================================
//
// no-port-check: Longpath-original, siehe Ts590Cat.h.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-07 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "Ts590Cat.h"

#include <QString>

#include <algorithm>
#include <array>
#include <cstdlib>

namespace Longpath::Ts590Cat {

namespace {

// Kenwoods Tabellen fuer SSB, FM und FM Data.
constexpr std::array<int, 14> kHighCut = {1000, 1200, 1400, 1600, 1800, 2000, 2200,
                                          2400, 2600, 2800, 3000, 3400, 4000, 5000};
constexpr std::array<int, 12> kLowCut  = {0, 50, 100, 200, 300, 400, 500, 600,
                                          700, 800, 900, 1000};
// Am TS-590S nachgelesen (2026-11-07): FW nimmt jeden dieser Werte in CW an.
constexpr std::array<int, 14> kCwBreite = {50, 80, 100, 150, 200, 250, 300,
                                           400, 500, 600, 1000, 1500, 2000, 2500};

template <std::size_t N>
int naechster(const std::array<int, N>& tabelle, int hz)
{
    int beste = tabelle.front();
    for (int w : tabelle) {
        if (std::abs(w - hz) < std::abs(beste - hz)) { beste = w; }
    }
    return beste;
}

int zahl(const QByteArray& a, int ab, int laenge)
{
    bool ok = false;
    const int wert = a.mid(ab, laenge).toInt(&ok);
    return ok ? wert : -1;
}

} // namespace

QList<QByteArray> abfragen()
{
    return {QByteArrayLiteral("AN;"), QByteArrayLiteral("PA;"), QByteArrayLiteral("RA;"),
            QByteArrayLiteral("RG;"), QByteArrayLiteral("GC;"), QByteArrayLiteral("NB;"),
            QByteArrayLiteral("NR;"), QByteArrayLiteral("SL;"), QByteArrayLiteral("SH;"),
            QByteArrayLiteral("PC;"), QByteArrayLiteral("FW;"), QByteArrayLiteral("FL;"),
            QByteArrayLiteral("IF;"), QByteArrayLiteral("NT;"), QByteArrayLiteral("NL;"),
            QByteArrayLiteral("RL;")};
}

QByteArray abfrageMeter() { return QByteArrayLiteral("SM0;"); }
QByteArray abfrageSwr()   { return QByteArrayLiteral("RM;"); }

bool leseAntwort(QByteArray a, Zustand& z)
{
    a = a.trimmed();
    while (!a.isEmpty() && (a.back() == '\0' || a.back() == ';')) { a.chop(1); }
    if (a.size() < 3) { return false; }

    const QByteArray k = a.left(2);
    if (k == "AN" && a.size() >= 5) {
        const int ant = zahl(a, 2, 1), rx = zahl(a, 3, 1), drv = zahl(a, 4, 1);
        if (ant < 0 || rx < 0 || drv < 0) { return false; }
        z.antenne = ant + 1;
        z.rxAnt   = rx;
        z.drive   = drv;
        return true;
    }
    if (k == "PA") { z.vorverstaerker = zahl(a, 2, 1); return z.vorverstaerker >= 0; }
    if (k == "RA") { z.daempfung = zahl(a, 2, 2); return z.daempfung >= 0; }
    if (k == "RG") { z.rfGain = zahl(a, 2, 3); return z.rfGain >= 0; }
    if (k == "GC") { z.agc = zahl(a, 2, 1); return z.agc >= 0; }
    if (k == "NB") { z.nb = zahl(a, 2, 1); return z.nb >= 0; }
    if (k == "NR") { z.nr = zahl(a, 2, 1); return z.nr >= 0; }
    if (k == "PC") { z.leistungW = zahl(a, 2, 3); return z.leistungW >= 0; }
    if (k == "SM" && a.size() >= 7) { z.meterPunkte = zahl(a, 3, 4); return z.meterPunkte >= 0; }
    if (k == "RM" && a.size() >= 7) {
        // Das Geraet meldet nur das Instrument, das an ihm gewaehlt ist:
        // 1 SWR, 2 Kompression, 3 ALC.
        const int wert = zahl(a, 3, 4);
        if (wert < 0) { return false; }
        if (a.at(2) == '1') { z.swrPunkte = wert; }
        if (a.at(2) == '3') { z.alcPunkte = wert; }
        return true;
    }
    // IF: an Stelle 18 das Vorzeichen (Leerzeichen heisst +), 19-22 der
    // Versatz in Hz, 23 RIT an. Am TS-590S gemessen (2026-11-12).
    if (k == "IF" && a.size() >= 24) {
        const int hz = zahl(a, 19, 4);
        const int an = zahl(a, 23, 1);
        if (hz < 0 || an < 0) { return false; }
        z.ritHz = a.at(18) == '-' ? -hz : hz;
        z.ritAn = an;
        return true;
    }
    if (k == "NT") { z.notch = zahl(a, 2, 1); return z.notch >= 0; }
    if (k == "NL") { z.nbPegel = zahl(a, 2, 3); return z.nbPegel >= 0; }
    if (k == "RL") { z.nrPegel = zahl(a, 2, 2); return z.nrPegel >= 0; }
    if (k == "SL") {
        const int i = zahl(a, 2, 2);
        if (i < 0 || i >= int(kLowCut.size())) { return false; }
        z.lowCutHz = kLowCut[std::size_t(i)];
        return true;
    }
    if (k == "SH") {
        const int i = zahl(a, 2, 2);
        if (i < 0 || i >= int(kHighCut.size())) { return false; }
        z.highCutHz = kHighCut[std::size_t(i)];
        return true;
    }
    if (k == "FW") {
        const int hz = zahl(a, 2, 4);
        if (hz <= 0) { return false; }
        z.cwBreiteHz = hz;
        return true;
    }
    if (k == "FL") {
        const int nummer = zahl(a, 2, 1);
        if (nummer != 1 && nummer != 2) { return false; }
        z.filter = nummer;
        return true;
    }
    return false;
}

double punkteZuDbm(int punkte)
{
    const int p = std::clamp(punkte, 0, 30);
    if (p <= 15) { return -73.0 - (15 - p) * 3.6; }
    return -73.0 + (p - 15) * 4.0;
}

int naechsterLowCut(int hz)  { return naechster(kLowCut, hz); }
int naechsterHighCut(int hz) { return naechster(kHighCut, hz); }
int naechsteCwBreite(int hz) { return naechster(kCwBreite, hz); }

QByteArray stelleVorverstaerker(bool an)
{
    return an ? QByteArrayLiteral("L PREAMP 12\n") : QByteArrayLiteral("L PREAMP 0\n");
}

QByteArray stelleDaempfung(bool an)
{
    return an ? QByteArrayLiteral("L ATT 12\n") : QByteArrayLiteral("L ATT 0\n");
}

QByteArray stelleRfGain(int wert)
{
    const double teil = std::clamp(wert, 0, 255) / 255.0;
    return QStringLiteral("L RF %1\n").arg(teil, 0, 'f', 3).toLatin1();
}

QByteArray stelleAgc(Agc agc)
{
    // Nicht `L AGC`: Hamlib 4.7.2 meldet dort RPRT 0, und das Geraet bleibt
    // stehen; am TS-590S gemessen. `W ... 0` schickt roh und wartet auf
    // nichts, `w` dagegen liefe 400 ms in die Zeitgrenze von rigctld.
    switch (agc) {
    case Agc::Aus:     return QByteArrayLiteral("W GC0; 0\n");
    case Agc::Langsam: return QByteArrayLiteral("W GC1; 0\n");
    case Agc::Schnell: return QByteArrayLiteral("W GC2; 0\n");
    }
    return {};
}

QByteArray stelleNb(bool an)
{
    return an ? QByteArrayLiteral("U NB 1\n") : QByteArrayLiteral("U NB 0\n");
}

QByteArray stelleNr(bool an)
{
    return an ? QByteArrayLiteral("U NR 1\n") : QByteArrayLiteral("U NR 0\n");
}

// Nicht `L SLOPE_...`: das lehnt Hamlib 4.7.2 in LSB mit RPRT -1 ab, am
// TS-590S gemessen. Die Geraetebefehle wirken in beiden Seitenbaendern.
QByteArray stelleLowCut(int hz)
{
    const int wert = naechsterLowCut(hz);
    const auto i = std::distance(kLowCut.begin(), std::find(kLowCut.begin(), kLowCut.end(), wert));
    return QStringLiteral("W SL%1; 0\n").arg(i, 2, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stelleHighCut(int hz)
{
    const int wert = naechsterHighCut(hz);
    const auto i = std::distance(kHighCut.begin(), std::find(kHighCut.begin(), kHighCut.end(), wert));
    return QStringLiteral("W SH%1; 0\n").arg(i, 2, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stelleCwBreite(int hz)
{
    return QStringLiteral("W FW%1; 0\n")
        .arg(naechsteCwBreite(hz), 4, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stelleRit(bool an)
{
    return an ? QByteArrayLiteral("W RT1; 0\n") : QByteArrayLiteral("W RT0; 0\n");
}

QByteArray stelleRitSchritt(int hz)
{
    const int betrag = std::clamp(std::abs(hz), 1, 9999);
    return QStringLiteral("W %1%2; 0\n")
        .arg(hz < 0 ? QStringLiteral("RD") : QStringLiteral("RU"))
        .arg(betrag, 5, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stelleRitNull() { return QByteArrayLiteral("W RC; 0\n"); }

QByteArray stelleNotch(bool an)
{
    return an ? QByteArrayLiteral("W NT10; 0\n") : QByteArrayLiteral("W NT00; 0\n");
}

QByteArray stelleNbPegel(int pegel)
{
    return QStringLiteral("W NL%1; 0\n").arg(std::clamp(pegel, 1, 10), 3, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stelleNrPegel(int pegel)
{
    return QStringLiteral("W RL%1; 0\n").arg(std::clamp(pegel, 1, 10), 2, 10, QLatin1Char('0')).toLatin1();
}

QByteArray stelleFilter(int filter)
{
    return filter == 1 ? QByteArrayLiteral("W FL1; 0\n") : QByteArrayLiteral("W FL2; 0\n");
}

} // namespace Longpath::Ts590Cat
