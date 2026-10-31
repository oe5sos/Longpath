// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/Ft8Ablauf.cpp  (Longpath)
// =================================================================
// Siehe Ft8Ablauf.h fuer den Ablauf und die Begruendung, warum diese
// Klasse weder sendet noch tastet noch eine Uhr kennt.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include "Ft8Ablauf.h"

#include <QtGlobal>
#include <QRegularExpression>
#include <QStringList>

namespace Longpath {

namespace {

/// Der Rapport, wie er in einer FT8-Nachricht steht: „-12", „+03",
/// „R-09". Gibt kKeinReport zurueck, wenn das Feld keiner ist.
int rapportAus(const QString& feld)
{
    QString f = feld;
    if (f.startsWith(QLatin1Char('R'), Qt::CaseInsensitive) && f.size() > 1) {
        f = f.mid(1);
    }
    if (f.size() < 2) { return Ft8Ablauf::kKeinReport; }
    if (f.at(0) != QLatin1Char('-') && f.at(0) != QLatin1Char('+')) {
        return Ft8Ablauf::kKeinReport;
    }
    bool ok = false;
    const int wert = f.toInt(&ok);
    return ok ? wert : Ft8Ablauf::kKeinReport;
}

/// Trägt das Feld ein „R" vor dem Rapport? Das unterscheidet Tx2 von
/// Tx3 und entscheidet, ob die Gegenseite meinen Rapport schon hat.
bool istBestaetigterRapport(const QString& feld)
{
    return feld.size() > 2
           && feld.startsWith(QLatin1Char('R'), Qt::CaseInsensitive)
           && rapportAus(feld) != Ft8Ablauf::kKeinReport;
}


/// Der Rapport, wie er gesendet wird: immer mit Vorzeichen und zwei
/// Stellen, so wie WSJT-X ihn schreibt (-12, +03).
QString rapportText(int snr)
{
    const QChar vz = snr < 0 ? QLatin1Char('-') : QLatin1Char('+');
    return QStringLiteral("%1%2").arg(vz)
        .arg(qAbs(snr), 2, 10, QLatin1Char('0'));
}

} // namespace

bool Ft8Ablauf::istAbschied(const QString& feld)
{
    return feld.compare(QStringLiteral("RR73"), Qt::CaseInsensitive) == 0
           || feld.compare(QStringLiteral("RRR"), Qt::CaseInsensitive) == 0
           || feld.compare(QStringLiteral("73"), Qt::CaseInsensitive) == 0;
}

bool Ft8Ablauf::wieLocator(const QString& feld)
{
    // ── „RR73" ist KEIN Feldkenner ───────────────────────────────────
    //
    // Formal ist es einer: Feld RR, Quadrat 73 — ein Punkt mitten im
    // Pazifik, und die Maidenhead-Regel unten laesst ihn durch. Im
    // FT8-Protokoll steht RR73 aber an genau der Stelle, an der sonst
    // der Feldkenner steht, und bedeutet dort „verstanden, Ende".
    //
    // Ohne diese Zeile landete bei jedem zweiten QSO „RR73" als
    // GRIDSQUARE im Logbuch. Der Pruefstand hat es beim ersten Lauf
    // gefangen (2026-10-16).
    if (feld.compare(QStringLiteral("RR73"), Qt::CaseInsensitive) == 0) {
        return false;
    }

    // Zwei Buchstaben, zwei Ziffern, wahlweise zwei weitere
    // Buchstaben: JN67 oder JN67VV. Mehr Stellen gibt es in einer
    // FT8-Nachricht vom Standardtyp nicht.
    static const QRegularExpression re(
        QStringLiteral("^[A-R]{2}[0-9]{2}([A-X]{2})?$"),
        QRegularExpression::CaseInsensitiveOption);
    return re.match(feld).hasMatch();
}

void Ft8Ablauf::starteAlsAntworter(const QString& partner,
                                   const QString& meinRuf,
                                   const QString& meinLocator,
                                   int snr,
                                   const QString& seinLocator)
{
    m_partnerLocator = wieLocator(seinLocator.trimmed())
                           ? seinLocator.trimmed().toUpper() : QString();
    m_rolle           = Rolle::Antworter;
    m_partner         = partner.trimmed().toUpper();
    m_meinRuf         = meinRuf.trimmed().toUpper();
    m_meinLocator     = meinLocator.trimmed().left(4).toUpper();
    m_reportGesendet  = snr;      // das gebe ich ihm, sobald er dran ist
    m_reportEmpfangen = kKeinReport;
    m_wiederholungen  = 0;
    m_zustand = (m_partner.isEmpty() || m_meinRuf.isEmpty())
                    ? Zustand::Ruht : Zustand::RufeAn;
}

void Ft8Ablauf::starteMitAnrufer(const QString& partner,
                                 const QString& meinRuf,
                                 const QString& meinLocator,
                                 int snr,
                                 const QString& seinLocator)
{
    // Derselbe Einstieg wie `Zustand::RufeCq` ihn baut, wenn jemand
    // auf mein CQ antwortet — nur ohne das CQ davor.
    starteAlsAntworter(partner, meinRuf, meinLocator, snr, seinLocator);
    if (m_zustand != Zustand::Ruht) { m_zustand = Zustand::SendeReport; }
}

void Ft8Ablauf::starteAlsAnrufer(const QString& meinRuf,
                                 const QString& meinLocator)
{
    m_rolle           = Rolle::Anrufer;
    m_partner.clear();
    m_partnerLocator.clear();
    m_meinRuf         = meinRuf.trimmed().toUpper();
    m_meinLocator     = meinLocator.trimmed().left(4).toUpper();
    m_reportGesendet  = kKeinReport;
    m_reportEmpfangen = kKeinReport;
    m_wiederholungen  = 0;
    m_zustand = m_meinRuf.isEmpty() ? Zustand::Ruht : Zustand::RufeCq;
}

void Ft8Ablauf::starteAlsHound(const QString& fox,
                               const QString& meinRuf,
                               const QString& meinLocator)
{
    m_rolle           = Rolle::Hound;
    m_partner         = fox.trimmed().toUpper();
    // Die spitzen Klammern sind die Schreibweise fuer „kam als Hash"
    // (siehe `ftx_message_decode_dxpedition`) -- im Satz, den ich
    // SENDE, haben sie nichts verloren.
    m_partner.remove(QLatin1Char('<')).remove(QLatin1Char('>'));
    m_partnerLocator.clear();
    m_meinRuf         = meinRuf.trimmed().toUpper();
    m_meinLocator     = meinLocator.trimmed().left(4).toUpper();
    m_reportGesendet  = kKeinReport;
    m_reportEmpfangen = kKeinReport;
    m_wiederholungen  = 0;
    m_zustand = (m_meinRuf.isEmpty() || m_partner.isEmpty())
                    ? Zustand::Ruht : Zustand::RufeAn;
}

bool Ft8Ablauf::laeuft() const
{
    switch (m_zustand) {
        case Zustand::Ruht:
        case Zustand::Fertig:
        case Zustand::Abgebrochen:
            return false;
        default:
            return true;
    }
}

QString Ft8Ablauf::naechsteNachricht() const
{
    return nachrichtFuer(m_zustand, m_partner, m_meinRuf, m_meinLocator,
                         m_reportGesendet);
}

QString Ft8Ablauf::nachrichtFuer(Zustand z,
                                 const QString& partner,
                                 const QString& meinRuf,
                                 const QString& meinLocator,
                                 int reportDb)
{
    switch (z) {
        case Zustand::RufeCq:
            return meinLocator.isEmpty()
                ? QStringLiteral("CQ %1").arg(meinRuf)
                : QStringLiteral("CQ %1 %2").arg(meinRuf, meinLocator);
        case Zustand::RufeAn:
            return meinLocator.isEmpty()
                ? QStringLiteral("%1 %2").arg(partner, meinRuf)
                : QStringLiteral("%1 %2 %3").arg(partner, meinRuf,
                                                 meinLocator);
        case Zustand::SendeReport:
            return QStringLiteral("%1 %2 %3")
                .arg(partner, meinRuf, rapportText(reportDb));
        case Zustand::SendeRReport:
            return QStringLiteral("%1 %2 R%3")
                .arg(partner, meinRuf, rapportText(reportDb));
        case Zustand::SendeRR73:
            return QStringLiteral("%1 %2 RR73").arg(partner, meinRuf);
        case Zustand::Sende73:
            return QStringLiteral("%1 %2 73").arg(partner, meinRuf);
        default:
            return {};
    }
}

bool Ft8Ablauf::anMich(const QString& nachricht, QString* vonWem) const
{
    const QStringList f =
        nachricht.simplified().toUpper().split(QLatin1Char(' '),
                                               Qt::SkipEmptyParts);
    if (f.size() < 2) { return false; }
    if (f.at(0) != m_meinRuf) { return false; }
    // Der Absender kann in spitzen Klammern stehen: dann kam sein
    // Rufzeichen als Hash im Paket, nicht als Text (0.1-Format, und
    // auch der Standardsatz kennt gehashte Rufzeichen). „<KH1DX>" und
    // „KH1DX" sind dieselbe Station -- wer das nicht zusammenfuehrt,
    // laesst den Fox genau in dem Augenblick fallen, in dem er einen
    // aufruft.
    QString wer = f.at(1);
    wer.remove(QLatin1Char('<')).remove(QLatin1Char('>'));
    if (vonWem) { *vonWem = wer; }
    return true;
}

bool Ft8Ablauf::verarbeite(const QString& nachricht, int snr)
{
    if (!laeuft()) { return false; }

    QString vonWem;
    if (!anMich(nachricht, &vonWem)) { return false; }

    const QStringList f =
        nachricht.simplified().toUpper().split(QLatin1Char(' '),
                                               Qt::SkipEmptyParts);

    // ── Der Abschluss des Fox steht nicht im ueblichen Satzbau ───
    //
    // „K1ABC RR73; W9XYZ <KH1DX> -17": an der Stelle des Absenders
    // steht „RR73;", und dahinter die NAECHSTE Station. Der Vergleich
    // darunter wuerde den Satz darum wegwerfen -- und zwar genau den
    // einen, der sagt, dass ich im Logbuch der Expedition stehe.
    const bool foxSchluss =
        m_rolle == Rolle::Hound && f.size() >= 2
        && f.at(1).startsWith(QStringLiteral("RR73"));

    // Laeuft das QSO schon mit jemandem, zaehlt nur dieser. Sonst
    // koennte ein Dritter, der gleichzeitig ruft, den Ablauf
    // uebernehmen.
    if (!foxSchluss && !m_partner.isEmpty() && vonWem != m_partner) {
        return false;
    }
    const QString letztes = f.size() >= 3 ? f.at(2) : QString();

    switch (m_zustand) {
        case Zustand::RufeCq: {
            // ── Ein Abschied ist kein Anruf ───────────────────
            //
            // Am 2026-10-30 im Betrieb aufgefallen. HB9ZHK war fertig
            // gearbeitet und im Logbuch (12:50:43); fuenfzehn Sekunden
            // spaeter kam sein letztes Wort:
            //
            //     125045  16  2138  OE5SOS HB9ZHK 73
            //
            // Das ist an mich gerichtet, also nahm dieser Zweig es als
            // Antwort auf mein CQ und begann dasselbe QSO von vorn:
            // viermal „HB9ZHK OE5SOS +16" ins Leere, 53 s Sendezeit,
            // dann Abbruch. Waehrenddessen riefen PD4RJ (dreimal) und
            // 9A6T (zweimal) vergeblich an.
            //
            // Ein „73", „RR73" oder „RRR" beendet ein QSO, es beginnt
            // keines. Wer sich verabschiedet, wird nicht angerufen.
            if (Ft8Ablauf::istAbschied(letztes)) { return false; }

            // ── Er ist schon weiter als ich (2026-10-30) ──────
            //
            // Am Geraet gemessen, FT4 auf 40 m:
            //
            //   19:00:21 … 19:01:20  DL7ACA OE5SOS +13   (fuenfmal)
            //            -> aufgegeben, CQ laeuft weiter
            //   19:01:52  OE5SOS DL7ACA R-13
            //
            // Seine Antwort kam 32 s nach meiner letzten Sendung --
            // da hatte der Ablauf laengst aufgegeben und rief wieder
            // CQ. Das „R" vor der Zahl heiszt aber: er HAT meinen
            // Rapport, er bestaetigt ihn. Ihm jetzt noch einmal einen
            // Rapport zu schicken wirft das QSO um einen ganzen Zug
            // zurueck -- und bei einer Station, die ohnehin schwer
            // durchkommt, heiszt das meistens: gar kein QSO.
            //
            // Ihm fehlt nur noch das RR73. Dieselbe Unterscheidung
            // trifft `Zustand::RufeAn` seit jeher
            // (`istBestaetigterRapport`); hier fehlte sie.
            //
            // Betreiber dazu: „das muss alles automatisch laufen."
            if (istBestaetigterRapport(letztes)) {
                m_partner         = vonWem;
                m_reportGesendet  = snr;
                m_reportEmpfangen = rapportAus(letztes);
                m_wiederholungen  = 0;
                m_zustand         = Zustand::SendeRR73;
                return true;
            }

            // Jemand antwortet auf mein CQ — mit Locator oder blank.
            m_partner        = vonWem;
            if (wieLocator(letztes)) { m_partnerLocator = letztes; }
            m_reportGesendet = snr;
            m_wiederholungen = 0;
            m_zustand        = Zustand::SendeReport;
            return true;
        }
        case Zustand::RufeAn: {
            // Er gibt mir seinen Rapport.
            const int r = rapportAus(letztes);
            if (r == kKeinReport) { return false; }
            m_reportEmpfangen = r;
            m_wiederholungen  = 0;
            // Als Hound geht es IMMER ueber den bestaetigten Rapport:
            // den Schluss macht der Fox, nicht ich (siehe
            // `starteAlsHound`). Wer hier auf RR73 abbiegt, sendet
            // einen Satz, auf den die Expedition nicht wartet.
            if (m_rolle == Rolle::Hound) {
                // Mein Rapport AN den Fox ist das, was ich von ihm
                // gemessen habe -- und das ist erst jetzt bekannt.
                // Beim Antworter kommt dieselbe Zahl beim Start
                // herein (aus der angeklickten Zeile); der Hound
                // startet, ohne den Fox gehoert zu haben.
                m_reportGesendet = snr;
                m_zustand = Zustand::SendeRReport;
                return true;
            }
            // Hat er meinen Rapport schon bestaetigt (R-09), kann ich
            // gleich zum Schluss; sonst bestaetige ich seinen.
            m_zustand = istBestaetigterRapport(letztes)
                            ? Zustand::SendeRR73 : Zustand::SendeRReport;
            return true;
        }
        case Zustand::SendeReport: {
            // Erwartet: sein bestaetigter Rapport (R-09).
            const int r = rapportAus(letztes);
            if (r != kKeinReport) {
                m_reportEmpfangen = r;
                m_wiederholungen  = 0;
                m_zustand         = Zustand::SendeRR73;
                return true;
            }
            if (Ft8Ablauf::istAbschied(letztes)) {
                m_wiederholungen = 0;
                m_zustand        = Zustand::Fertig;
                return true;
            }
            return false;
        }
        case Zustand::SendeRReport: {
            // ── Der Fox schlieszt ab (2026-10-30) ────────────
            //
            // Sein Satz sieht anders aus als jeder andere:
            //   „K1ABC RR73; W9XYZ <KH1DX> -17"
            // Mein Rufzeichen steht vorn, dahinter „RR73;" -- und
            // danach steht schon die NAECHSTE Station, nicht mehr
            // meine. Darum wird hier das ZWEITE Feld geprueft, nicht
            // das dritte wie sonst.
            //
            // From WSJT-X lib/DXped_Operations.txt:148-150 [v2.7.0] --
            // „AutoSeq will watch for a message that starts with
            // 'MyCall RR73; ...'. When that is received, you're in his
            // log."
            if (m_rolle == Rolle::Hound && f.size() >= 2
                && f.at(1).startsWith(QStringLiteral("RR73"))) {
                m_wiederholungen = 0;
                m_zustand        = Zustand::Fertig;
                return true;
            }
            if (Ft8Ablauf::istAbschied(letztes)) {
                m_wiederholungen = 0;
                m_zustand        = Zustand::Sende73;
                return true;
            }
            return false;
        }
        case Zustand::SendeRR73:
        case Zustand::Sende73: {
            // Er bestaetigt den Schluss — mehr ist nicht noetig.
            if (Ft8Ablauf::istAbschied(letztes)) {
                m_wiederholungen = 0;
                m_zustand        = Zustand::Fertig;
                return true;
            }
            return false;
        }
        default:
            return false;
    }
}

void Ft8Ablauf::fensterVerstrichen()
{
    if (!laeuft()) { return; }

    // RR73 und 73 sind Hoeflichkeiten: ist eins davon einmal
    // hinausgegangen, ist das QSO vollstaendig. Darauf zu warten, dass
    // er es bestaetigt, haelt den Sender ohne Not laenger auf.
    if (m_zustand == Zustand::SendeRR73 || m_zustand == Zustand::Sende73) {
        m_zustand = Zustand::Fertig;
        return;
    }

    ++m_wiederholungen;

    // Der CQ-Ruf laeuft so oft, wie der Betreiber es im Fenster
    // eingestellt hat; alles andere bleibt bei den fuenf Zuegen.
    const int grenze = m_zustand == Zustand::RufeCq ? m_cqWiederholungen
                                                    : kMaxWiederholungen;
    if (m_wiederholungen >= grenze) {
        m_zustand = Zustand::Abgebrochen;
    }
}

void Ft8Ablauf::setzeCqWiederholungen(int n)
{
    // Eine Null oder etwas Negatives wuerde den Ruf im ersten Fenster
    // abwuergen; die Obergrenze ist dieselbe wie im Drehfeld.
    m_cqWiederholungen = qBound(1, n, 30);
}

void Ft8Ablauf::abbrechen()
{
    if (laeuft()) { m_zustand = Zustand::Abgebrochen; }
}

QString Ft8Ablauf::zustandName(Zustand z)
{
    switch (z) {
        case Zustand::Ruht:         return QStringLiteral("Ruht");
        case Zustand::RufeCq:       return QStringLiteral("RufeCq");
        case Zustand::RufeAn:       return QStringLiteral("RufeAn");
        case Zustand::SendeReport:  return QStringLiteral("SendeReport");
        case Zustand::SendeRReport: return QStringLiteral("SendeRReport");
        case Zustand::SendeRR73:    return QStringLiteral("SendeRR73");
        case Zustand::Sende73:      return QStringLiteral("Sende73");
        case Zustand::Fertig:       return QStringLiteral("Fertig");
        case Zustand::Abgebrochen:  return QStringLiteral("Abgebrochen");
    }
    return QStringLiteral("?");
}

} // namespace Longpath
