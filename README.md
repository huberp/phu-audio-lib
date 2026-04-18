# phu-audio-lib

Shared JUCE utility library for PHU plugins.

## Hero / Highlights
- DSP core blocks for real-time audio workflows
- DAW-sync event state tracking
- UDP multicast instance-to-instance data/control channels
- Debug logging pipeline decoupled from UI
- OpenGL helper layer for shader/renderer patterns
- SIMD-aligned memory utilities

## Topic Map (for users + coding agents)
- [audio](docs/audio.md) — fifo, fft, filters, note↔freq, buckets, ppq-ring
- [events](docs/events.md) — daw globals, listeners, event source
- [network](docs/network.md) — udp multicast, spectrum/samples/commands/ctrl, state helpers
- [debug](docs/debug.md) — mpsc log queue, sink, editor logger
- [gl](docs/gl.md) — glsl builder, renderer base, snapshot pattern
- [memory](docs/memory.md) — aligned allocator, aligned vector
- [util](docs/util.md) — safe string copy

## License
MIT
