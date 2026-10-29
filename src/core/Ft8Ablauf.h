// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/Ft8Ablauf.h  (Longpath)
// =================================================================
//
// Der Ablauf eines FT8/FT4-QSOs als reine Rechenvorschrift.
//
// ── Was diese Klasse NICHT tut ──────────────────────────────────────
//
// Sie sendet nicht, sie tastet nicht und sie kennt keine Uhr. Sie
// SAGT nur, was im naechsten Fenster zu senden WAERE. Ob das
// geschieht, entscheidet das Applet — und dort steht auch das Tasten
// (MoxController) und der Bandplan.
//
// Das ist dieselbe Trennung wie bei Ft8Tx (siehe dort): was Ton
// erzeugt oder tastet, laesst sich nicht mehr gefahrlos pruefen. Hier
// bleibt eine Klasse ohne Qt-Fenster, ohne Zeitgeber und ohne
// Seiteneffekte — jeder Zustandsuebergang ist eine Zeile im
// Prueflauf.
//
// ── Der Ablauf ──────────────────────────────────────────────────────
//
// Nach WSJT-X, mit dessen Nachrichtennummern:
//
//   Als ANRUFER (ich rufe CQ):
//     Tx6  CQ OE5SOS JN67
//          <- OE5SOS DL1ABC JO31      jemand antwortet
//     Tx2  DL1ABC OE5SOS -12          mein Rapport an ihn
//          <- OE5SOS DL1ABC R-09      sein Rapport, bestaetigt
//     Tx4  DL1ABC OE5SOS RR73         fertig, ins Logbuch
//
//   Als ANTWORTER (ich antworte auf ein CQ):
//     Tx1  DL1ABC OE5SOS JN67
//          <- OE5SOS DL1ABC -12       sein Rapport an mich
//     Tx3  DL1ABC OE5SOS R-09         mein Rapport, bestaetigt
//          <- OE5SOS DL1ABC RR73      er ist fertig
//     Tx5  DL1ABC OE5SOS 73           Hoeflichkeit, dann fertig
//
// ── Warum eine Wiederholungsgrenze ──────────────────────────────────
//
// Ein Ablauf, der ohne Antwort immer weiterruft, ist ein Sender, der
// sich selbst nicht abschaltet. `maxWiederholungen` begrenzt, wie oft
// dieselbe Nachricht ohne passende Antwort wiederholt wird; danach
// geht der Ablauf in `Abgebrochen` und sagt nichts mehr zu senden.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#pragma once

#include <QString>

namespace Longpath {

class Ft8Ablauf
{
public:
    enum class Rolle {
        Anrufer,     ///< ich rufe CQ und werde angerufen
        Antworter    ///< ich antworte auf ein gehoertes CQ
    };

    enum class Zustand {
        Ruht,           ///< nichts im Gange
        RufeCq,         ///< Tx6
        RufeAn,         ///< Tx1
        SendeReport,    ///< Tx2
        SendeRReport,   ///< Tx3
        SendeRR73,      ///< Tx4
        Sende73,        ///< Tx5
        Fertig,         ///< QSO vollstaendig — ab ins Logbuch
        Abgebrochen     ///< aufgegeben oder abgebrochen
    };

    /// Wie oft dieselbe Nachricht ohne passende Antwort wiederholt
    /// wird, bevor aufgegeben wird. WSJT-X nimmt hier voreingestellt
    /// ebenfalls eine kleine Zahl; mehr hilft selten und sendet lange.
    ///
    /// Gilt fuer die Zuege MITTEN im QSO. Fuer den CQ-Ruf selbst
    /// entscheidet der Betreiber -- siehe `setzeCqWiederholungen`.
    static constexpr int kMaxWiederholungen = 5;

    /// Wie oft ein CQ-Ruf sich wiederholt, bevor der Ablauf aufgibt.
    ///
    /// ── Warum das getrennt ist (2026-10-29) ─────────────────────────
    ///
    /// Es gab zwei Zaehler fuer dieselbe Sache, und der kleinere hat
    /// gewonnen -- der, den der Betreiber nicht sehen konnte. Im
    /// Fenster stand „x16"; der Ruf brach nach der FUENFTEN Sendung ab,
    /// weil `fensterVerstrichen()` hier starr `kMaxWiederholungen`
    /// nahm. Am Geraet gemessen (Protokoll 2026-10-29, 12:45:00 bis
    /// 12:47:12): genau fuenf Sendungen, danach kein „Senden
    /// vorbereitet" mehr. Betreiber, zweimal: „anschliessend wieder
    /// keine Leistung nach dem dritten oder vierten Durchgang".
    ///
    /// Die Zahl des Betreibers gilt jetzt fuer den Ruf. Die Fuenf
    /// bleibt, wo sie hingehoert: ein hoeflicher Zug im laufenden QSO
    /// wird nicht sechzehnmal wiederholt.
    void setzeCqWiederholungen(int n);
    int  cqWiederholungen() const { return m_cqWiederholungen; }

    /// Ich antworte auf ein gehoertes CQ.
    /// `snr` ist das, was ICH von ihm gemessen habe — es wird spaeter
    /// sein Rapport.
    void starteAlsAntworter(const QString& partner,
                            const QString& meinRuf,
                            const QString& meinLocator,
                            int snr,
                            const QString& seinLocator = QString());

    /// Ich rufe CQ. Der Partner ergibt sich aus der ersten Antwort.
    void starteAlsAnrufer(const QString& meinRuf,
                          const QString& meinLocator);

    /// Was im naechsten Fenster zu senden waere, oder leer.
    QString naechsteNachricht() const;

    /// Derselbe Textbau, ohne Ablauf: welcher Satz gehoert zu welchem
    /// Zustand?
    ///
    /// Herausgezogen am 2026-10-18 fuer die Tx1-Tx6-Knoepfe im Fenster.
    /// Sie bauen die Saetze NICHT selbst -- sonst gaebe es zwei
    /// Wahrheiten, und die zweite waere die, die beim naechsten
    /// Protokollbesuch vergessen wird.
    static QString nachrichtFuer(Zustand z,
                                 const QString& partner,
                                 const QString& meinRuf,
                                 const QString& meinLocator,
                                 int reportDb);

    /// Eine gehoerte Nachricht einspeisen. Gibt true, wenn sie den
    /// Ablauf weitergebracht hat (dann ist der Zaehler zurueckgesetzt).
    /// Nachrichten, die nicht an mich gerichtet sind, werden still
    /// uebergangen.
    bool verarbeite(const QString& nachricht, int snr);

    /// Ein Fenster ist vergangen, ohne dass etwas Passendes kam.
    /// Nach kMaxWiederholungen geht der Ablauf in `Abgebrochen`.
    void fensterVerstrichen();

    void abbrechen();

    Zustand zustand()        const { return m_zustand; }
    bool    laeuft()         const;
    QString partner()        const { return m_partner; }
    int     wiederholungen() const { return m_wiederholungen; }

    /// Fuer das Logbuch: was ich ihm gegeben und was er mir gegeben
    /// hat. kKeinReport, solange es noch nicht feststeht.
    static constexpr int kKeinReport = -99;
    int gesendeterReport() const { return m_reportGesendet; }
    int empfangenerReport() const { return m_reportEmpfangen; }

    /// Sein Feldkenner, soweit er in einer Nachricht stand. Beim
    /// Antworter kommt er aus der angeklickten CQ-Zeile, beim Anrufer
    /// aus der Antwort auf das CQ. Leer, wenn nie einer kam — dann
    /// bleibt GRIDSQUARE im Logbuch leer, statt zu raten.
    QString partnerLocator() const { return m_partnerLocator; }

    /// Sieht das Feld aus wie ein Feldkenner (JN67, JN67VV)? Nicht
    /// jedes dritte Feld ist einer: „-12", „R-09", „RR73" und „73"
    /// stehen an derselben Stelle.
    static bool wieLocator(const QString& feld);

    /// Nur fuer Prueflaeufe und Anzeige: eine lesbare Form.
    static QString zustandName(Zustand z);

private:
    /// Ist die Nachricht an MICH gerichtet (erstes Feld = mein
    /// Rufzeichen)? Und von wem?
    bool anMich(const QString& nachricht, QString* vonWem) const;

    Rolle   m_rolle{Rolle::Antworter};
    Zustand m_zustand{Zustand::Ruht};
    QString m_partner;
    QString m_meinRuf;
    QString m_meinLocator;
    QString m_partnerLocator;
    int     m_reportGesendet{kKeinReport};
    int     m_reportEmpfangen{kKeinReport};
    int     m_wiederholungen{0};
    int     m_cqWiederholungen{kMaxWiederholungen};
};

} // namespace Longpath
