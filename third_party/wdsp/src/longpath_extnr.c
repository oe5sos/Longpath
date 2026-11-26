// no-port-check: Longpath-original wrapper around vendored WDSP; calls its create/execute functions (values as in RXA.c, attributed below and in WDSP-PROVENANCE.md) -- not a Thetis C# port.
// =============================================================================
// longpath_extnr.c  (Longpath-original)
// =============================================================================
//
// Freistehender Zugang zu WDSP-emnr (NR2) und WDSP-sbnr (NR4) fuer Ton, der
// schon demoduliert ankommt (Flex, Kiwi, TCI). Es ist DER GLEICHE Code wie im
// Empfangskanal der Anvelina -- nur ohne Kanal drumherum. Kein Algorithmus
// ist veraendert; die Huelle setzt nur Puffer und Schalter.
//
// Ein Aufruf = ein Block zu `bsize` Mono-Proben (float, 48 kHz).
//
// Copyright (C) 2026 Martin Fischer (OE5SOS)
// Argument values of the create calls as in RXA.c,
// Copyright (C) 2013, 2014, 2015, 2016, 2023, 2025 Warren Pratt, NR0V
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
// FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
// more details.
//
// Modification history (Longpath):
//   2026-11-24 — Created for Longpath by Martin Fischer, AI-assisted via
//                Anthropic Claude Code.
// =============================================================================

#include "comm.h"

typedef struct _lp_extnr
{
	int     kind;      // 2 = emnr, 4 = sbnr
	int     bsize;
	double* in;        // 2 * bsize (I, Q verschraenkt wie WDSP)
	double* out;
	EMNR    emnr;
	SBNR    sbnr;
} lp_extnr, *LP_EXTNR;

PORT LP_EXTNR lp_extnr_create_emnr(int bsize, int rate, int gain_method, int npe_method, int ae_run)
{
	LP_EXTNR a = (LP_EXTNR) malloc0(sizeof(lp_extnr));
	a->kind = 2;
	a->bsize = bsize;
	a->in  = (double*) malloc0(2 * bsize * sizeof(double));
	a->out = (double*) malloc0(2 * bsize * sizeof(double));
	// Werte wie RXA.c: FFT 4096, Ueberlappung 4, Fenster 0, Gain 1.0, Lauf an.
	a->emnr = create_emnr(1, 0, bsize, a->in, a->out, 4096, 4, rate, 0, 1.0,
	                      gain_method, npe_method, ae_run);
	return a;
}

PORT void lp_extnr_set_emnr(LP_EXTNR a, int gain_method, int npe_method, int ae_run)
{
	if (!a || a->kind != 2) return;
	a->emnr->g.gain_method = gain_method;
	a->emnr->g.npe_method  = npe_method;
	a->emnr->g.ae_run      = ae_run;
}

PORT LP_EXTNR lp_extnr_create_sbnr(int bsize, int rate)
{
	LP_EXTNR a = (LP_EXTNR) malloc0(sizeof(lp_extnr));
	a->kind = 4;
	a->bsize = bsize;
	a->in  = (double*) malloc0(2 * bsize * sizeof(double));
	a->out = (double*) malloc0(2 * bsize * sizeof(double));
	a->sbnr = create_sbnr(1, 0, bsize, a->in, a->out, rate);
	return a;
}

PORT void lp_extnr_set_sbnr(LP_EXTNR a, float reduction, float smoothing, float whitening,
                            float rescale, float post_thresh, int scaling_type)
{
	if (!a || a->kind != 4) return;
	a->sbnr->reduction_amount     = reduction;
	a->sbnr->smoothing_factor     = smoothing;
	a->sbnr->whitening_factor     = whitening;
	a->sbnr->noise_rescale        = rescale;
	a->sbnr->post_filter_threshold = post_thresh;
	a->sbnr->noise_scaling_type   = scaling_type;
}

PORT void lp_extnr_process(LP_EXTNR a, const float* in, float* out)
{
	int i;
	if (!a) return;
	for (i = 0; i < a->bsize; i++) { a->in[2 * i] = in[i]; a->in[2 * i + 1] = 0.0; }
	if (a->kind == 2) xemnr(a->emnr, 0);
	else              xsbnr(a->sbnr, 0);
	for (i = 0; i < a->bsize; i++) out[i] = (float) a->out[2 * i];
}

PORT void lp_extnr_flush(LP_EXTNR a)
{
	if (!a) return;
	if (a->kind == 2) flush_emnr(a->emnr);
}

PORT void lp_extnr_destroy(LP_EXTNR a)
{
	if (!a) return;
	if (a->kind == 2) destroy_emnr(a->emnr);
	else              destroy_sbnr(a->sbnr);
	_aligned_free(a->in);
	_aligned_free(a->out);
	_aligned_free(a);
}
