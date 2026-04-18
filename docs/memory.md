# memory/

SIMD-friendly aligned allocation helpers.

## `AlignedAllocator<T, Alignment>`
**Purpose**
- STL allocator that guarantees alignment boundary (`Alignment`, default 32 bytes).

**Need to know**
- POSIX: `posix_memalign`; Windows: `_aligned_malloc`.
- Throws `std::bad_alloc` on allocation failure.
- C++17 allocator traits-compatible (`rebind`, equality traits).

**Apply when**
- Buffer alignment matters for SIMD or cache layout.

**Don’t apply when**
- Default allocator alignment is already sufficient for your workload.

---

## `AlignedVector<T, Alignment>`
**Purpose**
- `std::vector` alias using `AlignedAllocator`.

**Need to know**
- Drop-in vector replacement with aligned storage.

**Apply when**
- FFT/DSP arrays with AVX/SSE/NEON expectations.

**Don’t apply when**
- Container type requires custom growth policy/allocator behavior not covered here.

**Example**
```cpp
phu::memory::AlignedVector<float> fftBuffer;
fftBuffer.resize(2048);
```
