// =================================================================
// src/gui/WaterfallHistoryBuffer.cpp  (NereusSDR)
// =================================================================
//
// Source attribution (AetherSDR — GPLv3):
//
//   Copyright (C) 2024-2026  Jeremy (KK7GWY) / AetherSDR contributors
//       — per https://github.com/ten9876/AetherSDR (GPLv3)
//
//   Port of AetherSDR `src/gui/WaterfallHistoryBuffer.cpp`
//   (AetherSDR 31b29583). See the header for why the history holds
//   intensity rather than colour.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-15 — Ported in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "gui/WaterfallHistoryBuffer.h"

#include <algorithm>
#include <utility>

namespace NereusSDR {

WaterfallHistoryBuffer::WaterfallHistoryBuffer(
    WaterfallHistoryBuffer&& other) noexcept
    : m_width(std::exchange(other.m_width, 0))
    , m_capacityRows(std::exchange(other.m_capacityRows, 0))
    , m_chunks(std::move(other.m_chunks))
{
    other.m_chunks.clear();
}

WaterfallHistoryBuffer& WaterfallHistoryBuffer::operator=(
    WaterfallHistoryBuffer&& other) noexcept
{
    if (this == &other) { return *this; }
    m_width = std::exchange(other.m_width, 0);
    m_capacityRows = std::exchange(other.m_capacityRows, 0);
    m_chunks = std::move(other.m_chunks);
    other.m_chunks.clear();
    return *this;
}

void WaterfallHistoryBuffer::configure(int width, int capacityRows)
{
    if (width <= 0 || capacityRows <= 0) {
        reset();
        return;
    }
    const int chunkCount = (capacityRows + kRowsPerChunk - 1) / kRowsPerChunk;

    // Gleiche Maße: nichts tun. Das ist keine Sparsamkeit, sondern der
    // Unterschied zwischen einer Historie und keiner — configure() wird
    // aus dem Zeichenweg gerufen, und ein Neuanlegen bei jedem Bild
    // hieße, dass nie eine zweite Zeile im Puffer steht.
    if (width == m_width && capacityRows == m_capacityRows
        && m_chunks.size() == chunkCount) {
        return;
    }

    m_width = width;
    m_capacityRows = capacityRows;
    m_chunks = QVector<QByteArray>(chunkCount);
}

void WaterfallHistoryBuffer::discardRows()
{
    for (QByteArray& chunk : m_chunks) {
        chunk.clear();
        // squeeze(), sonst behält QByteArray die Kapazität, und
        // „verwerfen" gäbe den Speicher nicht zurück — bei zwanzig
        // Minuten Historie ist das der ganze Zweck der Übung.
        chunk.squeeze();
    }
}

void WaterfallHistoryBuffer::reset()
{
    m_width = 0;
    m_capacityRows = 0;
    m_chunks.clear();
}

bool WaterfallHistoryBuffer::resizeWidth(int width)
{
    if (!isConfigured() || width <= 0) { return false; }
    if (width == m_width) { return true; }

    QVector<QByteArray> resized(m_chunks.size());
    for (int chunkIndex = 0; chunkIndex < m_chunks.size(); ++chunkIndex) {
        const QByteArray& src = m_chunks[chunkIndex];
        // Nie beschriebene Blöcke bleiben leer. Sie beim Umrechnen zu
        // belegen, machte aus der blockweisen Belegung wieder eine
        // vollständige.
        if (src.isEmpty()) { continue; }

        const int rowCount = rowsInChunk(chunkIndex);
        QByteArray& dst = resized[chunkIndex];
        dst.resize(static_cast<qsizetype>(rowCount) * width);
        for (int r = 0; r < rowCount; ++r) {
            const quint8* from = reinterpret_cast<const quint8*>(
                src.constData() + static_cast<qsizetype>(r) * m_width);
            quint8* to = reinterpret_cast<quint8*>(
                dst.data() + static_cast<qsizetype>(r) * width);
            for (int x = 0; x < width; ++x) {
                // Nächster Nachbar. Interpolieren wäre hier falsch: der
                // Wert ist ein Index in eine Farbtabelle, und der
                // Mittelwert zweier Indizes ist nicht die mittlere
                // Farbe.
                const int sx = std::min(
                    m_width - 1,
                    static_cast<int>(
                        static_cast<qint64>(x) * m_width / width));
                to[x] = from[sx];
            }
        }
    }

    m_width = width;
    m_chunks = std::move(resized);
    return true;
}

quint8* WaterfallHistoryBuffer::writableRow(int row)
{
    if (!isConfigured() || row < 0 || row >= m_capacityRows) {
        return nullptr;
    }
    const int chunkIndex = row / kRowsPerChunk;
    const int rowInChunk = row % kRowsPerChunk;
    QByteArray& chunk = m_chunks[chunkIndex];
    if (chunk.isEmpty()) {
        chunk.fill('\0',
                   static_cast<qsizetype>(rowsInChunk(chunkIndex)) * m_width);
    }
    return reinterpret_cast<quint8*>(
        chunk.data() + static_cast<qsizetype>(rowInChunk) * m_width);
}

const quint8* WaterfallHistoryBuffer::row(int row) const
{
    if (!isConfigured() || row < 0 || row >= m_capacityRows) {
        return nullptr;
    }
    const int chunkIndex = row / kRowsPerChunk;
    const int rowInChunk = row % kRowsPerChunk;
    const QByteArray& chunk = m_chunks[chunkIndex];
    if (chunk.isEmpty()) { return nullptr; }
    return reinterpret_cast<const quint8*>(
        chunk.constData() + static_cast<qsizetype>(rowInChunk) * m_width);
}

qsizetype WaterfallHistoryBuffer::allocatedBytes() const
{
    qsizetype bytes = 0;
    for (const QByteArray& chunk : m_chunks) { bytes += chunk.size(); }
    return bytes;
}

int WaterfallHistoryBuffer::allocatedChunkCount() const
{
    return static_cast<int>(std::count_if(
        m_chunks.cbegin(), m_chunks.cend(),
        [](const QByteArray& chunk) { return !chunk.isEmpty(); }));
}

int WaterfallHistoryBuffer::rowsInChunk(int chunkIndex) const
{
    const int firstRow = chunkIndex * kRowsPerChunk;
    return std::clamp(m_capacityRows - firstRow, 0, kRowsPerChunk);
}

} // namespace NereusSDR
