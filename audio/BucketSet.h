#pragma once

#include <algorithm>
#include <functional>
#include <iterator>
#include <vector>

namespace phu {
namespace audio {

/**
 * Bucket — a half-open index range [startIdx, endIdx) and a dirty flag.
 */
struct Bucket {
    int startIdx = 0;
    int endIdx = 0;
    bool dirty = true;
};

/**
 * BucketSet — partitions an index range [0, N) into contiguous buckets.
 *
 * The class is intentionally generic and independent from application-specific
 * buffer or timing concepts.
 */
class BucketSet {
  public:
    BucketSet() = default;

    /**
     * Partition [0, bufferSize) into bucketCount buckets.
     * Buckets are contiguous, non-overlapping and cover the full range.
     */
    void initializeBySize(int bufferSize, int bucketCount) {
        m_sizeFn = nullptr;
        m_bufferSize = bufferSize;
        m_bucketCountTarget = bucketCount;
        rebuild(bufferSize, bucketCount);
    }

    /**
     * Convenience overload for std::vector-like buffers.
     */
    template <typename T>
    void initializeByVector(const std::vector<T>& vec, int bucketCount) {
        initializeBySize(static_cast<int>(vec.size()), bucketCount);
    }

    /**
     * Bind to a dynamic size provider and build buckets immediately.
     */
    void initializeBySizeFn(std::function<int()> sizeFn, int bucketCount) {
        m_sizeFn = sizeFn;
        m_bucketCountTarget = bucketCount;
        const int size = sizeFn ? sizeFn() : 0;
        m_bufferSize = size;
        rebuild(size, bucketCount);
    }

    /**
     * Recompute boundaries with the bound size function (if present),
     * otherwise using the last known buffer size.
     */
    void recompute() {
        const int size = m_sizeFn ? m_sizeFn() : m_bufferSize;
        m_bufferSize = size;
        rebuild(size, m_bucketCountTarget);
    }

    /**
     * Mark every bucket overlapping [fromIdx, toIdx) as dirty.
     */
    void markDirty(int fromIdx, int toIdx) {
        for (auto& b : m_buckets) {
            if (b.startIdx < toIdx && b.endIdx > fromIdx)
                b.dirty = true;
        }
    }

    /**
     * Mark the bucket containing writeIdx as dirty.
     */
    void markDirtyIndex(int writeIdx) {
        if (m_buckets.empty())
            return;
        const int bucketIdx = findBucket(writeIdx);
        if (bucketIdx < 0 || bucketIdx >= static_cast<int>(m_buckets.size()))
            return;
        m_buckets[static_cast<size_t>(bucketIdx)].dirty = true;
    }

    /**
     * Mark every bucket touched by an inclusive range [startIdx, endIdx],
     * with wrap-around support when startIdx > endIdx.
     */
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

  private:
    static int mulDiv(int a, int b, int c) {
        if (c == 0)
            return 0;
        return static_cast<int>(static_cast<long long>(a) * b / c);
    }

    void rebuild(int N, int B) {
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

    int findBucket(int idx) const {
        if (m_bufferSize <= 0 || m_buckets.empty())
            return 0;

        const int B = static_cast<int>(m_buckets.size());
        int bi = mulDiv(idx, B, m_bufferSize);

        if (bi < 0)
            bi = 0;
        const int last = static_cast<int>(m_buckets.size()) - 1;
        if (bi > last)
            bi = last;
        return bi;
    }

    std::function<int()> m_sizeFn;
    int m_bufferSize = 0;
    int m_bucketCountTarget = 0;
    std::vector<Bucket> m_buckets;
};

} // namespace audio
} // namespace phu
