// no-port-check: Longpath-original file. Es gibt nichts zu portieren --
// Thetis kennt auf der Empfangsseite nur den NOTCH (`wdsp/fmd.c`,
// `snotch`) und hat keinen Tondetektor. Das Verfahren hier ist nach
// TIA-603-D gebaut. Der einzige Thetis-Verweis in dieser Datei und in
// der zugehoerigen .cpp betrifft die Liste der 49 Normtoene: die steht
// in der Norm, und `console.cs:236-241` ist als Fundstelle genannt,
// weil ich sie dort gegengelesen habe -- eine Normtabelle, kein Code.

#pragma once

// CTCSS-Tonerkennung fuer den Empfang (Tonsquelch).
//
// Longpath-eigen, kein Port: Thetis kennt auf der Empfangsseite nur den
// NOTCH, der den Subton aus dem Hoerbaren herausfiltert (wdsp/fmd.c,
// `snotch`, SetRXAFMCTCSSFreq/Run) -- es gibt dort nichts, was den Ton
// ERKENNT. Entsprechend gibt es in Thetis auch keinen Tonsquelch, und
// SliceModel::fmCtcssMode kennt zwar "Decode" und "Encode+Decode",
// bewirkt empfangsseitig aber bis heute nichts. Genau diese Luecke
// schliesst diese Klasse.
//
// Verfahren (nach TIA-603-D, dem Standard, der die Subtoene definiert):
//
//   1. Der Subton liegt zwischen 67,0 und 254,1 Hz, also weit unter der
//      Sprache. Ein Goertzel direkt bei 48 kHz braeuchte fuer die noetige
//      Trennschaerfe ein Fenster von zehntausenden Proben. Darum erst
//      dezimieren: Mittelwert ueber 48 Proben ergibt 1 kHz Rate.
//      Davor ein vierpoliger Tiefpass bei 300 Hz, aus zwei Gruenden --
//      beide am Pruefstand gemessen, nicht vermutet:
//        a) Der Mittelwert allein ist ein schwacher (sinc-)Tiefpass. Er
//           laesst Sprachanteile bei 550 und 660 Hz auf 450 und 340 Hz
//           ZURUECKKLAPPEN, mitten ins Messband. Aliasing, das das
//           Verhaeltnis in Punkt 3 verfaelscht.
//        b) Longpath sendet FM mit [-3000, +3000] Hz
//           (`TxChannel.cpp:965`) -- es gibt KEINEN 300-Hz-Hochpass im
//           Sprachweg. Eine tiefe Stimme (Grundton 85..180 Hz) liegt
//           damit mit im Subtonband. Ohne den Tiefpass drueckten ihre
//           Harmonischen die Bandleistung so weit hoch, dass die Sperre
//           bei lauter Sprache mitten im Durchgang zufiel.
//      Der Tiefpass daempft bei 254,1 Hz (dem hoechsten Normton) rund
//      1 dB -- und zwar Ton und Band gleichermassen, das Verhaeltnis
//      bleibt also unberuehrt.
//   2. Bei 1 kHz dann ein Goertzel ueber 250 Proben = 0,25 s. Die
//      Frequenzaufloesung von 4 Hz trennt benachbarte Normtoene
//      (kleinster Abstand 2,3 Hz bei 67,0/69,3) nicht vollstaendig, der
//      Nachbarton faellt aber deutlich ab; fuer das Oeffnen einer
//      Rauschsperre reicht das. Die Norm verlangt Erkennung in unter
//      500 ms -- 250 ms Fenster halten das mit Reserve ein.
//   3. Entschieden wird am VERHAELTNIS der Tonleistung zur Leistung des
//      ganzen Subtonbandes, nicht an einem absoluten Pegel: der Hub des
//      Subtons schwankt von Gegenstelle zu Gegenstelle, das Verhaeltnis
//      nicht. Am Pruefstand gemessen: von Hub 30 % bis 5 % herunter
//      bleibt das Verhaeltnis ueber 0,66, der Pegel geht also nicht ein.
//   3b. Das Verhaeltnis allein genuegt aber NICHT, und das ist gemessen,
//      nicht vermutet: die beiden engsten Normtoene liegen nur 2,3 Hz
//      auseinander (67,0 und 69,3). Das 250-ms-Fenster loest 4 Hz auf,
//      ein Sender auf 69,3 Hz erzeugt im 67,0-Zweig darum noch 0,29 --
//      praktisch dasselbe Verhaeltnis wie ein echter Subton unter
//      Sprache (0,35). Mit einer einzigen Schwelle sind die beiden Faelle
//      nicht zu trennen; eine Schwelle, die den Nachbarton sicher
//      abweist, weist auch den echten Ton ab.
//      Darum laufen DREI Goertzel-Zweige: der Soll-Ton und seine beiden
//      Nachbarn AUS DER NORMLISTE (nur die kommen real vor). Geoeffnet
//      wird erst, wenn der Soll-Zweig den staerkeren Nachbarn um
//      `kNeighbourMargin` uebertrifft. Ein Fremdsender auf dem
//      Nachbarton faellt damit heraus, ohne dass die Schwelle fuer den
//      eigenen Ton steigen muss.
//   4. Hysterese: oeffnen ab `openRatio`, schliessen erst unter
//      `closeRatio`. Ohne sie flattert die Rauschsperre an der Schwelle.
//      Die beiden Schwellen liegen weit auseinander, und das mit Absicht:
//      Oeffnen soll ein sauberer Ton verlangen (ein Durchgang beginnt mit
//      dem Subton, bevor gesprochen wird), Offenbleiben dagegen muss auch
//      durch laute Sprache hindurch halten.
//      Die Schliess-Schwelle liegt bei 0,10 und nicht hoeher: mit 0,18
//      schlug die Sperre an der Grenze viermal in 40 Bloecken um
//      (`sieFlattertNichtAnDerSchwelle`).
//
// Was hier bewusst NICHT drinsteht: ein getraegter Bezugswert und eine
// Haltezeit. Beides war einmal eingebaut, gegen ein Flattern, das ein
// 30-s-Durchgang mit lauter Stimme zu zeigen schien -- bis sich das
// Stimmmodell des Pruefstands als falsch erwies (ein frequenz-
// modulierter Ton, der ueber den Soll-Subton hinwegstreicht, erzeugt
// dort echte Dauertonleistung; das misst keine Sprache, sondern einen
// Suchlauf). Mit einer Stimme, die wie eine Stimme gebaut ist
// (Glottisimpulse mit Jitter), aenderten beide Zusaetze an keinem
// einzigen Prueffall etwas, und sie sind wieder heraus.

#include <QString>
#include <QVector>

#include <array>

namespace Longpath {

/// Die 50 Subtoene nach TIA-603-D, aufsteigend. Dieselbe Liste, die
/// Thetis in `console.cs:236-241 [@852bf0e]` als `CTCSS_array` fuehrt
/// (dort 49 Werte; 69,3 Hz ist in beiden enthalten).
const QVector<double>& ctcssStandardTonesHz();

/// Der naechstgelegene Normton zu `hz`, oder 0.0 wenn `hz` weiter als
/// 1 Hz von jedem Normton entfernt liegt.
double nearestCtcssToneHz(double hz);

class CtcssDetector {
public:
    /// `sampleRate` ist die Rate des hereingegebenen Tons (48 kHz im
    /// ganzen Empfangsweg). `toneHz` ist der gesuchte Subton.
    CtcssDetector(double sampleRate, double toneHz);

    void setToneHz(double toneHz);
    double toneHz() const { return m_toneHz; }

    /// Schwellen des Verhaeltnisses Tonleistung/Bandleistung. Vorgabe
    /// 0,30 zum Oeffnen und 0,10 zum Schliessen -- am Pruefstand gemessen
    /// gegen reinen Ton, Ton unter Rauschen, falschen Ton, und gegen eine
    /// Stimme mit Grundton IM Subtonband (bis hinauf zu einem Sprecher,
    /// dessen Grundton genau auf dem Subton sitzt). 0,30 darf nicht
    /// tiefer: der engste Nachbarton erzeugt im Soll-Zweig 0,29. 0,10
    /// darf nicht hoeher: mit 0,18 flatterte die Sperre an der Grenze.
    void setThresholds(double openRatio, double closeRatio);

    /// Einen Block Ton hineingeben. Liefert true, wenn sich der Zustand
    /// (`detected()`) mit diesem Block geaendert hat.
    bool process(const float* samples, int frames);

    /// Liegt der gesuchte Ton gerade an?
    bool detected() const { return m_detected; }

    /// Das zuletzt gemessene Verhaeltnis -- fuer Anzeige und Pruefstand.
    double lastRatio() const { return m_lastRatio; }

    /// Wieviel der Soll-Zweig den staerkeren Nachbarn zuletzt uebertroffen
    /// hat. Unter `kNeighbourMargin` sitzt die Gegenstelle auf einem
    /// Nachbarton, nicht auf dem eingestellten. Nur fuer Anzeige und
    /// Pruefstand.
    double lastNeighbourMargin() const { return m_lastNeighbourMargin; }

    /// Alles zuruecksetzen (Bandwechsel, Modusumschaltung).
    void reset();

    /// Laenge des Messfensters in Sekunden.
    static constexpr double kWindowSeconds = 0.25;
    /// Rate nach der Dezimation.
    static constexpr double kWorkRateHz = 1000.0;
    /// Grenzfrequenz des Tiefpasses vor der Dezimation.
    static constexpr double kLowpassHz = 300.0;
    /// Wieviel der Soll-Zweig den staerkeren Nachbarn uebertreffen muss.
    static constexpr double kNeighbourMargin = 1.2;

private:
    /// Ein Biquad in Direktform II (transponiert). Zwei davon in Reihe
    /// ergeben den vierpoligen Butterworth-Tiefpass.
    struct Biquad {
        double b0{1.0}, b1{0.0}, b2{0.0}, a1{0.0}, a2{0.0};
        double z1{0.0}, z2{0.0};

        double step(double x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
        void clear() { z1 = z2 = 0.0; }
    };

    void rebuild();
    /// Setzt `bq` auf einen Tiefpass bei `cutoffHz` mit Guete `q`.
    static void designLowpass(Biquad& bq, double cutoffHz, double sampleRate, double q);

    double m_sampleRate;
    double m_toneHz;
    double m_openRatio{0.30};
    double m_closeRatio{0.10};

    // Vierpoliger Butterworth als zwei Biquads (Gueten 0,5412 und
    // 1,3065 -- die Butterworth-Pole 4. Ordnung).
    Biquad m_lp1;
    Biquad m_lp2;

    int    m_decimation{48};     ///< sampleRate / kWorkRateHz
    int    m_windowFrames{250};  ///< kWindowSeconds * kWorkRateHz

    // Dezimation: Summe und Zaehler des laufenden Mittelwerts.
    double m_decimSum{0.0};
    int    m_decimCount{0};

    /// Ein Goertzel-Zweig auf einer festen Frequenz.
    struct Goertzel {
        double coeff{0.0};
        double s1{0.0}, s2{0.0};

        void step(double x)
        {
            const double s0 = x + coeff * s1 - s2;
            s2 = s1;
            s1 = s0;
        }
        double power() const { return s1 * s1 + s2 * s2 - coeff * s1 * s2; }
        void clear() { s1 = s2 = 0.0; }
    };

    /// Setzt `g` auf `hz`, bezogen auf `m_windowFrames` bei `workRate`.
    void tuneGoertzel(Goertzel& g, double hz, double workRate) const;

    // Soll-Ton und seine beiden Nachbarn aus der Normliste. Ein Nachbar
    // mit Frequenz 0 ist keiner (am Rand der Liste) und geht nicht ein.
    Goertzel m_target;
    Goertzel m_below;
    Goertzel m_above;
    double   m_belowHz{0.0};
    double   m_aboveHz{0.0};

    double m_bandPower{0.0};   ///< Summe der Quadrate im Fenster
    int    m_filled{0};

    bool   m_detected{false};
    double m_lastRatio{0.0};
    double m_lastNeighbourMargin{0.0};
};

} // namespace Longpath
