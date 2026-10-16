#pragma once

// =================================================================
// src/core/Ft8Tx.h  (Longpath)
// =================================================================
//
// Longpath-original. Die Sendeseite von FT8/FT4.
//
// ── Was sie ist ─────────────────────────────────────────────────────
//
// Eine Mikrofonquelle wie jede andere: `Ft8TxSource` erbt von
// TxMicRouter und liefert dem Sendeweg Tonproben, statt sie vom
// Mikrofon zu holen. Denselben Weg geht der Sprachspeicher
// (audio/VoiceKeyer.h, `WavTxSource`) — dort steht auch die
// Begruendung, warum eine solche Klasse den Sender NICHT selbst
// tastet.
//
// ── Was sie NICHT tut ───────────────────────────────────────────────
//
// Tasten. Kein MOX, keine Folgeschaltung, kein CQ von selbst. Diese
// Klasse weiss, WAS gesendet wuerde und WANN das Fenster beginnt; ob
// der Sender angeht, entscheidet jemand anderes. Eine Klasse, die Ton
// erzeugt UND tastet, laesst sich nicht mehr gefahrlos pruefen — und
// ein Fehler darin sendet.
//
// ── Die Gegenperiode ────────────────────────────────────────────────
//
// FT8 teilt die Minute in vier Fenster zu 15 s, FT4 in acht zu 7,5 s.
// Wer hoert, hoert in dem einen; wer antwortet, sendet im anderen.
// `Ft8Tx::istSendefenster()` sagt, ob ein gegebener Zeitpunkt in der
// gewaehlten Haelfte liegt — gerade oder ungerade, gezaehlt ab der
// vollen Minute nach UTC.
//
// Die Uhr ist austauschbar wie bei Ft8Stream: der Pruefstand laesst
// die Zeit springen, statt fuenfzehn Sekunden zu warten.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-15 — Original fuer Longpath von Martin Fischer (OE5SOS),
//                 KI-gestuetzt ueber Anthropic Claude.
// =================================================================

#include "core/Ft8Decoder.h"
#include "core/TxMicRouter.h"

#include <QDateTime>
#include <QString>
#include <QVector>

#include <atomic>
#include <functional>
#include <memory>

namespace Longpath {

class Resampler;

/// Welche Haelfte der Minute diese Station belegt.
enum class Ft8Periode {
    Gerade,   ///< Fenster 0, 2, … (Sekunde 0, 30 bei FT8)
    Ungerade  ///< Fenster 1, 3, … (Sekunde 15, 45 bei FT8)
};

/// Die Tonquelle. Liefert ein vorgerechnetes Fenster an den Sendeweg.
///
/// `vorbereiten()` rechnet das ganze Fenster EINMAL aus und taugt
/// nicht fuer den Tonfaden (es rechnet und belegt Speicher).
/// `pullSamples()` laeuft im Tonfaden und tut nichts weiter als
/// kopieren.
class Ft8TxSource : public TxMicRouter {
public:
    /// `micRateHz` ist die Rate des Sendewegs (48 000 im Programm).
    explicit Ft8TxSource(int micRateHz = 48000);
    ~Ft8TxSource() override;

    /// Eine Nachricht auf einen Ton legen. Danach steht sie bereit,
    /// aber es wird nichts gesendet, bis `starten()` gerufen wird.
    ///
    /// `toneHz` ist die Tonlage im Durchlassbereich (ueblich 1500 Hz).
    /// Gibt false, wenn ft8_lib die Nachricht nicht packen kann.
    bool vorbereiten(const QString& text,
                     float toneHz,
                     Ft8Decoder::Protocol protocol);

    /// Von vorn abspielen. Ohne vorbereitete Nachricht wirkungslos.
    void starten();

    /// Sofort aufhoeren. Der Rest des Fensters bleibt still.
    void abbrechen();

    bool laeuft() const { return m_laeuft.load(std::memory_order_relaxed); }

    /// Wie viele Proben das vorbereitete Fenster am Sendeweg hat.
    int probenImFenster() const { return m_proben.size(); }

    /// Die vorbereitete Nachricht, oder leer.
    QString nachricht() const { return m_text; }

    // TxMicRouter — laeuft im Tonfaden.
    int pullSamples(float* dst, int n) override;

private:
    const int m_micRateHz;
    std::unique_ptr<Resampler> m_umtaster;
    QVector<float> m_proben;        ///< ganzes Fenster, Mikrofonrate
    QString m_text;
    std::atomic<bool> m_laeuft{false};
    std::atomic<int>  m_pos{0};
};

/// Die Uhr: liegt dieser Zeitpunkt in meinem Sendefenster?
class Ft8Tx {
public:
    /// Laenge eines Fensters in Millisekunden.
    static int fensterMs(Ft8Decoder::Protocol p)
    {
        return p == Ft8Decoder::Protocol::Ft4 ? 7500 : 15000;
    }

    /// Nummer des Fensters seit der vollen Minute (0..3 bei FT8).
    static int fensterNummer(qint64 utcMs, Ft8Decoder::Protocol p)
    {
        const qint64 inMinute = ((utcMs % 60000) + 60000) % 60000;
        return static_cast<int>(inMinute / fensterMs(p));
    }

    /// Liegt dieser Zeitpunkt in der gewaehlten Haelfte?
    static bool istSendefenster(qint64 utcMs,
                                Ft8Decoder::Protocol p,
                                Ft8Periode periode)
    {
        const bool gerade = (fensterNummer(utcMs, p) % 2) == 0;
        return periode == Ft8Periode::Gerade ? gerade : !gerade;
    }

    /// Beginn des naechsten Sendefensters, in Millisekunden ab
    /// `utcMs`. Null, wenn `utcMs` selbst genau auf einem liegt.
    static qint64 bisZumNaechsten(qint64 utcMs,
                                  Ft8Decoder::Protocol p,
                                  Ft8Periode periode);
};

} // namespace Longpath
