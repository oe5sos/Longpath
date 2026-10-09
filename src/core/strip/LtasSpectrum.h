#pragma once

// =================================================================
// src/core/strip/LtasSpectrum.h  (Longpath)
// =================================================================
//
// Longpath-original.
//
// Das Langzeitspektrum einer Stimme, auf ein festes Raster gelegt.
//
// Gebraucht fuer die eine Frage, die diesen ganzen Tag getragen hat:
// WAS MACHT DIE KETTE? Nicht „was habe ich eingestellt" -- das steht
// in den Reglern und sagt bekanntlich wenig -- sondern was vorne
// hineingeht und hinten herauskommt, gemessen, uebereinandergelegt.
//
// Ein festes Raster und nicht die FFT-Faecher selbst, aus zwei
// Gruenden. Erstens sind zwei Messungen nur dann vergleichbar, wenn
// sie auf denselben Stuetzstellen liegen; die Differenz zweier
// Spektren ist sonst eine Differenz zweier Rasterungen. Zweitens
// ist ein Sechstel-Oktav-Raster das, was man SIEHT: unter 200 Hz
// liegen bei 11,7 Hz je Fach zwei Faecher in einer Terz, oben
// vierhundert. Linear aufgetragen ist eine Stimme ein Strich links
// und Rauschen rechts.
//
// Die FFT ist dieselbe kleine Radix-2 wie in VoiceAnalyzer.cpp und
// aus demselben Grund selbstgebaut: der Baum hat FFTW, aber eine
// Messung, die beim Zeichnen zehnmal in der Sekunde laeuft, soll
// keinen DSP-Unterbau in einen Prueftand ziehen.
//
// =================================================================
// Modification history (Longpath):
//   2026-10-09 — Created in C++20, AI-assisted via Anthropic Claude
//                 (Claude Code), operator Martin Fischer.
// =================================================================

#include <vector>

namespace Longpath::Ltas {

// Die Mittenfrequenzen des Rasters, in Hertz. Sechstel-Oktaven von
// 50 Hz bis 8 kHz -- unten, wo die Stimme ihren Koerper hat, bis
// oben, wo sie nur noch zischt.
const std::vector<double>& centresHz();

// Das Langzeitspektrum von `frames` Abtastwerten, in dB auf das
// Raster gelegt. Leerer Rueckgabewert, wenn zu wenig Material da
// ist (unter einem Fensterlauf).
//
// Die Werte sind RELATIV: der Mittelwert ueber 300..3000 Hz liegt
// bei 0 dB. Absolute Pegel haengen an der Mikrofonverstaerkung und
// taugen nicht zum Vergleich zweier Durchgaenge; die FORM taugt,
// und nur um die geht es hier.
std::vector<double> analyse(const float* samples, int frames, int sampleRate);

} // namespace Longpath::Ltas
