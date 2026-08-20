// =================================================================
// src/core/audio/VoiceKeyer.cpp  (NereusSDR)
// =================================================================
//
// NereusSDR-original. Aufbau und Begruendungen stehen im Header.
//
// =================================================================
// Modification history (NereusSDR):
//   2026-08-19 — Original fuer NereusSDR von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================

#include "core/audio/VoiceKeyer.h"

#include "core/AppSettings.h"
#include "core/audio/WavFile.h"

#include <QDir>
#include <QFileInfo>

#include <algorithm>
#include <cmath>

namespace Longpath {

namespace {

// Vorgabe-Tasten: F1 bis F10. Genau die Reihe, die Funkprogramme seit
// je fuer Ansagen benutzen — wer von woanders kommt, greift richtig.
QString defaultShortcut(int index)
{
    return QStringLiteral("F%1").arg(index + 1);
}

QString settingsKeyFor(int index, const char* what)
{
    return QStringLiteral("VoiceKeyer_%1_%2")
        .arg(index)
        .arg(QString::fromLatin1(what));
}

} // namespace

VoiceKeyerStore::VoiceKeyerStore(QObject* parent)
    : QObject(parent)
    , m_slots(kVoiceKeyerSlots)
{
    for (int i = 0; i < kVoiceKeyerSlots; ++i) {
        m_slots[i].shortcut = defaultShortcut(i);
    }
}

void VoiceKeyerStore::setFolder(const QString& path)
{
    m_folder = path;
    if (!m_folder.isEmpty()) {
        QDir().mkpath(m_folder);
    }
}

const VoiceKeyerSlot& VoiceKeyerStore::slot(int index) const
{
    static const VoiceKeyerSlot empty;
    if (index < 0 || index >= m_slots.size()) { return empty; }
    return m_slots[index];
}

QString VoiceKeyerStore::slotFileName(int index) const
{
    // Fester Name je Platz statt Zeitstempel: der Platz IST die
    // Kennung, und eine Ansage, die bei jeder Aufnahme einen neuen
    // Dateinamen bekaeme, liesse den Ordner volllaufen.
    return QStringLiteral("%1/voice-%2.wav")
        .arg(m_folder)
        .arg(index, 2, 10, QLatin1Char('0'));
}

bool VoiceKeyerStore::setRecording(int index, const QVector<float>& samples,
                                   int sampleRate, QString* error)
{
    if (index < 0 || index >= m_slots.size()) {
        if (error) { *error = QStringLiteral("no such slot"); }
        return false;
    }
    if (m_folder.isEmpty()) {
        if (error) { *error = QStringLiteral("no folder set"); }
        return false;
    }
    if (samples.isEmpty()) {
        // Eine leere Aufnahme ist fast immer ein Bedienfehler (Knopf
        // zweimal gedrueckt). Sie stillschweigend zu speichern hiesse,
        // dem Betreiber im Betrieb eine stumme Ansage zu senden.
        if (error) { *error = QStringLiteral("nothing was recorded"); }
        return false;
    }

    QDir().mkpath(m_folder);
    const QString path = slotFileName(index);
    if (!writeWavMono(path, samples, sampleRate, error)) {
        return false;
    }

    m_slots[index].wavPath = path;
    m_slots[index].seconds =
        static_cast<double>(samples.size()) / std::max(1, sampleRate);
    save();
    emit slotChanged(index);
    return true;
}

bool VoiceKeyerStore::importFile(int index, const QString& path, QString* error)
{
    if (index < 0 || index >= m_slots.size()) {
        if (error) { *error = QStringLiteral("no such slot"); }
        return false;
    }

    // Lesen statt nur den Pfad merken: so faellt eine unlesbare oder
    // fremde Datei JETZT auf und nicht mitten im CQ-Ruf.
    QString readErr;
    const WavData data = readWavMono(path, &readErr);
    if (!data.ok) {
        if (error) { *error = readErr; }
        return false;
    }

    m_slots[index].wavPath = path;
    m_slots[index].seconds = data.sampleRate > 0
        ? static_cast<double>(data.samples.size()) / data.sampleRate
        : 0.0;
    save();
    emit slotChanged(index);
    return true;
}

void VoiceKeyerStore::setLabel(int index, const QString& label)
{
    if (index < 0 || index >= m_slots.size()) { return; }
    if (m_slots[index].label == label) { return; }
    m_slots[index].label = label;
    save();
    emit slotChanged(index);
}

void VoiceKeyerStore::setShortcut(int index, const QString& keys)
{
    if (index < 0 || index >= m_slots.size()) { return; }
    if (m_slots[index].shortcut == keys) { return; }
    m_slots[index].shortcut = keys;
    save();
    emit slotChanged(index);
}

void VoiceKeyerStore::clearSlot(int index)
{
    if (index < 0 || index >= m_slots.size()) { return; }
    m_slots[index].wavPath.clear();
    m_slots[index].seconds = 0.0;
    save();
    emit slotChanged(index);
}

void VoiceKeyerStore::load()
{
    auto& s = AppSettings::instance();
    for (int i = 0; i < m_slots.size(); ++i) {
        m_slots[i].label =
            s.value(settingsKeyFor(i, "Label"), QString{}).toString();
        m_slots[i].wavPath =
            s.value(settingsKeyFor(i, "Wav"), QString{}).toString();
        m_slots[i].shortcut =
            s.value(settingsKeyFor(i, "Key"), defaultShortcut(i)).toString();

        // Dauer nicht aus den Einstellungen glauben, sondern von der
        // Datei nehmen: sie ist die Wahrheit, und eine geloeschte Datei
        // faellt so beim Start auf statt beim Senden.
        if (!m_slots[i].wavPath.isEmpty()) {
            if (!QFileInfo::exists(m_slots[i].wavPath)) {
                m_slots[i].wavPath.clear();
                m_slots[i].seconds = 0.0;
            } else {
                m_slots[i].seconds = wavDurationSeconds(m_slots[i].wavPath);
            }
        }
        emit slotChanged(i);
    }
}

void VoiceKeyerStore::save() const
{
    auto& s = AppSettings::instance();
    for (int i = 0; i < m_slots.size(); ++i) {
        s.setValue(settingsKeyFor(i, "Label"), m_slots[i].label);
        s.setValue(settingsKeyFor(i, "Wav"), m_slots[i].wavPath);
        s.setValue(settingsKeyFor(i, "Key"), m_slots[i].shortcut);
    }
}

// ── WavTxSource ─────────────────────────────────────────────────────

bool WavTxSource::load(const QString& path, int txSampleRate, QString* error)
{
    stop();

    if (txSampleRate <= 0) {
        if (error) { *error = QStringLiteral("invalid TX sample rate"); }
        return false;
    }

    const WavData data = readWavMono(path, error);
    if (!data.ok) { return false; }

    if (data.sampleRate == txSampleRate) {
        m_samples = data.samples;
    } else {
        // Umtasten HIER, nicht im Audio-Faden. Lineare Interpolation
        // genuegt fuer Sprache: der Unterschied zu einem ordentlichen
        // Filter liegt weit oberhalb dessen, was ein 2,9-kHz-Sendefilter
        // durchlaesst — und ein Interpolationsfehler faellt nach dem
        // Filter nicht mehr auf.
        const double ratio = static_cast<double>(txSampleRate) / data.sampleRate;
        const int outCount = static_cast<int>(data.samples.size() * ratio);
        m_samples.resize(outCount);
        for (int i = 0; i < outCount; ++i) {
            const double src = i / ratio;
            const int    i0  = static_cast<int>(src);
            const int    i1  = std::min(i0 + 1,
                                        static_cast<int>(data.samples.size()) - 1);
            const double f   = src - i0;
            m_samples[i] = static_cast<float>(
                data.samples[i0] * (1.0 - f) + data.samples[i1] * f);
        }
    }

    m_rate = txSampleRate;
    m_pos.store(0, std::memory_order_relaxed);
    return true;
}

void WavTxSource::play()
{
    if (m_samples.isEmpty()) { return; }
    m_pos.store(0, std::memory_order_relaxed);
    m_playing.store(true, std::memory_order_release);
}

void WavTxSource::stop()
{
    m_playing.store(false, std::memory_order_release);
    m_pos.store(0, std::memory_order_relaxed);
}

void WavTxSource::setRepeat(bool on, double gapSeconds)
{
    m_repeat.store(on, std::memory_order_relaxed);
    m_gapSamples = m_rate > 0
        ? static_cast<int>(std::max(0.0, gapSeconds) * m_rate)
        : 0;
}

double WavTxSource::seconds() const
{
    if (m_rate <= 0) { return 0.0; }
    return static_cast<double>(m_samples.size()) / m_rate;
}

int WavTxSource::pullSamples(float* dst, int n)
{
    if (dst == nullptr || n <= 0) { return 0; }

    // Nicht am Spielen: Stille liefern statt nichts. Ein Sendeweg, der
    // keine Abtastwerte bekommt, laeuft leer und knackt.
    if (!m_playing.load(std::memory_order_acquire) || m_samples.isEmpty()) {
        std::fill_n(dst, n, 0.0f);
        return n;
    }

    const int total = m_samples.size();
    const int end   = total + (m_repeat.load(std::memory_order_relaxed)
                                   ? m_gapSamples : 0);
    int pos = m_pos.load(std::memory_order_relaxed);

    for (int i = 0; i < n; ++i) {
        if (pos < total) {
            dst[i] = m_samples[pos];
        } else {
            dst[i] = 0.0f;          // die Pause zwischen zwei Rufen
        }
        ++pos;

        if (pos >= end) {
            if (m_repeat.load(std::memory_order_relaxed)) {
                pos = 0;
            } else {
                // Fertig. Ab hier Stille, und das Spielen endet — der
                // Aufrufer sieht es an isPlaying() und kann den Sender
                // abschalten. Von hier aus wird NICHT getastet:
                // Begruendung im Header.
                m_playing.store(false, std::memory_order_release);
                std::fill_n(dst + i + 1, n - i - 1, 0.0f);
                m_pos.store(0, std::memory_order_relaxed);
                return n;
            }
        }
    }

    m_pos.store(pos, std::memory_order_relaxed);
    return n;
}

} // namespace Longpath
