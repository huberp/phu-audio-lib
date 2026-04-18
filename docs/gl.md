# gl/

OpenGL helper utilities for shader setup + renderer state handoff.

## `GLSLShaderBuilder`
**Purpose**
- Centralized GLSL version detection, fragment assembly, compile/link flow.

**Need to know**
- `getVersionDirective()` chooses `#version` by runtime context.
- `buildFragmentShader(...)` adapts output syntax (`fragColor` vs `gl_FragColor`).
- `compileShaderProgram(...)` standardizes error logging.

**Apply when**
- Building GL shaders across mixed GLSL versions.

**Don’t apply when**
- Using fixed-pipeline/no custom shaders.

**Example**
```cpp
auto vert = GLSLShaderBuilder::getVersionDirective() + "...";
auto frag = GLSLShaderBuilder::buildFragmentShader("uniform vec4 u;\n", "    fragColor = u;\n");
```

---

## `GLRendererBase`
**Purpose**
- Base class for renderer lifecycle + shader ownership.

**Need to know**
- Override `create(ctx)`.
- Call `release()` on context close.
- `compileShader(...)` helper stores `m_shader`.
- `isReady()` checks compiled program availability.

**Apply when**
- Implementing reusable renderer components.

**Don’t apply when**
- One-off direct GL calls without reusable object lifecycle.

---

## `GLSnapshotRenderer<SnapshotType>`
**Purpose**
- Double-buffered snapshot handoff between UI thread and GL thread.

**Need to know**
- UI thread: `setSnapshot(...)`.
- GL thread: `swapSnapshot()` then render from `m_render`.
- SpinLock-protected short critical sections.

**Apply when**
- Need frame-stable render data transfer without heavy locking.

**Don’t apply when**
- Render data is immutable/static.

**Example**
```cpp
setSnapshot(snapshotFromUi);   // UI thread
if (swapSnapshot()) {          // GL thread
    auto s = getRenderSnapshot();
}
```
