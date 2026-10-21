// =================================================================
// src/core/GeniusStatusGate.h  (Longpath)
// =================================================================
//
// Eine `status`-Abfrage zur Zeit auf der Strecke zum Power Genius XL oder
// Tuner Genius XL. Ein `status` geht nicht hinaus, solange der vorige
// unbeantwortet ist; nach kReplyWaitMs gilt die Antwort als verloren und
// der naechste geht, damit eine verschluckte Antwort die Anzeigen nicht
// fuer immer stillstellt. Es wird NICHTS aufgestaut — die naechste
// Abfrage fragt.
//
// no-port-check: Longpath-eigene Datei. Worauf sie sich stuetzt:
//   - Der Befund stammt aus dem Upstream NereusSDR (J.J. Boyd, KG4VCF,
//     2026-10-12, Fassung 2026.10.1): an einer Power Genius XL mit
//     Firmware 3.9.9 legte der Verstaerker die Verbindung in zwoelf
//     Minuten viermal auf, jeweils 30 bis 175 ms nach einem `status`, der
//     hinausging, waehrend der vorige noch unbeantwortet war. Unter
//     Windows faellt das als Abbruch etwa 90 s nach dem Verbinden auf,
//     weil dort der 30-s-Keepalive und die 5-Hz-Abfrage zusammenfallen.
//   - Dass die Hersteller-Programme dauernd `status` abfragen, steht in
//     unserem eigenen Mitschnittblatt:
//     captures/flex-pgxl-tgxl-capture-NOTES.md:105 (PowerGeniusDesktop /
//     TunerGeniusDesktop ziehen `status`).
//   - Dass die Antwort die Folgenummer mitfuehrt — und damit zuordenbar
//     ist — steht im Rahmenformat, das PgxlConnection::processLine liest
//     (`R<seq>|<hex>|<body>`) bzw. TgxlConnection (`S<seq>|…`).
//     Genau diese Nummer vergleicht answered().
//
// Modification history (Longpath):
//   2026-10-21 — Angelegt; gemeinsam genutzt von PgxlConnection und
//                TgxlConnection. Uebernommen aus Upstream NereusSDR
//                (GeniusStatusGate, 2026-10-12). Longpath, KI-gestuetzt
//                via Anthropic Claude Code.
// =================================================================
#pragma once

#include <QElapsedTimer>
#include <QtGlobal>

namespace Longpath {

class GeniusStatusGate {
public:
    // Wie lange ein unbeantworteter `status` den naechsten zurueckhaelt.
    // Die langsamsten Antworten, die upstream an der Bank sah: 0,26 s
    // (Verstaerker) und 0,65 s (Tuner). 2 s laesst also reichlich Luft und
    // verliert im schlimmsten Fall zehn Abfragen der 5-Hz-Reihe.
    static constexpr int kReplyWaitMs = 2000;

    // Wahr, solange der letzte `status` weder beantwortet noch
    // abgewartet ist.
    bool answerPending() const
    {
        return m_inFlightSeq != 0 && m_sinceSent.isValid()
            && m_sinceSent.elapsed() < m_replyWaitMs;
    }

    // Ein `status` mit dieser Folgenummer ging auf die Strecke.
    void sent(quint32 seq)
    {
        m_inFlightSeq = seq;
        m_sinceSent.start();
    }

    // Das Geraet hat diese Folgenummer beantwortet — ganz gleich, was in
    // der Antwort steht.
    void answered(quint32 seq)
    {
        if (seq == m_inFlightSeq) {
            m_inFlightSeq = 0;
        }
    }

    // Neue Verbindung: ein `status`, den die alte nie beantwortet hat,
    // haelt hier nichts zurueck.
    void reset() { m_inFlightSeq = 0; }

    // Nur fuer Pruefstaende.
    void setReplyWaitMsForTesting(int ms) { m_replyWaitMs = ms > 0 ? ms : 1; }
    quint32 inFlightSeqForTesting() const { return m_inFlightSeq; }

private:
    int           m_replyWaitMs{kReplyWaitMs};
    quint32       m_inFlightSeq{0};
    QElapsedTimer m_sinceSent;
};

}  // namespace Longpath
