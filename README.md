# phu-audio-lib

Shared library for PHU JUCE audio plugins. Contains common code extracted from:

- **phu-splitter** — Multi-band frequency splitter
- **phu-compressor** — Beat-synced compressor
- **phu-beat-sync-multi-scope** — Beat-synced multi-instance oscilloscope
- **phu-bark-fft-compressor** — Bark-band FFT compressor

## CMake Targets

| Target | Type | Description |
|---|---|---|
| `PhuAudioLib` | INTERFACE (header-only) | Core audio, events, memory, and utility headers |
| `PhuNetworkLib` | STATIC | Compiled network broadcasters and debug logger |
| `PhuGLUtils` | STATIC | OpenGL utilities (shader builder, snapshot renderer) |

## Usage

Add as a git submodule and include in your CMakeLists.txt:

```cmake
add_subdirectory(phu-audio-lib)

# Link the targets your plugin needs:
target_link_libraries(YourPlugin PRIVATE PhuAudioLib)        # header-only core
target_link_libraries(YourPlugin PRIVATE PhuNetworkLib)      # network + debug
target_link_libraries(YourPlugin PRIVATE PhuGLUtils)         # OpenGL utilities
```

## Directory Structure

```
audio/       — Audio DSP utilities (FIFO, FFT, filters, note conversion)
events/      — Event system (DAW sync globals, listener pattern)
network/     — UDP multicast broadcasters (spectrum, samples, commands, ctrl)
debug/       — Debug logging (MPSC queue, sink interface)
gl/          — OpenGL utilities (shader builder, snapshot renderer)
memory/      — SIMD-aligned allocator
util/        — String utilities
```

## License

MIT
