// =================================================================
// tests/flex_vita_golden.h  (Longpath)
// =================================================================
//
// Goldene Bytes aus dem echten VITA-49-Strom einer FLEX-8400,
// aufgezeichnet am 2026-10-18 an OE5SOS' Station (172.30.30.12,
// SmartSDR 4.3.16.42034, API 1.4.0.0).
//
// **Diese Bytes sind gelesener Draht, nicht erfunden.** Enthalten sind
// jeweils Kopf und Unterkopf vollstaendig — also genau die Stellen, an
// denen eine falsche Annahme toedlich und nicht zu sehen ist — plus ein
// kurzes Stueck Nutzlast am Anfang und am Ende. Die Mitte der
// Nutzlasten ist weggelassen: das sind Felder von uint16 bzw. float32,
// da entscheidet kein Satzbau mehr, und der vollstaendige Mitschnitt
// waere 5,7 MB.
//
// Wo ein Pruefstand ganze Rahmen braucht, baut er sie aus diesen echten
// Koepfen und aufgefuellten Werten zusammen; das steht dort jeweils
// dran.
//
// Satzbau und Messprotokoll: docs/protocols/flex-vita49-gemessen.md
//
// =================================================================
// Modification history (Longpath):
//   2026-10-18 — Original for Longpath by Martin Fischer, AI-assisted
//                 via Anthropic Claude (Cowork). Mechanisch aus dem
//                 Mitschnitt erzeugt.
// =================================================================

// no-port-check: Longpath-original. Die Bytes sind gelesener Draht,
// kein portierter Code.

#pragma once

namespace LongpathTest::FlexVitaGolden {

// FFT, Paket A: Kopf 28 + Unterkopf 12, echt
static const char* const kFftKopfA =
    "385c015a4000000000001c2d534c80036ad1074400000000000000000000029e"
    "0002040000000006";

// FFT, Paket B: Kopf 28 + Unterkopf 12, echt
static const char* const kFftKopfB =
    "385d00bc4000000000001c2d534c80036ad107440000000000000000029e0162"
    "0002040000000006";

// FFT A: die ersten 8 Bins, echt
static const char* const kFftBinsA16 =
    "000f000f000f000f000f000f000f000f";

// FFT A: letzte 16 Byte — 6 Bins + 4 Byte Fuellung
static const char* const kFftEndeA16 =
    "07d8074e07a407970793079e00000000";

// Wasserfall, Kachel 0: Kopf 28 + Unterkopf 36, echt
static const char* const kKachelKopf0 =
    "3856015a4200000000001c2d534c80046ad4b516000000000000000000000d3a"
    "08380000000000000bb8000000000050029200010000000400001f21099c0000";

// Wasserfall, Kachel 1: Kopf 28 + Unterkopf 36, echt
static const char* const kKachelKopf1 =
    "3857015a4200000000001c2d534c80046ad4b516000000000000000000000d3a"
    "08380000000000000bb8000000000050029200010000000400001f21099c0292";

// Wasserfall, Kachel 2: Kopf 28 + Unterkopf 36, echt
static const char* const kKachelKopf2 =
    "3858015a4200000000001c2d534c80046ad4b516000000000000000000000d3a"
    "08380000000000000bb8000000000050029200010000000400001f21099c0524";

// Wasserfall, Kachel 3: Kopf 28 + Unterkopf 36, echt
static const char* const kKachelKopf3 =
    "385901044200000000001c2d534c80046ad4b516000000000000000000000d3a"
    "08380000000000000bb800000000005001e600010000000400001f21099c07b6";

// Kachel 0: die ersten 8 Werte, echt
static const char* const kKachelWerte0_16 =
    "2cae2c4e2cce2d762d4e2d0e2cb62c16";

// Ton: Kopf 28, echt
static const char* const kTonKopf =
    "385001070400000800001c2d534c03e36ad391ed0000000000002b01";

// Ton: die ersten 4 float32-Werte, echt
static const char* const kTonWerte16 =
    "3ea604d1bdb26ecc3e76516cbe394134";

}  // namespace LongpathTest::FlexVitaGolden
