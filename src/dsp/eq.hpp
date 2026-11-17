// Mastering Suite — broad tone EQ
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/hdsp.hpp>

#include "dsp/controls.hpp"
#include "params.hpp"

namespace mastering {

// Exact magnitude of a biquad at a frequency, used by the EQ curve display.
inline double biquadGainDb(const hdsp::Biquad& f, double hz, double sr) {
  const double w = 2.0 * 3.14159265358979323846 * hz / sr;
  const double c1 = std::cos(w);
  const double s1 = std::sin(w);
  const double c2 = std::cos(2.0 * w);
  const double s2 = std::sin(2.0 * w);
  const double nr = f.b0 + f.b1 * c1 + f.b2 * c2;
  const double ni = -f.b1 * s1 - f.b2 * s2;
  const double dr = 1.0 + f.a1 * c1 + f.a2 * c2;
  const double di = -f.a1 * s1 - f.a2 * s2;
  const double n = std::sqrt(nr * nr + ni * ni);
  const double d = std::sqrt(dr * dr + di * di);
  return 20.0 * std::log10(std::max(n / std::max(d, 1e-20), 1e-12));
}

class MasteringEq {
public:
  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    reset();
    setControls(controls_);
  }

  void setControls(const EqControls& c) noexcept {
    const bool wasEnabled = controls_.enabled;
    controls_ = c;
    if (c.enabled && !wasEnabled) {
      reset();
    }
    if (!c.enabled) {
      return;
    }
    const float lowGain = lowGainDb(c.lowGain);
    const float midGain = midGainDb(c.midGain);
    const float highGain = highGainDb(c.highGain);
    const float low = lowFreqHz(c.lowFreq);
    const float mid = midFreqHz(c.midFreq);
    const float high = highFreqHz(c.highFreq);
    const float q = midQ(c.midQ);
    const float hp = hpfHz(c.hpf);
    const float tilt = eqTiltDb(c.tilt);
    for (int ch = 0; ch < 2; ++ch) {
      low_[ch].lowShelf(low, lowGain, 0.65f, rate_);
      mid_[ch].peak(mid, midGain, q, rate_);
      high_[ch].highShelf(high, highGain, 0.65f, rate_);
      tiltLow_[ch].lowShelf(120.0f, -0.5f * tilt, 0.7071f, rate_);
      tiltHigh_[ch].highShelf(8000.0f, 0.5f * tilt, 0.7071f, rate_);
      hpf_[ch].highPass(hp, 0.7071f, rate_);
    }
  }

  inline void process(float& left, float& right) noexcept {
    if (!controls_.enabled) {
      return;
    }
    left = low_[0].process(left);
    right = low_[1].process(right);
    left = mid_[0].process(left);
    right = mid_[1].process(right);
    left = high_[0].process(left);
    right = high_[1].process(right);
    left = tiltLow_[0].process(left);
    right = tiltLow_[1].process(right);
    left = tiltHigh_[0].process(left);
    right = tiltHigh_[1].process(right);
    left = hpf_[0].process(left);
    right = hpf_[1].process(right);
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      low_[ch].reset();
      mid_[ch].reset();
      high_[ch].reset();
      tiltLow_[ch].reset();
      tiltHigh_[ch].reset();
      hpf_[ch].reset();
    }
  }

  // Combined curve for the display, in dB.
  double gainDbAt(double hz) const noexcept {
    if (!controls_.enabled) {
      return 0.0;
    }
    return biquadGainDb(low_[0], hz, rate_) +
           biquadGainDb(mid_[0], hz, rate_) +
           biquadGainDb(high_[0], hz, rate_) +
           biquadGainDb(tiltLow_[0], hz, rate_) +
           biquadGainDb(tiltHigh_[0], hz, rate_) +
           biquadGainDb(hpf_[0], hz, rate_);
  }


private:
  float rate_ = 48000.0f;
  EqControls controls_;
  hdsp::Biquad low_[2];
  hdsp::Biquad mid_[2];
  hdsp::Biquad high_[2];
  hdsp::Biquad tiltLow_[2];
  hdsp::Biquad tiltHigh_[2];
  hdsp::Biquad hpf_[2];
};

} // namespace mastering
