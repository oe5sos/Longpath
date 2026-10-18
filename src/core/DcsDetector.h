// no-port-check: Longpath-original file. Weder Thetis noch WDSP kennen
// DCS, in keiner Richtung -- es gibt nichts zu portieren. Gebaut nach
// der oeffentlichen Beschreibung des Digital Coded Squelch; die
// Golay-Arithmetik steht in DcsCode.h.

#pragma once

// DCS-Erkennung fuer den Empfang.
//
// Ein DCS-Sender legt unter die Sprache einen fortlaufenden Datenstrom:
// immer dasselbe 23-Bit-Wort, mit 134,4 bit/s, also 171 ms je Wort.
// Es gibt keine Synchronbits.
//
// Der Bau folgt EINER Entscheidung, und die ist gemessen, nicht
// geraten:
//
//   Der Detektor dekodiert NICHT und vergleicht dann -- er erzeugt das
//   erwartete Wort und korreliert den Bitstrom dagegen.
//
// Der Grund: Golay(23,12) ist zyklisch, jede ringfoermige Verschiebung
// eines gueltigen Codewortes ist wieder ein gueltiges Codewort. Beim
// Messen kamen von 22 Verschiebungen alle 22 durch die Pruefsumme, und
// zwei sogar durch die drei festen Bits. Wer "dekodieren, dann
// vergleichen" baut, liest also je nach Phasenlage irgendeinen Code --
// und weil der Datenstrom endlos periodisch ist, bleibt auch eine
// falsche Lage stabil. Die Pruefsumme taugt nicht zum Synchronisieren.
//
// Gegen das erwartete Wort korreliert, sieht es ganz anders aus
// (ueber alle 104 x 104 x 23 Paarungen gemessen):
//
//   eigener Code, richtige Phase   23 von 23 Bits
//   eigener Code, falsche Phase    hoechstens 15
//   FREMDER Code, beste Phase      hoechstens 16
//
// Mit der Schwelle 20 von 23 bleiben vier Bit Reserve -- und dieselbe
// Schwelle erlaubt bis zu drei falsche Bits, was zur Korrekturleistung
// von Golay(23,12) passt.
//
// Die Schwelle allein genuegt aber NICHT, und das hat der Pruefstand
// erzwungen: reines Rauschen riss die Sperre in 10 von 15 Laeufen auf.
// Die Rechnung dazu ist eindeutig. Bei 23 zufaelligen Bits trifft eine
// Lage mit Wahrscheinlichkeit (C(23,20)+C(23,21)+C(23,22)+C(23,23))
// / 2^23 = 1 zu 4096. Das klingt selten, aber bei 134,4 Bit je Sekunde
// kommt die Gelegenheit oft genug: in vier Sekunden ueber fuenfhundert
// Mal, und damit ist der Fehltreffer so gut wie sicher.
//
// Daraus folgen zwei Dinge, die diesen Detektor ausmachen:
//
//   * KEINE Schleife ueber die 23 Lagen. Wer bei jedem Bit prueft,
//     durchlaeuft die Phasen ohnehin nacheinander -- die Schleife
//     vervielfacht nur die Gelegenheiten zum Fehltreffer.
//   * Ein einzelner Treffer oeffnet nicht. Ein echter Sender wiederholt
//     das Wort endlos, der naechste Treffer muss also GENAU 23 Bit
//     spaeter kommen. Erst `kRequiredWords` solche Treffer in Folge
//     oeffnen die Sperre. Aus 1 zu 4096 wird damit 1 zu 16 Millionen je
//     Gelegenheit.
//
// Polaritaet: Geraete fuehren jeden Code als N und I ("023N", "023I").
// Invertiert heisst, jedes Bit ist gekippt. Der Detektor prueft beide,
// wenn `DcsPolarity::Any` eingestellt ist.

#include "core/DcsCode.h"

#include <cstdint>

namespace Longpath {

class DcsDetector {
public:
    /// `sampleRate` ist die Rate des hereingegebenen Basisbands
    /// (48 kHz im ganzen Empfangsweg), `oktal` der gesuchte Code.
    DcsDetector(double sampleRate, int oktal, DcsPolarity polarity = DcsPolarity::Normal);

    void setCode(int oktal, DcsPolarity polarity);
    int  code() const { return m_oktal; }
    DcsPolarity polarity() const { return m_polarity; }

    /// Ab wievielen der 23 Bits das Wort als erkannt gilt. Vorgabe 20 --
    /// darunter kaeme ein fremder Code durch (gemessen: hoechstens 16),
    /// darueber wird es gegen Bitfehler unnoetig streng.
    void setThresholdBits(int bits);
    int  thresholdBits() const { return m_schwelle; }

    /// Einen Block Basisband hineingeben. Liefert true, wenn sich
    /// `detected()` mit diesem Block geaendert hat.
    bool process(const float* samples, int frames);

    bool detected() const { return m_detected; }
    /// Die beste Uebereinstimmung im letzten Wortfenster, 0..23.
    int  lastAgreement() const { return m_lastAgreement; }

    void reset();

    /// Wieviele Wortlaengen ohne Treffer vergehen duerfen, bevor die
    /// Sperre zufaellt. Ein Wort sind 171 ms; zwei geben der Erkennung
    /// Luft, ohne dass der Nachlauf am Durchgangsende stoert.
    static constexpr int kHoldWords = 2;

    /// Wieviele Treffer im Wortraster hintereinander die Sperre oeffnen.
    /// Zwei kosten 342 ms Erkennungszeit und druecken den Fehltreffer
    /// aus reinem Rauschen von 1 zu 4096 auf 1 zu 16 Millionen.
    static constexpr int kRequiredWords = 2;

private:
    void rebuild();

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
    static void designLowpass(Biquad& bq, double cutoffHz, double sampleRate, double q);

    double m_sampleRate;
    int    m_oktal;
    DcsPolarity m_polarity;
    uint32_t m_erwartet{0};
    uint32_t m_erwartetInvers{0};
    int    m_schwelle{20};

    // Tiefpass: der Datenstrom liegt unter 134 Hz, 300 Hz laesst ihn
    // ganz durch und haelt die Sprache draussen.
    Biquad m_lp1;
    Biquad m_lp2;

    // Gleichanteil: ein NRZ-Strom ist nicht ausgeglichen, ohne
    // Nachfuehrung wandert die Entscheidungsschwelle mit dem Pegel.
    double m_dc{0.0};
    double m_dcAlpha{0.0};

    // Bittakt als Phasenzaehler: 134,4 bit/s passen in kein ganzzahliges
    // Verhaeltnis zu 48 kHz (357,14 Proben je Bit), darum ein Zaehler
    // statt eines festen Teilers.
    double m_phase{0.0};
    double m_phaseStep{0.0};
    double m_bitSumme{0.0};
    int    m_bitProben{0};
    double m_vorigeProbe{0.0};

    uint32_t m_schieberegister{0};
    int    m_bitsGesehen{0};
    /// Fortlaufende Bitnummer, um den Abstand zweier Treffer zu messen.
    long   m_bitZaehler{0};
    /// Bitnummer des letzten Treffers; -1 heisst "noch keiner".
    long   m_letzterTreffer{-1};
    /// Wieviele Treffer bisher im Wortraster aufeinanderfolgten.
    int    m_folgeTreffer{0};

    bool   m_detected{false};
    int    m_lastAgreement{0};
};

} // namespace Longpath
