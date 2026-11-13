#pragma once
// =================================================================
// src/core/Ft8Uhrversatz.h  (Longpath)
// =================================================================
//
// Longpath-original. Kein Port: WSJT-X zeigt DT je Zeile an und
// urteilt nicht; JTDX zeigt einen laufenden Mittelwert. Beides
// ueberlaesst dem Betreiber das Rechnen. Hier wird gerechnet.
//
// ── Warum es das geben muss ─────────────────────────────────────────
//
// FT8 steht und faellt mit der Uhr. Das Zeitfenster ist 15 s lang,
// die Nachricht selbst 12,64 s; der Dekoder sucht den Anfang nur in
// einem schmalen Band um die Null (ft8_lib `decode.c:205`: -10 bis
// +19 Symbole zu je 0,16 s, also -1,60 s bis +3,04 s). Bei FT4 sind
// es dieselben Symbolzahlen bei 0,048 s Symboldauer -- nur noch
// -0,48 s bis +0,91 s.
//
// Geht die Uhr des Rechners falsch, faellt der Empfang NICHT auf
// einmal aus. Er wird langsam schlechter: erst die schwachen Funde,
// dann die halben Perioden, zuletzt gar nichts. Von innen sieht das
// aus wie ein totes Band. Genau diese Verwechslung soll die Anzeige
// verhindern.
//
// ── Warum Median und nicht Mittelwert ───────────────────────────────
//
// Eine einzelne Gegenstation kann selbst eine schlechte Uhr haben --
// das kommt an jedem Abend vor und sagt ueber die eigene nichts. Ein
// Mittelwert ueber vier Funde, von denen einer bei +2,5 s liegt, ist
// unbrauchbar. Darum zweimal Median: einmal ueber die Funde EINER
// Periode, einmal ueber die Perioden. Was beide ueberlebt, ist der
// gemeinsame Versatz -- und der ist der eigene.
//
// Eine Periode zaehlt erst ab zwei Funden mit (`kMindestFunde`), und
// ein Urteil gibt es erst ab drei gezaehlten Perioden
// (`kMindestPerioden`). Lieber eine Minute lang nichts sagen als
// einmal falsch warnen.
//
// ── Was hier NICHT passiert ─────────────────────────────────────────
//
// Sie stellt keine Uhr. Das ist Sache des Betriebssystems, und ein
// Programm, das daran dreht, ist ein Programm, dem man nicht mehr
// traut. Sie sagt nur, dass etwas nicht stimmt.
//
// Modification history (Longpath):
//   2026-11-13 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <deque>
#include <vector>

namespace Longpath {

class Ft8Uhrversatz
{
public:
    enum class Lage {
        Unbekannt,   ///< noch zu wenig gehoert
        Ok,          ///< der Versatz ist unauffaellig
        Auffaellig,  ///< es wird eng, aber es laeuft noch
        Schlecht     ///< so gehen Funde verloren
    };

    /// FT4 hat dieselben Symbolzahlen bei einem Drittel Symboldauer,
    /// also ein Drittel Spielraum. Die Schwellen muessen mitgehen.
    void setzeFt4(bool ft4);
    bool istFt4() const { return m_ft4; }

    /// Ein Fund der laufenden Periode.
    void merke(double dtSek);

    /// Die laufende Periode abschliessen. Hat sie weniger als
    /// `kMindestFunde` Funde gebracht, zaehlt sie nicht mit -- ein
    /// einzelner Fund ist eine Stimme, kein Chor.
    void periodeFertig();

    /// Alles vergessen. Beim Starten, beim Anhalten und beim Wechsel
    /// zwischen FT8 und FT4: die alten Perioden gehoeren dann zu
    /// einem anderen Lauf.
    void leere();

    bool   hatUrteil()  const { return m_perioden.size() >= kMindestPerioden; }
    /// Median der Periodenmediane. Ohne Urteil 0.
    double versatzSek() const;
    Lage   lage()       const;

    int periodenGezaehlt() const { return int(m_perioden.size()); }

    // ── Die Zahlen ──────────────────────────────────────────────
    static constexpr std::size_t kMindestPerioden = 3;
    static constexpr std::size_t kPerioden        = 8;
    static constexpr std::size_t kMindestFunde    = 2;

    /// FT8: bis 0,80 s ist alles in Ordnung (die uebliche Mischung aus
    /// Laufzeit und Puffer liegt bei +0,1 bis +0,5 s); ab 1,50 s
    /// faellt der Anfang aus dem Suchfenster.
    static constexpr double kAuffaelligFt8 = 0.80;
    static constexpr double kSchlechtFt8   = 1.50;
    /// FT4: ein Drittel davon, abgerundet auf runde Zahlen.
    static constexpr double kAuffaelligFt4 = 0.25;
    static constexpr double kSchlechtFt4   = 0.50;

private:
    static double median(std::vector<double> werte);

    std::vector<double> m_laufend;
    std::deque<double>  m_perioden;
    bool                m_ft4{false};
};

}  // namespace Longpath
