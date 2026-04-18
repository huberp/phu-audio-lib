#pragma once

#include "audio/PpqAddressedRingBuffer.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <iterator>
#include <vector>

namespace phu {
namespace audio {

struct Bucket {
    int startIdx = 0;
    int endIdx = 0;
    bool dirty = true;
};

/**
 * Unified BucketSet:
 * - Fixed-count partitioning (overflow-safe integer boundaries)
 * - Music-aware RMS/cancel partitioning used by beat-sync workflows
 */
class BucketSet {
  public:
    enum class Mode { FixedCount, Rms16th, Cancel4ms };

    static constexpr int kMaxRmsBuckets = 128;
    static constexpr int kMaxCancelBuckets = 256;
    static constexpr double kSixteenthNotesPerBeat = 16.0;
    static constexpr double kCancelBucketDurationSeconds = 0.004; // 4 ms
    static constexpr double kDefaultRmsDenominator = 1.0;

    BucketSet() = default;

    void initializeBySize(int bufferSize, int bucketCount) {
        m_mode = Mode::FixedCount;
        m_sizeFn = nullptr;
        m_bufferSize = bufferSize;
        m_bucketCountTarget = bucketCount;
        rebuildFixedCount(bufferSize, bucketCount);
    }

    template <typename T>
    void initializeByVector(const std::vector<T>& vec, int bucketCount) {
        initializeBySize(static_cast<int>(vec.size()), bucketCount);
    }

    void initializeBySizeFn(std::function<int()> sizeFn, int bucketCount) {
        m_mode = Mode::FixedCount;
        m_sizeFn = sizeFn;
        m_bucketCountTarget = bucketCount;
        const int sz = sizeFn ? sizeFn() : 0;
        m_bufferSize = sz;
        rebuildFixedCount(sz, bucketCount);
    }

    void initializeByMusical(Mode mode, double sampleRate, double displayBeats, int bufferSize) {
        m_mode = mode;
        m_sampleRate = sampleRate;
        m_displayBeats = displayBeats;
        m_bufferSize = bufferSize;
        rebuildMusical(bufferSize);
    }

    void recompute() {
        const int sz = m_sizeFn ? m_sizeFn() : m_bufferSize;
        m_bufferSize = sz;

        if (m_mode == Mode::FixedCount) {
            rebuildFixedCount(sz, m_bucketCountTarget);
        } else {
            rebuildMusical(sz);
        }
    }

    void markDirty(int fromIdx, int toIdx) {
        for (auto& b : m_buckets) {
            if (b.startIdx < toIdx && b.endIdx > fromIdx)
                b.dirty = true;
        }
    }

    void markDirtyIndex(int writeIdx) {
        if (m_buckets.empty())
            return;
        const int bucketIdx = findBucket(writeIdx);
        if (bucketIdx < 0 || bucketIdx >= static_cast<int>(m_buckets.size()))
            return;
        m_buckets[static_cast<size_t>(bucketIdx)].dirty = true;
    }

    void markDirtyRange(int startIdx, int endIdx) {
        if (m_buckets.empty())
            return;

        const int last = static_cast<int>(m_buckets.size()) - 1;
        const int i1 = findBucket(startIdx);
        const int i2 = findBucket(endIdx);

        if (i1 <= i2) {
            for (int i = i1; i <= i2; ++i)
                m_buckets[static_cast<size_t>(i)].dirty = true;
        } else {
            for (int i = i1; i <= last; ++i)
                m_buckets[static_cast<size_t>(i)].dirty = true;
            for (int i = 0; i <= i2; ++i)
                m_buckets[static_cast<size_t>(i)].dirty = true;
        }
    }

    void setDirty(const PpqWriteResult& result) {
        if (!result.ok)
            return;
        if (result.range1.valid())
            markDirty(result.range1.start, result.range1.end);
        if (result.range2.valid())
            markDirty(result.range2.start, result.range2.end);
    }

    class DirtyIterator {
      public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = Bucket;
        using difference_type = std::ptrdiff_t;
        using pointer = Bucket*;
        using reference = Bucket&;

        DirtyIterator(std::vector<Bucket>::iterator cur, std::vector<Bucket>::iterator end) noexcept
            : m_cur(cur), m_end(end) {
            skipClean();
        }

        reference operator*() const noexcept { return *m_cur; }
        pointer operator->() const noexcept { return &*m_cur; }

        DirtyIterator& operator++() noexcept {
            ++m_cur;
            skipClean();
            return *this;
        }

        bool operator==(const DirtyIterator& o) const noexcept { return m_cur == o.m_cur; }
        bool operator!=(const DirtyIterator& o) const noexcept { return m_cur != o.m_cur; }

      private:
        void skipClean() noexcept {
            while (m_cur != m_end && !m_cur->dirty)
                ++m_cur;
        }

        std::vector<Bucket>::iterator m_cur;
        std::vector<Bucket>::iterator m_end;
    };

    DirtyIterator dirtyBegin() { return {m_buckets.begin(), m_buckets.end()}; }
    DirtyIterator dirtyEnd() { return {m_buckets.end(), m_buckets.end()}; }

    int bucketCount() const { return static_cast<int>(m_buckets.size()); }
    const Bucket& bucket(int i) const { return m_buckets[static_cast<size_t>(i)]; }
    Bucket& bucket(int i) { return m_buckets[static_cast<size_t>(i)]; }
    const std::vector<Bucket>& buckets() const { return m_buckets; }

    int bufferSize() const { return m_bufferSize; }
    Mode mode() const { return m_mode; }

  private:
    static int mulDiv(int a, int b, int c) {
        if (c == 0)
            return 0;
        return static_cast<int>(static_cast<long long>(a) * b / c);
    }

    void rebuildFixedCount(int N, int B) {
        m_buckets.clear();
        if (N <= 0 || B <= 0)
            return;

        const int count = std::min(B, N);
        m_buckets.reserve(static_cast<size_t>(count));

        for (int i = 0; i < count; ++i) {
            const int start = mulDiv(i, N, count);
            const int end = mulDiv(i + 1, N, count);
            m_buckets.push_back({start, end, true});
        }
    }

    void rebuildMusical(int N) {
        m_buckets.clear();
        if (N <= 0) {
            m_bucketSize = 1;
            return;
        }

        if (m_mode == Mode::FixedCount) {
            rebuildFixedCount(N, m_bucketCountTarget);
            return;
        }

        m_bucketSize = calculateMusicalBucketSize(N);
        const int maxBuckets = (m_mode == Mode::Rms16th) ? kMaxRmsBuckets : kMaxCancelBuckets;

        int start = 0;
        while (start < N && static_cast<int>(m_buckets.size()) < maxBuckets) {
            const int end = std::min(start + m_bucketSize, N);
            m_buckets.push_back({start, end, true});
            start = end;
        }
    }

    int findBucket(int idx) const {
        if (m_bufferSize <= 0 || m_buckets.empty())
            return 0;

        int bi = 0;
        if (m_mode == Mode::FixedCount) {
            const int B = static_cast<int>(m_buckets.size());
            bi = mulDiv(idx, B, m_bufferSize);
        } else {
            bi = idx / std::max(1, m_bucketSize);
        }

        if (bi < 0)
            bi = 0;
        const int last = static_cast<int>(m_buckets.size()) - 1;
        if (bi > last)
            bi = last;
        return bi;
    }

    int calculateMusicalBucketSize(int bufferSize) const {
        if (m_mode == Mode::Rms16th) {
            const double denom =
                (m_displayBeats > 0.0)
                    ? (m_displayBeats * kSixteenthNotesPerBeat)
                    : kDefaultRmsDenominator;
            return std::max(1, static_cast<int>(static_cast<double>(bufferSize) / denom));
        }

        return std::max(
            1, static_cast<int>(std::ceil(m_sampleRate * kCancelBucketDurationSeconds)));
    }

    Mode m_mode = Mode::FixedCount;
    std::function<int()> m_sizeFn;
    double m_sampleRate = 44100.0;
    double m_displayBeats = 1.0;
    int m_bufferSize = 0;
    int m_bucketSize = 1;
    int m_bucketCountTarget = 0;
    std::vector<Bucket> m_buckets;
};

} // namespace audio
} // namespace phu
