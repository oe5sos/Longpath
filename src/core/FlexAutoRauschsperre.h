// no-port-check: Longpath-original, no Thetis logic (Thetis drives no Flex squelch).
//
// =================================================================
// src/core/FlexAutoRauschsperre.h  (Longpath)
// =================================================================
//
// ── Auto-SQL fuer die Flex (2026-11-08) ─────────────────────────────
//
// Betreiber: „sql muss schon fast bei 100 prozent sein, um das rauschen
// weg zu bekommen". Kein Uebertragungsfehler -- Longpath schickt den
// Regler unveraendert. Die Maschine liest `squelch_level` als ABSOLUTE
// Schwelle:
//
//   Stufe = dBm + 160          (Stufe 0 = -160 dBm, Stufe 100 = -60 dBm)
//
// From AetherSDR SpectrumWidget.cpp:4642-4646 [@d58e2b8a] --
// `kSqlMinDbm = -160.0f`, `level = clamp(targetDbm - kSqlMinDbm, 1, 100)`;
// dieselbe Umrechnung malt dort die Squelch-Linie (:4556).
//
// Gemessen in Martins Protokoll (longpath-20261108-200948.log, 40 m LSB,
// abends): der Pegel der Scheibe (`LEVEL`, src=SLC) lag in 47 Proben bei
// p10 -90,2 / p50 -88,2 / p75 -83,2 dBm. Von Hand landete er bei Stufe
// 83..98, also -77..-62 dBm. Drei Viertel des Reglers liegen auf
// Kurzwelle unter jedem Rauschen.
//
// Darum, nach AetherSDRs Vorbild (RxApplet.cpp:962-1002 [@d58e2b8a],
// Knopf Aus -> SQL -> AUTO): die Schwelle folgt dem gemessenen Rauschen,
// der Regler stellt nur noch den ABSTAND darueber ein.
//
// ── Zwei Longpath-Abweichungen, beide aus der Messung ──────────────
//
// 1. Bezug ist der Pegel der Scheibe, nicht der Rauschboden des Bildes.
//    AetherSDR schaetzt aus den Panadapter-Punkten (Mittel der Punkte
//    unter dem Mittel). Dieselbe Stunde ergab dort -101 dBm JE PUNKT
//    (86 Hz breit) -- der Durchlass von 2,7 kHz rauscht 15 dB hoeher, bei
//    -88 dBm. Mit AetherSDRs groesstem Abstand (20 dB) laege die
//    Schwelle bei -81 dBm, also im Rauschen: die Sperre bliebe offen.
//    Verglichen wird in der Maschine mit dem Pegel der Scheibe (am
//    2026-10-22 standen LEVEL und SQUELCH in einer Zeile auf demselben
//    Wert, RadioModel.cpp im Messwertstrom). Also wird der genommen.
//
// 2. Schaetzer: das untere Zehntel der letzten 20 Sekunden. Steht eine
//    Station im Durchlass, hebt sie den Mittelwert, aber nicht die
//    Sprechpausen -- und solange in 20 s mindestens 10 % Pause liegen,
//    bleibt der Bezug das Rauschen. Faellt das Rauschen, folgt die
//    Schwelle in zwei Sekunden; steigt es, erst nach rund 18. Diese
//    Schieflage ist Absicht: zu tief heisst kurz Rauschen hoeren, zu
//    hoch hiesse die Station verlieren, die man gerade hoert.
//    Ein Traeger ohne Pause ueber 18 s zieht die Schwelle ueber sich
//    selbst -- das ist die Grenze dieses Verfahrens.
//
// Vorgabe 20 dB: mit dem p10 von oben (-90,2 dBm) ergibt das Stufe 90,
// genau dort, wo der Betreiber den Regler am 2026-11-08 von Hand
// hingestellt hat. Bereich 3..40 statt AetherSDRs 5..20, weil der
// Bezug ein anderer ist (siehe 1.) und die Messung oberhalb von 20 lag.

#pragma once

#include <QtGlobal>

#include <deque>
#include <optional>
#include <utility>

namespace Longpath {

class FlexAutoRauschsperre {
public:
    // From AetherSDR SpectrumWidget.cpp:4642 [@d58e2b8a] -- kSqlMinDbm.
    static constexpr double kStufeNullDbm   = -160.0;
    static constexpr int    kMargeMinDb     = 3;
    static constexpr int    kMargeMaxDb     = 40;
    static constexpr int    kMargeVorgabeDb = 20;

    static constexpr qint64 kFensterMs      = 20000;  // 20 s Gedaechtnis
    static constexpr double kAnteil         = 0.10;   // unteres Zehntel
    static constexpr int    kMindestProben  = 10;     // ~1 s bei 10/s

    // Senden: bei einem Sprung von zwei Stufen nach einer Sekunde, bei
    // einer Stufe erst nach zehn -- sonst kippt der Wert an der Grenze
    // zweier Stufen jede Sekunde hin und her, und jeder Kipp ist ein
    // Befehl an die Maschine.
    static constexpr int    kTotbandStufen  = 2;
    static constexpr qint64 kMindestAbstandMs = 1000;
    static constexpr qint64 kNachziehenMs   = 10000;

    // AetherSDR klemmt auf 1..100 (:4645); 0 waere „immer offen".
    static int    stufeFuerDbm(double dbm);
    static double dbmFuerStufe(int stufe);

    // Abstand in AppSettings ("FlexAutoSqlMarginDb"), eine Zahl fuer
    // alle Baender wie AetherSDRs "AutoSqlMarginDb".
    static int  ladeMarge();
    static void speichereMarge(int db);

    void setMarge(int db);
    int  marge() const { return m_marge; }

    // Ein Pegel der Scheibe, in dBm, zum Zeitpunkt jetztMs.
    void messung(double pegelDbm, qint64 jetztMs);

    // Die Stufe, die JETZT hinaus soll -- oder nichts. Wer sie nimmt,
    // muss sie senden: sie gilt danach als gesendet.
    std::optional<int> faellig(qint64 jetztMs);

    // Naechste faellige Stufe sofort, ohne Totband und Abstand: beim
    // Einschalten und wenn der Abstand verstellt wurde.
    void neuSenden() { m_sofort = true; }

    // Neu messen: anderes Band, andere Flanken, andere Betriebsart. Die
    // zuletzt gesendete Stufe bleibt an der Maschine stehen, bis wieder
    // genug Proben da sind.
    void zuruecksetzen();

    std::optional<double> rauschDbm() const { return m_rausch; }
    std::optional<int>    stufe() const;        // was jetzt richtig waere
    int  gesendeteStufe() const { return m_gesendet; }  // -1 = noch keine
    int  probenAnzahl() const { return static_cast<int>(m_proben.size()); }

private:
    std::deque<std::pair<qint64, double>> m_proben;
    std::optional<double> m_rausch;
    int    m_marge{kMargeVorgabeDb};
    int    m_gesendet{-1};
    qint64 m_gesendetUm{0};
    bool   m_sofort{true};
};

} // namespace Longpath
