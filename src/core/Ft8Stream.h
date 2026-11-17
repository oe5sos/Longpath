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

    /// ── Was vor dem Hinhoeren schon feststeht (2026-11-13) ───────
    ///
    /// Ein Lieferant, kein Wert: die Vorgabe haengt am laufenden QSO
    /// und wechselt mitten im Betrieb. Wuerde sie hier liegen, muesste
    /// jeder, der den Ablauf anfasst, daran denken sie nachzuziehen --
    /// und genau das vergisst man. So wird sie je Fenster frisch
    /// geholt, von der Stelle, die sie ohnehin kennt.
    ///
    /// Nicht gesetzt oder leere Antwort heisst: dekodieren wie frueher.
    /// Siehe core/Ft8Apriori.h.
    void setVorwissenQuelle(std::function<QVector<ApVorgabe>()> quelle);

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

    /// ── Warum der FT4-Block spaeter beginnt (2026-10-17) ───────────
    ///
    /// ft8_lib sucht den Zeitversatz in einem FESTEN Bereich von -10
    /// bis +19 SYMBOLEN (`third_party/ft8_lib/ft8/decode.c:205`). Mal
    /// der Symboldauer ergibt das bei FT4 (0,048 s) ein Suchband von
    /// -0,48 s bis +0,91 s, bei FT8 (0,160 s) eines von -1,6 s bis
    /// +3,04 s. Gemessen am 2026-10-17, Nachricht an verschobenen
    /// Stellen in ein leeres 7,5-s-Fenster gelegt:
    ///
    ///     -600 ms  nein     850 ms  JA
    ///     -500 ms  JA       900 ms  JA
    ///      ...              925 ms  nein
    ///
    /// Also -500 bis +900 ms, 1,40 s breit — die Theorie auf 10 ms
    /// genau. Eine FT4-Sendung beginnt rund 0,5 s nach der
    /// Fenstergrenze. Sie liegt damit im Band, aber nur 0,4 s vom
    /// oberen Rand entfernt, waehrend unten 1,0 s frei bleiben. Jede
    /// Laufzeit im Tonweg (WDSP, Umtaster, Ring) frisst von genau
    /// diesen 0,4 s.
    ///
    /// Darum faengt der Block, den FT4 zu sehen bekommt, 300 ms nach
    /// der Fenstergrenze an. Das ist nicht gerundet, sondern die
    /// Mitte: (-500 + 900) / 2 = +200, und 500 - 200 = 300. Danach
    /// liegt das Band bei -200 bis +1200 ms, und die Soll-Sendezeit
    /// von +500 ms steht mit 700 ms Luft nach beiden Seiten darin.
    ///
    /// Nicht an ft8_lib geschraubt: der Bereich dort gilt fuer FT8
    /// mit, wo er reichlich ist, und ein veraenderter Fremdcode
    /// kostet bei jedem Abgleich wieder Arbeit.
    static constexpr int kFt4BlockVersatzMs = 300;

    /// ── FT2: dieselbe Rechnung, eigene Zahlen (2026-11-17) ──────────
    ///
    /// Suchband -10..+19 Symbole zu 0,024 s = −240..+456 ms, Mitte
    /// +108 ms. Gesendet wird FT2 nicht einheitlich:
    ///
    ///     WSJT-X Improved 3.1.0  Modulator.cpp:80   delay_ms=150
    ///     Decodium 4.0           Modulator.cpp:209  0 (Linux) / 300 (sonst)
    ///
    /// Der Block beginnt so, dass 150 ms -- die Mitte beider Lager --
    /// in der Mitte des Bandes liegt: 150 − 108 = 42, auf 40 gerundet.
    /// Danach reicht das Band von −200 bis +496 ms: 0 ms hat 200 ms
    /// Luft nach unten, 300 ms knapp 200 nach oben.
    static constexpr int kFt2BlockVersatzMs = 40;

    /// ── Und ein ZWEITER Block, 480 ms spaeter (2026-11-17, abends) ───
    ///
    /// Am Geraet (FLEX-8400, 3,578 MHz): kraeftige FT2-Signale im
    /// Wasserfall, im Takt von 3,75 s -- und 0 Funde in jedem Fenster.
    /// Das Suchband oben reicht bis +496 ms; kommt der Ton der Flex mit
    /// ein paar hundert Millisekunden Laufzeit an, liegt jede Sendung
    /// dahinter. WSJT-X Improved sucht weiter: FT4-Sync von
    /// `ibmin=-344` bis `ibmax=1012` bei 12000/18 Hz
    /// (`lib/ft4_decode.f90:402-411`), fuer FT2 auf halbe Zeit
    /// gestreckt also −0,26 … +0,76 s.
    ///
    /// ft8_libs Band ist fest (−10..+19 Symbole), darum ein zweiter
    /// Durchlauf auf demselben Fenster, um 20 Symbole (480 ms) versetzt:
    /// zusammen −200 … +976 ms ab der Grenze. Was beide finden, steht
    /// einmal da; der Zeitversatz gilt immer ab dem ERSTEN Block.
    static constexpr int kFt2ZweiterBlockMs = kFt2BlockVersatzMs + 480;

    /// ── FT2 wird VOR der Fenstergrenze dekodiert ────────────────────
    ///
    /// Bei FT8 und FT4 wird an der Grenze dekodiert, und die Antwort
    /// geht mit der Dekoderfrist (250 ms) in das neue Fenster. Bei FT2
    /// ist das Fenster 3,75 s lang und die Sendung beginnt 150 ms nach
    /// der Grenze -- dafuer reicht die Zeit nicht.
    ///
    /// WSJT-X Improved dekodiert FT2 nach `23*1728` Proben
    /// (`widgets/mainwindow.cpp:2646`, `m_hsymStop=23` mit 1728 je
    /// Schritt), also nach 3,312 s. Hier etwas spaeter, nach 3,5 s:
    /// mit dem zweiten Block (oben) kann eine Sendung bis +976 ms
    /// beginnen und endet dann bei 976 + 2520 = 3496 ms. Danach bleiben
    /// bis zur Grenze 250 ms -- dekodiert ist ein FT2-Fenster in
    /// wenigen Millisekunden. Der Rest bis zur Grenze wird verworfen.
    static constexpr int kFt2DekodierProben = 42000;   // 3,5 s bei 12 kHz

    /// FT2 auswerten wie im Betrieb: beide Bloecke, zusammengefuehrt.
    /// `fenster` beginnt an der Fenstergrenze. Oeffentlich, damit der
    /// Pruefstand GENAU diesen Weg faehrt und keinen nachgebauten.
    static QVector<Ft8Decode> dekodiereFt2(const float* fenster, int proben,
                                           const QVector<ApVorgabe>& vorwissen
                                               = QVector<ApVorgabe>{});

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
    /// Auswerten, was im Fenster liegt, und melden. Von
    /// `schliesseFenster` an der Grenze gerufen, bei FT2 schon vorher.
    void werteAus();

    Ft8Decoder::Protocol m_protocol{Ft8Decoder::Protocol::Ft8};
    std::function<qint64()> m_uhr;
    std::unique_ptr<Resampler> m_umtaster;
    std::function<QVector<ApVorgabe>()> m_vorwissenQuelle;
    QVector<float> m_fenster;      ///< 12 kHz, laufendes Fenster
    qint64 m_fensterBeginnMs{-1};  ///< UTC, Anfang des laufenden Fensters
    /// FT2: dieses Fenster ist schon ausgewertet (vor der Grenze).
    bool m_schonAusgewertet{false};
};

} // namespace Longpath
