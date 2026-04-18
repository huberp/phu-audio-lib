#pragma once

namespace phu {
namespace audio {

struct Range {
    int start = 0;
    int end = 0;
    bool valid() const noexcept { return end > start; }
};

struct RingBufferInsertResult {
    Range range1;
    Range range2;
    bool ok = false;
};

} // namespace audio
} // namespace phu
