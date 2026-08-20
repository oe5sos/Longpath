// =================================================================
// src/core/MemoryLock.h  (NereusSDR-native)
// =================================================================
// 2026-05-26  J.J. Boyd (KG4VCF), AI-assisted via Anthropic Claude.
//
// Pin memory regions so the OS can not compress or page them out.
//
// Scope, narrowed 2026-08-16: page locks belong on the real-time audio
// path (audio ring, FFT scratch), where a fault on a hot buffer is an
// audible dropout.  They do NOT belong on display buffers.  The
// waterfall and both overlay images were pinned here until that date;
// the failure a pin prevents there is one stuttered frame, and the
// price was an mlock + munlock of ~12 MB per intermediate window size
// during a resize drag.  Wrong trade, removed.  If a future buffer
// wants a lock, the question to answer first is what a page fault on
// it actually costs the operator.
//
// Backed by mlock(2) on macOS / Linux and VirtualLock on Windows.
// All three OSes enforce a per-process / per-user lockable byte
// limit (macOS RLIMIT_MEMLOCK ~ 96 MiB default; Linux similar but
// often capped lower without CAP_IPC_LOCK; Windows working-set
// minimum / SetProcessWorkingSetSize).  We log warnings on failure
// but continue -- locked memory is an optimization, not a
// correctness requirement.
//
// Caller responsibilities:
//
//   * Pass a stable pointer (the start of an allocation that won't
//     move).  std::vector::data() after resize is fine until the
//     next resize; QImage::bits() is fine until the QImage is
//     reassigned or resized.
//
//   * Unlock before resize / reallocation / destruction so the kernel
//     doesn't carry a stale lock past the lifetime of the pages.
//
//   * Each lock() must be paired with exactly one unlock() of the
//     same (addr, bytes) pair.
//
// Stats are aggregated globally so the perf overlay (or a future
// "/perf memlock" report) can show how much we have pinned.
// =================================================================
#pragma once

#include <cstddef>

namespace Longpath {

/// Pin the given memory region.  Internally aligns to the kernel's
/// page size (round addr down, length up) so callers can pass any
/// pointer / length pair without thinking about pages.
///
/// Returns true on success; logs a warning + returns false on OS
/// refusal (RLIMIT_MEMLOCK exceeded, region not resident, EPERM).
/// On failure the region is still usable -- just pageable.
bool lockMemory(const void* addr, std::size_t bytes, const char* tag = "");

/// Release a previously-locked region.  Same addr / bytes contract
/// as lock; aligns internally.  Safe to call with (nullptr, 0).
///
/// A no-op -- including for the stats -- when this region is not
/// currently locked.  Callers routinely unlock unconditionally in
/// destructors and realloc paths without checking whether the matching
/// lockMemory() succeeded, which is the common case on Linux once
/// RLIMIT_MEMLOCK is exhausted.  Internally only successfully-locked
/// ranges are tracked, so bytesLocked / regionsLocked can never
/// underflow.
///
/// If the region was locked with a different length than the one
/// passed here (buffer resized between lock and unlock), the range
/// that was actually locked is released.
void unlockMemory(const void* addr, std::size_t bytes);

/// Aggregate stats across all currently-locked regions.  Read-only;
/// used by the perf overlay / diagnostics.
struct MemoryLockStats {
    std::size_t bytesLocked{0};   // sum of currently-held lock byte counts
    int regionsLocked{0};         // currently-held lock count
    int lockFailuresTotal{0};     // cumulative lock() returns false
};
MemoryLockStats memoryLockStats();

} // namespace Longpath
