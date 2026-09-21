/*  nnr_model_stub.c

This file is part of a program that implements a Software-Defined Radio.

Copyright (C) 2026 NereusSDR contributors (NereusSDR-original glue stub)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

*/

// no-port-check: NereusSDR-original glue stub. Not a port of Thetis logic;
// provides the `nnr_model_0_data` / `nnr_model_0_size` and
// `nnr_model_1_data` / `nnr_model_1_size` symbols that `nnet.c` (WDSP 2.10,
// Warren Pratt NR0V) declares `extern` and links against as the compiled-in
// fallback weights for NNR (Neural Noise Reduction) model slots 0 and 1.
//
// Upstream ships those symbols from `nnr_model_0.c` / `nnr_model_1.c` —
// Warren Pratt's own trained weights, generated as C hex-byte-array
// literals (~10.7 MB and ~24 MB of source text for ~2.0 MB and ~4.5 MB of
// actual tensor data respectively; see docs/attribution/WDSP-PROVENANCE.md).
// NereusSDR does not compile those generated files in, for the same reason
// RNNoise's `rnnoise_data.c` is excluded (see
// docs/attribution/RNNOISE-PROVENANCE.md): `nnet.c`'s `nnet_build()`
// (nnet.c:1079-1088 [wdsp 2.10]) already tries an external file at
// `nnet_model_path[slot]` (default "wdsp_nnr_0.bin" / "wdsp_nnr_1.bin",
// overridable via the upstream-exported `SetNNRModelPathSlot`) *before*
// falling back to the compiled-in `nnr_builtin[slot]` table, so the real
// weights only need to exist once, as the two small binary tensor files
// this stub's zero-length arrays never override. `RxChannel` points
// `SetNNRModelPathSlot` at the bundled `wdsp_nnr_0.bin` / `wdsp_nnr_1.bin`
// resource files on radio connect (mirroring `RNNRloadModel`'s timing).
//
// If a distribution ever ships without those two resource files, nnet.c's
// own fallback path handles a zero-size `nnr_builtin` entry safely:
// `nnet_build()` returns 0, the model is marked not-ready, and `xnnr()`
// passes audio through unmodified — the same inert-passthrough behavior
// already established for RNNR when no rnnoise model is loaded.


// =================================================================
// third_party/wdsp/src/nnr_model_stub.c (NereusSDR)
// =================================================================
//
// Replaces upstream `nnr_model_0.c` + `nnr_model_1.c` (WDSP 2.10,
// `wdsp 2.10/Source/nnr_model_{0,1}.c`, Warren Pratt NR0V) with empty
// placeholder arrays. The real weights ship as
// `third_party/wdsp/models/wdsp_nnr_0.bin` / `wdsp_nnr_1.bin` instead —
// extracted byte-for-byte from the upstream generated arrays via
// `scripts/extract-nnr-models.py` (see WDSP-PROVENANCE.md for the
// extraction record and checksum).
//
// =================================================================
// Modification history (Longpath):
//   2026-09-14 — NereusSDR-original glue stub created during the WDSP 2.10
//                 NNR port, with AI-assisted implementation via Anthropic
//                 Claude Code. Keeps the ~34 MB of generated model-weight
//                 C source out of the compiled binary; see rationale above.
// =================================================================

const unsigned char nnr_model_0_data[] = { 0 };
const unsigned int  nnr_model_0_size = 0u;

const unsigned char nnr_model_1_data[] = { 0 };
const unsigned int  nnr_model_1_size = 0u;
