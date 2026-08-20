// =================================================================
// src/core/strip/TargetFromFile.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. See TargetFromFile.h for why a recording beats
// an adjective.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-08 — Created in C++20/Qt6 for NereusSDR by Martin Fischer,
//                 AI-assisted via Anthropic Claude (Cowork).
// =================================================================

#include "core/strip/TargetFromFile.h"

#include "core/strip/StripTargets.h"

#include <QFile>

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace Longpath::TargetFromFile {

namespace {

constexpr int kFft = 4096;

uint32_t rd32(const char* p)
{
    return uint32_t(uint8_t(p[0]))
         | (uint32_t(uint8_t(p[1])) << 8)
         | (uint32_t(uint8_t(p[2])) << 16)
         | (uint32_t(uint8_t(p[3])) << 24);
}

uint16_t rd16(const char* p)
{
    return uint16_t(uint16_t(uint8_t(p[0])) | (uint16_t(uint8_t(p[1])) << 8));
}

void fftInPlace(std::vector<std::complex<double>>& a)
{
    const size_t n = a.size();
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) { j ^= bit; }
        j ^= bit;
        if (i < j) { std::swap(a[i], a[j]); }
    }
    for (size_t len = 2; len <= n; len <<= 1) {
        const double ang = -2.0 * M_PI / double(len);
        const std::complex<double> wl(std::cos(ang), std::sin(ang));
        for (size_t i = 0; i < n; i += len) {
            std::complex<double> w(1.0, 0.0);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<double> u = a[i + k];
                const std::complex<double> v = a[i + k + len / 2] * w;
                a[i + k] = u + v;
                a[i + k + len / 2] = u - v;
                w *= wl;
            }
        }
    }
}

} // namespace

WavInfo parseWavHeader(const char* bytes, int64_t size)
{
    WavInfo info;
    if (!bytes || size < 12) {
        info.error = QStringLiteral("Not a WAV file — too short to hold a "
                                    "header.");
        return info;
    }
    if (std::memcmp(bytes, "RIFF", 4) != 0
        || std::memcmp(bytes + 8, "WAVE", 4) != 0) {
        info.error = QStringLiteral("Not a WAV file. Convert the recording "
                                    "to uncompressed WAV first.");
        return info;
    }

    // Walk the chunks. Their order is not fixed and real files put all
    // sorts between `fmt ` and `data`; a reader that assumes byte 44 is
    // the first sample plays several kilobytes of metadata as audio.
    bool haveFmt = false;
    int64_t pos = 12;
    uint16_t format = 0;
    while (pos + 8 <= size) {
        const char* id = bytes + pos;
        const uint32_t len = rd32(bytes + pos + 4);
        const int64_t body = pos + 8;
        if (std::memcmp(id, "fmt ", 4) == 0 && len >= 16
            && body + 16 <= size) {
            format              = rd16(bytes + body);
            info.channels       = rd16(bytes + body + 2);
            info.sampleRate     = int(rd32(bytes + body + 4));
            info.bitsPerSample  = rd16(bytes + body + 14);
            // WAVE_FORMAT_EXTENSIBLE hides the real format in a GUID
            // whose first two bytes are the old tag.
            if (format == 0xFFFE && len >= 40 && body + 26 <= size) {
                format = rd16(bytes + body + 24);
            }
            info.isFloat = (format == 3);
            haveFmt = true;
        } else if (std::memcmp(id, "data", 4) == 0) {
            info.dataOffset = body;
            info.dataBytes  = std::min<int64_t>(len, size - body);
            break;
        }
        pos = body + len + (len & 1);   // chunks are word-aligned
    }

    if (!haveFmt || info.dataBytes <= 0) {
        info.error = QStringLiteral("WAV file is missing its format or data "
                                    "chunk.");
        return info;
    }
    if (format != 1 && format != 3) {
        info.error = QStringLiteral("This WAV is compressed. Only "
                                    "uncompressed PCM or float WAV can be "
                                    "read here — export it again as PCM.");
        return info;
    }
    if (info.channels < 1 || info.sampleRate < 4000) {
        info.error = QStringLiteral("WAV header does not make sense "
                                    "(%1 channels at %2 Hz).")
                         .arg(info.channels).arg(info.sampleRate);
        return info;
    }
    if (!(info.bitsPerSample == 8 || info.bitsPerSample == 16
          || info.bitsPerSample == 24 || info.bitsPerSample == 32)) {
        info.error = QStringLiteral("%1-bit samples are not supported.")
                         .arg(info.bitsPerSample);
        return info;
    }

    info.ok = true;
    return info;
}

std::vector<float> decodeMono(const char* bytes, int64_t size,
                              const WavInfo& info)
{
    std::vector<float> out;
    if (!info.ok || !bytes) { return out; }

    const int bps    = info.bitsPerSample / 8;
    const int stride = bps * info.channels;
    if (stride <= 0) { return out; }

    const int64_t avail = std::min(info.dataBytes, size - info.dataOffset);
    const int64_t frames = avail / stride;
    if (frames <= 0) { return out; }

    out.resize(static_cast<size_t>(frames));
    const char* d = bytes + info.dataOffset;

    for (int64_t fr = 0; fr < frames; ++fr) {
        double sum = 0.0;
        for (int ch = 0; ch < info.channels; ++ch) {
            const char* s = d + fr * stride + int64_t(ch) * bps;
            double v = 0.0;
            if (info.isFloat) {
                if (bps == 4) {
                    float f = 0.0f;
                    std::memcpy(&f, s, sizeof(f));
                    v = double(f);
                } else {
                    double g = 0.0;
                    std::memcpy(&g, s, sizeof(g));
                    v = g;
                }
            } else if (bps == 1) {
                // 8-bit WAV is unsigned, alone among the PCM widths.
                v = (double(uint8_t(s[0])) - 128.0) / 128.0;
            } else if (bps == 2) {
                v = double(int16_t(rd16(s))) / 32768.0;
            } else if (bps == 3) {
                int32_t x = int32_t(uint8_t(s[0]))
                          | (int32_t(uint8_t(s[1])) << 8)
                          | (int32_t(uint8_t(s[2])) << 16);
                if (x & 0x800000) { x |= int32_t(0xFF000000u); }
                v = double(x) / 8388608.0;
            } else {
                v = double(int32_t(rd32(s))) / 2147483648.0;
            }
            sum += v;
        }
        out[static_cast<size_t>(fr)] = float(sum / info.channels);
    }
    return out;
}

QVector<double> ltasAtTargetPoints(const std::vector<float>& mono,
                                   int sampleRate, QString* error)
{
    QVector<double> out;
    auto fail = [&](const QString& why) {
        if (error) { *error = why; }
        return QVector<double>();
    };

    if (sampleRate < 4000) { return fail(QStringLiteral("Sample rate is "
                                                        "too low to use.")); }
    if (mono.size() < size_t(kFft) * 2) {
        return fail(QStringLiteral("Recording is too short — about a second "
                                   "of sound is the least that can be "
                                   "averaged into a useful shape."));
    }

    const int bins = kFft / 2;
    std::vector<double> power(static_cast<size_t>(bins), 0.0);
    int windows = 0;

    std::vector<std::complex<double>> a(kFft);
    const size_t n = mono.size();
    for (size_t start = 0; start + kFft <= n; start += kFft / 2) {
        // Skip the pauses. A recording usually opens and closes with
        // silence, and averaging that in pulls the whole curve toward
        // the noise floor and flattens the peaks the target exists to
        // capture.
        double rms = 0.0;
        for (int i = 0; i < kFft; ++i) {
            const double s = mono[start + size_t(i)];
            rms += s * s;
        }
        rms = std::sqrt(rms / kFft);
        if (rms < std::pow(10.0, kSilenceDbFs / 20.0)) { continue; }

        for (int i = 0; i < kFft; ++i) {
            const double w =
                0.5 * (1.0 - std::cos(2.0 * M_PI * i / (kFft - 1)));
            a[static_cast<size_t>(i)] =
                std::complex<double>(mono[start + size_t(i)] * w, 0.0);
        }
        fftInPlace(a);
        for (int i = 0; i < bins; ++i) {
            const double m = std::abs(a[static_cast<size_t>(i)]) / (kFft / 4.0);
            power[static_cast<size_t>(i)] += m * m;
        }
        ++windows;
    }

    if (windows == 0) {
        return fail(QStringLiteral("The recording is silent, or so quiet "
                                   "that nothing in it can be measured."));
    }

    std::vector<double> db(static_cast<size_t>(bins), -120.0);
    for (int i = 0; i < bins; ++i) {
        const double mean = power[static_cast<size_t>(i)] / windows;
        db[static_cast<size_t>(i)] =
            mean > 1e-18 ? 10.0 * std::log10(mean) : -120.0;
    }

    // Smoothed to a third of an octave before sampling. The raw
    // spectrum of a voice is a comb of that speaker's harmonics; a
    // target taken from the comb would be a target for that speaker's
    // pitch, which nobody else has and which they themselves do not
    // have on a different day.
    const double binHz  = double(sampleRate) / kFft;
    const double factor = std::pow(2.0, 1.0 / 6.0);
    auto smoothed = [&](double hz) {
        const int lo = std::max(1, int(hz / factor / binHz));
        const int hi = std::min(bins - 1, int(hz * factor / binHz));
        if (hi < lo) { return -120.0; }
        double sum = 0.0;
        for (int k = lo; k <= hi; ++k) { sum += db[static_cast<size_t>(k)]; }
        return sum / (hi - lo + 1);
    };

    const double nyq = sampleRate / 2.0;

    // Where the recording actually has energy. Everything below
    // peak - kDynamicRangeDb is leakage and rounding, not sound; see
    // the header for what happens when that is treated as a
    // measurement.
    double peak = -1000.0;
    for (int k = 0; k < 40; ++k) {
        const double hz = 100.0 * std::pow(std::min(6000.0, nyq * 0.95)
                                               / 100.0,
                                           double(k) / 39.0);
        peak = std::max(peak, smoothed(hz));
    }
    const double floorDb = peak - kDynamicRangeDb;

    const double ref = (1000.0 < nyq) ? smoothed(1000.0) : peak;
    if (ref < floorDb) {
        return fail(QStringLiteral(
            "There is almost nothing at 1 kHz in this recording, so there "
            "is no sensible level to measure the rest against. This does "
            "not look like speech — a tone, a test signal or a badly "
            "filtered file will do this."));
    }

    const double* f = StripTargets::userPointFreqs();
    out.reserve(StripTargets::kUserPointCount);
    for (int i = 0; i < StripTargets::kUserPointCount; ++i) {
        // Above Nyquist the file says nothing. Reporting -120 there
        // would tell the operator to cut a band the recording simply
        // never covered; carrying the last real value forward says "no
        // opinion", which is the truth.
        const double hz = f[i];
        out.append(hz >= nyq ? (out.isEmpty() ? 0.0 : out.last())
                             : std::max(smoothed(hz), floorDb) - ref);
    }
    return out;
}

QVector<double> fromWavFile(const QString& path, QString* error)
{
    auto fail = [&](const QString& why) {
        if (error) { *error = why; }
        return QVector<double>();
    };

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("Cannot open %1.").arg(path));
    }
    // Sixty seconds at 48 kHz stereo 24-bit is about 17 MB; a cap keeps
    // a mistakenly chosen hour-long file from being pulled into memory
    // whole, and a minute is far more than the average needs.
    const QByteArray raw = file.read(48LL * 1024 * 1024);
    file.close();

    const WavInfo info = parseWavHeader(raw.constData(), raw.size());
    if (!info.ok) { return fail(info.error); }

    const std::vector<float> mono =
        decodeMono(raw.constData(), raw.size(), info);
    if (mono.empty()) {
        return fail(QStringLiteral("No audio could be read from the file."));
    }
    return ltasAtTargetPoints(mono, info.sampleRate, error);
}

} // namespace Longpath::TargetFromFile
