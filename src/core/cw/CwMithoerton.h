#pragma once

// =================================================================
// src/core/cw/CwMithoerton.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// ── Der Ton unter der eigenen Taste ──────────────────────────────────
//
// Wer mit einem Paddle gibt, hoert sich selbst zu -- der Ton fuehrt die
// Hand. Der Mithoerton der Flex kommt ueber das Netz und damit um ein
// Vielfaches einer Punktlaenge zu spaet (bei 25 WpM ist ein Punkt 48 ms);
// mit ihm kann man nicht geben. AetherSDR hat aus demselben Grund einen
// lokalen Tongeber (CwSidetoneGenerator). Dieser hier ist die kleine
// Fassung fuer die erste Stufe: ein Sinus mit weichen Flanken, vom
// Tastzustand geoeffnet und geschlossen.
//
// Der Tastzustand ist ein atomarer Schalter, damit der Geber ihn von
// seinem eigenen Faden umlegen kann, ohne auf irgendetwas zu warten. Die
// Flanke ist ein Kosinusanstieg ueber 5 ms -- hart getastet knackt ein
// Sinus, und ein Knacken bei jedem Zeichen ermuedet schneller als jeder
// Stoerpegel.
//
// Bewusst NICHT hier: probengenaue Flanken nach dem Zeitraster des
// Gebers (AetherSDR #4823/#4890). Die Flanke faellt auf die naechste
// Blockgrenze des Ausgabegeraets; bei einem kleinen Puffer sind das
// wenige Millisekunden. Reicht das nicht, ist das der naechste Schritt.
//
// `fuelle()` ist die ganze Tonerzeugung als reine Funktion ueber einem
// Puffer -- am Schreibtisch pruefbar, ohne Tonausgabe.
//
// =================================================================
// Modification history (Longpath):
//   2026-11-17 — Created for Longpath by Martin Fischer (OE5SOS),
//                 AI-assisted via Anthropic Claude.
// =================================================================

#include <QIODevice>
#include <QObject>

#include <atomic>
#include <memory>

class QAudioSink;

namespace Longpath {

class CwMithoerton : public QObject {
    Q_OBJECT
public:
    static constexpr int kAbtastrate = 48000;
    /// Flankendauer. 5 ms: die uebliche Zahl fuer CW-Flanken, kurz genug
    /// fuer 40 WpM, lang genug gegen das Knacken.
    static constexpr int kFlankeMs = 5;

    explicit CwMithoerton(QObject* eltern = nullptr);
    ~CwMithoerton() override;

    /// Tonausgabe oeffnen / schliessen. Ohne Geraet (Pruefstand,
    /// Offscreen) bleibt der Geber stumm und alles andere geht weiter.
    bool start();
    void stop();
    bool laeuft() const { return m_sink != nullptr; }

    /// Fadenfest: der Geber ruft das von seinem eigenen Faden.
    void setTaste(bool unten) noexcept { m_taste.store(unten, std::memory_order_release); }
    bool taste() const noexcept { return m_taste.load(std::memory_order_acquire); }

    void setTonhoeheHz(int hz) noexcept;     ///< 300 … 1200
    void setLautstaerke(int prozent) noexcept; ///< 0 … 100
    int tonhoeheHz() const noexcept { return m_hz.load(); }
    int lautstaerke() const noexcept { return m_prozent.load(); }

    /// `rahmen` Proben mono (Gleitkomma, -1 … +1) nach `ziel` schreiben.
    /// Traegt Phase und Huellkurve von Ruf zu Ruf weiter.
    void fuelle(float* ziel, int rahmen) noexcept;

private:
    class Quelle;

    std::atomic<bool> m_taste{false};
    std::atomic<int>  m_hz{600};
    std::atomic<int>  m_prozent{40};

    double m_phase{0.0};
    double m_huelle{0.0};   ///< 0 … 1, laeuft der Taste nach

    std::unique_ptr<QAudioSink> m_sink;
    std::unique_ptr<Quelle> m_quelle;
};

} // namespace Longpath
