# events/

DAW-global event model (BPM, play-state, sample-rate) + listener plumbing.

## `Event`
**Purpose**
- Base payload with `source` + per-frame `Context`.

**Typical use case**
- Define plugin-specific events that all carry the same process-frame context fields.

**Need to know**
- `Context` carries buffer/midi/position pointers and frame metadata.

**Apply when**
- Defining new event types needing shared context shape.

**Don’t apply when**
- You only need immediate local state checks (no event propagation).

---

## `EventSource<ListenerType>`
**Purpose**
- Generic listener registration/removal/count logic.

**Typical use case**
- Reuse shared add/remove/notify plumbing for a new event emitter class.

**Need to know**
- Prevents duplicate listener insertion.
- Stores raw listener pointers.

**Apply when**
- Building source classes with callback fan-out.

**Don’t apply when**
- Listener ownership/lifetime cannot be externally guaranteed.

**Example**
```cpp
source.addEventListener(&listener);
source.removeEventListener(&listener);
```

---

## `SyncGlobalsListener` types
**Purpose**
- Event structs (`BPMEvent`, `IsPlayingEvent`, `SampleRateEvent`) + listener interface.

**Typical use case**
- React only to tempo or transport changes by overriding the needed callbacks.

**Need to know**
- `GlobalsEventListener` has default no-op handlers.
- Override only callbacks you need.

**Apply when**
- Reacting to DAW transport/tempo/sample-rate changes.

**Don’t apply when**
- Polling state each frame is already sufficient.

---

## `GlobalsEventSource`
**Purpose**
- Concrete emitter for global events to registered listeners.

**Typical use case**
- Broadcast BPM/play-state/sample-rate updates from one owner to multiple observers.

**Need to know**
- Fires via explicit methods: `fireBPMChanged`, `fireIsPlayingChanged`, `fireSampleRateChanged`.

---

## `SyncGlobals`
**Purpose**
- Runtime DAW global-state tracker and event producer.

**Typical use case**
- Update DAW globals every process block and notify editor/DSP subscribers on change.

**Need to know**
- `updateSampleRate()` fires on sample-rate changes.
- `updateDAWGlobals(...)` updates BPM/play-state/PPQ and emits events.
- Tracks run count + processed sample count.
- `getPpqEndOfBlock()`/`setPpqEndOfBlock()` for cross-thread PPQ handoff.

**Apply when**
- Need centralized transport/tempo state in processor/editor flows.

**Don’t apply when**
- Project has no DAW sync dependency.

**Example**
```cpp
auto& g = syncGlobals;
g.addEventListener(&listener);
auto ctx = g.updateDAWGlobals(buffer, midi, positionInfo);
g.finishRun(buffer.getNumSamples());
```
