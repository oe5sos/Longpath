#pragma once

// =================================================================
// src/core/FlexVitaReader.h  (Longpath)
// =================================================================
//
// Ported from AetherSDR (https://github.com/aethersdr/AetherSDR),
// GPLv3, primary author Jeremy [KK7GWY]:
//   src/core/PanadapterStream.cpp at d58e2b8a
//     — processDatagram (Kopf lesen, an der Paketklasse verteilen),
//       decodeFFT (Unterkopf, Rahmenbau, Bin → dBm),
//       decodeWaterfallTile (Unterkopf, Festkomma-Frequenzen,
//       Zeilenbau), decodeNarrowAudio (float32-Stereo).
//   src/core/VitaBinCoverage.h at d58e2b8a
//     — boundedVitaPayloadBinCount, VitaBinCoverage,
//       hasZeroFilledFftGrowthSuffix, FftGrowthSuffixGuard.
//   src/core/VitaTileFrequency.h at d58e2b8a
//     — decodeTileFrequencyMhz (Hz × 2²⁰).
//
// AetherSDR carries no per-file licence headers, so per
// docs/attribution/HOW-TO-PORT.md rule 6 the citation is at project
// level: there is no verbatim block to copy. Both projects are GPLv3,
// so the code carries forward under the same licence per GPLv3 section 5.
//
// =================================================================
// Was hier KEIN Sockel ist
// =================================================================
//
// Diese Datei liest Bytes und baut Rahmen. Sie oeffnet nichts, bindet
// nichts und kennt kein Netz — damit ist sie am Schreibtisch pruefbar,
// gegen den echten Mitschnitt vom 2026-10-18. Der Sockel und die
// Einspeisung in Ton und Bild kommen daneben, nicht hier.
//
// =================================================================
// Der Draht, gemessen
// =================================================================
//
// Vollstaendig in docs/protocols/flex-vita49-gemessen.md: zwei Laeufe
// an OE5SOS' FLEX-8400 (SmartSDR 4.3.16.42034, API 1.4.0.0), 5160
// Datagramme. Vier Dinge daraus bestimmen diese Schnittstelle:
//
// 1. **Die StreamID im Kopf ist die Kennung aus dem TCP-Zustand.**
//    Panadapter `0x40000000`, Wasserfall `0x42000000`, Ton
//    `0x04000008` — dieselben Zahlen, die `display pan` und
//    `stream create` gemeldet hatten. Es gibt nichts zuzuordnen,
//    deshalb traegt jede Meldung hier ihre StreamID.
//
// 2. **Die vier Null-Byte am Paketende sind KEIN VITA-Anhang.** Das
//    T-Bit im Kopf ist ueberall 0, beim Ton fehlen sie ganz. Wer die
//    Werteanzahl aus der Datagrammlaenge ableitet statt aus dem
//    Unterkopf, liest zwei Werte zu viel — beide 0, und 0 ist
//    Vollausschlag. Deshalb zaehlt hier ueberall der Unterkopf.
//
// 3. **Die FFT-Bins sind Y-Pixel in der ANGEFORDERTEN Hoehe, und die
//    Maschine meldet `y_pixels` nie zurueck.** Zweimal gegengeprueft
//    (ypixels 700/1000 → Anteil beide 0,770; Spanne −130..−40 bzw.
//    −150..−50 → beide −109,34 dBm). Der Maszstab muss also von der
//    Stelle kommen, die ihn gesetzt hat; `binsToDbm()` nimmt ihn als
//    Argument und liest ihn nicht irgendwo ab.
//
// 4. **Die Kachelwerte sind absolut und KEINE dBm.** Bei geaenderter
//    dBm-Spanne Byte fuer Byte unveraendert. Sie heissen hier deshalb
//    `colourValue`, nicht `dbm`.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Ported to Longpath by Martin Fischer, AI-assisted via
//                 Anthropic Claude (Cowork). Namespace AetherSDR →
//                 Longpath. Aus der Socket-Klasse herausgezogen (dort
//                 sind Lesen, Rahmenbau, Telemetrie, Opus, DAX und
//                 WAN-Anmeldung in einer Klasse); Qt-Signale durch
//                 Rueckgabewerte ersetzt, damit ohne Ereignisschleife
//                 geprueft werden kann.
// =================================================================

#include <QByteArray>
#include <QList>
#include <QMap>
#include <QVector>

#include <optional>

namespace Longpath {

// From AetherSDR PanadapterStream.cpp:84-85 [@d58e2b8a] +
// PanadapterStream.h:24-33 — die Paketklassen in Byte 14-15.
enum class FlexVitaClass : quint16 {
    NarrowAudio      = 0x03E3,  // float32 Stereo, big-endian
    ReducedBwAudio   = 0x0123,  // int16 Mono, reduzierte Bandbreite
    Meter            = 0x8002,  // Messwerte (id/Wert-Paare)
    PanadapterFft    = 0x8003,  // FFT-Bins
    WaterfallTile    = 0x8004   // Wasserfall-Kacheln
};

namespace FlexVita {

constexpr int kHeaderBytes      = 28;
constexpr int kFftSubBytes      = 12;
constexpr int kTileSubBytes     = 36;
// Byte 8-15 des Kopfes: pad + OUI FlexRadio Systems + Info-Klasse „SL".
// Die Paketklasse in 14-15 bleibt frei, die wird verteilt.
constexpr quint16 kInfoClassSl  = 0x534C;

struct Header {
    bool    valid{false};
    quint8  packetType{0};      // 3 = ExtDataWithStream
    bool    classIdPresent{false};
    bool    trailerPresent{false};   // T-Bit; im Mitschnitt UEBERALL 0
    quint8  counter{0};         // 4-Bit-Rollzaehler — das einzige, was laeuft
    int     sizeWords{0};
    int     sizeBytes{0};
    quint32 streamId{0};
    quint16 infoClass{0};
    quint16 packetClass{0};
    quint32 timestampSeconds{0};
    quint64 timestampFractional{0};
};

// Liest die 28 Byte Kopf. `valid` nur, wenn Laenge, OUI und
// Info-Klasse stimmen — sonst ist es kein FlexRadio-VITA-Paket.
Header parseHeader(const QByteArray& datagram);

// From AetherSDR PanadapterStream.cpp:898-903 [@d58e2b8a].
struct FftSubHeader {
    bool    valid{false};
    quint16 startBin{0};
    quint16 numBins{0};
    quint16 binSize{0};     // gemessen immer 2 (uint16)
    quint16 totalBins{0};
    quint32 frameIndex{0};
};

FftSubHeader parseFftSubHeader(const QByteArray& datagram);

// From AetherSDR PanadapterStream.cpp:1025-1035 [@d58e2b8a].
struct TileSubHeader {
    bool    valid{false};
    qint64  frameLowFreqRaw{0};  // Hz × 2²⁰
    qint64  binBandwidthRaw{0};  // Hz × 2²⁰
    double  lowFreqMhz{0.0};     // schon umgerechnet
    double  binBandwidthMhz{0.0};
    quint32 lineDurationMs{0};   // Offset 16; aus dem Satzbau bei
                                 // PanadapterStream.cpp:1002 [@d58e2b8a].
                                 // Beim ersten Durchgang uebersehen, weil
                                 // der Leser dort Offset 16-19 ueberspringt.
    quint16 tileWidth{0};
    quint16 tileHeight{0};       // gemessen immer 1 (eine Zeile je Kachel)
    quint32 timecode{0};         // trennt die Zeilen
    quint32 autoBlackLevel{0};   // Schwarzpunkt, KEIN Pegel
    quint16 totalBinsInFrame{0};
    quint16 firstBinIndex{0};
};

TileSubHeader parseTileSubHeader(const QByteArray& datagram);

// From AetherSDR VitaTileFrequency.h [@d58e2b8a] — Festkomma Hz × 2²⁰.
// BEDINGUNGSLOS so gelesen, nicht „wenn es gross aussieht": eine
// Kachel, die etwas unter DC ueberhaengt, waere sonst von glatten Hz
// nicht zu unterscheiden (deren #4412).
double vitaFrequencyToMhz(qint64 raw);

// From AetherSDR VitaBinCoverage.h:11-21 [@d58e2b8a] —
// boundedVitaPayloadBinCount(). Ein Unterkopf darf einen vollen
// Abschnitt ankuendigen, obwohl das Datagramm frueher endet; dann
// darf der fehlende Schwanz weder gelesen noch als vorhanden
// vermerkt werden.
int boundedPayloadValueCount(int declaredValues, int bytesPerValue,
                             int availableBytes);

// Bin → dBm. From AetherSDR PanadapterStream.cpp:988-993 [@d58e2b8a]:
//
//   pixel = clamp(bin, 0, yPix-1)
//   dbm   = maxDbm - (pixel / (yPix-1)) * (maxDbm - minDbm)
//   dbm   = clamp(dbm, minDbm, maxDbm)
//
// Beide Klammerungen sind dort begruendet: Werte AB `y_pixels` sind am
// Boden abgeschnittene Messpunkte, kein Hinweis darauf, dass der Rahmen
// still auf eine hoehere Skala gewechselt haette — wer die Hoehe je
// Rahmen erraet, laesst einen abgeschnittenen Rauschboden wie gueltige,
// umskalierte Daten aussehen.
//
// `yPixels`, `minDbm` und `maxDbm` kommen von der Stelle, die sie am
// Gerät gesetzt hat — die Maschine meldet `y_pixels` NICHT zurueck
// (siehe Kopf, Punkt 3). Belegt durch zwei Proben:
//   ypixels 700 → Mittel 539 | ypixels 1000 → Mittel 770  (Anteil 0,770)
//   Spanne −130..−40 und −150..−50 → beide −109,4 dBm
float binToDbm(quint16 bin, double minDbm, double maxDbm, int yPixels);

QVector<float> binsToDbm(const QVector<quint16>& bins,
                         double minDbm, double maxDbm, int yPixels);

// From AetherSDR PanadapterStream.cpp:1093-1095 [@d58e2b8a] —
// (qint16)wert / 128. Heisst hier `colourValue` und nicht `dbm`.
//
// Gemessen: bei geaenderter dBm-Spanne bleiben die Rohwerte Byte fuer
// Byte gleich (10470/11305,8/12310 in beiden Laeufen), waehrend die
// FFT-Bins sich von 770 auf 594 verschoben — der Wasserfall traegt also
// einen eigenen, absoluten Maszstab. /128 ergibt hier ~88,3, was kein
// Pegel ist.
//
// Die Vorlage sagt an ihrem Satzbau dasselbe
// (PanadapterStream.cpp:1014-1016 [@d58e2b8a]):
//   „Conversion: treat as signed int16, divide by 128. Typical noise
//    floor ~96-106, signal peaks ~110-115. Colour-mapped in
//    SpectrumWidget."
// Also eine Farbskala mit `autoBlackLevel` als Schwarzpunkt, und
// nirgends ein dBm-Wert. Daraus keinen Pegel anzeigen, solange er nicht
// gegen ein bekanntes Signal geprueft ist.
float tileColourValue(quint16 raw);

// From AetherSDR VitaBinCoverage.h:23-48 [@d58e2b8a] —
// hasZeroFilledFftGrowthSuffix(). Eine FLEX-8600 mit Firmware 4.2.18
// schickt nach einer x_pixels-VERGROESSERUNG einen Uebergangsrahmen:
// der alte Teil stimmt, jeder neu dazugekommene Bin ist null. Null ist
// im Pixelcode oben, also max_dbm — der Rahmen baut rechts ein hohes
// Brett und verseucht die Mittelung mehrere Zeilen lang.
//
// **Auf Martins 8400 (4.3.16) ist dieser Uebergangsrahmen NICHT
// beobachtet worden.** Im Mitschnitt kam zwar eine Breitenaenderung vor
// (50 → 1024 Bins), aber kein nullgefuellter Anhang. Die Wache wird
// trotzdem mitportiert: sie kostet nichts, faellt zu statt auf, und ein
// bekannter Fehlerweg bleibt sonst offen. Geprueft ist sie mit
// erfundenen Werten, nicht an diesem Geraet — das ist der Unterschied
// zu allem anderen in dieser Datei.
bool hasZeroFilledFftGrowthSuffix(const QVector<quint16>& bins,
                                  int previousAcceptedBins);

// Ton: nach dem Kopf direkt big-endian float32, Stereo verschraenkt
// (L,R,L,R,…), KEIN Unterkopf und KEINE Fuellung.
// From AetherSDR PanadapterStream.cpp:1142-1157 [@d58e2b8a].
// Gemessen: 1052 Byte = 28 + 1024 = 128 Rahmen, genau aufgehend,
// alle Werte endlich und in [-1,1], links ungleich rechts.
QVector<float> narrowAudioSamples(const QByteArray& datagram);

}  // namespace FlexVita

// From AetherSDR VitaBinCoverage.h:88-134 [@d58e2b8a] —
// FftGrowthSuffixAction + FftGrowthSuffixGuard. Normalerweise kommt
// hoechstens EIN nullgefuellter Uebergangsrahmen; jeden solchen Rahmen
// fuer immer abzulehnen waere aber falsch, weil ein endlos wiederholter
// Platzhalter den Panadapter dann ewig auf der alten Breite einfriert.
// Nach kurzer, begrenzter Wartezeit darf der gueltige Teil mit dem
// neuen Anhang auf dem Boden ausgegeben werden; der erste wirklich
// gefuellte Rahmen setzt die Wache zurueck.
enum class FlexFftGrowthAction {
    Accept,
    Reject,
    EmitWithFloorSuffix,
};

class FlexFftGrowthGuard
{
public:
    static constexpr int kRejectedFramesBeforeFloorFallback = 3;

    FlexFftGrowthAction observe(bool zeroFilledGrowth, int totalBins);
    void reset();
    int  consecutiveRejectedFrames() const { return m_consecutiveRejected; }

private:
    int m_rejectedTotalBins{0};
    int m_consecutiveRejected{0};
};

// From AetherSDR VitaBinCoverage.h:52-86 [@d58e2b8a] —
// VitaBinCoverage. Paketzahlen koennen Vollstaendigkeit NICHT belegen:
// doppelte oder ueberlappende UDP-Abschnitte wiederholen Positionen und
// lassen woanders eine Luecke.
class FlexBinCoverage
{
public:
    void reset(int totalValues);
    bool markRange(int firstValue, int count);
    bool isComplete() const;
    int  uniqueValues() const { return m_unique; }
    int  totalValues() const { return m_received.size(); }

private:
    QVector<quint8> m_received;
    int m_unique{0};
};

// Baut die FFT-Rahmen je Strom zusammen. `feed()` gibt den fertigen
// Rahmen zurueck, sobald er vollstaendig ist — sonst nichts.
class FlexFftAssembler
{
public:
    struct Frame {
        quint32 streamId{0};
        quint32 frameIndex{0};
        QVector<quint16> bins;   // Y-Pixel, roh
    };

    std::optional<Frame> feed(const QByteArray& datagram);

    // Nur fuer Pruefstaende und Diagnose.
    int framesStarted() const { return m_framesStarted; }
    int framesRestarted() const { return m_framesRestarted; }
    int framesCompleted() const { return m_framesCompleted; }

private:
    struct Partial {
        quint32 frameIndex{0};
        int     totalBins{0};
        QVector<quint16> buf;
        FlexBinCoverage  coverage;
    };

    QMap<quint32, Partial> m_partial;
    int m_framesStarted{0};
    int m_framesRestarted{0};
    int m_framesCompleted{0};
};

// Baut die Wasserfall-Zeilen je Strom zusammen. Ein neuer Rahmen
// beginnt bei Wechsel von `timecode` ODER `totalBinsInFrame` — ohne die
// zweite Bedingung liesse ein Paket, das einen Zeitcode mit
// aufgeblaehtem `totalBinsInFrame` wiederverwendet, den Puffer zu klein
// und der Schreiblauf ginge darueber hinaus
// (AetherSDR PanadapterStream.cpp:1065-1076 [@d58e2b8a],
// GHSA-7gvg-x594-pprq).
class FlexTileAssembler
{
public:
    struct Row {
        quint32 streamId{0};
        quint32 timecode{0};
        double  lowFreqMhz{0.0};
        double  highFreqMhz{0.0};
        quint32 autoBlackLevel{0};
        QVector<quint16> values;   // roh; Farbmaszstab, keine dBm
    };

    std::optional<Row> feed(const QByteArray& datagram);

    int rowsStarted() const { return m_rowsStarted; }
    int rowsRestarted() const { return m_rowsRestarted; }
    int rowsCompleted() const { return m_rowsCompleted; }

private:
    struct Partial {
        quint32 timecode{0};
        int     totalBins{0};
        double  lowFreqMhz{0.0};
        double  binBandwidthMhz{0.0};
        quint32 autoBlackLevel{0};
        QVector<quint16> buf;
        FlexBinCoverage  coverage;
    };

    QMap<quint32, Partial> m_partial;
    int m_rowsStarted{0};
    int m_rowsRestarted{0};
    int m_rowsCompleted{0};
};

}  // namespace Longpath
