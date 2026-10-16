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

bool istSchluss(const QString& feld)
{
    return feld.compare(QStringLiteral("RR73"), Qt::CaseInsensitive) == 0
           || feld.compare(QStringLiteral("RRR"), Qt::CaseInsensitive) == 0
           || feld.compare(QStringLiteral("73"), Qt::CaseInsensitive) == 0;
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

void Ft8Ablauf::starteAlsAntworter(const QString& partner,
                                   const QString& meinRuf,
                                   const QString& meinLocator,
                                   int snr)
{
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

void Ft8Ablauf::starteAlsAnrufer(const QString& meinRuf,
                                 const QString& meinLocator)
{
    m_rolle           = Rolle::Anrufer;
    m_partner.clear();
    m_meinRuf         = meinRuf.trimmed().toUpper();
    m_meinLocator     = meinLocator.trimmed().left(4).toUpper();
    m_reportGesendet  = kKeinReport;
    m_reportEmpfangen = kKeinReport;
    m_wiederholungen  = 0;
    m_zustand = m_meinRuf.isEmpty() ? Zustand::Ruht : Zustand::RufeCq;
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
    switch (m_zustand) {
        case Zustand::RufeCq:
            return m_meinLocator.isEmpty()
                ? QStringLiteral("CQ %1").arg(m_meinRuf)
                : QStringLiteral("CQ %1 %2").arg(m_meinRuf, m_meinLocator);
        case Zustand::RufeAn:
            return m_meinLocator.isEmpty()
                ? QStringLiteral("%1 %2").arg(m_partner, m_meinRuf)
                : QStringLiteral("%1 %2 %3").arg(m_partner, m_meinRuf,
                                                 m_meinLocator);
        case Zustand::SendeReport:
            return QStringLiteral("%1 %2 %3")
                .arg(m_partner, m_meinRuf, rapportText(m_reportGesendet));
        case Zustand::SendeRReport:
            return QStringLiteral("%1 %2 R%3")
                .arg(m_partner, m_meinRuf, rapportText(m_reportGesendet));
        case Zustand::SendeRR73:
            return QStringLiteral("%1 %2 RR73").arg(m_partner, m_meinRuf);
        case Zustand::Sende73:
            return QStringLiteral("%1 %2 73").arg(m_partner, m_meinRuf);
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
    if (vonWem) { *vonWem = f.at(1); }
    return true;
}

bool Ft8Ablauf::verarbeite(const QString& nachricht, int snr)
{
    if (!laeuft()) { return false; }

    QString vonWem;
    if (!anMich(nachricht, &vonWem)) { return false; }

    // Laeuft das QSO schon mit jemandem, zaehlt nur dieser. Sonst
    // koennte ein Dritter, der gleichzeitig ruft, den Ablauf
    // uebernehmen.
    if (!m_partner.isEmpty() && vonWem != m_partner) { return false; }

    const QStringList f =
        nachricht.simplified().toUpper().split(QLatin1Char(' '),
                                               Qt::SkipEmptyParts);
    const QString letztes = f.size() >= 3 ? f.at(2) : QString();

    switch (m_zustand) {
        case Zustand::RufeCq: {
            // Jemand antwortet auf mein CQ — mit Locator oder blank.
            m_partner        = vonWem;
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
            if (istSchluss(letztes)) {
                m_wiederholungen = 0;
                m_zustand        = Zustand::Fertig;
                return true;
            }
            return false;
        }
        case Zustand::SendeRReport: {
            if (istSchluss(letztes)) {
                m_wiederholungen = 0;
                m_zustand        = Zustand::Sende73;
                return true;
            }
            return false;
        }
        case Zustand::SendeRR73:
        case Zustand::Sende73: {
            // Er bestaetigt den Schluss — mehr ist nicht noetig.
            if (istSchluss(letztes)) {
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
    if (m_wiederholungen >= kMaxWiederholungen) {
        m_zustand = Zustand::Abgebrochen;
    }
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
