#pragma once

#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <limits>

namespace phu {
namespace audio {

/**
 * Iso226 — ISO 226:2003 Equal-Loudness Contour utility.
 *
 * Computes equal-loudness (phon) contours as defined by ISO 226:2003.
 * Given a loudness level in phons, returns the sound pressure level (dB SPL)
 * at each frequency that produces the same perceived loudness.
 *
 * Three modes are provided:
 *  - byIsoFreqs  : standard 29 ISO reference frequencies (20 Hz – 12.5 kHz)
 *  - byGivenFreqs: arbitrary user frequencies; natural cubic spline interpolation
 *                  between the 29 reference points in log10-frequency space;
 *                  linear extrapolation above 12.5 kHz
 *  - byFFT       : helper that fills an array with FFT bin frequencies for use
 *                  with byGivenFreqs
 *
 * All functions accept a pre-allocated std::array to avoid heap allocation.
 * The array may be larger than needed; the return value is the number of
 * elements written.
 *
 * Valid phon range: 0 – 90 phons (ISO 226:2003 Table 1 bounds).
 *
 * Reference MATLAB implementation:
 *   https://www.dsprelated.com/showcode/174.php
 */
class Iso226 {
  public:
    /** Number of standard reference frequencies defined by ISO 226:2003. */
    static constexpr int kNumFreqs = 29;

    /** Standard ISO 226:2003 reference frequencies in Hz (Table 1). */
    static constexpr std::array<double, kNumFreqs> kFrequencies = {
        20.0,   25.0,   31.5,   40.0,   50.0,   63.0,   80.0,  100.0,  125.0,
        160.0,  200.0,  250.0,  315.0,  400.0,  500.0,  630.0,  800.0, 1000.0,
        1250.0, 1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0,
        10000.0, 12500.0
    };

    /**
     * Compute the equal-loudness contour at the 29 standard ISO reference frequencies.
     *
     * @param phonLevel  Loudness level in phons (valid range: 0 – 90).
     * @param out        Pre-allocated output array; must hold at least kNumFreqs (29) elements.
     *                   out[i] receives the dB SPL for kFrequencies[i].
     * @return           Number of values written (always kNumFreqs = 29).
     */
    template <std::size_t N>
    static std::size_t byIsoFreqs(double phonLevel, std::array<double, N>& out) {
        static_assert(N >= static_cast<std::size_t>(kNumFreqs),
                      "Output array must hold at least kNumFreqs (29) elements");
        computeContour(phonLevel, kAfData, kLuData, kTfData, kNumFreqs, out.data());
        return static_cast<std::size_t>(kNumFreqs);
    }

    /**
     * Compute the equal-loudness contour at user-specified frequencies.
     *
     * Uses natural cubic spline interpolation (in the log10-frequency domain) between
     * the 29 ISO reference points. Frequencies above 12,500 Hz are handled by linear
     * extrapolation using the slope of the spline at its upper boundary. Frequencies
     * at or below 0 Hz produce NaN in the corresponding output slot.
     *
     * @param phonLevel  Loudness level in phons (valid range: 0 – 90).
     * @param freqs      Input frequencies in Hz.
     * @param out        Pre-allocated output array; must hold at least NFreq elements.
     *                   out[i] receives the dB SPL for freqs[i].
     * @return           Number of values written (= NFreq).
     */
    template <std::size_t NFreq, std::size_t NOut>
    static std::size_t byGivenFreqs(double phonLevel,
                                    const std::array<double, NFreq>& freqs,
                                    std::array<double, NOut>& out) {
        static_assert(NOut >= NFreq, "Output array must be at least as large as the frequencies array");

        // Compute SPL at the 29 reference points
        std::array<double, kNumFreqs> refSpl{};
        computeContour(phonLevel, kAfData, kLuData, kTfData, kNumFreqs, refSpl.data());

        // Work in log10(f) space — ISO frequencies are distributed logarithmically
        std::array<double, kNumFreqs> logX{};
        for (int i = 0; i < kNumFreqs; ++i)
            logX[i] = std::log10(kFreqData[i]);

        // Build natural cubic spline coefficients
        std::array<double, kNumFreqs - 1> sa{}, sb{}, sc{}, sd{};
        buildSpline(logX.data(), refSpl.data(), kNumFreqs,
                    sa.data(), sb.data(), sc.data(), sd.data());

        const double logXMax = logX[kNumFreqs - 1];

        for (std::size_t i = 0; i < NFreq; ++i) {
            const double f = freqs[i];
            if (f <= 0.0) {
                out[i] = std::numeric_limits<double>::quiet_NaN();
                continue;
            }
            const double logF = std::log10(f);
            if (logF <= logXMax) {
                out[i] = evalSpline(logX.data(), sa.data(), sb.data(), sc.data(), sd.data(),
                                    kNumFreqs, logF);
            } else {
                // Linear extrapolation beyond 12,500 Hz
                const int last = kNumFreqs - 2;
                const double h   = logXMax - logX[last];
                const double slope = sb[last] + 2.0 * sc[last] * h + 3.0 * sd[last] * h * h;
                out[i] = refSpl[kNumFreqs - 1] + slope * (logF - logXMax);
            }
        }
        return NFreq;
    }

    /**
     * Fill an array with FFT bin frequencies for use with byGivenFreqs().
     *
     * Fills out[i] = i * sampleRate / fftSize for bins 0 … fftSize/2 (inclusive),
     * producing fftSize/2 + 1 values (DC at index 0, Nyquist at index fftSize/2).
     *
     * @code
     *   std::array<double, 1025> binFreqs;
     *   std::array<double, 1025> contour;
     *   Iso226::byFFT(48000.0, 2048, binFreqs);
     *   Iso226::byGivenFreqs(40.0, binFreqs, contour);
     * @endcode
     *
     * @note Bin 0 is DC (0 Hz) and will produce NaN in byGivenFreqs; skip index 0 or
     *       use bin 1 as the lowest meaningful frequency.
     *
     * @param sampleRate  Sample rate in Hz.
     * @param fftSize     FFT size (e.g. 1024, 2048).
     * @param out         Pre-allocated array; must hold at least fftSize/2 + 1 elements.
     * @return            Number of values written (= fftSize/2 + 1).
     */
    template <std::size_t N>
    static std::size_t byFFT(double sampleRate, int fftSize, std::array<double, N>& out) {
        const int numBins = fftSize / 2 + 1;
        assert(static_cast<std::size_t>(numBins) <= N &&
               "Output array must hold at least fftSize/2 + 1 elements");
        for (int i = 0; i < numBins; ++i)
            out[i] = static_cast<double>(i) * sampleRate / static_cast<double>(fftSize);
        return static_cast<std::size_t>(numBins);
    }

  private:
    // -------------------------------------------------------------------------
    // ISO 226:2003 Table 1 data
    // -------------------------------------------------------------------------

    static constexpr double kFreqData[kNumFreqs] = {
        20.0,   25.0,   31.5,   40.0,   50.0,   63.0,   80.0,   100.0,  125.0,
        160.0,  200.0,  250.0,  315.0,  400.0,  500.0,  630.0,  800.0,  1000.0,
        1250.0, 1600.0, 2000.0, 2500.0, 3150.0, 4000.0, 5000.0, 6300.0, 8000.0,
        10000.0, 12500.0
    };

    // Exponent for loudness level (α_f), ISO 226:2003 Table 1
    static constexpr double kAfData[kNumFreqs] = {
        0.532, 0.506, 0.480, 0.455, 0.432, 0.409, 0.387, 0.367, 0.349,
        0.330, 0.315, 0.301, 0.288, 0.276, 0.267, 0.259, 0.253, 0.250,
        0.246, 0.244, 0.243, 0.243, 0.243, 0.242, 0.242, 0.245, 0.254,
        0.271, 0.301
    };

    // Magnitude normalized to free field (L_u), ISO 226:2003 Table 1
    static constexpr double kLuData[kNumFreqs] = {
        -31.6, -27.2, -23.0, -19.1, -15.9, -13.0, -10.3,  -8.1,  -6.2,
         -4.5,  -3.1,  -2.0,  -1.1,  -0.4,   0.0,   0.3,   0.5,   0.0,
         -2.7,  -4.1,  -1.0,   1.7,   2.5,   1.2,  -2.1,  -7.1, -11.2,
        -10.7,  -3.1
    };

    // Absolute threshold in quiet (T_f), ISO 226:2003 Table 1
    static constexpr double kTfData[kNumFreqs] = {
        78.5, 68.7, 59.5, 51.1, 44.0, 37.5, 31.5, 26.5, 22.1,
        17.9, 14.4, 11.4,  8.6,  6.2,  4.4,  3.0,  2.2,  2.4,
         3.5,  1.7, -1.3, -4.2, -6.0, -5.4, -1.5,  6.0, 12.6,
        13.9, 12.3
    };

    // -------------------------------------------------------------------------
    // Core formula
    // -------------------------------------------------------------------------

    /**
     * Compute dB SPL from phon level at each of the n reference frequencies.
     * ISO 226:2003 formula (Section 4.2 / Table 1):
     *   A_f = 4.47e-3 * (10^(0.025*N) - 1.14) + (0.4 * 10^((T_f + L_u)/10 - 9))^α_f
     *   L_p = (10/α_f) * log10(A_f) - L_u + 94
     */
    static void computeContour(double phonLevel,
                                const double* af, const double* lu, const double* tf,
                                int n, double* out) {
        const double t1 = 4.47e-3 * (std::pow(10.0, 0.025 * phonLevel) - 1.14);
        for (int i = 0; i < n; ++i) {
            const double base = 0.4 * std::pow(10.0, (tf[i] + lu[i]) / 10.0 - 9.0);
            const double Af   = t1 + std::pow(base, af[i]);
            out[i] = (10.0 / af[i]) * std::log10(Af) - lu[i] + 94.0;
        }
    }

    // -------------------------------------------------------------------------
    // Natural cubic spline
    // -------------------------------------------------------------------------

    /**
     * Compute natural cubic spline coefficients for n knot points (x[], y[]).
     *
     * On segment i the spline evaluates as:
     *   S_i(t) = a[i] + b[i]*(t-x[i]) + c[i]*(t-x[i])^2 + d[i]*(t-x[i])^3
     * with n-1 segments (indices 0 .. n-2).
     *
     * Natural boundary conditions: S''(x[0]) = S''(x[n-1]) = 0.
     * x values must be strictly increasing.
     */
    static void buildSpline(const double* x, const double* y, int n,
                             double* a, double* b, double* c, double* d) {
        const int m = n - 1; // number of segments

        // Fixed-size intermediates (n is always kNumFreqs = 29, m = 28)
        double h[28]{};      // segment widths
        double rhs[29]{};    // right-hand side
        double diag[29]{};   // main diagonal (forward sweep)
        double mu[28]{};     // off-diagonal ratio (forward sweep)
        double z[29]{};      // forward sweep result
        double cFull[29]{};  // second derivatives at knots

        for (int i = 0; i < m; ++i) {
            h[i] = x[i + 1] - x[i];
            a[i] = y[i];
        }

        // Natural spline: cFull[0] = 0
        diag[0] = 1.0; mu[0] = 0.0; z[0] = 0.0;

        for (int i = 1; i < m; ++i) {
            rhs[i]  = 3.0 * ((y[i + 1] - y[i]) / h[i] - (y[i] - y[i - 1]) / h[i - 1]);
            diag[i] = 2.0 * (x[i + 1] - x[i - 1]) - h[i - 1] * mu[i - 1];
            mu[i]   = h[i] / diag[i];
            z[i]    = (rhs[i] - h[i - 1] * z[i - 1]) / diag[i];
        }

        // Natural spline: cFull[m] = 0
        diag[m] = 1.0; z[m] = 0.0; cFull[m] = 0.0;

        // Back substitution
        for (int j = m - 1; j >= 0; --j) {
            cFull[j] = z[j] - mu[j] * cFull[j + 1];
            b[j] = (y[j + 1] - y[j]) / h[j] - h[j] * (cFull[j + 1] + 2.0 * cFull[j]) / 3.0;
            d[j] = (cFull[j + 1] - cFull[j]) / (3.0 * h[j]);
            c[j] = cFull[j];
        }
    }

    /**
     * Evaluate the piecewise cubic spline at position xq.
     * Uses binary search to locate the correct segment.
     * Clamps to the first knot value for xq < x[0].
     */
    static double evalSpline(const double* x,
                              const double* a, const double* b,
                              const double* c, const double* d,
                              int n, double xq) {
        if (xq <= x[0])
            return a[0];

        int lo = 0, hi = n - 2;
        while (lo < hi) {
            const int mid = (lo + hi + 1) / 2;
            if (x[mid] <= xq) lo = mid;
            else              hi = mid - 1;
        }
        const double t = xq - x[lo];
        return a[lo] + b[lo] * t + c[lo] * (t * t) + d[lo] * (t * t * t);
    }
};

} // namespace audio
} // namespace phu
