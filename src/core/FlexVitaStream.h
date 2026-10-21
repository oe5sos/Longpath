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

#include "FlexVitaReader.h"

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

    // Der Maszstab je Strom. Siehe Kopf: es gibt kein Echo.
    void setYPixels(quint32 streamId, int yPixels);
    void setDbmRange(quint32 streamId, double minDbm, double maxDbm);

    // Zaehler fuer Diagnose und Pruefstaende.
    qint64 datagramsReceived() const { return m_datagrams; }
    qint64 datagramsIgnored() const { return m_ignored; }
    /// Wieviele Pakete je VITA-Klasse angekommen sind. Fuer Prueflaeufe
    /// und fuer die Frage „kommt ueberhaupt ein Bild?".
    QMap<quint16, qint64> packetsPerClass() const { return m_proKlasse; }

    // Nur fuer Pruefstaende: schiebt ein Datagramm durch denselben Weg,
    // den der Socket nimmt.
    void ingestDatagramForTesting(const QByteArray& datagram);

signals:
    // Fertiger FFT-Rahmen, schon in dBm umgerechnet.
    void spectrumReady(quint32 streamId, const QVector<float>& dbm);
    // Fertige Wasserfallzeile. `values` sind ROH — ein Farbmaszstab,
    // keine dBm (gemessen; siehe FlexVitaReader.h).
    void waterfallRowReady(quint32 streamId, const QVector<quint16>& values,
                           double lowFreqMhz, double highFreqMhz,
                           quint32 autoBlackLevel);
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
    QTimer m_bericht;
    qint64 m_zuletztGemeldet{-1};
    qint64 m_fftRahmen{0};
    qint64 m_wasserfallZeilen{0};
};

}  // namespace Longpath
