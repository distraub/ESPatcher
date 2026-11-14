#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace GlitchDrums {

// Phase 10a: ZDF (topology-preserving transform) 4-pole lowpass ladder with
// per-stage tanh saturation, for the ANALOG (DFAM-style) voice model. The
// resonance range reaches stable self-oscillation (feedback k up to 4.2;
// the stage saturators bound the amplitude).
//
// Two coefficient paths:
//   - Slow path: setCutoff() computes the exact prewarped g = tan(pi*fc/fs).
//     Render thread calls it only at block or render-event segment
//     boundaries, matching the PadChannelStrip timing contract.
//   - Fast path: processModulated() accepts a per-sample cutoff for
//     audio-rate filter FM (VCF MOD). It uses fastTan(), a bounded rational
//     approximation validated in Tools/ladder_harness.cpp to well under 1%
//     relative error across the full 20 Hz ... 0.45*fs coefficient range,
//     so no transcendental trig runs per sample.
//
// process()/processModulated() allocate nothing and take no locks. State is
// four floats; reset() clears all integrators.
class GDLadderFilter {
public:
    static constexpr float kMaxFeedback = 4.2f;
    static constexpr float kMinCutoffHz = 20.0f;

    void prepare(double newSampleRate) {
        sampleRate = std::max(newSampleRate, 8000.0);
        inverseSampleRate = static_cast<float>(1.0 / sampleRate);
        nyquistLimit = 0.45f * static_cast<float>(sampleRate);
        setCutoff(cutoffHz);
        reset();
    }

    void reset() {
        state1 = state2 = state3 = state4 = 0.0f;
    }

    // Slow path only: exact coefficient, block/segment boundary.
    void setCutoff(float hz) {
        cutoffHz = std::clamp(hz, kMinCutoffHz, nyquistLimit);
        g = std::tan(kPi * cutoffHz * inverseSampleRate);
        const float G1 = g / (1.0f + g);
        G = G1;
        G2 = G1 * G1;
        G3 = G2 * G1;
        G4 = G2 * G2;
    }

    // 0...1 normalized; 1.0 maps to kMaxFeedback (past self-oscillation).
    void setResonance(float normalized) {
        feedback = std::clamp(normalized, 0.0f, 1.0f) * kMaxFeedback;
    }

    float currentCutoff() const { return cutoffHz; }

    float process(float x) {
        return run(x, G, G2, G3, G4);
    }

    // Fast path: per-sample modulated cutoff for audio-rate VCF MOD.
    float processModulated(float x, float modulatedCutoffHz) {
        const float fc = std::clamp(modulatedCutoffHz, kMinCutoffHz, nyquistLimit);
        const float gm = fastTan(kPi * fc * inverseSampleRate);
        const float G1 = gm / (1.0f + gm);
        const float Gm2 = G1 * G1;
        return run(x, G1, Gm2, Gm2 * G1, Gm2 * Gm2);
    }

    // Rational tan approximation (continued-fraction Pade) for
    // x in (0, 0.45*pi]. Relative error < 0.1% over that range.
    static float fastTan(float x) {
        const float x2 = x * x;
        const float numerator = x * (10395.0f + x2 * (-1260.0f + 21.0f * x2));
        const float denominator = 10395.0f + x2 * (-4725.0f + x2 * (210.0f - x2));
        return numerator / denominator;
    }

private:
    static constexpr float kPi = 3.14159265358979323846f;

    static float onePoleStep(float in, float &s, float G1) {
        const float v = (in - s) * G1;
        const float y = v + s;
        s = y + v;
        return y;
    }

    // Linear zero-delay feedback solve, then saturated stage evaluation.
    // The solve uses the linearized prediction; tanh at every stage input
    // bounds self-oscillation without per-sample coefficient recompute.
    float run(float x, float G1, float G2c, float G3c, float G4c) {
        const float oneMinusG = 1.0f - G1;
        const float S1 = state1 * oneMinusG;
        const float S2 = state2 * oneMinusG;
        const float S3 = state3 * oneMinusG;
        const float S4 = state4 * oneMinusG;
        const float feedbackSum = G3c * S1 + G2c * S2 + G1 * S3 + S4;
        const float u = (x - feedback * feedbackSum) / (1.0f + feedback * G4c);
        float y = onePoleStep(std::tanh(u), state1, G1);
        y = onePoleStep(std::tanh(y), state2, G1);
        y = onePoleStep(std::tanh(y), state3, G1);
        y = onePoleStep(std::tanh(y), state4, G1);
        return y;
    }

    double sampleRate = 48000.0;
    float inverseSampleRate = 1.0f / 48000.0f;
    float nyquistLimit = 0.45f * 48000.0f;
    float cutoffHz = 1000.0f;
    float g = 0.0654f;
    float G = 0.0614f;
    float G2 = 0.0038f;
    float G3 = 0.0002f;
    float G4 = 0.0000f;
    float feedback = 0.0f;
    float state1 = 0.0f;
    float state2 = 0.0f;
    float state3 = 0.0f;
    float state4 = 0.0f;
};

} // namespace GlitchDrums
