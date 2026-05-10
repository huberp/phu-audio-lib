#pragma once

namespace Analog {

/// Full-scale sample amplitude (±1.0) maps to ±10 V (Eurorack convention).
///
/// This convention is used throughout the analog circuit simulation library:
/// ±1.0 normalised sample = ±10 V circuit voltage.  Plugin input/output stages
/// must apply the same mapping for the tube models to operate at their designed
/// operating points.
///
/// If your plugin uses a different voltage scale (e.g. ±5 V), wrap these
/// functions or apply an additional scaling factor before calling library DSP.
constexpr float kVoltsPerSample = 10.0f;
constexpr float kSamplesPerVolt = 1.0f / kVoltsPerSample;

/// Convert a normalised sample value (±1.0 full-scale) to volts.
[[nodiscard]] inline float sampleToVolts(float sample) noexcept {
    return sample * kVoltsPerSample;
}

/// Convert a voltage value to a normalised sample value (±1.0 full-scale).
[[nodiscard]] inline float voltsToSample(float volts) noexcept {
    return volts * kSamplesPerVolt;
}

} // namespace Analog
