# audio/

Audio DSP + audio-thread-safe utility blocks.

## `AudioSampleFifo<NumChannels, SampleType>`
**Purpose**
- Lock-free single-writer/single-reader sample transfer (audio thread → UI thread).

**Typical use case**
- Transfer the latest audio samples from the processor callback to editor analyzers.

**Need to know**
- Fixed capacity: `kFifoSize = 32768`.
- `push()` writes channel arrays.
- `pull()` returns newest window (drops older unread data by design).
- Uses `juce::AbstractFifo` semantics.

**Apply when**
- Need latest waveform/FFT window in UI.
- Need no locks in audio callback.

**Don’t apply when**
- Need guaranteed delivery of every sample chunk.
- Need multi-reader or multi-writer queue.

**Example**
```cpp
phu::audio::AudioSampleFifo<2> fifo;
fifo.push(channelPtrs, numSamples);
int got = fifo.pull(destPtrs, 2048);
```

---

## `FFTProcessor<SampleType>`
**Purpose**
- UI-thread FFT + temporal/frequency smoothing for spectrum rendering.

**Typical use case**
- Consume FIFO audio in the editor and render a smoothed real-time spectrum.

**Need to know**
- Sliding mono ring buffer decouples update rate from FFT size.
- `setFFTOrder(10..15)` clamps FFT size range.
- `process(fifo)` consumes new FIFO data.
- `getMagnitudeSpectrum()` exposes linear magnitudes.

**Apply when**
- Need stable, display-oriented spectrum data.

**Don’t apply when**
- Need low-level FFT primitives for custom transform pipeline.
- Need true per-block audio-thread spectral processing.

**Example**
```cpp
phu::audio::FFTProcessor<float> fft(14);
fft.setTemporalSmoothing(0.7f, 0.95f);
fft.setFrequencySmoothing(0.3f);
if (fft.process(fifo)) {
    auto* mags = fft.getMagnitudeSpectrum();
}
```

---

## `LinkwitzRiley` module
**Typical use case**
- Build a real-time multiband crossover stage with phase-coherent recombination.

### `LinkwitzRileyFilter<SampleType>` (mono)
**Purpose**
- LR lowpass/highpass/allpass building block (DB24 or DB48).

**Typical use case**
- Process one mono lane through a lowpass/highpass split with matched phase behavior.

**Need to know**
- `setParams(type, slope, freq, sampleRate)` recalculates cascaded biquads.
- `processSample()` is sample-by-sample API.

**Apply when**
- Need phase-coherent crossover primitives.

**Don’t apply when**
- Need linear-phase crossover.

### `StereoLinkwitzRileyFilter<SampleType>`
**Purpose**
- Left/right wrapper around two mono LR filters.

**Typical use case**
- Apply the same LR filter settings to stereo channels without per-channel boilerplate.

### `CrossOver<SampleType>`
**Purpose**
- 2-band split (LP + HP) for stereo input.

**Typical use case**
- Split a stereo signal into low/high bands for independent dynamics processing.

### `MultiBandN<SampleType>`
**Purpose**
- 2..7 band serial crossover with allpass phase compensation.

**Typical use case**
- Create 3–5 crossover bands for multiband compression or visualization.

**Need to know**
- `initialize(... freqs, numFreqs, ...)`: `numFreqs = bands - 1`.
- `processSample()` or `processBlock()` for split.
- `sumBands()` for recombination with optional per-band gains.

**Apply when**
- Need real-time multiband splitting with flat summed response.

**Don’t apply when**
- Need dynamic band count changes every block.

**Example**
```cpp
using namespace phu::audio::LinkwitzRiley;
float freqs[] = {300.0f, 3000.0f};
MultiBandN<float> xo(Slope::DB24, freqs, 2, 48000.0f); // 3 bands
xo.processSample(inL, inR, bandsL, bandsR);
```

---

## `NoteToFreq`
**Purpose**
- Note-name/MIDI/frequency conversion helpers.

**Typical use case**
- Convert user note input (e.g. `A3`) to cutoff/oscillator frequency controls.

**Need to know**
- Accepts note text like `A4`, `C#3`, `Db3`.
- `toFrequency()` and `noteNameToMidi()` return `std::optional`.
- Uses A4 = 440 Hz equal temperament.

**Apply when**
- Parsing user note labels into numeric tuning values.

**Don’t apply when**
- Need alternate tuning systems (non-equal temperament).

**Example**
```cpp
auto hz = phu::audio::NoteToFreq::toFrequency("Eb2");
std::string n = phu::audio::NoteToFreq::midiToNoteName(69); // A4
```

---

## `BucketSet` + `RingBufferInsertResult`
**Purpose**
- Track dirty index partitions over ring-buffer style updates.

**Typical use case**
- Redraw only the chart buckets touched by the latest wrapped ring-buffer write.

**Need to know**
- `initializeBySize(...)` partitions `[0, N)` into contiguous buckets.
- `setDirty(result)` consumes `RingBufferInsertResult` ranges.
- `dirtyBegin()/dirtyEnd()` iterate dirty-only buckets.

**Apply when**
- Need partial redraw/recompute only for touched regions.

**Don’t apply when**
- Full-buffer recompute is cheap and always performed.

**Example**
```cpp
phu::audio::BucketSet buckets;
buckets.initializeBySize(bufferSize, 64);
auto res = ring.insert(ppq, samples, count);
buckets.setDirty(res);
for (auto it = buckets.dirtyBegin(); it != buckets.dirtyEnd(); ++it) {
    // use it->startIdx / it->endIdx
}
```

---

## `PpqAddressedRingBuffer<T>`
**Purpose**
- Ring buffer addressed by PPQ timeline position.

**Typical use case**
- Store timeline data in a cyclic 1/2/4-beat UI window independent of host block edges.

**Need to know**
- `setWorkingSize(bpm, sampleRate, displayBeats)` defines active span.
- `indexForPpq(ppq)` wraps by display-beat window.
- `write()` for single sample, `insert()` for block + wrap-aware dirty ranges.

**Apply when**
- Need beat-synchronized data placement in cyclic view windows.

**Don’t apply when**
- Timeline index is absolute samples only (no musical position).

**Example**
```cpp
phu::audio::PpqAddressedRingBuffer<float> rb;
rb.prepare(4.0, 120.0, 48000.0);
auto changed = rb.insert(ppqStart, block, num);
```

---

## Combined use cases (multi-module)

### Transfer samples to UI thread to compute FFT
- `AudioSampleFifo` on the audio thread pushes incoming channel samples each block.
- On the UI/message thread, `FFTProcessor` pulls available samples from that FIFO.
- `FFTProcessor::process(...)` computes and smooths the spectrum for rendering widgets.

### Compute RMS over 1/16 of a buffer that spans 2 beats
- `SyncGlobals` provides current BPM and sample-rate context per process run.
- `PpqAddressedRingBuffer` stores the 2-beat rolling buffer keyed by PPQ.
- Convert 1/16 note duration to samples from BPM/sample-rate, read that segment, then compute RMS.

### Beat-synced remote scope with partial redraw
- `SampleBroadcaster` receives remote raw packets carrying PPQ/BPM metadata.
- Insert data into `PpqAddressedRingBuffer` and capture `RingBufferInsertResult`.
- Mark dirty partitions with `BucketSet` so the UI redraws only updated regions.

### Cross-instance spectral monitor
- Local instance computes magnitudes with `FFTProcessor`.
- `SpectrumBroadcaster` multicasts quantized spectrum snapshots to peers.
- Remote UI overlays `getReceivedSpectrums()` with local data for side-by-side monitoring.
