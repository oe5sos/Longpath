// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RttyTx.h  (Longpath)
// =================================================================
//
// RTTY senden: ASCII hinein, FSK-Toene hinaus.
//
// ── Was das Gegenstueck dazu ist ────────────────────────────────────
//
// RttyDecoder (src/core/RttyDecoder.{h,cpp}) hoert RTTY. Diese Klasse
// ist seine Umkehrung und benutzt dieselben Zahlen: Baudot/ITA2,
// Mark 2295 Hz, Shift 170 Hz, 45,45 Baud, 48 kHz. Der Prueflauf macht
// daraus einen Rundlauf — was hier hinausgeht, liest der Dekoder
// wieder ein.
//
// ── Was sie NICHT tut ───────────────────────────────────────────────
//
// Tasten. Kein MOX, keine Folgeschaltung. Dieselbe Trennung wie bei
// Ft8Tx: eine Klasse, die Ton erzeugt UND tastet, laesst sich nicht
// mehr gefahrlos pruefen. Das Tasten entscheidet der MoxController.
//
// ── Der Rahmen eines Zeichens ───────────────────────────────────────
//
// Baudot ist 5 Bit breit und kennt zwei Belegungen (Buchstaben und
// Ziffern/Zeichen), zwischen denen mit LTRS (11111) und FIGS (11011)
// umgeschaltet wird. Ein Zeichen auf dem Band ist:
//
//     1 Startbit    Space
//     5 Datenbits   LSB zuerst
//     1,5 Stoppbits Mark
//
// Die anderthalb Stoppbits sind keine Marotte: bei 45,45 Baud ist das
// die Norm, und ein Dekoder, der auf 2 wartet, verliert den Takt.
//
// ── Warum die Phase weiterlaeuft ────────────────────────────────────
//
// Beim Wechsel zwischen Mark und Space wird NICHT neu angesetzt,
// sondern der Phasenwinkel weitergefuehrt. Ein Sprung in der Phase
// ist ein Knacks im Signal und verbreitert das Spektrum weit ueber
// die 170 Hz hinaus — auf dem Band hoert man das als Rasseln neben
// dem eigenen Signal.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#pragma once

#include "TxMicRouter.h"

#include <QString>
#include <QVector>

#include <atomic>

namespace Longpath {

class RttyTxSource : public TxMicRouter
{
public:
    /// `micRateHz` ist die Rate des Sendewegs (48 000 im Programm).
    explicit RttyTxSource(int micRateHz = 48000);

    /// Einen Text auf Toene legen. Danach steht er bereit, aber es
    /// geht nichts hinaus, bis `starten()` gerufen wird.
    ///
    /// `markHz` ist der obere Ton, `shiftHz` der Abstand nach unten
    /// (Space = markHz - shiftHz) — dieselbe Konvention wie im
    /// Dekoder. Gibt false bei leerem Text oder unsinnigen Zahlen.
    bool vorbereiten(const QString& text,
                     int   markHz  = 2295,
                     int   shiftHz = 170,
                     float baud    = 45.45f);

    /// Von vorn abspielen. Ohne vorbereiteten Text wirkungslos.
    void starten();

    /// Sofort aufhoeren. Der Rest bleibt still.
    void abbrechen();

    bool laeuft() const { return m_laeuft.load(std::memory_order_relaxed); }

    /// Wie viele Proben der vorbereitete Text am Sendeweg hat.
    int proben() const { return m_proben.size(); }

    /// Der vorbereitete Text, oder leer.
    QString text() const { return m_text; }

    /// ASCII zu Baudot: die Folge der 5-Bit-Zeichen, mit den noetigen
    /// LTRS/FIGS-Umschaltungen dazwischen. Statisch und ohne Zustand,
    /// damit die Regel ohne Tonerzeugung zu pruefen ist.
    ///
    /// Zeichen, die Baudot nicht kennt, fallen weg — ein Fragezeichen
    /// einzusetzen waere schlimmer, weil es den Text verfaelscht.
    static QVector<int> nachBaudot(const QString& text);

    /// Die beiden Umschaltzeichen, wie sie auf dem Band stehen.
    static constexpr int kLtrs = 0x1F;   ///< 11111
    static constexpr int kFigs = 0x1B;   ///< 11011

    // ── TxMicRouter ──────────────────────────────────────────────────
    int pullSamples(float* dst, int n) override;

private:
    int               m_micRateHz;
    QString           m_text;
    QVector<float>    m_proben;
    std::atomic<bool> m_laeuft{false};
    std::atomic<int>  m_pos{0};
};

} // namespace Longpath
