# debug/

Debug logging primitives (queue + sink + logger integration).

## `DebugLogSink`
**Purpose**
- Abstract UI/consumer interface for log output.

**Typical use case**
- Implement an editor-side log panel that receives queued debug lines.

**Need to know**
- Required: `onLogMessage(...)`.
- Optional: `onLogQueueOverflow(...)`.

**Apply when**
- Implementing a panel/component that displays logs.

**Don’t apply when**
- You only need console logging.

---

## `DebugLogEventQueue`
**Purpose**
- Bounded MPSC ring queue for log messages.

**Typical use case**
- Push logs from audio/network threads and drain them later on the message thread.

**Need to know**
- `tryPush()` from many producer threads.
- `popBatch()` single-consumer batch drain.
- Fixed limits: slot count, message length, batch size.

**Apply when**
- Need low-overhead cross-thread log buffering.

**Don’t apply when**
- You need unbounded guaranteed log retention.

**Example**
```cpp
queue.tryPush(msgUtf8, len);
DebugLogEventQueue::LogEntry batch[32];
int n = queue.popBatch(batch, 32);
```

---

## `EditorLogger` (`PHU_DEBUG_UI` builds)
**Purpose**
- JUCE `Logger` implementation writing into `DebugLogEventQueue`.

**Typical use case**
- Replace JUCE’s current logger in debug builds so plugin logs appear in a custom UI sink.

**Need to know**
- `setSink()` binds UI sink.
- `logMessage()` enqueues UTF-8 text.
- Tracks dropped messages when queue is full.
- Macro `LOG_MESSAGE(loggerPtr, msg)` is no-op in release builds.

**Apply when**
- Need thread-safe debug logs visible in editor UI.

**Don’t apply when**
- In release build or when `PHU_DEBUG_UI` is disabled.

**Example**
```cpp
phu::debug::EditorLogger logger;
logger.setSink(&panel);
juce::Logger::setCurrentLogger(&logger);
LOG_MESSAGE(&logger, "network init ok");
```
