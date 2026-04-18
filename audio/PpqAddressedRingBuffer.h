#pragma once

#include "audio/RingBufferInsertResult.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace phu {
namespace audio {

/**
 * PpqAddressedRingBuffer:
 * - Position-addressed single-sample writes (RawSampleBuffer-style)
 * - Block inserts with wrap-aware dirty ranges (PpqRingBuffer-style)
 */
template <typename T>
class PpqAddressedRingBuffer {
  public:
    static constexpr int kMaxCapacitySamples = 10 * 1024 * 1024;

    PpqAddressedRingBuffer() = default;
    PpqAddressedRingBuffer(double minBpm, double maxSampleRate, double maxBeats) {
        reserveByWorstCase(minBpm, maxSampleRate, maxBeats);
    }

    void reserveByWorstCase(double minBpm, double maxSampleRate, double maxBeats) {
        if (minBpm <= 0.0 || maxSampleRate <= 0.0 || maxBeats <= 0.0)
            return;

        const double computedCapacity =
            std::ceil((maxBeats * 60.0 * maxSampleRate) / minBpm);
        const double maxIntAsDouble = static_cast<double>(std::numeric_limits<int>::max());
        const double clampedCapacity = std::isfinite(computedCapacity)
            ? std::min(computedCapacity, maxIntAsDouble)
            : maxIntAsDouble;
        const int cap = std::min(static_cast<int>(clampedCapacity), kMaxCapacitySamples);
        m_buffer.resize(static_cast<size_t>(cap), T{});
    }

    bool setWorkingSize(double bpm, double sampleRate, double displayBeats) {
        if (bpm <= 0.0 || sampleRate <= 0.0 || displayBeats <= 0.0)
            return false;

        const int desired =
            static_cast<int>(std::ceil((displayBeats * 60.0 * sampleRate) / bpm));
        const int capped = capacity() > 0 ? std::min(desired, capacity()) : desired;
        const bool changed = capped != m_workingSize;
        m_workingSize = std::max(0, capped);
        m_displayBeats = displayBeats;
        m_ppqToIndex = (m_workingSize > 0)
            ? static_cast<double>(m_workingSize) / m_displayBeats
            : 0.0;

        if (capacity() == 0) {
            m_buffer.assign(static_cast<size_t>(m_workingSize), T{});
        }
        return changed;
    }

    void prepare(double displayBeats, double bpm, double sampleRate) {
        setWorkingSize(bpm, sampleRate, displayBeats);
        clear();
    }

    void clear() {
        if (m_workingSize > 0) {
            std::fill(m_buffer.begin(), m_buffer.begin() + m_workingSize, T{});
        }
    }

    int indexForPpq(double ppq) const {
        if (m_workingSize <= 0 || m_ppqToIndex <= 0.0 || m_displayBeats <= 0.0)
            return 0;

        double ppqMod = std::fmod(ppq, m_displayBeats);
        if (ppqMod < 0.0)
            ppqMod += m_displayBeats;

        int idx = static_cast<int>(ppqMod * m_ppqToIndex) % m_workingSize;
        if (idx < 0) idx += m_workingSize;
        return idx;
    }

    Range write(T sample, double ppq) {
        if (m_workingSize <= 0)
            return {};

        const int idx = indexForPpq(ppq);
        m_buffer[static_cast<size_t>(idx)] = sample;
        return {idx, idx + 1};
    }

    void writeAt(int idx, T value) {
        if (m_workingSize <= 0)
            return;
        if (idx < 0) idx = 0;
        if (idx >= m_workingSize) idx = m_workingSize - 1;
        m_buffer[static_cast<size_t>(idx)] = value;
    }

    RingBufferInsertResult insert(double ppq, const T* samples, int count) {
        RingBufferInsertResult result;
        if (m_workingSize <= 0 || samples == nullptr || count <= 0 || count > m_workingSize)
            return result;

        const int startIdx = indexForPpq(ppq);
        const int endIdx = startIdx + count;

        if (endIdx <= m_workingSize) {
            std::memcpy(m_buffer.data() + startIdx, samples, static_cast<size_t>(count) * sizeof(T));
            result.range1 = {startIdx, endIdx};
        } else {
            const int firstPart = m_workingSize - startIdx;
            const int secondPart = count - firstPart;

            std::memcpy(m_buffer.data() + startIdx, samples, static_cast<size_t>(firstPart) * sizeof(T));
            std::memcpy(m_buffer.data(), samples + firstPart, static_cast<size_t>(secondPart) * sizeof(T));
            result.range1 = {startIdx, m_workingSize};
            result.range2 = {0, secondPart};
        }

        result.ok = true;
        return result;
    }

    const T* data() const noexcept { return m_buffer.data(); }
    int capacity() const noexcept { return static_cast<int>(m_buffer.size()); }
    int size() const noexcept { return m_workingSize; }
    int workingSize() const noexcept { return m_workingSize; }
    double displayBeats() const noexcept { return m_displayBeats; }

  private:
    std::vector<T> m_buffer;
    int m_workingSize = 0;
    double m_displayBeats = 1.0;
    double m_ppqToIndex = 0.0;
};

using PpqAddressedRingBufferF = PpqAddressedRingBuffer<float>;
using PpqAddressedRingBufferD = PpqAddressedRingBuffer<double>;

} // namespace audio
} // namespace phu
