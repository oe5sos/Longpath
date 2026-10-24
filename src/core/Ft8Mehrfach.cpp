// =================================================================
// src/core/Ft8Mehrfach.cpp  (Longpath)
// =================================================================
//
// Siehe Ft8Mehrfach.h fuer das Modell und die MSHV-Herkunft.
//
// Modification history (Longpath):
//   2026-10-24 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Mehrfach.h"

#include <algorithm>

namespace Longpath {

void Ft8Mehrfach::setEigeneDaten(const QString& meinRuf,
                                 const QString& meinLocator)
{
    m_meinRuf     = meinRuf.trimmed().toUpper();
    m_meinLocator = meinLocator.trimmed().toUpper();
}

void Ft8Mehrfach::setPlaetze(int n)
{
    m_plaetze = std::clamp(n, 1, kMaxPlaetze);
}

void Ft8Mehrfach::setGrundtonHz(float hz)
{
    m_grundtonHz = hz;
}

bool Ft8Mehrfach::kennt(const QString& partner) const
{
    const QString p = partner.trimmed().toUpper();
    for (const Ft8Ablauf& a : m_laufend) {
        if (a.partner().toUpper() == p) { return true; }
    }
    for (const Wartend& w : m_warteschlange) {
        if (w.partner.toUpper() == p) { return true; }
    }
    return false;
}

int Ft8Mehrfach::aktive() const
{
    int n = 0;
    for (const Ft8Ablauf& a : m_laufend) {
        if (a.laeuft()) { ++n; }
    }
    return n;
}

bool Ft8Mehrfach::nimmAnrufer(const QString& partner, int snr,
                              const QString& seinLocator)
{
    const QString p = partner.trimmed().toUpper();
    if (p.isEmpty() || m_meinRuf.isEmpty()) { return false; }

    // Doppelte abweisen. Ohne das antwortete man demselben Rufzeichen
    // im selben Fenster zweimal, auf zwei Toenen -- und zaehlte es am
    // Ende als zwei QSOs.
    if (kennt(p)) { return false; }

    if (aktive() < m_plaetze) {
        Ft8Ablauf a;
        // Wir sind hier der ANRUFER (wir haben CQ gerufen, er hat
        // geantwortet), aber der Ablauf beginnt fuer uns bei Tx2: der
        // Partner steht schon fest. `starteAlsAntworter` baut genau
        // diesen Einstieg -- der Name meint die Rolle im Gespraech,
        // nicht wer zuerst gerufen hat.
        a.starteAlsAntworter(p, m_meinRuf, m_meinLocator, snr, seinLocator);
        m_laufend.push_back(a);
        return true;
    }

    m_warteschlange.push_back(Wartend{p, snr, seinLocator.trimmed().toUpper()});
    return true;
}

int Ft8Mehrfach::verarbeite(const QString& nachricht, int snr)
{
    int vorangekommen = 0;
    for (Ft8Ablauf& a : m_laufend) {
        if (!a.laeuft()) { continue; }
        if (a.verarbeite(nachricht, snr)) { ++vorangekommen; }
    }
    return vorangekommen;
}

QVector<Ft8TxSource::Platz> Ft8Mehrfach::naechsteNachrichten() const
{
    QVector<Ft8TxSource::Platz> raus;
    const QVector<float> toene =
        Ft8TxSource::tonleiter(m_grundtonHz, m_laufend.size());

    for (int i = 0; i < m_laufend.size(); ++i) {
        if (!m_laufend[i].laeuft()) { continue; }
        const QString text = m_laufend[i].naechsteNachricht();
        if (text.isEmpty()) { continue; }
        Ft8TxSource::Platz p;
        p.text   = text;
        p.toneHz = (i < toene.size()) ? toene[i] : m_grundtonHz;
        raus.push_back(p);
    }
    return raus;
}

void Ft8Mehrfach::fensterVerstrichen()
{
    for (Ft8Ablauf& a : m_laufend) {
        if (a.laeuft()) { a.fensterVerstrichen(); }
    }

    // Fertige beiseitelegen, Abgebrochene fallen lassen -- beides gibt
    // den Platz frei.
    QVector<Ft8Ablauf> bleibt;
    bleibt.reserve(m_laufend.size());
    for (const Ft8Ablauf& a : m_laufend) {
        if (a.zustand() == Ft8Ablauf::Zustand::Fertig) {
            m_fertig.push_back(a);
            continue;
        }
        if (a.zustand() == Ft8Ablauf::Zustand::Abgebrochen) {
            continue;
        }
        bleibt.push_back(a);
    }
    m_laufend = bleibt;

    rueckeNach();
}

void Ft8Mehrfach::rueckeNach()
{
    while (!m_warteschlange.isEmpty() && aktive() < m_plaetze) {
        const Wartend w = m_warteschlange.takeFirst();
        Ft8Ablauf a;
        a.starteAlsAntworter(w.partner, m_meinRuf, m_meinLocator,
                             w.snr, w.locator);
        m_laufend.push_back(a);
    }
}

QVector<Ft8Ablauf> Ft8Mehrfach::nimmFertige()
{
    QVector<Ft8Ablauf> raus = m_fertig;
    m_fertig.clear();
    return raus;
}

QVector<Ft8Mehrfach::Belegung> Ft8Mehrfach::belegung() const
{
    QVector<Belegung> raus;
    const QVector<float> toene =
        Ft8TxSource::tonleiter(m_grundtonHz, m_laufend.size());
    for (int i = 0; i < m_laufend.size(); ++i) {
        Belegung b;
        b.partner = m_laufend[i].partner();
        b.zustand = m_laufend[i].zustand();
        b.toneHz  = (i < toene.size()) ? toene[i] : m_grundtonHz;
        raus.push_back(b);
    }
    return raus;
}

void Ft8Mehrfach::abbrechen()
{
    for (Ft8Ablauf& a : m_laufend) { a.abbrechen(); }
    m_laufend.clear();
    m_warteschlange.clear();
}

}  // namespace Longpath
