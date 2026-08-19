// =================================================================
// src/core/audio/WavFile.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Begruendung und Umfang stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/WavFile.h"

#include <QFile>
#include <QtEndian>

#include <cmath>
#include <cstring>
#include <random>

namespace NereusSDR {

namespace {

// Ein Blockkopf im RIFF-Format: vier Zeichen Kennung, vier Bytes Laenge.
constexpr int kChunkHeaderBytes = 8;

// Formatkennungen aus der WAV-Spezifikation.
constexpr quint16 kFormatPcm        = 0x0001;
constexpr quint16 kFormatIeeeFloat  = 0x0003;
constexpr quint16 kFormatExtensible = 0xFFFE;

quint16 readU16(const char* p)
{
    return qFromLittleEndian<quint16>(reinterpret_cast<const uchar*>(p));
}

quint32 readU32(const char* p)
{
    return qFromLittleEndian<quint32>(reinterpret_cast<const uchar*>(p));
}

void fail(QString* error, const QString& text)
{
    if (error) { *error = text; }
}

// Einen Abtastwert aus dem Rohblock holen und auf -1..+1 bringen.
float sampleAt(const char* data, int byteOffset, quint16 format, quint16 bits)
{
    if (format == kFormatIeeeFloat && bits == 32) {
        float v = 0.0f;
        std::memcpy(&v, data + byteOffset, sizeof(float));
        return v;
    }

    switch (bits) {
    case 8: {
        // 8 Bit ist im WAV VORZEICHENLOS mit 128 als Ruhelage — der
        // einzige Fall, in dem das so ist. Wer das uebersieht, bekommt
        // eine Aufnahme mit einem Gleichanteil von einem halben
        // Vollausschlag.
        const quint8 raw = static_cast<quint8>(data[byteOffset]);
        return (static_cast<float>(raw) - 128.0f) / 128.0f;
    }
    case 16: {
        const qint16 raw = static_cast<qint16>(readU16(data + byteOffset));
        return static_cast<float>(raw) / 32768.0f;
    }
    case 24: {
        // Drei Bytes, vorzeichenbehaftet. Das oberste Byte traegt das
        // Vorzeichen; heraufziehen und wieder herunterschieben ist der
        // kuerzeste Weg, es zu erhalten.
        const qint32 raw =
            (static_cast<qint32>(static_cast<quint8>(data[byteOffset]))      <<  8)
          | (static_cast<qint32>(static_cast<quint8>(data[byteOffset + 1]))  << 16)
          | (static_cast<qint32>(static_cast<qint8> (data[byteOffset + 2]))  << 24);
        return static_cast<float>(raw >> 8) / 8388608.0f;
    }
    case 32: {
        const qint32 raw = static_cast<qint32>(readU32(data + byteOffset));
        return static_cast<float>(raw) / 2147483648.0f;
    }
    default:
        return 0.0f;
    }
}

} // namespace

WavData readWavMono(const QString& path, QString* error)
{
    WavData out;

    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        fail(error, QStringLiteral("cannot open %1").arg(path));
        return out;
    }
    const QByteArray blob = f.readAll();
    f.close();

    if (blob.size() < 12
        || std::memcmp(blob.constData(), "RIFF", 4) != 0
        || std::memcmp(blob.constData() + 8, "WAVE", 4) != 0) {
        fail(error, QStringLiteral("not a WAV file"));
        return out;
    }

    quint16 format = 0, channels = 0, bits = 0;
    quint32 rate = 0;
    const char* dataPtr = nullptr;
    quint32 dataLen = 0;

    // Bloecke der Reihe nach durchgehen statt Byte 44 zu glauben: viele
    // Werkzeuge schieben LIST- oder fact-Bloecke zwischen fmt und data,
    // und ein Leser mit fester Kopfgroesse liest dann Text als Ton.
    int pos = 12;
    while (pos + kChunkHeaderBytes <= blob.size()) {
        const char* id = blob.constData() + pos;
        const quint32 len = readU32(blob.constData() + pos + 4);
        const int body = pos + kChunkHeaderBytes;
        if (body + static_cast<int>(len) > blob.size()) { break; }

        if (std::memcmp(id, "fmt ", 4) == 0 && len >= 16) {
            format   = readU16(blob.constData() + body);
            channels = readU16(blob.constData() + body + 2);
            rate     = readU32(blob.constData() + body + 4);
            bits     = readU16(blob.constData() + body + 14);

            // WAVE_FORMAT_EXTENSIBLE versteckt das eigentliche Format in
            // einer GUID; deren erste zwei Bytes sind die Kennung.
            if (format == kFormatExtensible && len >= 40) {
                format = readU16(blob.constData() + body + 24);
            }
        } else if (std::memcmp(id, "data", 4) == 0) {
            dataPtr = blob.constData() + body;
            dataLen = len;
        }

        // Bloecke ungerader Laenge sind auf gerade aufgefuellt.
        pos = body + static_cast<int>(len) + (len & 1u);
    }

    if (dataPtr == nullptr || rate == 0 || channels == 0 || bits == 0) {
        fail(error, QStringLiteral("WAV without usable fmt/data chunk"));
        return out;
    }
    if (format != kFormatPcm && format != kFormatIeeeFloat) {
        fail(error, QStringLiteral("unsupported WAV format 0x%1")
                        .arg(format, 4, 16, QLatin1Char('0')));
        return out;
    }
    if (bits != 8 && bits != 16 && bits != 24 && bits != 32) {
        fail(error, QStringLiteral("unsupported bit depth %1").arg(bits));
        return out;
    }

    const int bytesPerSample = bits / 8;
    const int frameBytes     = bytesPerSample * channels;
    if (frameBytes <= 0) {
        fail(error, QStringLiteral("WAV with zero-size frames"));
        return out;
    }
    const int frames = static_cast<int>(dataLen) / frameBytes;

    out.samples.resize(frames);
    for (int i = 0; i < frames; ++i) {
        // Kanaele mitteln: ein Sprachspeicher ist einkanalig, und den
        // linken zu nehmen verliert alles, was jemand rechts
        // eingesprochen hat.
        float sum = 0.0f;
        for (int c = 0; c < channels; ++c) {
            sum += sampleAt(dataPtr, i * frameBytes + c * bytesPerSample,
                            format, bits);
        }
        out.samples[i] = sum / static_cast<float>(channels);
    }

    out.sampleRate = static_cast<int>(rate);
    out.ok = true;
    return out;
}

bool writeWavMono(const QString& path, const QVector<float>& samples,
                  int sampleRate, QString* error)
{
    if (sampleRate <= 0) {
        fail(error, QStringLiteral("sample rate must be positive"));
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(error, QStringLiteral("cannot write %1").arg(path));
        return false;
    }

    const quint32 dataBytes = static_cast<quint32>(samples.size()) * 4u;
    const quint32 riffBytes = 36u + dataBytes;

    auto putU32 = [&f](quint32 v) {
        uchar b[4];
        qToLittleEndian(v, b);
        f.write(reinterpret_cast<const char*>(b), 4);
    };
    auto putU16 = [&f](quint16 v) {
        uchar b[2];
        qToLittleEndian(v, b);
        f.write(reinterpret_cast<const char*>(b), 2);
    };

    f.write("RIFF", 4);
    putU32(riffBytes);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    putU32(16);
    putU16(kFormatIeeeFloat);
    putU16(1);                                  // einkanalig
    putU32(static_cast<quint32>(sampleRate));
    putU32(static_cast<quint32>(sampleRate) * 4u);  // Bytes je Sekunde
    putU16(4);                                  // Bytes je Rahmen
    putU16(32);                                 // Bits

    f.write("data", 4);
    putU32(dataBytes);
    for (float v : samples) {
        uchar b[4];
        std::memcpy(b, &v, 4);
        f.write(reinterpret_cast<const char*>(b), 4);
    }

    f.close();
    return true;
}

bool writeWavStereo(const QString& path, const QVector<float>& interleaved,
                    int sampleRate, QString* error)
{
    if (sampleRate <= 0) {
        fail(error, QStringLiteral("sample rate must be positive"));
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(error, QStringLiteral("cannot write %1").arg(path));
        return false;
    }

    const quint32 dataBytes = static_cast<quint32>(interleaved.size()) * 4u;

    auto putU32 = [&f](quint32 v) {
        uchar b[4]; qToLittleEndian(v, b); f.write(reinterpret_cast<const char*>(b), 4);
    };
    auto putU16 = [&f](quint16 v) {
        uchar b[2]; qToLittleEndian(v, b); f.write(reinterpret_cast<const char*>(b), 2);
    };

    f.write("RIFF", 4);
    putU32(36u + dataBytes);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    putU32(16);
    putU16(kFormatIeeeFloat);
    putU16(2);                                       // zwei Kanaele
    putU32(static_cast<quint32>(sampleRate));
    putU32(static_cast<quint32>(sampleRate) * 8u);   // Bytes je Sekunde
    putU16(8);                                       // Bytes je Rahmen
    putU16(32);

    f.write("data", 4);
    putU32(dataBytes);
    for (float v : interleaved) {
        uchar b[4];
        std::memcpy(b, &v, 4);
        f.write(reinterpret_cast<const char*>(b), 4);
    }

    f.close();
    return true;
}

bool writeWavStereo16(const QString& path, const QVector<float>& interleaved,
                      int sampleRate, bool dither, QString* error)
{
    if (sampleRate <= 0) {
        fail(error, QStringLiteral("sample rate must be positive"));
        return false;
    }

    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        fail(error, QStringLiteral("cannot write %1").arg(path));
        return false;
    }

    const quint32 dataBytes = static_cast<quint32>(interleaved.size()) * 2u;

    auto putU32 = [&f](quint32 v) {
        uchar b[4]; qToLittleEndian(v, b); f.write(reinterpret_cast<const char*>(b), 4);
    };
    auto putU16 = [&f](quint16 v) {
        uchar b[2]; qToLittleEndian(v, b); f.write(reinterpret_cast<const char*>(b), 2);
    };

    f.write("RIFF", 4);
    putU32(36u + dataBytes);
    f.write("WAVE", 4);

    f.write("fmt ", 4);
    putU32(16);
    putU16(kFormatPcm);
    putU16(2);                                       // zwei Kanaele
    putU32(static_cast<quint32>(sampleRate));
    putU32(static_cast<quint32>(sampleRate) * 4u);   // Bytes je Sekunde
    putU16(4);                                       // Bytes je Rahmen
    putU16(16);

    f.write("data", 4);
    putU32(dataBytes);

    // Fester Startwert: eine Aufnahme, die zweimal gespeichert wird,
    // soll zweimal dieselbe Datei ergeben. Zufall, der sich nicht
    // wiederholen laesst, macht jeden Vergleich unmoeglich.
    std::mt19937 rng(20260819u);
    std::uniform_real_distribution<float> half(-0.5f, 0.5f);

    QByteArray block;
    block.reserve(interleaved.size() * 2);
    for (float v : interleaved) {
        // Dreieckverteilt: zwei gleichverteilte Zufallszahlen addiert.
        // Gleichverteilter Dither allein loest die Modulation des
        // Rundungsfehlers nur halb auf.
        float x = v * 32767.0f;
        if (dither) { x += half(rng) + half(rng); }

        const int r = static_cast<int>(std::lround(x));
        const qint16 s16 = static_cast<qint16>(std::clamp(r, -32768, 32767));

        uchar b[2];
        qToLittleEndian(s16, b);
        block.append(reinterpret_cast<const char*>(b), 2);
    }
    f.write(block);

    f.close();
    return true;
}

double wavDurationSeconds(const QString& path)
{
    const WavData d = readWavMono(path, nullptr);
    if (!d.ok || d.sampleRate <= 0) { return 0.0; }
    return static_cast<double>(d.samples.size()) / d.sampleRate;
}

} // namespace NereusSDR
