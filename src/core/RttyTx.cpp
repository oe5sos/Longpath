// SPDX-License-Identifier: GPL-3.0-or-later
// =================================================================
// src/core/RttyTx.cpp  (Longpath)
// =================================================================
// Siehe RttyTx.h fuer den Rahmen eines Zeichens und die Begruendung,
// warum die Phase beim Tonwechsel weiterlaeuft.
//
// Modification history (Longpath):
//   2026-10-16 — Original fuer Longpath von Martin Fischer,
//                 KI-gestuetzt ueber Anthropic Claude (Cowork).
// =================================================================
#include "RttyTx.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Longpath {

namespace {

// Dieselben Tabellen wie im Dekoder (RttyDecoder.cpp), nur
// andersherum gelesen. '\0' heisst „dieser Platz traegt kein
// druckbares Zeichen".
const char kLtrsTabelle[32] = {
    '\0', 'E', '\n', 'A', ' ', 'S', 'I', 'U',
    '\r', 'D',  'R', 'J', 'N', 'F', 'C', 'K',
     'T', 'Z',  'L', 'W', 'H', 'Y', 'P', 'Q',
     'O', 'B',  'G', '\0', 'M', 'X', 'V', '\0'
};
const char kFigsTabelle[32] = {
    '\0', '3', '\n', '-', ' ', '\a', '8', '7',
    '\r', '$',  '4','\'', ',', '!', ':', '(',
     '5', '"',  ')', '2', '#', '6', '0', '1',
     '9', '?',  '&', '\0', '.', '/', ';', '\0'
};

/// Wo steht dieses Zeichen in der Buchstabentafel? -1, wenn nicht.
int imLtrs(char c)
{
    for (int i = 0; i < 32; ++i) {
        if (kLtrsTabelle[i] == c && c != '\0') { return i; }
    }
    return -1;
}

int imFigs(char c)
{
    for (int i = 0; i < 32; ++i) {
        if (kFigsTabelle[i] == c && c != '\0') { return i; }
    }
    return -1;
}

}  // namespace

QVector<int> RttyTxSource::nachBaudot(const QString& text)
{
    QVector<int> aus;
    if (text.isEmpty()) { return aus; }

    // Der Dekoder faengt in der Buchstabenbelegung an, und so faengt
    // jede Sendung an: mit LTRS. Ohne dieses erste Zeichen steht der
    // Empfaenger unter Umstaenden noch in FIGS vom vorigen Durchgang
    // und druckt Ziffern statt Buchstaben.
    aus.append(kLtrs);
    bool figs = false;

    const QString gross = text.toUpper();
    for (const QChar qc : gross) {
        const char c = qc.toLatin1();
        if (c == '\0') { continue; }          // nichts aus Latin-1

        const int imL = imLtrs(c);
        const int imF = imFigs(c);

        // Leerzeichen, Zeilenende und Wagenruecklauf stehen in BEIDEN
        // Tafeln auf demselben Platz. Fuer sie wird NICHT umgeschaltet
        // — sonst steht nach jedem Wort eine Umschaltung mehr auf dem
        // Band, und bei 45 Baud kostet jede davon 22 ms.
        const bool inBeiden = (imL >= 0 && imF >= 0 && imL == imF);

        if (inBeiden) {
            aus.append(imL);
            continue;
        }
        if (imL >= 0 && !figs) { aus.append(imL); continue; }
        if (imF >= 0 &&  figs) { aus.append(imF); continue; }
        if (imL >= 0) {                       // in der anderen Tafel
            aus.append(kLtrs);
            figs = false;
            aus.append(imL);
            continue;
        }
        if (imF >= 0) {
            aus.append(kFigs);
            figs = true;
            aus.append(imF);
            continue;
        }
        // Baudot kennt dieses Zeichen nicht. Es faellt weg; ein
        // Ersatzzeichen waere schlimmer, weil es den Text verfaelscht.
    }
    return aus;
}

RttyTxSource::RttyTxSource(int micRateHz)
    : m_micRateHz(micRateHz > 0 ? micRateHz : 48000)
{
}

bool RttyTxSource::vorbereiten(const QString& text, int markHz,
                               int shiftHz, float baud)
{
    abbrechen();
    m_text.clear();
    m_proben.clear();

    if (text.trimmed().isEmpty()) { return false; }
    if (markHz <= 0 || shiftHz <= 0 || baud <= 0.0f) { return false; }
    const int spaceHz = markHz - shiftHz;
    if (spaceHz <= 0) { return false; }

    const QVector<int> zeichen = nachBaudot(text);
    if (zeichen.isEmpty()) { return false; }

    // Ein Zeichen sind 7,5 Bitzeiten: Start + 5 Daten + 1,5 Stopp.
    // Die Bitdauer ist bei 45,45 Baud 22,0 ms — bei 48 kHz sind das
    // 1056,x Proben, also KEINE ganze Zahl. Darum wird nicht in
    // Proben je Bit gerechnet, sondern ueber die Zeit: fuer jede Probe
    // steht fest, in welchem Bit sie liegt. Sonst laeuft der Takt ueber
    // eine lange Sendung hinweg davon.
    const double probenJeBit = static_cast<double>(m_micRateHz) / baud;
    const double bitsJeZeichen = 7.5;
    const double probenGesamt =
        probenJeBit * bitsJeZeichen * static_cast<double>(zeichen.size());

    const int anzahl = static_cast<int>(std::llround(probenGesamt));
    m_proben.resize(anzahl);

    // Die Phase laeuft durch, auch ueber Tonwechsel hinweg — siehe die
    // Begruendung in RttyTx.h.
    double phase = 0.0;
    const double zweiPi = 2.0 * M_PI;

    for (int i = 0; i < anzahl; ++i) {
        const double bitIndex = static_cast<double>(i) / probenJeBit;
        const int    zIndex   = static_cast<int>(bitIndex / bitsJeZeichen);
        const double imZeichen = bitIndex - zIndex * bitsJeZeichen;

        bool mark = true;                     // Ruhe und Stopp sind Mark
        if (zIndex < zeichen.size()) {
            if (imZeichen < 1.0) {
                mark = false;                 // Startbit ist Space
            } else if (imZeichen < 6.0) {
                const int bit = static_cast<int>(imZeichen) - 1;   // 0..4
                mark = ((zeichen[zIndex] >> bit) & 1) != 0;        // LSB zuerst
            }
        }

        const double f = mark ? markHz : spaceHz;
        phase += zweiPi * f / static_cast<double>(m_micRateHz);
        if (phase > zweiPi) { phase -= zweiPi; }

        // 0,5 statt Vollaussteuerung: dieselbe Hoehe, mit der die
        // anderen Tonquellen in den Sendeweg gehen.
        m_proben[i] = 0.5f * static_cast<float>(std::sin(phase));
    }

    m_text = text;
    return true;
}

void RttyTxSource::starten()
{
    if (m_proben.isEmpty()) { return; }
    m_pos.store(0, std::memory_order_release);
    m_laeuft.store(true, std::memory_order_release);
}

void RttyTxSource::abbrechen()
{
    m_laeuft.store(false, std::memory_order_release);
    m_pos.store(0, std::memory_order_release);
}

int RttyTxSource::pullSamples(float* dst, int n)
{
    if (!dst || n <= 0) { return 0; }

    // Laeuft nichts, kommt Stille — nicht etwa der Rest von vorhin.
    if (!m_laeuft.load(std::memory_order_acquire)) {
        std::memset(dst, 0, static_cast<size_t>(n) * sizeof(float));
        return n;
    }

    const int pos   = m_pos.load(std::memory_order_acquire);
    const int übrig = m_proben.size() - pos;
    const int nehme = std::clamp(übrig, 0, n);

    if (nehme > 0) {
        std::memcpy(dst, m_proben.constData() + pos,
                    static_cast<size_t>(nehme) * sizeof(float));
        m_pos.store(pos + nehme, std::memory_order_release);
    }
    if (nehme < n) {
        std::memset(dst + nehme, 0,
                    static_cast<size_t>(n - nehme) * sizeof(float));
        // Durch: von selbst aufhoeren, damit nicht der naechste Zug
        // den Rest von diesem hoert.
        m_laeuft.store(false, std::memory_order_release);
    }
    return n;
}

} // namespace Longpath
