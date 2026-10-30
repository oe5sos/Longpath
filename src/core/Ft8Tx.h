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

#include <QtGlobal>
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

    // ── Mehrere Stationen in einem Fenster (MSHV „Multi Answering") ──
    //
    // Betreiber am 2026-10-22: MSHV ist die erste Wahl, und MSHVs
    // Besonderheit ist genau das -- mehrere Anrufer in DEMSELBEN
    // Fenster abarbeiten statt nacheinander.
    //
    // Verfahren aus MSHV gelesen, nicht erfunden
    // (`src/HvMsPlayer/libsound/HvGenFt8/gen_ft8.cpp:195-315`,
    // Copyright 2018 Hrisimir Hristov LZ2HV, GPLv3 -- dieselbe Lizenz,
    // unter der Longpath steht):
    //
    //   * je Platz ein eigener Ton, **60 Hz** auseinander
    //     (`f0 = f_tx + 60.0*islot`, dort Zeile 262)
    //   * begrenzt auf 100..5000 Hz (ebenda)
    //   * alle Kurven in EINE summiert (`d_iwave[k1] += sin(phi)`)
    //   * und am Ende durch die Zahl der Plaetze geteilt
    //     (`8380000.0*(d_iwave[z]/(double)nslots)`, Zeile 303)
    //
    // Die Teilung ist der Preis und keine Wahl: der Sender hat eine
    // Hoechstamplitude, und wer zwei Stationen gleichzeitig antwortet,
    // gibt jeder 6 dB weniger, bei vier 12 dB. Nicht wegzurechnen --
    // wer es anders macht, uebersteuert.
    //
    // Was DIESE Klasse weiterhin nicht tut: tasten. Siehe Kopf.
    struct Platz {
        QString text;
        float   toneHz{0.0f};
    };

    /// Mehrere Nachrichten in ein Fenster legen. Leer oder nur eine ->
    /// verhaelt sich wie `vorbereiten()`. Gibt false, sobald EINE der
    /// Nachrichten nicht packbar ist: ein Fenster, in dem stillschweigend
    /// eine Station fehlt, ist schlimmer als keines.
    bool vorbereitenMehrere(const QVector<Platz>& plaetze,
                            Ft8Decoder::Protocol protocol);

    /// MSHVs Tonleiter: `grundTonHz`, dann je Platz 60 Hz hoeher,
    /// begrenzt auf 100..5000 Hz. `anzahl` Plaetze.
    static QVector<float> tonleiter(float grundTonHz, int anzahl);

    /// Abstand zweier Plaetze, aus MSHV (gen_ft8.cpp:262).
    static constexpr float kPlatzAbstandHz = 60.0f;
    static constexpr float kMinTonHz = 100.0f;
    static constexpr float kMaxTonHz = 5000.0f;

    /// Abspielen, ab `abMs` Millisekunden in der Wellenform.
    ///
    /// `abMs = 0` spielt von vorn — dann steht die fuehrende Stille
    /// der Wellenform vorne dran (1180 ms bei FT8, siehe
    /// `Ft8Tx::stilleMs`). Wer spaeter tastet, ueberspringt genau so
    /// viel davon und haelt damit den Zeitversatz, statt ihn zu
    /// vergroeszern. Die Rechnung steht in `Ft8Tx::sendeplan`.
    ///
    /// Ohne vorbereitete Nachricht wirkungslos. Ein `abMs`, das ueber
    /// das Fenster hinausreicht, wird eingefangen.
    void starten(qint64 abMs = 0);

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

    /// Wie lange das SIGNAL dauert -- nicht das Fenster.
    ///
    /// Gemessen am 2026-10-22 am Geraet: der Sender war 14,9 s
    /// getastet (`mox=true` 06:17:30,295 bis 06:17:45,170), obwohl ein
    /// FT8-Signal nur 12,64 s lang ist. Betreiber: „hat zu lange
    /// gedauert." Er kam dadurch zu spaet auf Empfang zurueck, um die
    /// Antwort im naechsten Fenster noch ganz zu hoeren.
    ///
    /// 79 Symbole zu 0,16 s bei FT8, 105 zu 0,048 s bei FT4 -- die
    /// Zahlen stehen in ft8_lib (`ft8/constants.h`: FT8_NN,
    /// FT8_SYMBOL_PERIOD) und werden hier nicht neu erfunden.
    static int signalMs(Ft8Decoder::Protocol p)
    {
        // From ft8_lib ft8/constants.h [@9fec6ca]:
        //   #define FT8_SYMBOL_PERIOD (0.160f)   #define FT8_NN (79)
        //   #define FT4_SYMBOL_PERIOD (0.048f)   #define FT4_NN (105)
        // Hier als Zahlen statt als Einbindung: eine C-Kopfdatei aus
        // third_party in eine oeffentliche Qt-Kopfdatei zu ziehen,
        // zoege sie durch das halbe Programm.
        return p == Ft8Decoder::Protocol::Ft4
                   ? static_cast<int>(105 * 0.048 * 1000.0 + 0.5)   // 5040 ms
                   : static_cast<int>(79 * 0.160 * 1000.0 + 0.5);   // 12640 ms
    }

    /// Fuehrende Stille der Wellenform, in Millisekunden.
    ///
    /// ft8_lib zentriert das Signal im Fenster:
    ///   From ft8_lib demo/gen_ft8.c:160-186 [@9fec6ca] —
    ///   `num_silence = (num_total_samples - num_samples) / 2`,
    /// nachgebaut in `Ft8Decoder::encodeBlock`. Bei FT8 sind das
    /// (15000 − 12640) / 2 = 1180 ms, bei FT4 (7500 − 5040) / 2 = 1230.
    ///
    /// WSJT-X sendet NICHT zentriert, sondern laesst das Signal ~0,5 s
    /// nach der Fenstergrenze beginnen. Wer die Wellenform von vorn
    /// abspielt, liegt also 0,68 s hinter der Norm — und wer dazu nur
    /// `signalMs` lang tastet, schneidet das Ende ab. Siehe
    /// `sendeplan()`.
    static int stilleMs(Ft8Decoder::Protocol p)
    {
        return (fensterMs(p) - signalMs(p)) / 2;
    }

    /// Die drei Zeiten einer Sendung, alle ab der Fenstergrenze.
    struct Sendeplan {
        qint64 tastenNachMs{0};   ///< so lange nach der Grenze warten
        qint64 probenAbMs{0};     ///< ab hier aus der Wellenform ziehen
        qint64 tastdauerMs{0};    ///< so lange getastet bleiben
        qint64 tonBeiMs{0};       ///< wann der erste Ton hinausgeht
    };

    // ── Warum es diese Rechnung gibt (2026-10-30) ───────────────
    //
    // Drei Dinge gingen vorher jedes fuer sich schief, und sie haengen
    // an derselben Zahl — den 1180 ms fuehrender Stille:
    //
    //  1. **Das Signalende wurde abgeschnitten.** Getastet wurde
    //     `signalMs + 300` = 12 940 ms ab der Fenstergrenze, aber das
    //     Signal begann erst bei 1180 ms und lief bis 13 820. Am
    //     Geraet gemessen (Protokoll 2026-10-30, 07:09:30,003 getastet
    //     bis 07:09:42,945 geloest, erster Ton 07:09:31,185): die
    //     letzten 880 ms fehlten, also 5,5 von 79 Symbolen. Das
    //     Verfahren verkraftet das teilweise — aber es kostet
    //     Empfindlichkeit, und es erklaert, warum derselbe Zug
    //     dreimal hinausmusste.
    //
    //  2. **Der Zeitversatz lag bei +0,68 s** gegenueber der
    //     WSJT-X-Norm (Signal 0,5 s nach der Grenze). Dekodierbar,
    //     aber am Rand des Gewohnten.
    //
    //  3. **Der Zug kam ein ganzes Fenster zu spaet.** Der Dekoder ist
    //     erst ~40 ms NACH dem Fensterende fertig (gemessen:
    //     07:09:30,039), getastet wurde 3 ms danach. Die Antwort der
    //     Gegenstation kam also immer zu spaet fuer dieses Fenster,
    //     und jeder Zug kostete 30 s statt 15.
    //
    // Die fuehrende Stille ist der Platz, in dem alle drei zu
    // beheben sind: spaeter tasten (Punkt 3), dafuer entsprechend
    // weiter vorn aus der Wellenform ziehen (Punkt 2), und so lange
    // tasten, wie das Signal WIRKLICH braucht (Punkt 1).
    //
    // `dekoderFristMs`  wie lange nach der Fenstergrenze auf den
    //                   Dekoder gewartet wird
    // `zielVersatzMs`   wo das Signal beginnen soll (WSJT-X: 500)
    // `reserveMs`       Nachlauf, damit der letzte Ton draussen ist
    static Sendeplan sendeplan(Ft8Decoder::Protocol p,
                               qint64 dekoderFristMs,
                               qint64 zielVersatzMs,
                               qint64 reserveMs)
    {
        Sendeplan s;
        const qint64 stille = stilleMs(p);
        const qint64 frist  = qBound<qint64>(0, dekoderFristMs, stille);

        // Der Zielversatz kann nicht kleiner sein als die Frist: vor
        // dem Tasten geht kein Ton hinaus.
        const qint64 ziel = qBound(frist, zielVersatzMs, stille + frist);

        s.tastenNachMs = frist;
        // Ab wo aus der Wellenform: so viel von der Stille
        // ueberspringen, dass der Ton genau beim Zielversatz kommt.
        s.probenAbMs   = stille + frist - ziel;
        s.tonBeiMs     = ziel;
        // Getastet wird, bis das Signal durch ist — gerechnet vom
        // ZIELVERSATZ, nicht von der Fenstergrenze.
        s.tastdauerMs  = ziel + signalMs(p) + reserveMs - frist;
        return s;
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
