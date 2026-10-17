#pragma once

// =================================================================
// src/core/Ft8Stream.h  (Longpath)
// =================================================================
//
// Longpath-original. Das Dekodieren selbst macht Ft8Decoder; siehe
// dort und third_party/ft8_lib/FT8LIB-PROVENANCE.md.
//
// ── Was hier dazukommt: die Uhr ─────────────────────────────────────
//
// Ft8Decoder ist eine reine Funktion — Block hinein, Decodes heraus.
// Im Betrieb fehlen zwei Dinge: WANN ein Block zu Ende ist, und WOHER
// die Proben kommen.
//
// FT8 teilt die Minute in vier Fenster zu 15 Sekunden (FT4: acht zu
// 7,5). Sie beginnen auf der vollen Minute, nicht irgendwann — und
// zwar nach UTC, nicht nach Ortszeit. Wer die Grenze verfehlt,
// dekodiert nichts und sucht den Fehler beim Funkgeraet. Darum ist
// die Uhr hier AUSTAUSCHBAR: der Pruefstand setzt sie, statt zu
// warten.
//
// Der Ton kommt als Stereo mit 48 kHz, so wie ihn WDSP ausgibt und
// wie ihn auch der CW-Dekoder bekommt (AudioEngine::setCwTap). Hier
// wird er gemischt und auf die 12 kHz gebracht, mit denen ft8_lib
// rechnet.
//
// Das SENDEN steht bewusst nicht hier. Ein Dekoder, der von selbst
// sendet, ist ein anderes Ding als einer, der zuhoert.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Decoder.h"

#include <QDateTime>
#include <QObject>
#include <QVector>

#include <functional>
#include <memory>

namespace Longpath {

class Resampler;

class Ft8Stream : public QObject {
    Q_OBJECT
public:
    explicit Ft8Stream(QObject* parent = nullptr);
    ~Ft8Stream() override;

    void setProtocol(Ft8Decoder::Protocol p);
    Ft8Decoder::Protocol protocol() const { return m_protocol; }

    /// Die Uhr. Vorgabe ist QDateTime::currentMSecsSinceEpoch(); ein
    /// Pruefstand setzt sie und laesst die Zeit springen, statt
    /// fuenfzehn Sekunden zu warten.
    void setClock(std::function<qint64()> utcMs);

    /// Stereo, 48 kHz, so wie der CW-Abgriff. Wird gemischt und auf
    /// 12 kHz gebracht.
    void feedAudio(const float* interleavedStereo, int frames);

    /// Nur nach der Uhr sehen, ohne neue Proben.
    ///
    /// Ohne das haengt der Grenzuebertritt am naechsten Tonblock: steht
    /// der Ton (stumm, Verbindung weg, Scheibe gewechselt), wird das
    /// letzte Fenster nie geschlossen und die Anzeige friert auf dem
    /// vorletzten Stand ein. Wer ruft: der Taktgeber im Applet, neben
    /// dem Leeren des Rings.
    void poll();

    /// Wie viele 12-kHz-Proben im laufenden Fenster liegen.
    int bufferedSamples() const { return m_fenster.size(); }

    /// Laenge eines Fensters in Millisekunden.
    int slotMs() const;

signals:
    /// Ein Fenster ist zu Ende und ausgewertet. periodStart ist seine
    /// UTC-Anfangszeit — ohne die laesst sich ein Decode nicht
    /// eintragen.
    ///
    /// Wird AUCH mit leerer Liste gemeldet (2026-10-15): ein Fenster
    /// ohne Fund und ein toter Dekoder sehen sonst gleich aus, und
    /// genau diese Verwechslung hat hier schon Stunden gekostet. Ein
    /// angebrochenes Fenster (weniger als die halbe Laenge an Proben)
    /// wird weiter verschwiegen — das ist keine Messung.
    void decoded(const QVector<Longpath::Ft8Decode>& funde,
                 const QDateTime& periodStart);

private:
    void schliesseFenster(qint64 endeMs);

    Ft8Decoder::Protocol m_protocol{Ft8Decoder::Protocol::Ft8};
    std::function<qint64()> m_uhr;
    std::unique_ptr<Resampler> m_umtaster;
    QVector<float> m_fenster;      ///< 12 kHz, laufendes Fenster
    qint64 m_fensterBeginnMs{-1};  ///< UTC, Anfang des laufenden Fensters
};

} // namespace Longpath
