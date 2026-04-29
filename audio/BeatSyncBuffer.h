#pragma once

#include "memory/AlignedAllocator.h"

#include <algorithm>
#include <vector>

namespace phu {
namespace audio {

/**
 * BeatSyncBuffer — position-indexed overwrite buffer for beat-synced visualisation.
 *
 * Maps a normalised position in [0, 1) to a pre-allocated bin array. The caller is
 * responsible for computing the normalised position (e.g. PPQ modulo display range
 * divided by display range). Each `write()` call overwrites a single bin; there is
 * no chronological ordering — only the most-recent value at each position is retained.
 *
 * ## Relationship to PpqAddressedRingBuffer
 * These two classes are complementary patterns for beat-synced display:
 *
 * | Class                    | Storage model         | Write semantics       | Read semantics           |
 * |--------------------------|-----------------------|-----------------------|--------------------------|
 * | BeatSyncBuffer           | Fixed bin array       | Overwrite by position | Direct pointer / getBin  |
 * | PpqAddressedRingBuffer   | Chronological ring    | Append (block insert) | Dirty-range iteration    |
 *
 * Use `BeatSyncBuffer` when you want a stable, low-latency snapshot of the current
 * beat cycle suitable for direct pixel-mapped rendering. Use `PpqAddressedRingBuffer`
 * when you need a scrolling history of samples with precise PPQ addressing for
 * bucket-aggregation pipelines (e.g. RMS metering via BucketSet).
 *
 * ## Thread safety
 * Single `T` stores/loads are naturally atomic on x86/x64 for scalar types. The
 * audio thread writes; the UI thread reads. Worst case is a one-sample stale value
 * at a single bin — a sub-pixel visual difference. No explicit synchronisation is
 * required for this usage pattern.
 *
 * ## Memory layout
 * The internal vector uses `AlignedAllocator<T>` by default (32-byte / AVX
 * alignment), making the bin array suitable for SIMD iteration on the UI thread.
 * Swap in any standard-conforming allocator via the `Allocator` template parameter
 * without changing any call sites.
 *
 * @tparam T         Value type stored per bin (e.g. `float`, `double`).
 * @tparam Allocator Allocator for the internal vector. Defaults to
 *                   `phu::memory::AlignedAllocator<T>`.
 */
template <typename T = float,
          typename Allocator = phu::memory::AlignedAllocator<T>>
class BeatSyncBuffer {
  public:
    /** Construct with a sentinel clear value used by clear() and getBin() on out-of-range access. */
    explicit BeatSyncBuffer(T clearValue = T{-60}) : m_clearValue(clearValue) {}

    /** Allocate bins and fill with the stored clear value. Call from prepareToPlay (not real-time). */
    void prepare(int numBins) {
        m_bins.assign(static_cast<std::size_t>(numBins), m_clearValue);
        m_numBins = numBins;
    }

    /** Write a value at a normalised position [0, 1). O(1). Audio-thread safe on x86/x64. */
    void write(double normalizedPos, T value) {
        if (m_numBins <= 0) return;
        int idx = static_cast<int>(normalizedPos * m_numBins);
        if (idx < 0) idx = 0;
        if (idx >= m_numBins) idx = m_numBins - 1;
        m_bins[static_cast<std::size_t>(idx)] = value;
    }

    /** Reset all bins to the stored clear value. Not real-time safe. */
    void clear() {
        std::fill(m_bins.begin(), m_bins.end(), m_clearValue);
    }

    /** Update the clear value. Takes effect on the next clear() or prepare() call. */
    void setClearValue(T value) { m_clearValue = value; }

    /** Direct read access to the bin array for rendering (UI thread). */
    const T* data() const noexcept { return m_bins.data(); }

    /** Number of bins allocated by the last prepare() call. */
    int size() const noexcept { return m_numBins; }

    /** Bounds-checked single-bin read. Returns the clear value on out-of-range access. */
    T getBin(int index) const noexcept {
        if (index < 0 || index >= m_numBins) return m_clearValue;
        return m_bins[static_cast<std::size_t>(index)];
    }

  private:
    std::vector<T, Allocator> m_bins;
    int m_numBins    = 0;
    T   m_clearValue;
};

// Convenience aliases for the common float / double cases.
using BeatSyncBufferF = BeatSyncBuffer<float>;
using BeatSyncBufferD = BeatSyncBuffer<double>;

} // namespace audio
} // namespace phu
