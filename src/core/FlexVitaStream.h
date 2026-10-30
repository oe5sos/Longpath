#pragma once

// =================================================================
// src/core/FlexVitaStream.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/PanadapterStream.{h,cpp} at d58e2b8a
//     — start() (einen UDP-Port binden, Anmeldung per Null-Byte),
//       onDatagramReady / processDatagram (lesen, an der Paketklasse
//       verteilen), setDbmRange / setYPixels (der Maszstab je Strom).
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// =================================================================
// Ein Port fuer alles
// =================================================================
//
// Panadapter, Wasserfall, Ton und Messwerte kommen auf DEMSELBEN
// UDP-Port und werden an der Paketklasse in Byte 14-15 auseinander
// sortiert. Das Lesen selbst macht FlexVitaReader (kein Netz, am
// Schreibtisch pruefbar); diese Klasse ist nur der Sockel darum.
//
// Die Anmeldung, gemessen am 2026-10-18 an Martins FLEX-8400
// (docs/protocols/flex-vita49-gemessen.md §1):
//
//   1. eigenen UDP-Port binden (Betriebssystem waehlt)
//   2. **ein Null-Byte an die Maschine auf UDP 4992** — sie lernt
//      IP:Port aus der Absenderadresse
//      (PanadapterStream.cpp:261-268 [@d58e2b8a])
//   3. `client udpport <port>` ueber TCP
//
// Schritt 2 ist der wichtige. AetherSDR notiert, der TCP-Befehl aus
// Schritt 3 werde bei Firmware 1.4.0.0 mit 0x50001000 abgelehnt; an
// dieser Maschine antwortet er mit 0x0 — aber das Null-Byte ging im
// Messlauf VORHER hinaus, der Endpunkt war also schon bekannt. Belegt
// ist nur, dass der TCP-Befehl hier nicht abgelehnt wird; NICHT, dass
// er allein genuegt. Darum bleiben beide Wege, in dieser Reihenfolge.
//
// =================================================================
// Der Maszstab kommt von aussen
// =================================================================
//
// Die FFT-Bins sind Y-Pixel in der ANGEFORDERTEN Hoehe, und die
// Maschine meldet `y_pixels` NIE zurueck (gemessen: der Zustand bleibt
// auf `x_pixels=50 y_pixels=20`, obwohl das Setzen wirkt). Es gibt also
// kein Echo, auf das man warten koennte — wer den Maszstab setzt, muss
// ihn hier hinterlegen (`setYPixels` / `setDbmRange`). Ohne Hinterlegung
// gelten die Vorgaben aus AetherSDR (700 Pixel, −130..−40 dBm,
// PanadapterStream.cpp:965-966 [@d58e2b8a]).
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 Longpath. Nur Sockel und Verteilung: das Lesen liegt
//                 in FlexVitaReader, der Rahmenbau ebenfalls. Nicht
//                 uebernommen: WAN-Anmeldung samt Ping-Erhaltung,
//                 Opus, DAX-TX, PacketLossConcealment, PerfTelemetry,
//                 der Worker-Thread-Aufbau (Stufe 2 liest auf dem
//                 Hauptfaden) und das Neubinden nach Port-Streit.
// =================================================================

#include "FlexLueckenfuellung.h"
#include "FlexVitaReader.h"

#include <QElapsedTimer>
#include <QHostAddress>
#include <QMap>
#include <QObject>
#include <QTimer>
#include <QUdpSocket>
#include <QVector>

namespace Longpath {

class FlexVitaStream : public QObject
{
    Q_OBJECT

public:
    // From AetherSDR PanadapterStream.cpp:965-966 [@d58e2b8a] — die
    // Vorgaben, wenn niemand einen Maszstab hinterlegt hat.
    static constexpr int    kDefaultYPixels = 700;
    static constexpr double kDefaultMinDbm  = -130.0;
    static constexpr double kDefaultMaxDbm  = -40.0;
    // Die Maschine hoert auf 4992 (Erkennung, Befehlskanal UND die
    // Port-Anmeldung per Null-Byte).
    static constexpr quint16 kRadioPort = 4992;

    explicit FlexVitaStream(QObject* parent = nullptr);

    // Bindet einen eigenen UDP-Port und schickt das Null-Byte an die
    // Maschine. Gibt den gebundenen Port zurueck, 0 bei Fehlschlag —
    // der Aufrufer schickt damit `client udpport <port>`.
    quint16 start(const QHostAddress& radioAddress);
    void    stop();
    bool    isRunning() const;
    quint16 localPort() const { return m_localPort; }

    // ── Der Sendeton hinaus (2026-10-23, Stufe 2) ────────────────────
    //
    // Dieselbe Buchse wie der Empfang, aber an Port 4991 statt 4992:
    // die Anmeldung geht an 4992 (dort lernt die Maschine unsere
    // Adresse), die VITA-Pakete gehen an 4991.
    //
    // From AetherSDR PanadapterStream.cpp:285 [@d58e2b8a] —
    //   m_radioPort = 4991;   // „Store radio address for sendToRadio
    //                         //  (DAX TX path)"
    //
    // Gibt die Zahl gesendeter Bytes zurueck, 0 wenn nichts hinausging
    // (keine Adresse, Buchse zu, leeres Paket). Der Aufrufer darf das
    // ignorieren; gezaehlt wird es trotzdem, damit „es geht nichts
    // hinaus" nicht erst am Geraet auffaellt.
    qint64 sendeTxPaket(const QByteArray& paket);

    /// Wie viele Sendepakete bisher hinausgingen. Nur zum Ablesen.
    qint64 txPaketeGesendet() const { return m_txPaketeGesendet; }

    static constexpr quint16 kTxPort = 4991;

    // Der Maszstab je Strom. Siehe Kopf: es gibt kein Echo.
    void setYPixels(quint32 streamId, int yPixels);
    void setDbmRange(quint32 streamId, double minDbm, double maxDbm);

    // Zaehler fuer Diagnose und Pruefstaende.
    qint64 datagramsReceived() const { return m_datagrams; }
    qint64 datagramsIgnored() const { return m_ignored; }
    /// Wieviele Pakete je VITA-Klasse angekommen sind. Fuer Prueflaeufe
    /// und fuer die Frage „kommt ueberhaupt ein Bild?".
    QMap<quint16, qint64> packetsPerClass() const { return m_proKlasse; }

    // ── Luecken im Strom (2026-10-29) ─────────────────────────
    //
    // Zwei Zahlen, und sie trennen zwei Ursachen, die von aussen
    // gleich aussehen:
    //
    //   `verloren`   Spruenge im Rollzaehler — Pakete sind unterwegs
    //                abgeblieben. Das ist das Netz.
    //   `luecke`     Zeit zwischen zwei Paketen, ohne Zaehlersprung —
    //                die Maschine hat nichts geschickt. Das ist nicht
    //                das Netz.
    //
    // Am 2026-10-29 fiel der Empfang zweimal ganz aus (99. Perzentil
    // von −68,3 auf −101,0 dBm in 30 s) und es gab kein Instrument,
    // das diese Frage beantwortet haette. Jetzt gibt es eines.
    //
    // ACHTUNG beim Lesen: waehrend des Sendens schickt die Maschine
    // keinen Empfangston. Die groesste Luecke faellt dann mit dem
    // Tasten zusammen und ist KEIN Befund — darum steht die Uhrzeit
    // daneben.
    struct Luecken {
        quint16 klasse{0};
        quint32 strom{0};
        qint64  pakete{0};
        qint64  verloren{0};      ///< untere Schranke, siehe fehlendeNachZaehler
        qint64  spruenge{0};      ///< wie oft der Zaehler sprang
        qint64  groessteLueckeMs{0};
        QString groessteLueckeUm; ///< Uhrzeit dazu, sonst ist sie nichts wert
        /// Pausen ueber einem Zaehlerumlauf -- KEIN Netzverlust.
        /// Die eigene Sendung ist die haeufigste davon.
        qint64  unterbrechungen{0};
        qint64  laengsteUnterbrechungMs{0};
    };
    /// Je Stromkennung ein Eintrag, nach Klasse sortiert.
    QVector<Luecken> luecken() const;
    /// Summe der Zaehlerspruenge ueber alle Stroeme.
    qint64 verlorenePaketeGesamt() const { return m_verlorenGesamt; }

    /// ── Wann eine Pause keine Luecke ist (2026-10-30) ───────────
    ///
    /// Der Zaehler im VITA-Kopf hat VIER Bit und laeuft nach
    /// sechzehn Paketen um -- beim Tonstrom in 86 ms. Ist die Pause
    /// laenger, hat er mindestens einmal umgeschlagen, und sein
    /// Sprung sagt nichts mehr: aus 2500 fehlenden Paketen wird
    /// eine Zahl zwischen 1 und 15.
    ///
    /// Gemessen am 2026-10-30 im Protokoll des Betreibers:
    ///
    ///     groeszte Pause 13422 ms um 13:52:13
    ///
    /// 13,2 s -- das ist genau seine FT8-Tastung. Waehrend er sendet,
    /// schickt die Maschine kein Bild; das als Netzverlust zu
    /// melden, verfaelscht das Instrument, das genau dafuer gebaut
    /// wurde, den Netzweg zu beurteilen.
    ///
    /// Solche Pausen zaehlen jetzt getrennt als `unterbrechungen`.
    /// Die Schwelle ist das Sechzehnfache des ueblichen Abstands,
    /// also genau ein Zaehlerumlauf -- nicht eine feste
    /// Millisekundenzahl, denn die vier Stroeme laufen verschieden
    /// schnell (Ton 187 Pakete/s, Wasserfall deutlich weniger).
    static constexpr int kUmlaufFaktor = 16;
    /// Bis der mittlere Abstand steht, wird nichts als Unterbrechung
    /// gewertet: eine Schwelle aus zwei Messwerten ist geraten.
    static constexpr int kMindestPaketeFuerMittel = 32;

    // ── Lueckenfuellung im Tonweg (2026-10-30) ───────────────────
    //
    // Fehlt ein Tonpaket, folgt ohne Fuellung auf die letzte Probe des
    // einen unmittelbar die erste des naechsten: die Welle springt
    // (Knacken) und der Ton laeuft um die fehlende Zeit zu kurz. Die
    // Fuellung schiebt die Zeit als ausgeblendete Stille wieder ein.
    // Sie erfindet keinen Ton — siehe FlexLueckenfuellung.h.
    //
    // Vorgabe an. `start()` holt den Wert aus AppSettings
    // (`FlexAudioLueckenfuellung`); ohne `start()` — also im
    // Pruefstand — bleibt es bei der Vorgabe.
    void setzeLueckenfuellung(bool an) { m_lueckenfuellung = an; }
    bool lueckenfuellung() const { return m_lueckenfuellung; }
    /// Wieviele Rahmen die Fuellung bisher eingeschoben hat.
    qint64 gefuellteRahmen() const { return m_gefuellteRahmen; }

    // Nur fuer Pruefstaende: schiebt ein Datagramm durch denselben Weg,
    // den der Socket nimmt.
    void ingestDatagramForTesting(const QByteArray& datagram);

    /// Nur fuer Prueflaeufe: einen Zaehlerstand mit vorgegebener Zeit
    /// einspeisen, ohne Datagramm und ohne Uhr.
    void pruefeZaehlerForTest(quint16 klasse, quint32 strom,
                              quint8 zaehler, qint64 jetztNs)
    {
        FlexVita::Header h{};
        h.packetClass = klasse;
        h.streamId    = strom;
        h.counter     = zaehler;
        zaehleLuecke(h, jetztNs);
    }

signals:
    // Fertiger FFT-Rahmen, schon in dBm umgerechnet.
    void spectrumReady(quint32 streamId, const QVector<float>& dbm);
    // Fertige Wasserfallzeile. `values` sind ROH — ein Farbmaszstab,
    // keine dBm (gemessen; siehe FlexVitaReader.h).
    void waterfallRowReady(quint32 streamId, const QVector<quint16>& values,
                           double lowFreqMhz, double highFreqMhz,
                           quint32 autoBlackLevel);
    /// Messwerte der Maschine: Paare aus laufendem Index (1…59) und
    /// Rohwert. Der Teiler haengt an der EINHEIT und steht in den
    /// `meter`-Statuszeilen, nicht im Paket.
    void meterSamplesReady(quint32 streamId,
                           const QVector<FlexVita::FlexMeterSample>& werte);
    // Ton: big-endian float32 Stereo verschraenkt, 24 kHz (gemessen).
    void audioSamplesReady(quint32 streamId, const QVector<float>& interleavedStereo);

private slots:
    void onReadyRead();

private:
    void berichteKlassen();

    void handleDatagram(const QByteArray& datagram);

    struct Scale {
        int    yPixels{kDefaultYPixels};
        double minDbm{kDefaultMinDbm};
        double maxDbm{kDefaultMaxDbm};
    };

    QUdpSocket m_socket;
    QHostAddress m_radioAddress;
    quint16 m_localPort{0};
    qint64 m_txPaketeGesendet{0};

    FlexFftAssembler  m_fft;
    FlexTileAssembler m_tiles;
    QMap<quint32, Scale> m_scales;

    qint64 m_datagrams{0};
    qint64 m_ignored{0};

    // Was wirklich ankommt, nach Klasse getrennt. Ohne das ist jede
    // Aussage ueber „es kommt kein Bild" geraten: der Strom hat bis
    // 2026-10-21 GENAU EINE Protokollzeile gehabt, und die stand vor
    // dem ersten Paket. Am 2026-10-21 hat das eine Stunde gekostet:
    // „es kommt kein Bild" und „es kommt gar nichts" sehen von aussen
    // gleich aus, und ohne diese Zaehler war beides nur geraten.
    QMap<quint16, qint64> m_proKlasse;

    // Ein Strang je Stromkennung: der Rollzaehler laeuft JE STROM, nicht
    // je Klasse. Mit zwei Scheiben kaemen sonst zwei verschraenkte
    // Zaehler zusammen und jeder zweite Schritt saehe wie ein Verlust
    // aus — ein Instrument, das Phantome erzeugt, ist schlimmer als
    // keines.
    struct Strang {
        quint16 klasse{0};
        quint32 strom{0};
        quint8  letzterZaehler{0};
        bool    gesehen{false};
        qint64  pakete{0};
        qint64  verloren{0};
        qint64  spruenge{0};
        qint64  letzteAnkunftNs{-1};
        qint64  groessteLueckeNs{0};
        QString groessteLueckeUm;
        /// Der uebliche Abstand zwischen zwei Paketen, gleitend
        /// gemittelt. Daraus ergibt sich, ab wann eine Pause keine
        /// Luecke mehr ist, sondern eine Unterbrechung -- siehe
        /// `kUmlaufFaktor` und `unterbrechungen`.
        qint64  mittelAbstandNs{0};
        /// Pausen, die laenger waren als ein Zaehlerumlauf. Ihr
        /// Zaehlersprung ist bedeutungslos und wird NICHT als
        /// Verlust gezaehlt.
        qint64  unterbrechungen{0};
        qint64  laengsteUnterbrechungNs{0};
    };
    /// Ab wann ein Abstand keine Luecke mehr ist, sondern eine
    /// Unterbrechung (Senden, Bandwechsel, Neuverbindung). 200 ms
    /// sind rund vierzig Paketabstaende und damit weit jenseits
    /// dessen, was die Decke von acht Paketen ueberhaupt fuellt.
    static constexpr qint64 kUnterbrechungAbNs = 200LL * 1000000LL;
    /// `jetztNsVorgabe` < 0 heisst: die eigene Uhr fragen.
    /// Ein Prueflauf gibt sie vor -- eine Klasse mit eigener Uhr
    /// laesst sich nur in Echtzeit pruefen, und eine Pause von
    /// 13 Sekunden waere dann ein Prueflauf von 13 Sekunden.
    void zaehleLuecke(const FlexVita::Header& h,
                      qint64 jetztNsVorgabe = -1);
    bool m_lueckenfuellung{true};
    QMap<quint32, FlexVita::Lueckenzustand> m_tonLuecken;  ///< je Tonstrom
    qint64 m_gefuellteRahmen{0};
    QMap<quint64, Strang> m_straenge;   ///< Schluessel: Klasse<<32 | Strom
    QElapsedTimer m_uhr;
    qint64 m_verlorenGesamt{0};
    qint64 m_verlorenGemeldet{0};

    QTimer m_bericht;
    qint64 m_zuletztGemeldet{-1};
    qint64 m_fftRahmen{0};
    qint64 m_wasserfallZeilen{0};
    quint16 m_binKleinsterJe{65535};
    quint16 m_binGroessterJe{0};
};

}  // namespace Longpath
