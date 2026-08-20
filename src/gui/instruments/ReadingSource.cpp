// no-port-check: die einzige Thetis-Erwaehnung hier ist ein
// NEGATIVBEFUND. enums.cs:498 wird genannt, um festzuhalten, dass
// Thetis die Kuehlkoerpertemperatur als Fehlerbit fuehrt und eben
// KEINE Skala hat, die man portieren koennte. Uebernommen wird aus
// Thetis in dieser Datei nichts; die Skalen stammen aus dem eigenen
// Baum, die Teilung aus den Entwuerfen des Betreibers. Die Erwaehnung
// steht da, damit die Suche nicht ein zweites Mal gemacht wird.

// =================================================================
// src/gui/instruments/ReadingSource.cpp  (NereusSDR)
// =================================================================
// Siehe ReadingSource.h — eine Liste, nicht eine zweite.
// =================================================================

#include "gui/instruments/ReadingSource.h"

#include "gui/meters/MeterItem.h"     // readingName()
#include "gui/meters/MeterPoller.h"   // MeterBinding

#include <QtGlobal>

#include <algorithm>

namespace Longpath {

namespace {

// ── Die Empfangsskala ────────────────────────────────────────────────

double sigFractionFromDbm(double dbm) { return signalFraction(dbm); }

QString sigFormat(double dbm)
{
    const double s = sUnitsFromDbm(dbm);
    if (s <= 9.0) {
        return QStringLiteral("S%1").arg(qRound(s));
    }
    return QStringLiteral("S9+%1").arg(qRound((s - 9.0) * 10.0));
}

// ── Die Tabelle ──────────────────────────────────────────────────────
//
// Beschriftungen wörtlich aus BaseItemEditor.cpp:252-283 übernommen,
// damit die Umstellung dort keinen einzigen sichtbaren Text ändert.
//
// Skalen nur, wo eine belastbare Zahl im Baum steht. Herkunft je Zahl
// im Kommentar; wo keine steht, trägt der Eintrag keine Skala.

QList<ReadingDescriptor> buildTable()
{
    QList<ReadingDescriptor> t;

    auto plain = [&t](int id, const char* label) {
        ReadingDescriptor d;
        d.bindingId = id;
        d.label = QString::fromLatin1(label);
        t.append(d);
    };

    // ── RX ───────────────────────────────────────────────────────────
    //
    // Die drei Grössen, die auf einer S-Skala stehen. Bereich und
    // Kennlinie aus Thetis MeterManager.cs:22870-22872
    // [v2.10.3.15-5-g852bf0e] — S0 = -127 dBm, S9 = -73 dBm,
    // S9+60 = -13 dBm; siehe ReadingSource.h fuer den Wortlaut und die
    // zwei gewollten Abweichungen. KEINE Schwelle: „Wo es keine
    // Schwelle gibt — Empfangsskala —, gibt es keinen roten Abschnitt"
    // (OE5SOS, 2026-08-17).
    //
    // Max Bin gehört dazu, weil es dieselbe Grösse ist: Thetis rechnet
    // ihm denselben Kalibrieroffset an wie den beiden anderen
    // (console.cs:46881 [v2.10.3.13] gegen :46824 und :46828). Es stand
    // vorher als `plain` ohne Skala hier und war damit für die
    // Instrumente unsichtbar — wählbar war es nur über den
    // Rechtsklick der analogen S-Meter-Anzeige, also über eine zweite
    // Auswahlmechanik neben dieser Liste.
    for (const auto& sig : {
             std::pair<int, const char*>{MeterBinding::SignalPeak,   "RX: Signal Peak"},
             std::pair<int, const char*>{MeterBinding::SignalAvg,    "RX: Signal Avg"},
             std::pair<int, const char*>{MeterBinding::SignalMaxBin, "RX: Signal Max Bin"}}) {
        ReadingDescriptor d;
        d.bindingId  = sig.first;
        d.label      = QString::fromLatin1(sig.second);
        d.unit       = QStringLiteral("dBm");
        d.hasScale   = true;
        d.min        = kS0Dbm;
        d.max        = kSMaxDbm;
        d.threshold  = std::nullopt;
        d.decimals   = 0;
        d.fractionOf = &sigFractionFromDbm;
        d.format     = &sigFormat;
        // Beschriftet jede zweite S-Stufe, feine Striche bei jeder —
        // aus dem Entwurf (zeiger-verfeinert.html, sigCfg).
        d.ticks = {
            {kS0Dbm + 1 * 6.0, QStringLiteral("1")},
            {kS0Dbm + 3 * 6.0, QStringLiteral("3")},
            {kS0Dbm + 5 * 6.0, QStringLiteral("5")},
            {kS0Dbm + 7 * 6.0, QStringLiteral("7")},
            {kS9Dbm,           QStringLiteral("9")},
            {kS9Dbm + 20.0,    QStringLiteral("+20")},
            {kS9Dbm + 40.0,    QStringLiteral("+40")},
            {kS9Dbm + 60.0,    QStringLiteral("+60")},
        };
        d.minorTicks = {
            kS0Dbm + 2 * 6.0, kS0Dbm + 4 * 6.0, kS0Dbm + 6 * 6.0,
            kS0Dbm + 8 * 6.0, kS9Dbm + 10.0, kS9Dbm + 30.0, kS9Dbm + 50.0,
        };
        t.append(d);
    }

    plain(MeterBinding::AdcPeak,      "RX: ADC Peak");
    plain(MeterBinding::AdcAvg,       "RX: ADC Avg");
    plain(MeterBinding::AgcGain,      "RX: AGC Gain");
    plain(MeterBinding::AgcPeak,      "RX: AGC Peak");
    plain(MeterBinding::AgcAvg,       "RX: AGC Avg");
    plain(MeterBinding::PbSnr,        "RX: PB SNR");

    // ── Rauschflur ───────────────────────────────────────────────────
    //
    // OE5SOS, 2026-08-18: „gehört zum Empfang, also ins Instrument als
    // Quelle." Er stand vorher nur als Beschriftung an der analogen
    // Anzeige im Panelkopf.
    //
    // Auf der dBm-ACHSE, nicht auf der S-Skala: ein Rauschflur ist kein
    // Signal, und die S-Stauchung ueber S9 saehe hier aus wie eine
    // Eigenschaft des Rauschens. Ausdruecklich so gewuenscht.
    //
    // Bereich = der Anzeigebereich des Panadapters,
    // PanadapterModel.h:186-187 (Vorgaben aus Thetis): Boden -140 dBm,
    // Decke -40 dBm. Dieselben Grenzen, in denen der Wert entsteht —
    // eine eigene Zahl hier waere eine zweite Behauptung ueber
    // denselben Bereich.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::NoiseFloor;
        d.label     = QStringLiteral("RX: Rauschflur");
        d.unit      = QStringLiteral("dBm");
        d.hasScale  = true;
        d.min       = -140.0;
        d.max       =  -40.0;
        // Keine Schwelle. Ein hoher Rauschflur ist ein schlechter
        // Standort, keine Gefahr — und rot heisst hier Gefahr.
        d.threshold = std::nullopt;
        d.decimals  = 0;
        d.ticks = {
            {-140.0, QStringLiteral("-140")},
            {-120.0, QStringLiteral("-120")},
            {-100.0, QStringLiteral("-100")},
            { -80.0, QStringLiteral("-80")},
            { -60.0, QStringLiteral("-60")},
            { -40.0, QStringLiteral("-40")},
        };
        d.minorTicks = {-130.0, -110.0, -90.0, -70.0, -50.0};
        t.append(d);
    }

    // ── RADE-SNR ─────────────────────────────────────────────────────
    //
    // Der Bereich ist AUS DEM RADE-QUELLTEXT GERECHNET, nicht geraten.
    //
    // third_party/rade/src/rade_ofdm.c:412-424 [@b289102]:
    //     float snr_est = S1 / (2.0f * S2) - 1.0f;
    //     if (snr_est <= 0.0f) snr_est = 0.1f;          // Zeile 413
    //     float snrdB_est = 10.0f * log10f(snr_est);
    //     snrdB_est = (snrdB_est - c_corr) / m_corr;    // 4.1343 / 0.7650
    //     snrdB_3k = snrdB_est + 10log10(Rs*Nc/3000) + 10log10((M+Ncp)/M);
    //
    // mit RADE_FS 8000, RADE_NC 30, RADE_M 160, RADE_NCP 32
    // (rade_dsp.h:59-65 [@b289102]). Die beiden Korrekturterme sind
    // -3.01 dB und +0.79 dB.
    //
    // BODEN: die Klemme bei 0.1 in Zeile 413 ist der kleinste Wert, den
    // der Schaetzer ueberhaupt melden kann. Eingesetzt ergibt das
    // -20.69 dB; abgerundet -21 dB. Ein Instrument, das darunter noch
    // Skala zeigte, zeigte Bereich, den die Quelle nie erreicht.
    //
    // DECKE: +30 dB. Der lineare Schaetzwert +30 dB landet bei
    // +31.6 dB(3k), die Decke liegt also innerhalb dessen, was die
    // Rechnung hergibt.
    //
    // KEINE SCHWELLE, obwohl der Baum eine Grenze kennt: VfoWidget.cpp:930
    // faerbt unter 5 dB gedaempft. Die ist aber UMGEKEHRT — klein ist
    // schlecht —, und `threshold` heisst „darueber ist es eine Warnung".
    // Eine umgekehrte Schwelle waere ein zweiter Begriff mit demselben
    // Namen; sie gehoert in die Beschreibung, nicht in dieses Feld.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::RadeSnr;
        d.label     = QStringLiteral("RX: RADE-SNR");
        d.unit      = QStringLiteral("dB");
        d.hasScale  = true;
        d.min       = -21.0;
        d.max       =  30.0;
        d.threshold = std::nullopt;
        d.decimals  = 0;
        d.ticks = {
            {-20.0, QStringLiteral("-20")},
            {-10.0, QStringLiteral("-10")},
            {  0.0, QStringLiteral("0")},
            { 10.0, QStringLiteral("10")},
            { 20.0, QStringLiteral("20")},
            { 30.0, QStringLiteral("30")},
        };
        d.minorTicks = {-15.0, -5.0, 5.0, 15.0, 25.0};
        t.append(d);
    }

    // ── TX: Vorlaufleistung ──────────────────────────────────────────
    //
    // Bereich 0-120 W aus ItemGroup.cpp:680 (pwrBar->setRange(0,120)),
    // Schwelle 100 W aus TxApplet.h:174 („HGauge 0-120 W, red > 100 W").
    //
    // TxApplet skaliert die Anzeige je SKU nach (TxApplet.h:225). Diese
    // Tabelle trägt die Grundskala; wer sie je SKU nachziehen will,
    // setzt sie am Instrument, nicht hier.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::TxPower;
        d.label     = QStringLiteral("TX: Forward Power");
        d.unit      = QStringLiteral("W");
        d.hasScale  = true;
        d.min       = 0.0;
        d.max       = 120.0;
        d.threshold = 100.0;
        d.decimals  = 0;
        d.ticks = {{0, QStringLiteral("0")},   {30, QStringLiteral("30")},
                   {60, QStringLiteral("60")}, {90, QStringLiteral("90")},
                   {120, QStringLiteral("120")}};
        d.minorTicks = {15, 45, 75, 105};
        t.append(d);
    }

    plain(MeterBinding::TxReversePower, "TX: Reverse Power");

    // ── TX: SWR ──────────────────────────────────────────────────────
    //
    // 1,0-3,0 mit Grenze 2,5 — aus TxApplet.h:175 („HGauge 1.0-3.0,
    // red > 2.5") und deckungsgleich mit dem Entwurf des Betreibers
    // (zeiger-verfeinert.html, swrCfg: min 1, max 3, threshold 2.5).
    //
    // ABWEICHUNG, bewusst nicht stillschweigend übernommen:
    // ItemGroup.cpp:730 setzt denselben Messwert auf 1,0-5,0. Welche
    // der beiden für die Meter-Items gilt, ist hier nicht entschieden —
    // diese Tabelle beschreibt die Skala der INSTRUMENTE, und für die
    // ist der Entwurf die Vorlage.
    {
        ReadingDescriptor d;
        d.bindingId = MeterBinding::TxSwr;
        d.label     = QStringLiteral("TX: SWR");
        d.hasScale  = true;
        d.min       = 1.0;
        d.max       = 3.0;
        d.threshold = 2.5;
        d.decimals  = 2;
        d.ticks = {{1.0, QStringLiteral("1")},   {1.5, QStringLiteral("1.5")},
                   {2.0, QStringLiteral("2")},   {2.5, QStringLiteral("2.5")},
                   {3.0, QStringLiteral("3")}};
        d.minorTicks = {1.25, 1.75, 2.25, 2.75};
        t.append(d);
    }

    plain(MeterBinding::TxMic,          "TX: Mic");
    plain(MeterBinding::TxComp,         "TX: Compressor");
    plain(MeterBinding::TxAlc,          "TX: ALC");
    plain(MeterBinding::TxEq,           "TX: EQ");
    plain(MeterBinding::TxLeveler,      "TX: Leveler");
    plain(MeterBinding::TxLevelerGain,  "TX: Leveler Gain");
    plain(MeterBinding::TxAlcGain,      "TX: ALC Gain");
    plain(MeterBinding::TxAlcGroup,     "TX: ALC Group");
    plain(MeterBinding::TxCfc,          "TX: CFC");
    plain(MeterBinding::TxCfcGain,      "TX: CFC Gain");

    plain(MeterBinding::HwVolts,        "HW: Volts");
    plain(MeterBinding::HwAmps,         "HW: Amps");

    // ── HW: Temperatur — ohne Skala, und das mit Absicht ─────────────
    //
    // Der Betreiber hat sie als eine der vier Quellen für die
    // Instrumente genannt. Sie bekommt trotzdem keine Skala, und das
    // ist nachgesehen und nicht dahingestellt:
    //
    //   Im eigenen Baum steht weder Bereich noch Grenze. RadioStatus
    //   reicht die Zahl durch (setPaTemperature), SystemTile zeigt sie
    //   ohne Zone.
    //
    //   In Thetis auch nicht — nachgesehen am 2026-08-17 gegen
    //   ../Thetis [@852bf0e]. Thetis kennt die Kühlkörpertemperatur
    //   NICHT als Messwert mit Bereich, sondern als FEHLERBIT:
    //   PAstatusIndicatorState.HeatsinkTemperature (enums.cs:498),
    //   ein Flag im PA-Statuswort, das der Verstärker meldet. Es gibt
    //   dort keine Skala zu portieren, weil dort keine gezeichnet
    //   wird.
    //
    // Damit ist die Frage keine Portierungsfrage mehr, sondern eine
    // Angabe: ab welcher Temperatur soll das Instrument warnen. Das
    // steht in keinem Quelltext, sondern im Datenblatt des jeweiligen
    // Geräts — und die Antwort gehört dem Betreiber.
    //
    // Eine erfundene Skala sähe aus wie eine Messung. Sobald Bereich
    // und Grenze belegt sind, sind es drei Zeilen hier, und die Größe
    // steht sofort in der Auswahl der Instrumente. Die Prüfung in
    // tst_reading_source fällt dann um und erinnert daran, die
    // Herkunft dazuzuschreiben.
    plain(MeterBinding::HwTemperature,  "HW: Temperature");

    plain(MeterBinding::RotatorAz,      "Rotator: Azimuth");
    plain(MeterBinding::RotatorEle,     "Rotator: Elevation");

    return t;
}

} // namespace

// ── Empfangsskala ────────────────────────────────────────────────────

double sUnitsFromDbm(double dbm)
{
    if (dbm <= kS9Dbm) {
        return (dbm - kS0Dbm) / 6.0;      // 6 dB je S-Stufe
    }
    return 9.0 + (dbm - kS9Dbm) / 10.0;   // darüber 10 dB je Stufe
}

double signalFraction(double dbm)
{
    const double s = sUnitsFromDbm(dbm);
    const double f = (s <= 9.0)
        ? (s / 9.0) * kS9Fraction
        : kS9Fraction + ((s - 9.0) / 6.0) * (1.0 - kS9Fraction);
    return qBound(0.0, f, 1.0);
}

// ── ReadingDescriptor ────────────────────────────────────────────────

double ReadingDescriptor::fraction(double value) const
{
    if (fractionOf) {
        return qBound(0.0, fractionOf(value), 1.0);
    }
    if (max <= min) { return 0.0; }
    return qBound(0.0, (value - min) / (max - min), 1.0);
}

QString ReadingDescriptor::text(double value) const
{
    if (format) { return format(value); }
    return QString::number(value, 'f', decimals);
}

QString ReadingDescriptor::thetisName() const
{
    return readingName(bindingId);
}

// ── Zugriff ──────────────────────────────────────────────────────────

const QList<ReadingDescriptor>& allReadings()
{
    static const QList<ReadingDescriptor> kTable = buildTable();
    return kTable;
}

const ReadingDescriptor* readingFor(int bindingId)
{
    const auto& t = allReadings();
    auto it = std::find_if(t.constBegin(), t.constEnd(),
                           [bindingId](const ReadingDescriptor& d) {
                               return d.bindingId == bindingId;
                           });
    return (it == t.constEnd()) ? nullptr : &(*it);
}

QList<const ReadingDescriptor*> readingsWithScale()
{
    QList<const ReadingDescriptor*> out;
    for (const ReadingDescriptor& d : allReadings()) {
        if (d.hasScale) { out.append(&d); }
    }
    return out;
}

} // namespace Longpath
