#pragma once

#include <cmath>

namespace Analog::Models::Sidechain {

/// Whether the release phase uses a fixed time constant or two parallel
/// branches (fast + slow) for programme-dependent behaviour.
enum class TimingKind { Fixed, AutoRelease };

/// Compute a first-order IIR smoothing coefficient from a time constant and
/// sample rate.
///
/// Returns α = exp(−1 / (τ · fs)), where α → 1 means very slow (smooth),
/// α → 0 means very fast (tracking).
///
/// @param tauSec     Time constant (seconds); must be > 0.
/// @param sampleRate Sample rate (Hz); must be > 0.
/// @return           Smoothing coefficient in [0, 1).
[[nodiscard]] inline double computeAlpha(double tauSec, double sampleRate) noexcept
{
    return std::exp(-1.0 / (tauSec * sampleRate));
}

} // namespace Analog::Models::Sidechain
