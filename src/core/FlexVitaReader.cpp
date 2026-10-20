// =================================================================
// src/core/FlexVitaReader.cpp  (Longpath)
// =================================================================
//
// Herkunft, Lizenz und das Messprotokoll: siehe FlexVitaReader.h und
// docs/protocols/flex-vita49-gemessen.md.
// Ported from AetherSDR src/core/PanadapterStream.cpp,
// src/core/VitaBinCoverage.h and src/core/VitaTileFrequency.h at
// d58e2b8a (GPLv3, primary author Jeremy [KK7GWY]) — project-level
// citation per docs/attribution/HOW-TO-PORT.md rule 6.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork).
// =================================================================

#include "FlexVitaReader.h"

#include <QLoggingCategory>
#include <QtEndian>

#include <algorithm>
#include <cstring>

namespace Longpath {

Q_LOGGING_CATEGORY(lcFlexVita, "longpath.flex.vita", QtInfoMsg)

namespace {

inline const uchar* bytes(const QByteArray& b)
{
    return reinterpret_cast<const uchar*>(b.constData());
}

}  // namespace

// ─── FlexVita: Kopf und Unterkoepfe ──────────────────────────────────

namespace FlexVita {

Header parseHeader(const QByteArray& datagram)
{
    Header h;
    if (datagram.size() < kHeaderBytes) {
        return h;
    }
    const uchar* u = bytes(datagram);

    const quint32 word0 = qFromBigEndian<quint32>(u + 0);
    h.packetType     = static_cast<quint8>((word0 >> 28) & 0xF);
    h.classIdPresent = ((word0 >> 27) & 1) != 0;
    // Das T-Bit. Im Mitschnitt UEBERALL 0 — die vier Null-Byte am Ende
    // sind also Fuellung, kein angekuendigter Anhang. Es wird hier
    // gelesen und weitergegeben, aber nirgends zum Zaehlen benutzt:
    // gezaehlt wird am Unterkopf (siehe Kopf von FlexVitaReader.h,
    // Punkt 2).
    h.trailerPresent = ((word0 >> 26) & 1) != 0;
    h.counter        = static_cast<quint8>((word0 >> 16) & 0xF);
    h.sizeWords      = static_cast<int>(word0 & 0xFFFF);
    h.sizeBytes      = h.sizeWords * 4;

    h.streamId            = qFromBigEndian<quint32>(u + 4);
    h.infoClass           = qFromBigEndian<quint16>(u + 12);
    h.packetClass         = qFromBigEndian<quint16>(u + 14);
    h.timestampSeconds    = qFromBigEndian<quint32>(u + 16);
    h.timestampFractional = qFromBigEndian<quint64>(u + 20);

    // Byte 8-11: pad + OUI 00-1C-2D (FlexRadio Systems). Zusammen mit
    // der Info-Klasse „SL" ist das die einzige Stelle, an der sich ein
    // FlexRadio-Paket von anderem Verkehr unterscheiden laesst.
    static const QByteArray kOuiBlock = QByteArray::fromHex("00001c2d");
    if (datagram.mid(8, 4) != kOuiBlock || h.infoClass != kInfoClassSl) {
        return h;   // valid bleibt false
    }

    h.valid = true;
    return h;
}

FftSubHeader parseFftSubHeader(const QByteArray& datagram)
{
    FftSubHeader s;
    if (datagram.size() < kHeaderBytes + kFftSubBytes) {
        return s;
    }
    const uchar* sub = bytes(datagram) + kHeaderBytes;

    // From AetherSDR PanadapterStream.cpp:898-903 [@d58e2b8a].
    s.startBin   = qFromBigEndian<quint16>(sub + 0);
    s.numBins    = qFromBigEndian<quint16>(sub + 2);
    s.binSize    = qFromBigEndian<quint16>(sub + 4);
    s.totalBins  = qFromBigEndian<quint16>(sub + 6);
    s.frameIndex = qFromBigEndian<quint32>(sub + 8);

    // From AetherSDR PanadapterStream.cpp:905 + :911 — null ist kein
    // Rahmen, und eine andere Binbreite als uint16 kann dieser Leser
    // nicht (gemessen ist binSize immer 2).
    if (s.numBins == 0 || s.binSize != sizeof(quint16) || s.totalBins == 0) {
        return s;
    }
    s.valid = true;
    return s;
}

TileSubHeader parseTileSubHeader(const QByteArray& datagram)
{
    TileSubHeader s;
    if (datagram.size() < kHeaderBytes + kTileSubBytes) {
        return s;
    }
    const uchar* sub = bytes(datagram) + kHeaderBytes;

    // From AetherSDR PanadapterStream.cpp:1025-1035 [@d58e2b8a].
    s.frameLowFreqRaw  = qFromBigEndian<qint64>(sub + 0);
    s.binBandwidthRaw  = qFromBigEndian<qint64>(sub + 8);
    s.lineDurationMs   = qFromBigEndian<quint32>(sub + 16);
    s.tileWidth        = qFromBigEndian<quint16>(sub + 20);
    s.tileHeight       = qFromBigEndian<quint16>(sub + 22);
    s.timecode         = qFromBigEndian<quint32>(sub + 24);
    s.autoBlackLevel   = qFromBigEndian<quint32>(sub + 28);
    s.totalBinsInFrame = qFromBigEndian<quint16>(sub + 32);
    s.firstBinIndex    = qFromBigEndian<quint16>(sub + 34);

    s.lowFreqMhz      = vitaFrequencyToMhz(s.frameLowFreqRaw);
    s.binBandwidthMhz = vitaFrequencyToMhz(s.binBandwidthRaw);

    // From AetherSDR PanadapterStream.cpp:1037.
    if (s.tileWidth == 0 || s.tileHeight == 0 || s.totalBinsInFrame == 0) {
        return s;
    }
    s.valid = true;
    return s;
}

// From AetherSDR VitaTileFrequency.h [@d58e2b8a] — Hz × 2²⁰ → MHz,
// BEDINGUNGSLOS. Die Geschichte dort ist eine Warnung: der erste
// Leser teilte und verwarf das Ergebnis als „unvernuenftig" ueber
// 1000 MHz, las es dann als glatte Hz — und zerstoerte damit jede
// Transverter-Kachel ueber 1 GHz. Eine spaetere Groessenheuristik nahm
// die Decke weg und brach dafuer nahe DC (deren #4412). Also keine
// Heuristik: der Feldtyp ist bekannt.
double vitaFrequencyToMhz(qint64 raw)
{
    constexpr double kVitaFrequencyToMhz = 1048576.0 * 1e6;   // 2²⁰ · 1e6
    return static_cast<double>(raw) / kVitaFrequencyToMhz;
}

// From AetherSDR VitaBinCoverage.h:11-21 [@d58e2b8a].
int boundedPayloadValueCount(int declaredValues, int bytesPerValue,
                             int availableBytes)
{
    if (declaredValues <= 0 || bytesPerValue <= 0
        || availableBytes < bytesPerValue) {
        return 0;
    }
    return std::min(declaredValues, availableBytes / bytesPerValue);
}

// From AetherSDR PanadapterStream.cpp:969 + :986-993 [@d58e2b8a].
float binToDbm(quint16 bin, double minDbm, double maxDbm, int yPixels)
{
    const double range = maxDbm - minDbm;
    if (range <= 0.0) {
        return static_cast<float>(minDbm);
    }
    // `yPix = max(yPixels, 2)` — unter 2 waere der Teiler null.
    const double yPix = static_cast<double>(std::max(yPixels, 2));
    const double pixel = std::clamp(static_cast<double>(bin), 0.0, yPix - 1.0);
    const double dbm = maxDbm - (pixel / (yPix - 1.0)) * range;
    return static_cast<float>(std::clamp(dbm, minDbm, maxDbm));
}

QVector<float> binsToDbm(const QVector<quint16>& bins,
                         double minDbm, double maxDbm, int yPixels)
{
    QVector<float> out;
    out.reserve(bins.size());
    for (const quint16 b : bins) {
        out.append(binToDbm(b, minDbm, maxDbm, yPixels));
    }
    return out;
}

// From AetherSDR PanadapterStream.cpp:1093-1095 [@d58e2b8a].
float tileColourValue(quint16 raw)
{
    return static_cast<float>(static_cast<qint16>(raw)) / 128.0f;
}

// From AetherSDR VitaBinCoverage.h:28-48 [@d58e2b8a] — die Pruefung
// ist eng gefasst, damit ein echtes uebersteuertes Signal nicht fuer
// den Platzhalter der Maschine gehalten wird: nur bei echtem Wachstum,
// mindestens 16 neue Bins, Anhang fast ganz null UND der alte Teil
// NICHT von Nullen beherrscht.
bool hasZeroFilledFftGrowthSuffix(const QVector<quint16>& bins,
                                  int previousAcceptedBins)
{
    if (previousAcceptedBins <= 0 || previousAcceptedBins >= bins.size()) {
        return false;
    }
    const int suffixBins = bins.size() - previousAcceptedBins;
    if (suffixBins < 16) {
        return false;
    }
    const int prefixZeros = static_cast<int>(std::count(
        bins.cbegin(), bins.cbegin() + previousAcceptedBins, quint16{0}));
    const int suffixZeros = static_cast<int>(std::count(
        bins.cbegin() + previousAcceptedBins, bins.cend(), quint16{0}));

    return suffixZeros * 10 >= suffixBins * 9
        && prefixZeros * 10 < previousAcceptedBins;
}

QVector<float> narrowAudioSamples(const QByteArray& datagram)
{
    QVector<float> out;
    if (datagram.size() <= kHeaderBytes) {
        return out;
    }
    // KEIN Unterkopf und KEINE Fuellung: nach dem Kopf direkt die
    // Werte. Gemessen: 1052 Byte = 28 + 1024 = 128 Rahmen
    // float32-Stereo, genau aufgehend.
    const int payloadBytes = datagram.size() - kHeaderBytes;
    const int count = payloadBytes / 4;
    if (count <= 0) {
        return out;
    }
    const uchar* src = bytes(datagram) + kHeaderBytes;

    out.resize(count);
    for (int i = 0; i < count; ++i) {
        // From AetherSDR PanadapterStream.cpp:1153-1156 — die Bytes
        // umdrehen und als float32 deuten, nicht ueber int16 rechnen.
        const quint32 u = qFromBigEndian<quint32>(src + i * 4);
        float f = 0.0f;
        std::memcpy(&f, &u, 4);
        out[i] = f;
    }
    return out;
}

}  // namespace FlexVita

// ─── FlexFftGrowthGuard ──────────────────────────────────────────────
// From AetherSDR VitaBinCoverage.h:103-127 [@d58e2b8a].

FlexFftGrowthAction FlexFftGrowthGuard::observe(bool zeroFilledGrowth,
                                                int totalBins)
{
    if (!zeroFilledGrowth || totalBins <= 0) {
        reset();
        return FlexFftGrowthAction::Accept;
    }
    if (totalBins != m_rejectedTotalBins) {
        m_rejectedTotalBins = totalBins;
        m_consecutiveRejected = 1;
    } else {
        ++m_consecutiveRejected;
    }
    return m_consecutiveRejected <= kRejectedFramesBeforeFloorFallback
        ? FlexFftGrowthAction::Reject
        : FlexFftGrowthAction::EmitWithFloorSuffix;
}

void FlexFftGrowthGuard::reset()
{
    m_rejectedTotalBins = 0;
    m_consecutiveRejected = 0;
}

// ─── FlexBinCoverage ─────────────────────────────────────────────────
// From AetherSDR VitaBinCoverage.h:52-86 [@d58e2b8a].

void FlexBinCoverage::reset(int totalValues)
{
    m_received.fill(0, std::max(0, totalValues));
    m_unique = 0;
}

bool FlexBinCoverage::markRange(int firstValue, int count)
{
    if (firstValue < 0 || count <= 0
        || firstValue > m_received.size() - count) {
        return false;
    }
    bool neu = false;
    for (int i = firstValue; i < firstValue + count; ++i) {
        if (m_received[i] == 0) {
            m_received[i] = 1;
            ++m_unique;
            neu = true;
        }
    }
    return neu;
}

bool FlexBinCoverage::isComplete() const
{
    return !m_received.isEmpty() && m_unique == m_received.size();
}

// ─── FlexFftAssembler ────────────────────────────────────────────────
// From AetherSDR PanadapterStream::decodeFFT:915-940 [@d58e2b8a].

std::optional<FlexFftAssembler::Frame>
FlexFftAssembler::feed(const QByteArray& datagram)
{
    const FlexVita::Header h = FlexVita::parseHeader(datagram);
    if (!h.valid
        || h.packetClass != static_cast<quint16>(FlexVitaClass::PanadapterFft)) {
        return std::nullopt;
    }
    const FlexVita::FftSubHeader s = FlexVita::parseFftSubHeader(datagram);
    if (!s.valid) {
        return std::nullopt;
    }

    const int dataOffset = FlexVita::kHeaderBytes + FlexVita::kFftSubBytes;
    const int available  = datagram.size() - dataOffset;
    // Hier NICHT nach Datagrammlaenge zaehlen: die vier Null-Byte am
    // Ende sind Fuellung, und 0 ist im Pixelcode Vollausschlag. Der
    // Unterkopf sagt, wie viele Bins es sind; `boundedPayloadValueCount`
    // kappt nur ein zu kurz geratenes Datagramm.
    const int binsToRead = FlexVita::boundedPayloadValueCount(
        s.numBins, s.binSize, available);
    if (binsToRead <= 0) {
        return std::nullopt;
    }

    Partial& p = m_partial[h.streamId];

    const bool neuerRahmen =
        s.frameIndex != p.frameIndex || s.totalBins != p.totalBins;
    if (neuerRahmen) {
        if (p.totalBins > 0 && !p.coverage.isComplete()) {
            ++m_framesRestarted;
        }
        p.frameIndex = s.frameIndex;
        p.totalBins  = s.totalBins;
        p.buf.fill(0, s.totalBins);
        p.coverage.reset(s.totalBins);
        ++m_framesStarted;
    }

    // Grenzen zuerst — danach wird geschrieben.
    if (static_cast<int>(s.startBin) + binsToRead > p.buf.size()) {
        return std::nullopt;
    }
    if (p.coverage.isComplete()) {
        return std::nullopt;   // schon abgeliefert
    }
    if (!p.coverage.markRange(s.startBin, binsToRead)) {
        return std::nullopt;   // nur Wiederholung, kein neuer Bin
    }

    const uchar* bin = bytes(datagram) + dataOffset;
    for (int i = 0; i < binsToRead; ++i) {
        p.buf[s.startBin + i] = qFromBigEndian<quint16>(bin + i * 2);
    }

    if (!p.coverage.isComplete()) {
        return std::nullopt;
    }

    ++m_framesCompleted;
    Frame f;
    f.streamId   = h.streamId;
    f.frameIndex = p.frameIndex;
    f.bins       = p.buf;
    return f;
}

// ─── FlexTileAssembler ───────────────────────────────────────────────
// From AetherSDR PanadapterStream::decodeWaterfallTile:1060-1104
// [@d58e2b8a].

std::optional<FlexTileAssembler::Row>
FlexTileAssembler::feed(const QByteArray& datagram)
{
    const FlexVita::Header h = FlexVita::parseHeader(datagram);
    if (!h.valid
        || h.packetClass != static_cast<quint16>(FlexVitaClass::WaterfallTile)) {
        return std::nullopt;
    }
    const FlexVita::TileSubHeader s = FlexVita::parseTileSubHeader(datagram);
    if (!s.valid) {
        return std::nullopt;
    }

    const int dataOffset = FlexVita::kHeaderBytes + FlexVita::kTileSubBytes;
    const int available  = datagram.size() - dataOffset;

    Partial& p = m_partial[h.streamId];

    // Neuer Rahmen bei Wechsel von `timecode` ODER `totalBinsInFrame`.
    // Die zweite Bedingung ist kein Schmuck: ohne sie laesst ein Paket,
    // das einen Zeitcode mit aufgeblaehtem `totalBinsInFrame`
    // wiederverwendet, den Puffer zu klein — und der Schreiblauf unten
    // ginge mit fremdgewaehlten Bytes darueber hinaus
    // (GHSA-7gvg-x594-pprq).
    if (s.timecode != p.timecode
        || static_cast<int>(s.totalBinsInFrame) != p.totalBins) {
        if (p.totalBins > 0 && !p.coverage.isComplete()) {
            ++m_rowsRestarted;
        }
        p.timecode        = s.timecode;
        p.totalBins       = s.totalBinsInFrame;
        p.lowFreqMhz      = s.lowFreqMhz;
        p.binBandwidthMhz = s.binBandwidthMhz;
        p.autoBlackLevel  = s.autoBlackLevel;
        p.buf.fill(0, s.totalBinsInFrame);
        p.coverage.reset(s.totalBinsInFrame);
        ++m_rowsStarted;
    }

    // Nur die erste Zeile; `tileHeight` ist gemessen immer 1.
    const int declared = std::min(static_cast<int>(s.tileWidth),
                                  static_cast<int>(s.totalBinsInFrame)
                                      - static_cast<int>(s.firstBinIndex));
    const int toRead = FlexVita::boundedPayloadValueCount(
        declared, static_cast<int>(sizeof(quint16)), available);
    if (toRead <= 0) {
        return std::nullopt;
    }

    // Tiefenstaffelung: selbst mit der Rahmenbedingung oben wird der
    // obere Schreibindex noch einmal gegen den Puffer geprueft — gegen
    // jeden kuenftigen Umbau, der die Zusage
    // `buf.size() == totalBins` bricht (GHSA-7gvg-x594-pprq).
    if (static_cast<int>(s.firstBinIndex) + toRead > p.buf.size()) {
        return std::nullopt;
    }
    if (p.coverage.isComplete()) {
        return std::nullopt;
    }
    if (!p.coverage.markRange(s.firstBinIndex, toRead)) {
        return std::nullopt;
    }

    const uchar* tile = bytes(datagram) + dataOffset;
    for (int i = 0; i < toRead; ++i) {
        p.buf[s.firstBinIndex + i] = qFromBigEndian<quint16>(tile + i * 2);
    }

    if (!p.coverage.isComplete()) {
        return std::nullopt;
    }

    ++m_rowsCompleted;
    Row r;
    r.streamId       = h.streamId;
    r.timecode       = p.timecode;
    r.lowFreqMhz     = p.lowFreqMhz;
    r.highFreqMhz    = p.lowFreqMhz + p.binBandwidthMhz * p.totalBins;
    r.autoBlackLevel = p.autoBlackLevel;
    r.values         = p.buf;
    return r;
}

}  // namespace Longpath
