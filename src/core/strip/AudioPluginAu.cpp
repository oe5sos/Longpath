// =================================================================
// src/core/strip/AudioPluginAu.cpp  (Longpath)
// =================================================================
//
// Longpath-original. Der Wirt fuer Audio Units auf dem Mac.
//
// Warum ausgerechnet AU zuerst, und nicht VST3: AU ist auf dem Mac
// Systemsache. Kein fremdes SDK im Baum, kein zweites Lizenzverhaeltnis,
// keine Abhaengigkeit, die jemand nachinstallieren muss -- eine
// C-Schnittstelle, die seit zwanzig Jahren steht. VST3 ist seit Ende
// 2025 MIT-lizenziert und damit fuer Windows und Linux offen; es kommt
// daneben, nicht an diese Stelle.
//
// Die Oberflaeche des Plugins wird NICHT geladen. Begruendung in
// AudioPlugin.h; kurz: wir lesen die Parameter aus und zeichnen sie in
// unseren eigenen Reglern.
//
// Auf anderen Plattformen uebersetzt diese Datei zu drei leeren
// Antworten. available() sagt dann „hier gibt es das nicht", was
// etwas anderes ist als „hier ist nichts installiert".
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20/Qt6, AI-assisted via Anthropic
//                 Claude (Claude Code), operator Martin Fischer.
// =================================================================

#include "core/strip/AudioPlugin.h"

#include "core/LogCategories.h"
#include "core/strip/AuPluginEditor.h"

#include <QtGlobal>

#ifdef Q_OS_MACOS

#include <AudioToolbox/AudioToolbox.h>
#include <CoreFoundation/CoreFoundation.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace Longpath {

namespace {

QString fromCfString(CFStringRef s)
{
    if (s == nullptr) { return {}; }
    const CFIndex len = CFStringGetLength(s);
    const CFIndex max = CFStringGetMaximumSizeForEncoding(len, kCFStringEncodingUTF8) + 1;
    std::vector<char> buf(static_cast<size_t>(max), 0);
    if (!CFStringGetCString(s, buf.data(), max, kCFStringEncodingUTF8)) { return {}; }
    return QString::fromUtf8(buf.data());
}

// Der Bezeichner, der in die Einstellungen wandert. Hexadezimal und
// nicht als Vierzeichenkuerzel, weil ein OSType jedes Byte enthalten
// darf -- auch solche, die in einer Einstellungsdatei nichts verloren
// haben.
QString identifierOf(const AudioComponentDescription& d)
{
    return QStringLiteral("au:%1:%2:%3")
        .arg(uint(d.componentType),         8, 16, QLatin1Char('0'))
        .arg(uint(d.componentSubType),      8, 16, QLatin1Char('0'))
        .arg(uint(d.componentManufacturer), 8, 16, QLatin1Char('0'));
}

bool descriptionFrom(const QString& identifier, AudioComponentDescription& out)
{
    const QStringList parts = identifier.split(QLatin1Char(':'));
    if (parts.size() != 4 || parts.at(0) != QLatin1String("au")) { return false; }
    bool a = false, b = false, c = false;
    out = AudioComponentDescription{};
    out.componentType         = OSType(parts.at(1).toUInt(&a, 16));
    out.componentSubType      = OSType(parts.at(2).toUInt(&b, 16));
    out.componentManufacturer = OSType(parts.at(3).toUInt(&c, 16));
    return a && b && c;
}

class AuPlugin final : public AudioPlugin {
public:
    AuPlugin(AudioComponentInstance unit, PluginDescriptor desc)
        : m_unit(unit), m_desc(std::move(desc)) {}

    ~AuPlugin() override
    {
        if (m_unit != nullptr) {
            // ZUERST das Fenster, dann die Audio Unit. Ein Fenster,
            // dessen Ansicht auf eine freigegebene Unit zeigt,
            // stuerzt beim naechsten Mausklick ab -- irgendwo tief in
            // fremdem Code, mit einem Protokoll, das nichts erklaert.
            AuPluginEditor::close(m_unit);
            AudioUnitUninitialize(m_unit);
            AudioComponentInstanceDispose(m_unit);
        }
    }

    bool hasEditor() const override
    {
        return m_unit != nullptr && AuPluginEditor::available();
    }

    bool showEditor(const QString& title) override
    {
        return AuPluginEditor::show(m_unit, title);
    }

    void closeEditor() override { AuPluginEditor::close(m_unit); }

    PluginDescriptor descriptor() const override { return m_desc; }

    bool prepare(double sampleRate, int maxBlockFrames) override
    {
        if (m_unit == nullptr) { return false; }
        if (m_initialised) { AudioUnitUninitialize(m_unit); m_initialised = false; }

        m_maxFrames = std::max(1, maxBlockFrames);

        // Erst einkanalig versuchen, dann zweikanalig. Viele Effekte
        // sind stereo und nur stereo; die bekommen dasselbe Signal auf
        // beide Seiten und wir hoeren links zurueck. Das ist kein
        // Rundungsfehler, sondern die einzige Art, ein Stereogeraet in
        // eine einkanalige Sendekette zu haengen, ohne zu luegen --
        // und was ein Plugin aus zwei gleichen Seiten macht, hoert man.
        for (const int channels : {1, 2}) {
            if (configure(sampleRate, channels)) {
                m_channels = channels;
                m_in.assign(size_t(m_channels * m_maxFrames), 0.0f);
                m_out.assign(size_t(m_channels * m_maxFrames), 0.0f);
                m_abl.assign(sizeof(AudioBufferList)
                             + sizeof(AudioBuffer) * size_t(std::max(0, m_channels - 1)),
                             0);
                m_sampleTime = 0.0;
                m_initialised = true;
                return true;
            }
        }
        qCWarning(lcAudio) << "AU plugin refuses mono and stereo:" << m_desc.name;
        return false;
    }

    void processMono(float* samples, int frames) noexcept override
    {
        if (!m_initialised || m_unit == nullptr
            || frames <= 0 || frames > m_maxFrames) {
            return;
        }

        for (int c = 0; c < m_channels; ++c) {
            std::copy_n(samples, frames, m_in.data() + size_t(c * m_maxFrames));
        }

        auto* abl = reinterpret_cast<AudioBufferList*>(m_abl.data());
        abl->mNumberBuffers = UInt32(m_channels);
        for (int c = 0; c < m_channels; ++c) {
            abl->mBuffers[c].mNumberChannels = 1;
            abl->mBuffers[c].mDataByteSize   = UInt32(frames * int(sizeof(float)));
            abl->mBuffers[c].mData           = m_out.data() + size_t(c * m_maxFrames);
        }

        AudioUnitRenderActionFlags flags = 0;
        AudioTimeStamp ts{};
        ts.mSampleTime = m_sampleTime;
        ts.mFlags      = kAudioTimeStampSampleTimeValid;

        const OSStatus err =
            AudioUnitRender(m_unit, &flags, &ts, 0, UInt32(frames), abl);
        m_sampleTime += double(frames);
        if (err != noErr) { return; }   // Block bleibt, wie er kam

        // Zurueckgelesen wird, worauf mData JETZT zeigt: ein Plugin
        // darf den Zeiger austauschen, statt in unseren Puffer zu
        // schreiben.
        if (const auto* got = static_cast<const float*>(abl->mBuffers[0].mData)) {
            std::copy_n(got, frames, samples);
        }
    }

    void reset() noexcept override
    {
        if (m_unit != nullptr && m_initialised) {
            AudioUnitReset(m_unit, kAudioUnitScope_Global, 0);
        }
        m_sampleTime = 0.0;
    }

    QVector<PluginParameter> parameters() const override
    {
        QVector<PluginParameter> out;
        if (m_unit == nullptr) { return out; }

        UInt32 size = 0;
        Boolean writable = false;
        if (AudioUnitGetPropertyInfo(m_unit, kAudioUnitProperty_ParameterList,
                                     kAudioUnitScope_Global, 0, &size, &writable)
            != noErr || size == 0) {
            return out;
        }
        std::vector<AudioUnitParameterID> ids(size / sizeof(AudioUnitParameterID));
        if (AudioUnitGetProperty(m_unit, kAudioUnitProperty_ParameterList,
                                 kAudioUnitScope_Global, 0, ids.data(), &size)
            != noErr) {
            return out;
        }

        for (const AudioUnitParameterID id : ids) {
            AudioUnitParameterInfo info{};
            UInt32 infoSize = sizeof(info);
            if (AudioUnitGetProperty(m_unit, kAudioUnitProperty_ParameterInfo,
                                     kAudioUnitScope_Global, id, &info, &infoSize)
                != noErr) {
                continue;
            }
            PluginParameter p;
            p.id = quint32(id);
            if ((info.flags & kAudioUnitParameterFlag_HasCFNameString) != 0
                && info.cfNameString != nullptr) {
                p.name = fromCfString(info.cfNameString);
                if ((info.flags & kAudioUnitParameterFlag_CFNameRelease) != 0) {
                    CFRelease(info.cfNameString);
                }
            }
            if (p.name.isEmpty()) {
                p.name = QString::fromUtf8(info.name);
            }
            if (p.name.isEmpty()) {
                p.name = QStringLiteral("Parameter %1").arg(uint(id));
            }
            p.minValue     = info.minValue;
            p.maxValue     = info.maxValue;
            p.defaultValue = info.defaultValue;
            p.isSwitch     = info.unit == kAudioUnitParameterUnit_Boolean;
            p.unit         = unitName(info.unit);
            out.push_back(p);
        }
        return out;
    }

    float parameterValue(quint32 id) const override
    {
        AudioUnitParameterValue v = 0.0f;
        if (m_unit == nullptr) { return 0.0f; }
        AudioUnitGetParameter(m_unit, AudioUnitParameterID(id),
                              kAudioUnitScope_Global, 0, &v);
        return float(v);
    }

    void setParameterValue(quint32 id, float value) override
    {
        if (m_unit == nullptr) { return; }
        AudioUnitSetParameter(m_unit, AudioUnitParameterID(id),
                              kAudioUnitScope_Global, 0,
                              AudioUnitParameterValue(value), 0);
    }

    QByteArray saveState() const override
    {
        if (m_unit == nullptr) { return {}; }
        CFPropertyListRef plist = nullptr;
        UInt32 size = sizeof(plist);
        if (AudioUnitGetProperty(m_unit, kAudioUnitProperty_ClassInfo,
                                 kAudioUnitScope_Global, 0, &plist, &size)
            != noErr || plist == nullptr) {
            return {};
        }
        CFDataRef data = CFPropertyListCreateData(
            kCFAllocatorDefault, plist, kCFPropertyListBinaryFormat_v1_0, 0, nullptr);
        CFRelease(plist);
        if (data == nullptr) { return {}; }
        const QByteArray out(reinterpret_cast<const char*>(CFDataGetBytePtr(data)),
                             int(CFDataGetLength(data)));
        CFRelease(data);
        return out;
    }

    bool restoreState(const QByteArray& blob) override
    {
        if (m_unit == nullptr || blob.isEmpty()) { return false; }
        CFDataRef data = CFDataCreate(
            kCFAllocatorDefault,
            reinterpret_cast<const UInt8*>(blob.constData()), blob.size());
        if (data == nullptr) { return false; }
        CFPropertyListRef plist = CFPropertyListCreateWithData(
            kCFAllocatorDefault, data, kCFPropertyListImmutable, nullptr, nullptr);
        CFRelease(data);
        if (plist == nullptr) { return false; }
        const OSStatus err =
            AudioUnitSetProperty(m_unit, kAudioUnitProperty_ClassInfo,
                                 kAudioUnitScope_Global, 0, &plist, sizeof(plist));
        CFRelease(plist);
        return err == noErr;
    }

    int latencyFrames() const override
    {
        if (m_unit == nullptr) { return 0; }
        Float64 seconds = 0.0;
        UInt32 size = sizeof(seconds);
        if (AudioUnitGetProperty(m_unit, kAudioUnitProperty_Latency,
                                 kAudioUnitScope_Global, 0, &seconds, &size)
            != noErr) {
            return 0;
        }
        return int(std::lround(seconds * m_sampleRate));
    }

private:
    static QString unitName(AudioUnitParameterUnit u)
    {
        switch (u) {
        case kAudioUnitParameterUnit_Decibels:    return QStringLiteral("dB");
        case kAudioUnitParameterUnit_Hertz:       return QStringLiteral("Hz");
        case kAudioUnitParameterUnit_Milliseconds:return QStringLiteral("ms");
        case kAudioUnitParameterUnit_Seconds:     return QStringLiteral("s");
        case kAudioUnitParameterUnit_Percent:     return QStringLiteral("%");
        case kAudioUnitParameterUnit_Ratio:       return QStringLiteral(":1");
        case kAudioUnitParameterUnit_Cents:       return QStringLiteral("ct");
        default:                                  return {};
        }
    }

    static OSStatus renderInput(void* self,
                                AudioUnitRenderActionFlags*,
                                const AudioTimeStamp*,
                                UInt32,
                                UInt32 frames,
                                AudioBufferList* io) noexcept
    {
        auto* me = static_cast<AuPlugin*>(self);
        if (io == nullptr) { return noErr; }
        for (UInt32 b = 0; b < io->mNumberBuffers; ++b) {
            auto* dst = static_cast<float*>(io->mBuffers[b].mData);
            if (dst == nullptr) { continue; }
            const size_t at = size_t(int(b) * me->m_maxFrames);
            if (int(b) < me->m_channels && at + frames <= me->m_in.size()) {
                std::copy_n(me->m_in.data() + at, frames, dst);
            } else {
                std::fill_n(dst, frames, 0.0f);
            }
        }
        return noErr;
    }

    bool configure(double sampleRate, int channels)
    {
        m_sampleRate = sampleRate;

        AudioStreamBasicDescription asbd{};
        asbd.mSampleRate       = sampleRate;
        asbd.mFormatID         = kAudioFormatLinearPCM;
        asbd.mFormatFlags      = kAudioFormatFlagIsFloat
                               | kAudioFormatFlagIsPacked
                               | kAudioFormatFlagIsNonInterleaved;
        asbd.mBitsPerChannel   = 32;
        asbd.mChannelsPerFrame = UInt32(channels);
        asbd.mFramesPerPacket  = 1;
        asbd.mBytesPerFrame    = 4;
        asbd.mBytesPerPacket   = 4;

        if (AudioUnitSetProperty(m_unit, kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Input, 0, &asbd, sizeof(asbd))
            != noErr) {
            return false;
        }
        if (AudioUnitSetProperty(m_unit, kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Output, 0, &asbd, sizeof(asbd))
            != noErr) {
            return false;
        }

        const UInt32 maxFrames = UInt32(m_maxFrames);
        AudioUnitSetProperty(m_unit, kAudioUnitProperty_MaximumFramesPerSlice,
                             kAudioUnitScope_Global, 0,
                             &maxFrames, sizeof(maxFrames));

        AURenderCallbackStruct cb{};
        cb.inputProc       = &AuPlugin::renderInput;
        cb.inputProcRefCon = this;
        if (AudioUnitSetProperty(m_unit, kAudioUnitProperty_SetRenderCallback,
                                 kAudioUnitScope_Input, 0, &cb, sizeof(cb))
            != noErr) {
            return false;
        }

        return AudioUnitInitialize(m_unit) == noErr;
    }

    AudioComponentInstance m_unit{nullptr};
    PluginDescriptor m_desc;
    bool   m_initialised{false};
    int    m_channels{1};
    int    m_maxFrames{512};
    double m_sampleRate{48000.0};
    double m_sampleTime{0.0};
    std::vector<float> m_in;
    std::vector<float> m_out;
    std::vector<char>  m_abl;
};

} // namespace

namespace PluginHost {

bool available() { return true; }

QVector<PluginDescriptor> scan()
{
    QVector<PluginDescriptor> out;

    // Effekte. Musikeffekte (die einen Takt erwarten) bleiben draussen:
    // eine Sendekette hat keinen.
    AudioComponentDescription want{};
    want.componentType = kAudioUnitType_Effect;

    AudioComponent c = nullptr;
    while ((c = AudioComponentFindNext(c, &want)) != nullptr) {
        AudioComponentDescription got{};
        if (AudioComponentGetDescription(c, &got) != noErr) { continue; }

        CFStringRef cfName = nullptr;
        AudioComponentCopyName(c, &cfName);
        const QString full = fromCfString(cfName);
        if (cfName != nullptr) { CFRelease(cfName); }

        PluginDescriptor d;
        d.identifier = identifierOf(got);
        d.format     = QStringLiteral("AU");
        // AudioComponentCopyName liefert "Hersteller: Name".
        const int colon = full.indexOf(QLatin1String(": "));
        if (colon > 0) {
            d.vendor = full.left(colon);
            d.name   = full.mid(colon + 2);
        } else {
            d.name = full;
        }
        if (d.name.isEmpty()) { d.name = d.identifier; }
        out.push_back(d);
    }

    std::sort(out.begin(), out.end(),
              [](const PluginDescriptor& a, const PluginDescriptor& b) {
        if (a.vendor != b.vendor) {
            return a.vendor.localeAwareCompare(b.vendor) < 0;
        }
        return a.name.localeAwareCompare(b.name) < 0;
    });
    return out;
}

std::unique_ptr<AudioPlugin> load(const QString& identifier)
{
    AudioComponentDescription want{};
    if (!descriptionFrom(identifier, want)) { return nullptr; }

    AudioComponent c = AudioComponentFindNext(nullptr, &want);
    if (c == nullptr) {
        // Regel 2: nicht installiert ist kein Fehler.
        qCInfo(lcAudio) << "AU plugin not on this machine:" << identifier;
        return nullptr;
    }

    AudioComponentInstance unit = nullptr;
    if (AudioComponentInstanceNew(c, &unit) != noErr || unit == nullptr) {
        qCWarning(lcAudio) << "AU plugin would not open:" << identifier;
        return nullptr;
    }

    CFStringRef cfName = nullptr;
    AudioComponentCopyName(c, &cfName);
    const QString full = fromCfString(cfName);
    if (cfName != nullptr) { CFRelease(cfName); }

    PluginDescriptor d;
    d.identifier = identifier;
    d.format     = QStringLiteral("AU");
    const int colon = full.indexOf(QLatin1String(": "));
    if (colon > 0) { d.vendor = full.left(colon); d.name = full.mid(colon + 2); }
    else           { d.name = full; }
    if (d.name.isEmpty()) { d.name = identifier; }

    return std::make_unique<AuPlugin>(unit, d);
}

} // namespace PluginHost

} // namespace Longpath

#else   // nicht macOS

namespace Longpath {
namespace PluginHost {

bool available() { return false; }
QVector<PluginDescriptor> scan() { return {}; }
std::unique_ptr<AudioPlugin> load(const QString&) { return nullptr; }

} // namespace PluginHost
} // namespace Longpath

#endif
