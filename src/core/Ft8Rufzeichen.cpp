// =================================================================
// src/core/Ft8Rufzeichen.cpp  (Longpath)
// =================================================================
// Siehe Ft8Rufzeichen.h für den Hergang.
//
// Modification history (Longpath):
//   2026-10-28 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork).
// =================================================================

// no-port-check: Longpath-original.

#include "core/Ft8Rufzeichen.h"

#include <QLoggingCategory>

extern "C" {
#include "ft8/message.h"
}

#include <cstring>

namespace Longpath {

namespace {
Q_LOGGING_CATEGORY(lcFt8Ruf, "longpath.ft8", QtInfoMsg)
}

Ft8Rufzeichenspeicher& Ft8Rufzeichenspeicher::instanz()
{
    static Ft8Rufzeichenspeicher s;
    return s;
}

void Ft8Rufzeichenspeicher::merke(const QString& rufzeichen, quint32 n22)
{
    const QString ruf = rufzeichen.trimmed().toUpper();
    if (ruf.isEmpty()) { return; }

    // Ein unaufgelöster Hash ist kein Rufzeichen. Ihn zu merken hieße,
    // den Platzhalter später als Antwort auszugeben.
    if (ruf.contains(QLatin1Char('<')) || ruf == QLatin1String("...")) {
        return;
    }

    QMutexLocker sperre(&m_sperre);

    // Die drei Schlüssel kommen aus EINER Rechnung — siehe Kopf.
    const quint32 n12 = n22 >> 10;
    const quint32 n10 = n22 >> 12;

    const bool neu = !m_n22.contains(n22);
    m_n22.insert(n22, ruf);
    m_n12.insert(n12, ruf);
    m_n10.insert(n10, ruf);

    if (neu) {
        m_reihenfolge.append(ruf);
        qCInfo(lcFt8Ruf).noquote()
            << QStringLiteral("FT8: Rufzeichen gemerkt — %1 (n22=%2), "
                              "jetzt %3")
                   .arg(ruf).arg(n22).arg(m_reihenfolge.size());

        // Ältestes hinaus, wenn es zu viele werden. Die kurzen
        // Tabellen werden dabei NICHT aufgeräumt: zwei Rufzeichen
        // können sich einen 10-Bit-Hash teilen, und wer blind löscht,
        // nimmt dem anderen seinen Eintrag weg. Ein verwaister
        // Eintrag kostet ein paar Byte; ein falsch gelöschter kostet
        // ein Rufzeichen.
        while (m_reihenfolge.size() > kHoechstzahl) {
            m_reihenfolge.removeFirst();
        }
    }
}

QString Ft8Rufzeichenspeicher::suche(int hashBits, quint32 hash) const
{
    QMutexLocker sperre(&m_sperre);
    switch (hashBits) {
    case 22: return m_n22.value(hash);
    case 12: return m_n12.value(hash);
    case 10: return m_n10.value(hash);
    default: break;
    }
    return {};
}

int Ft8Rufzeichenspeicher::anzahl() const
{
    QMutexLocker sperre(&m_sperre);
    return m_reihenfolge.size();
}

void Ft8Rufzeichenspeicher::leeren()
{
    QMutexLocker sperre(&m_sperre);
    m_n22.clear();
    m_n12.clear();
    m_n10.clear();
    m_reihenfolge.clear();
}

namespace {

bool suchenFuerFt8Lib(ftx_callsign_hash_type_t art, uint32_t hash,
                      char* rufzeichen)
{
    int bits = 22;
    if (art == FTX_CALLSIGN_HASH_12_BITS) { bits = 12; }
    else if (art == FTX_CALLSIGN_HASH_10_BITS) { bits = 10; }

    const QString gefunden =
        Ft8Rufzeichenspeicher::instanz().suche(bits, hash);
    if (gefunden.isEmpty()) { return false; }

    // ft8_lib gibt einen Puffer von 12 Byte (message.c:725, `char
    // c11[12]`). Mehr als elf Zeichen plus Abschluss darf nicht
    // hinein.
    const QByteArray roh = gefunden.left(11).toLatin1();
    std::memcpy(rufzeichen, roh.constData(), size_t(roh.size()));
    rufzeichen[roh.size()] = '\0';
    return true;
}

void merkenFuerFt8Lib(const char* rufzeichen, uint32_t n22)
{
    if (!rufzeichen) { return; }
    Ft8Rufzeichenspeicher::instanz().merke(
        QString::fromLatin1(rufzeichen), n22);
}

ftx_callsign_hash_interface_t g_schnittstelle{
    &suchenFuerFt8Lib,
    &merkenFuerFt8Lib,
};

}  // namespace

void* ft8RufzeichenSchnittstelle()
{
    return &g_schnittstelle;
}

}  // namespace Longpath
