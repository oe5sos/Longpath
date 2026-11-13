#ifndef _INCLUDE_LDPC_H_
#define _INCLUDE_LDPC_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

// codeword is 174 log-likelihoods.
// plain is a return value, 174 ints, to be 0 or 1.
// iters is how hard to try.
// ok == 87 means success.
void ldpc_decode(float codeword[], int max_iters, uint8_t plain[], int* ok);

void bp_decode(float codeword[], int max_iters, uint8_t plain[], int* ok);

// ── Longpath-Abweichung (2026-11-13): a-priori-Dekodierung ──────────
//
// Dieselbe Rechnung wie `bp_decode`, aber einzelne Bits gelten als
// BEKANNT: fuer sie zaehlt allein der mitgegebene Wert in `codeword`,
// die Nachrichten der Pruefknoten werden weder gelesen noch verrechnet.
//
// Vorbild ist WSJT-X, `lib/ft8/decode174_91.f90:55`:
//
//     if( apmask(i) .ne. 1 ) then
//        zn(i)=llr(i)+sum(tov(1:ncw,i))
//     else
//        zn(i)=llr(i)
//     endif
//
// `ap_mask` ist NULL oder zeigt auf FTX_LDPC_N Flaggen (0/1). Mit NULL
// verhaelt sich die Funktion Bit fuer Bit wie `bp_decode` -- deshalb
// ruft `bp_decode` sie auch selbst, statt die Schleife zu doppeln.
//
// Der Aufrufer traegt die Verantwortung dafuer, dass die bekannten
// Bits auch stimmen. Der CRC am Ende der Kette ist die Kontrolle:
// raet man falsch, faellt die Nachricht dort durch.
void bp_decode_ap(float codeword[], const uint8_t* ap_mask, int max_iters,
                  uint8_t plain[], int* ok);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_LDPC_H_
