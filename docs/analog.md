# analog/

Analog circuit modeling: MNA circuit solver, nonlinear tube models, sidechain detector, and transformer coloration.

---

## `Analog::Circuit::Circuit`

**Purpose**
- Linear Modified Nodal Analysis (MNA) circuit solver supporting resistors, capacitors, inductors, coupled inductors, and independent voltage sources.

**Typical use case**
- Model passive RC/RL networks, output coupling circuits, or any linear sub-circuit that surrounds a nonlinear element being iterated by Newton-Raphson.

**Need to know**
- Node `0` is ground; circuit nodes are numbered `1 … numNodes`.
- Stamp all elements before calling `prepare(sampleRate)`.
- `prepare()` computes capacitor/inductor companion conductances and factorises the A matrix (LU with partial pivoting); no heap allocation happens during per-sample processing.
- Per-sample loop: `beginStep()` → fill sources (`setVoltageSourceValue`, `injectCurrent`) → `solve()` → read `nodeVoltage()` / `vsrcCurrent()`.
- `solve()` returns `false` if the matrix is near-singular; call `reset()` to clear capacitor/inductor history on start-up.
- Companion models use the trapezoidal (bilinear) approximation, giving zero steady-state error for sinusoidal signals at any frequency below Nyquist.

**Apply when**
- Need a self-contained nodal solver for a fixed linear network embedded in a larger plugin.
- Simulating coupled transformers (use `stampCoupledInductors()`).

**Don't apply when**
- The network contains nonlinear elements — combine with `NRPolicy` and per-iteration re-stamping instead.
- Need a dynamic network (changing topology at runtime is not supported; call `prepare()` again after re-stamping).

**Example**
```cpp
// Simple RC low-pass filter: Vin → R(1 kΩ) → node 1 → C(1 µF) → GND
Analog::Circuit::Circuit mna(2, 1);   // 2 nodes, 1 voltage source
mna.stampVoltageSource(0, 1, 0);      // VS0: node 1 is the driven input
mna.stampResistor(1000.0, 1, 2);
mna.stampCapacitor(1e-6, 2, 0);
mna.prepare(44100.0);

// Per-sample:
mna.beginStep();
mna.setVoltageSourceValue(0, inputSample * 10.0);
mna.solve();
double out = mna.nodeVoltage(2);
```

---

## `Analog::Circuit::CapacitorCompanion` / `InductorCompanion` / `CoupledInductorCompanion`

**Purpose**
- Trapezoidal companion models for capacitors, inductors, and magnetically coupled inductor pairs.
- Used internally by `Circuit` and available standalone when building custom MNA solvers.

**Need to know**
- `prepare(T)` computes the companion conductance `Geq` from the element value and sample period `T = 1/sampleRate`; stamp `Geq` into the A matrix once.
- `update(Vc)` / `update(VL)` / `update(VL1, VL2)` advances the history state after each `solve()`.
- `reset()` restores zero initial conditions.
- `CoupledInductorCompanion` requires |k| < 1 (i.e. `M² < L1·L2`).

**Apply when**
- Writing a custom MNA loop that manages the A matrix manually.

**Don't apply when**
- Using `Analog::Circuit::Circuit` — it manages companions automatically.

---

## `Analog::dBToLinear` / `Analog::linearToDb`

**Purpose**
- Convert between decibels and linear gain factors.

**Need to know**
- `dBToLinear(0) == 1.0`; `dBToLinear(-6) ≈ 0.5`.
- `linearToDb` on zero or a negative value is implementation-defined (maps to −∞).

**Apply when**
- Scaling plugin gain parameters stored in dB to/from audio sample amplitude.

**Example**
```cpp
float gain = Analog::dBToLinear(-12.0f); // ≈ 0.25
float dB   = Analog::linearToDb(0.5f);  // ≈ -6 dB
```

---

## `Analog::sampleToVolts` / `Analog::voltsToSample` (UnitScaling)

**Purpose**
- Convert between normalised sample values (±1.0 full-scale) and circuit voltages.

**Need to know**
- Convention: ±1.0 full-scale ↔ ±10 V (Eurorack convention; `kVoltsPerSample = 10.0f`).
- All tube models and the MNA solver operate in volts; apply this mapping at the plugin boundary.
- If your plugin uses a different voltage scale (e.g. ±5 V) apply an additional factor on top.

**Apply when**
- Feeding normalised DAW audio into `TubeStage`, `VariableMuStage`, or a custom MNA circuit.

**Example**
```cpp
float volts  = Analog::sampleToVolts(inputSample);  // ±1.0 → ±10 V
float sample = Analog::voltsToSample(plateVoltage);  // V → normalised
```

---

## `Analog::Nonlinear::NRPolicy<N>`

**Purpose**
- Template Newton-Raphson iteration manager with per-element step limiting, damping, convergence checking, NaN/Inf sanitisation, and fallback to the last-known-good solution.

**Typical use case**
- Drive the inner NR loop of a nonlinear circuit element (e.g. `TubeStage`'s 2×2 plate/cathode system).

**Need to know**
- `N` is the state-vector size fixed at compile time; all scratch buffers are stack-allocated — no heap allocation in the hot path.
- The step callback `bool(std::array<double, N>& x)` receives the current iterate, overwrites it with the next Newton estimate, and returns `false` on linear-solve failure (triggers immediate fallback).
- `NRConfig` controls `maxIterations`, `convergenceTol`, `dampingFactor`, and `maxDeltaV`.
- If convergence fails `x` is restored to the value it had on entry to `solve()`.
- Debug builds accumulate `totalIterations()` for profiling; call `resetCounters()` to clear.

**Apply when**
- Solving any small nonlinear system in the audio callback where heap allocation is prohibited.
- Wrapping a custom nonlinear MNA stamp that needs a convergence guard and fallback policy.

**Don't apply when**
- The system is linear — use `Circuit::solve()` directly.
- Need large N (> ~8) or dynamic N — a heap-based solver may be more appropriate.

**Example**
```cpp
Analog::Nonlinear::NRPolicy<2> nr;
std::array<double, 2> x = {Vp0, Vk0}; // warm-start from previous sample
auto result = nr.solve(x, [&](std::array<double, 2>& x) {
    // Evaluate nonlinear residuals, update x with Newton step
    return true; // false = singular Jacobian
});
if (!result.converged) { /* x was rolled back to warm-start */ }
```

---

## `Analog::Nonlinear::TubeParams` + Koren triode functions

**Purpose**
- Parametric Koren (1996) vacuum-tube triode model: plate current `Ip`, and analytical partial derivatives `∂Ip/∂Vpk`, `∂Ip/∂Vgk`.

**Typical use case**
- Stamping the nonlinear triode conductances into an MNA Jacobian during each Newton-Raphson iteration inside `TubeStage` or a custom tube circuit.

**Need to know**
- `TubeParams` holds `mu`, `kp`, `kvb`, `kg1`, `x`.  Factory functions provide starting points for 12AX7, 12AU7, and 6072; tune to measurements for production use.
- `triodeIp(Vpk, Vgk, p)` — plate current; always ≥ 0 (cut-off clamped).
- `triodeDIpDVpk` / `triodeDIpDVgk` — partial derivatives for Jacobian stamping; return 0 in cut-off.
- `triodeIpAndPartials` — computes all three in one call with a single `std::pow`; use in NR loops where all three are needed every iteration.
- Internal softplus and sigmoid helpers are numerically stable across the full voltage range.
- Physical operating range: `Vpk ∈ [0, 300 V]`, `Vgk ∈ [−3, 0 V]` for small-signal triodes.

**Apply when**
- Building a custom triode circuit not covered by the higher-level `TubeStage`/`VariableMuStage` models.
- Fitting new tube parameters from measured I–V curves.

**Don't apply when**
- You only need the full common-cathode gain stage — use `TubeStage` instead.

**Example**
```cpp
auto p = Analog::Nonlinear::TubeParams::tubeParams12AX7();
double Ip, gds, gm;
Analog::Nonlinear::triodeIpAndPartials(Vpk, Vgk, p, Ip, gds, gm);
// Stamp Ip, gds, gm into MNA Jacobian …
```

---

## `Analog::Models::TubeStage`

**Purpose**
- Complete common-cathode triode gain stage solved per sample via MNA + Newton-Raphson.

**Typical use case**
- Add analog tube saturation and compression to a guitar amplifier or hardware-emulation plugin.

**Need to know**
- Topology: `Vcc → Rp → Plate → [Triode] → Cathode → Rk → GND`, with optional cathode-bypass cap `Ck`.
- Solves a 2×2 nonlinear KCL system in `Vp` (plate) and `Vk` (cathode) each sample via `NRPolicy<2>`; uses previous sample's operating point as warm-start (typically 1–3 iterations at steady state).
- Input/output are normalised (±1.0 full-scale); UnitScaling (±10 V) is applied internally.
- Input is clamped to `±inputClampV` (default 5 V) before the solve to protect convergence with extreme inputs.
- Defaults model a 12AX7 with B+ = 250 V, Rp = 100 kΩ, Rk = 1.5 kΩ, no bypass cap.
- Call `prepare(sampleRate)` before the first `processSample()`; call `reset()` to clear state between sessions.

**Apply when**
- Need a physically grounded tube saturation stage with correct compression behaviour.
- Need a warm-start NR loop that stays stable across a wide range of input levels.

**Don't apply when**
- Need a variable-gain (VCA-like) tube stage controlled by a sidechain — use `VariableMuStage`.
- Need gain-stage topologies other than common-cathode (e.g. cathode follower, pentode).

**Example**
```cpp
Analog::Models::TubeStage tube; // default 12AX7 config
tube.prepare(44100.0);

// Per-sample:
float out = tube.processSample(inputSample);
```

---

## `Analog::Models::VariableMuStage`

**Purpose**
- Variable-mu common-cathode triode gain stage where gain is controlled by an external DC bias (control voltage), enabling VCA-style gain reduction for compressor/limiter sidechains.

**Typical use case**
- Core gain element in a Fairchild-670-style levelling amplifier or optical-style tube compressor.

**Need to know**
- Topology is identical to `TubeStage`, but the grid sees an additional negative DC bias `cvBias_` supplied by `setCv()`.
- Increasing CV drives the grid more negative → less plate current → lower voltage gain (monotonic attenuation).
- CV is clamped to `[0, cvMaxV]` (default 6 V); values above `cvMaxV` are clamped to prevent deep cut-off and NR instability.
- Includes a built-in DC-blocking HPF (~10 Hz) that removes the quiescent plate-voltage DC offset; pre-charged in `prepare()` so there is no startup transient.
- Output is gain-normalised to unity small-signal gain at CV = 0 (open-loop gain computed from the quiescent Jacobian in `prepare()`); phase inversion of the common-cathode topology is corrected automatically.
- Defaults: 6072 tube, B+ = 250 V, Rp = 100 kΩ, Rk = 1.5 kΩ, no bypass cap.
- `setCathodeBypassCapacitance()` and `setNRConfig()` can be changed at runtime without calling `prepare()`.

**Apply when**
- Need a tube VCA driven by an external sidechain control voltage.
- Building a Fairchild-style levelling amplifier with programme-dependent release.

**Don't apply when**
- Need fixed-gain tube saturation only — use the simpler `TubeStage`.

**Example**
```cpp
Analog::Models::VariableMuStage vmu; // default 6072 config
vmu.prepare(44100.0);

// Per-sample (after sidechain has produced a CV):
vmu.setCv(detectorCV);  // V in [0, 6]; 0 = unity gain
float out = vmu.processSample(inputSample);
```

---

## `Analog::Models::Sidechain::RectifierDetector`

**Purpose**
- Full-wave rectifier followed by an attack/release RC envelope follower; outputs a smoothed control voltage (V) suitable for driving `VariableMuStage`.

**Typical use case**
- Sidechain detector in a tube levelling amplifier: convert the audio level into a slowly varying CV that controls gain reduction.

**Need to know**
- `TimingKind::Fixed` — single-pole attack and release (classic peak-follower).
- `TimingKind::AutoRelease` — single-pole attack; two parallel release branches (fast + slow), output is the max of the two branch envelopes, giving rapid recovery after brief transients and slow recovery after sustained loud passages.
- Input is full-wave rectified; scaling from normalised samples to volts is applied before rectification.
- `prepare(sampleRate)` must be called before `processSample()` and whenever the sample rate or config changes.
- `controlVoltage()` returns the current CV without advancing state.

**Apply when**
- Building a programme-dependent release compressor/limiter sidechain.
- Feeding control voltage into `VariableMuStage`.

**Don't apply when**
- Need RMS detection — this is a peak/rectifier follower only.
- Need a frequency-weighted (K-weighted, A-weighted) detection path.

**Example**
```cpp
Analog::Models::Sidechain::RectifierDetectorConfig cfg;
cfg.kind         = Analog::Models::Sidechain::TimingKind::AutoRelease;
cfg.attackSec    = 0.0002;
cfg.fastReleaseSec = 0.05;
cfg.slowReleaseSec = 10.0;

Analog::Models::Sidechain::RectifierDetector det(cfg);
det.prepare(44100.0);

// Per-sample:
float cv = det.processSample(sidechainSample); // V
```

---

## `Analog::Models::TransformerLinear`

**Purpose**
- Linear audio transformer coloration model: first-order HPF (magnetising inductance rolloff) + first-order LPF (leakage / winding capacitance bandwidth limit) + memoryless tanh core-saturation stage.

**Typical use case**
- Add vintage transformer coloration to the output stage of a bus compressor or tape machine emulation.

**Need to know**
- Both filters use the bilinear transform; the −3 dB points are exact at `hpfCutoffHz` and `lpfCutoffHz`.
- `drive = 1.0` is fully linear; `drive > 1.0` applies tanh soft-clipping with unity small-signal gain (only the knee sharpness changes, not the level). Values up to ~10 produce musically useful harmonic distortion.
- Instantiate one `TransformerLinear` per channel for stereo.
- `setConfig()` followed by `prepare()` updates coefficients at runtime (e.g. in response to a UI parameter change).
- Defaults: HPF at 30 Hz, LPF at 18 kHz, `drive = 1.0`.

**Apply when**
- Adding frequency shaping and soft saturation typical of vintage iron transformers.
- Need a lightweight coloration stage without the full MNA overhead.

**Don't apply when**
- Need to model transformer nonlinearity with hysteresis (this model is memoryless and static).
- Need asymmetric or even-order harmonics characteristic of certain transformer topologies.

**Example**
```cpp
Analog::Models::TransformerLinear xfmrL, xfmrR;
xfmrL.prepare(44100.0);
xfmrR.prepare(44100.0);

// Per-sample:
float outL = xfmrL.processSample(inL);
float outR = xfmrR.processSample(inR);
```

---

## Combined use cases (multi-module)

### Full tube levelling amplifier (Fairchild-670 style)

- `RectifierDetector` (AutoRelease) tracks the programme level and produces a slowly varying CV.
- `VariableMuStage` (6072 tube) applies gain reduction proportional to that CV.
- Two `VariableMuStage` instances (one per channel) share the same sidechain CV for linked stereo operation.
- A `TransformerLinear` on the output stage adds low-end rolloff and gentle iron saturation.

### Custom tube circuit with passive components

- Describe the full nodal topology and stamp it into `Circuit` (resistors, capacitors, voltage sources for B+ and bias rails).
- Replace the nonlinear triode with a linearised conductance `gds` + `gm` each iteration.
- Drive the outer loop with `NRPolicy<N>` using `triodeIpAndPartials()` to form the Jacobian.
- Read node voltages from `Circuit::nodeVoltage()` after each converged step.

### Sidechain detector feeding a linear gain network

- Use `RectifierDetector` to generate a CV from the sidechain bus.
- Feed the CV into `Circuit::injectCurrent()` or `setVoltageSourceValue()` to modulate a linear gain network (e.g. a voltage-controlled resistor approximation) without engaging the full nonlinear NR loop.
