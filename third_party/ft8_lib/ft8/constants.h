#ifndef _INCLUDE_CONSTANTS_H_
#define _INCLUDE_CONSTANTS_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define FT8_SYMBOL_PERIOD (0.160f) ///< FT8 symbol duration, defines tone deviation in Hz and symbol rate
#define FT8_SLOT_TIME     (15.0f)  ///< FT8 slot period

#define FT4_SYMBOL_PERIOD (0.048f) ///< FT4 symbol duration, defines tone deviation in Hz and symbol rate
#define FT4_SLOT_TIME     (7.5f)   ///< FT4 slot period

// Longpath-Erweiterung (2026-11-17): FT2 ist FT4 mit halber Symboldauer.
// Gleiche Toene, gleiche Synchronfolgen, gleiche Codierung -- nur
// 288 statt 576 Proben je Symbol bei 12 kHz und ein 3,75-s-Takt.
// Belegt in drei unabhaengigen Quellen (siehe FT8LIB-PROVENANCE.md):
//   Decodium 4.0  widgets/mainwindow.cpp  m_toneSpacing=12000.0/288.0, m_TRperiod=3.75
//   Decodium 4.0  Modulator/FtxMessageEncoder.cpp  map_ft2_ft4_tones (eine Abbildung fuer beide)
//   MSHV 2.76.7   HvGenFt2/gen_ft2.cpp  nsym=103, nsps=4*288 bei 48 kHz, icos4a..d wie FT4
#define FT2_SYMBOL_PERIOD (0.024f) ///< FT2 symbol duration (288 samples at 12 kHz)
#define FT2_SLOT_TIME     (3.75f)  ///< FT2 slot period

// Define FT8 symbol counts
// FT8 message structure:
//     S D1 S D2 S
// S  - sync block (7 symbols of Costas pattern)
// D1 - first data block (29 symbols each encoding 3 bits)
#define FT8_ND          (58) ///< Data symbols
#define FT8_NN          (79) ///< Total channel symbols (FT8_NS + FT8_ND)
#define FT8_LENGTH_SYNC (7)  ///< Length of each sync group
#define FT8_NUM_SYNC    (3)  ///< Number of sync groups
#define FT8_SYNC_OFFSET (36) ///< Offset between sync groups

// Define FT4 symbol counts
// FT4 message structure:
//     R Sa D1 Sb D2 Sc D3 Sd R
// R  - ramping symbol (no payload information conveyed)
// Sx - one of four _different_ sync blocks (4 symbols of Costas pattern)
// Dy - data block (29 symbols each encoding 2 bits)
#define FT4_ND          (87)  ///< Data symbols
#define FT4_NR          (2)   ///< Ramp symbols (beginning + end)
#define FT4_NN          (105) ///< Total channel symbols (FT4_NS + FT4_ND + FT4_NR)
#define FT4_LENGTH_SYNC (4)   ///< Length of each sync group
#define FT4_NUM_SYNC    (4)   ///< Number of sync groups
#define FT4_SYNC_OFFSET (33)  ///< Offset between sync groups

// Define LDPC parameters
#define FTX_LDPC_N       (174)                  ///< Number of bits in the encoded message (payload with LDPC checksum bits)
#define FTX_LDPC_K       (91)                   ///< Number of payload bits (including CRC)
#define FTX_LDPC_M       (83)                   ///< Number of LDPC checksum bits (FTX_LDPC_N - FTX_LDPC_K)
#define FTX_LDPC_N_BYTES ((FTX_LDPC_N + 7) / 8) ///< Number of whole bytes needed to store 174 bits (full message)
#define FTX_LDPC_K_BYTES ((FTX_LDPC_K + 7) / 8) ///< Number of whole bytes needed to store 91 bits (payload + CRC only)

// Define CRC parameters
#define FT8_CRC_POLYNOMIAL ((uint16_t)0x2757u) ///< CRC-14 polynomial without the leading (MSB) 1
#define FT8_CRC_WIDTH      (14)

typedef enum
{
    FTX_PROTOCOL_FT4,
    FTX_PROTOCOL_FT8,
    FTX_PROTOCOL_FT2 ///< Longpath-Erweiterung: FT4-Aufbau, halbe Symboldauer
} ftx_protocol_t;

/// FT2 teilt mit FT4 alles ausser der Zeit. Jede Stelle, die nach dem
/// Aufbau fragt (Toene, Sync, XOR-Folge), fragt hiermit.
#define FTX_PROTOCOL_IS_FT4_FAMILY(p) ((p) == FTX_PROTOCOL_FT4 || (p) == FTX_PROTOCOL_FT2)

/// Costas 7x7 tone pattern for synchronization
extern const uint8_t kFT8_Costas_pattern[7];
extern const uint8_t kFT4_Costas_pattern[4][4];

/// Gray code map to encode 8 symbols (tones)
extern const uint8_t kFT8_Gray_map[8];
extern const uint8_t kFT4_Gray_map[4];

extern const uint8_t kFT4_XOR_sequence[10];

/// Parity generator matrix for (174,91) LDPC code, stored in bitpacked format (MSB first)
extern const uint8_t kFTX_LDPC_generator[FTX_LDPC_M][FTX_LDPC_K_BYTES];

/// LDPC(174,91) parity check matrix, containing 83 rows,
/// each row describes one parity check,
/// each number is an index into the codeword (1-origin).
/// The codeword bits mentioned in each row must xor to zero.
/// From WSJT-X's ldpc_174_91_c_reordered_parity.f90.
extern const uint8_t kFTX_LDPC_Nm[FTX_LDPC_M][7];

/// Mn from WSJT-X's bpdecode174.f90. Each row corresponds to a codeword bit.
/// The numbers indicate which three parity checks (rows in Nm) refer to the codeword bit.
/// The numbers use 1 as the origin (first entry).
extern const uint8_t kFTX_LDPC_Mn[FTX_LDPC_N][3];

/// Number of rows (columns in C/C++) in the array Nm.
extern const uint8_t kFTX_LDPC_Num_rows[FTX_LDPC_M];

#ifdef __cplusplus
}
#endif

#endif // _INCLUDE_CONSTANTS_H_
