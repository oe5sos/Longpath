#pragma once

// =================================================================
// src/core/safety/FlexAbstimmRegler.h  (Longpath)
// =================================================================
//
// Betreiber 2026-10-31: „tunen will ich grundsaetzlich mit einem
// watt" — und, nachdem gemessen war, was wirklich herauskam:
// „ja, nachregeln."
//
// ── Was am 2026-10-31 gemessen wurde ────────────────────────────────
//
// Die Messwerttabelle der FLEX-8400, direkt von 172.30.30.12:4992
// gelesen:
//
//   FWDPWR  unit=dBm  low=0.0  hi=53.0
//
// Die Einheit ist also vom Geraet selbst erklaert, und unsere
// Umrechnung (roh/128, dann W = 10^((dBm-30)/10)) trifft den
// erklaerten Bereich. Die gemessenen 22,7…24,1 W waren damit ECHT --
// bei `tunepower=10`.
//
// Daraus folgt: `tunepower` ist ein PROZENTWERT auf einer nicht
// linearen Kurve. Zehn Prozent sind nicht zehn Watt. Wer ein Watt
// will, kann die Zahl nicht einstellen, sondern muss das Ergebnis
// gegenlesen.
//
// ── Die Regel, die diesen Regler sicher macht ───────────────────────
//
// WAEHREND eines laufenden Traegers geht es nur HINUNTER. Niemals
// hinauf. Ein Regler, der am Sender aufdreht, waehrend die Antenne
// schon haengt, ist kein Schutz, sondern ein zweites Risiko -- und
// hier haengt ein ACOM in der Leitung.
//
// Nach oben lernt er nur ZWISCHEN zwei Traegern, in kleinen Schritten
// und nie ueber die Decke, die der Betreiber gesetzt hat. So naehert
// er sich dem Ziel von unten an.
//
// ── Was er NICHT kann, und das gehoert gesagt ───────────────────────
//
// Die Maschine nimmt ganze Prozent (`transmit set tunepower=N`,
// 0…100). Unter einem Prozent gibt es nichts. Ergibt ein Prozent
// schon mehr als das Ziel, ist das Ziel mit diesem Regler NICHT
// erreichbar -- dann meldet er genau das, statt eine Zahl zu
// erfinden. Der naechste Hebel waere die Ansteuerung der Scheibe oder
// ein Daempfungsglied, und das ist eine andere Arbeit.
//
// Keine Uhr, kein Netz, kein Qt: alles kommt von auszen herein, wie
// bei `SwrRiegel`.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-31 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include <QHash>
#include <QString>

#include <optional>

namespace Longpath::safety {

class FlexAbstimmRegler
{
public:
    /// Das kleinste, was die Maschine kennt.
    static constexpr int kMinProzent = 1;

    /// Unterhalb dieser Leistung ist die Messung Rauschen -- dieselbe
    /// Grenze wie im `SwrRiegel`, und aus demselben Grund.
    static constexpr double kMinMessbarW = 0.5;

    /// Wie weit darf es daneben liegen, bevor nachgeregelt wird.
    /// 30 Prozent: enger waere Zappeln auf einer Kurve, die wir nicht
    /// kennen.
    static constexpr double kToleranz = 0.30;

    void   setzeZielWatt(double w);
    double zielWatt() const { return m_zielW; }

    /// Die Decke des Betreibers -- hoeher stellt der Regler nie.
    void setzeDecke(int prozent);
    int  decke() const { return m_decke; }

    void setzeAn(bool an) { m_an = an; }
    bool an() const { return m_an; }

    /// Womit dieser Traeger anfaengt. Gelernter Wert des Bandes, sonst
    /// das Kleinste -- von unten herankommen, nie von oben.
    int startProzent(const QString& band) const;

    /// Eine Messung waehrend des Traegers. Gibt einen NEUEN Prozentwert
    /// zurueck, wenn JETZT heruntergeregelt werden muss; sonst leer.
    /// Lernt dabei fuer das naechste Mal.
    std::optional<int> messung(const QString& band, int aktuellProzent,
                               double fwdWatt);

    /// Was der Regler fuer dieses Band gelernt hat (leer: noch nichts).
    std::optional<int> gelernt(const QString& band) const;

    /// Wahr, sobald bei `kMinProzent` schon mehr als das Ziel
    /// herauskam -- dann ist das Ziel mit diesem Hebel nicht
    /// erreichbar, und der Aufrufer soll es SAGEN.
    bool zielUnerreichbar(const QString& band) const;

    /// Die zuletzt gemessene Leistung dieses Bandes.
    std::optional<double> letzteLeistung(const QString& band) const;

private:
    bool    m_an{true};
    double  m_zielW{1.0};
    int     m_decke{100};
    QHash<QString, int>    m_gelernt;
    QHash<QString, double> m_letzteW;
    QHash<QString, bool>   m_unerreichbar;
};

}  // namespace Longpath::safety
