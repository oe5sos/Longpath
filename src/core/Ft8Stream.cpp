// =================================================================
// src/core/Ft8Stream.cpp  (Longpath)
// =================================================================
// Longpath-original. Siehe Ft8Stream.h.
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Stream.h"

#include "core/Resampler.h"

namespace Longpath {
namespace {

constexpr double kQuellRate = 48000.0;   // wie der CW-Abgriff
constexpr int    kBlock     = 4096;

} // namespace

Ft8Stream::Ft8Stream(QObject* parent)
    : QObject(parent),
      m_uhr([]() { return QDateTime::currentMSecsSinceEpoch(); }),
      m_umtaster(std::make_unique<Resampler>(kQuellRate,
                                             Ft8Decoder::kSampleRate,
                                             kBlock))
{
}

Ft8Stream::~Ft8Stream() = default;

void Ft8Stream::setProtocol(Ft8Decoder::Protocol p)
{
    if (p == m_protocol) { return; }
    m_protocol = p;
    // Das laufende Fenster gehoert zum alten Takt und waere im neuen
    // falsch ausgerichtet.
    m_fenster.clear();
    m_fensterBeginnMs = -1;
}

void Ft8Stream::setClock(std::function<qint64()> utcMs)
{
    if (utcMs) { m_uhr = std::move(utcMs); }
}

int Ft8Stream::slotMs() const
{
    return m_protocol == Ft8Decoder::Protocol::Ft4 ? 7500 : 15000;
}

void Ft8Stream::poll()
{
    // Vor dem ersten Ton gibt es kein Fenster — dann faengt feedAudio
    // an der naechsten Grenze an, nicht hier.
    if (m_fensterBeginnMs < 0) { return; }

    const qint64 fenster = slotMs();
    const qint64 beginn = (m_uhr() / fenster) * fenster;
    if (beginn != m_fensterBeginnMs) { schliesseFenster(beginn); }
}

void Ft8Stream::feedAudio(const float* interleavedStereo, int frames)
{
    if (!interleavedStereo || frames <= 0) { return; }

    const qint64 jetzt = m_uhr();
    const qint64 fenster = slotMs();
    const qint64 beginn = (jetzt / fenster) * fenster;

    // Erstes Fenster: an der naechsten Grenze anfangen, nicht
    // mittendrin. Ein halbes Fenster dekodiert nichts und sieht aus
    // wie ein toter Dekoder.
    if (m_fensterBeginnMs < 0) {
        m_fensterBeginnMs = beginn;
        m_fenster.clear();
        m_fenster.reserve(Ft8Decoder::kSampleRate * 16);
    }

    // Grenze ueberschritten? Dann ist das alte Fenster fertig.
    if (beginn != m_fensterBeginnMs) {
        schliesseFenster(beginn);
    }

    // Stereo -> Mono -> 12 kHz.
    QVector<float> mono(frames);
    for (int i = 0; i < frames; ++i) {
        mono[i] = 0.5f * (interleavedStereo[2 * i]
                          + interleavedStereo[2 * i + 1]);
    }
    for (int pos = 0; pos < frames; pos += kBlock) {
        const int n = qMin(kBlock, frames - pos);
        QVector<float> aus(n + 16);
        const int geschrieben = m_umtaster->processInto(mono.constData() + pos,
                                                        n, aus.data(),
                                                        aus.size());
        for (int i = 0; i < geschrieben; ++i) { m_fenster.append(aus[i]); }
    }
}

void Ft8Stream::schliesseFenster(qint64 neuerBeginnMs)
{
    const QDateTime beginn =
        QDateTime::fromMSecsSinceEpoch(m_fensterBeginnMs, Qt::UTC);

    // Zu kurz? Dann war es ein angebrochenes Fenster — das erste nach
    // dem Einschalten etwa. Nichts melden, nur weiterzaehlen.
    const int nötig = Ft8Decoder::kSampleRate * (slotMs() / 2) / 1000;
    if (m_fenster.size() >= nötig) {
        // Bei FT4 beginnt der Block spaeter als das Fenster — siehe
        // Ft8Stream.h, kFt4BlockVersatzMs, mit der Messung dazu. Die
        // Laengenpruefung oben zaehlt weiter das GANZE Fenster: ob
        // genug Ton da ist, hat mit dem Suchband nichts zu tun.
        int versatz = 0;
        if (m_protocol == Ft8Decoder::Protocol::Ft4) {
            versatz = qMin(Ft8Decoder::kSampleRate * kFt4BlockVersatzMs / 1000,
                           m_fenster.size());
        }
        const auto funde = Ft8Decoder::decodeBlock(m_fenster.constData() + versatz,
                                                   m_fenster.size() - versatz,
                                                   m_protocol);
        // Auch leer melden: ein stilles Fenster ist eine Aussage, ein
        // ausbleibendes Fenster ein Fehler. Siehe Ft8Stream.h.
        emit decoded(funde, beginn);
    }

    m_fenster.clear();
    m_fensterBeginnMs = neuerBeginnMs;
}

} // namespace Longpath
