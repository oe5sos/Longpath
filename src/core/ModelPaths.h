#pragma once

// =================================================================
// src/core/ModelPaths.h  (NereusSDR)
// =================================================================
//
// NereusSDR-original (no Thetis equivalent — Thetis bundles rnnoise
// as a DLL and loads it via P/Invoke; NereusSDR resolves the .bin
// file path at runtime across platform install layouts).
//
// =================================================================
// Modification history (NereusSDR):
//   2026-04-23 — Written for NereusSDR by J.J. Boyd (KG4VCF),
//                 with AI-assisted development via Anthropic Claude Code.
//   2026-04-23 — Added rnnoiseDefaultSmallBin() and dfnrModelTarball()
//                 helpers for Sub-epic C-1 packaging.
// =================================================================

#include <QString>

namespace Longpath::ModelPaths {

// Path to the bundled rnnoise "Default (large)" model. Resolves to:
//   macOS:   .app/Contents/Resources/models/rnnoise/Default_large.bin
//   Linux:   <exe-dir>/../share/NereusSDR/models/rnnoise/Default_large.bin
//            OR <XDG_DATA>/NereusSDR/models/rnnoise/Default_large.bin
//   Windows: <exe-dir>/models/rnnoise/Default_large.bin
//   Dev:     <source>/third_party/rnnoise/models/Default_large.bin
//            (located via relative path from the build-dir binary)
//
// Returns empty QString if the file isn't found at any of the search
// locations. The caller should warn and disable NR3 in that case.
// See docs/architecture/phase3g-rx-epic-c1-nr-xplat-plan.md Task 22
// for packaging details.
QString rnnoiseDefaultLargeBin();

// Path to the bundled rnnoise "Default (small)" model. Same probe
// sequence as rnnoiseDefaultLargeBin() but for Default_small.bin.
// Returns empty QString if not found.
QString rnnoiseDefaultSmallBin();

// Path to the bundled DeepFilterNet3 ONNX model tarball. Resolves to:
//   macOS:   .app/Contents/Resources/models/dfnet3/DeepFilterNet3_onnx.tar.gz
//   Linux:   <exe-dir>/../share/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
//            OR <XDG_DATA>/NereusSDR/models/dfnet3/DeepFilterNet3_onnx.tar.gz
//   Windows: <exe-dir>/models/dfnet3/DeepFilterNet3_onnx.tar.gz
//   Dev:     <source>/third_party/deepfilter/models/DeepFilterNet3_onnx.tar.gz
//            (located via relative path from the build-dir binary)
//
// Returns empty QString if not found. The caller should warn and
// disable DFNR in that case.
QString dfnrModelTarball();

// Path to the bundled NNR (Neural Noise Reduction, WDSP 2.10) model-0
// weight file. Same probe sequence as rnnoiseDefaultLargeBin(), but under
// the "nnr" subdir (dev-build fallback resolves to
// third_party/wdsp/models/wdsp_nnr_0.bin — NNR ships inside the already
// in-tree wdsp vendor directory, not a separate third_party/ subproject).
// Returns empty QString if not found.
QString nnrModel0Bin();

// Path to the bundled NNR model-1 (larger) weight file. Same probe
// sequence as nnrModel0Bin().
QString nnrModel1Bin();

} // namespace Longpath::ModelPaths
