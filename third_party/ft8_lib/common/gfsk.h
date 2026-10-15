#ifndef _INCLUDE_GFSK_H_
#define _INCLUDE_GFSK_H_

// Kopfdatei fuer die beiden Funktionen, die in ft8_lib nur im
// Beispielprogramm demo/gen_ft8.c stehen und dort keine eigene
// Kopfdatei haben. Siehe gfsk.c fuer die Herkunft.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void gfsk_pulse(int n_spsym, float symbol_bt, float* pulse);

void synth_gfsk(const uint8_t* symbols, int n_sym, float f0, float symbol_bt,
                float symbol_period, int signal_rate, float* signal);

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_GFSK_H_
