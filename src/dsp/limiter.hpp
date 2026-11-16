// Mastering Suite — lookahead true-peak limiter
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

#include <hdsp/hdsp.hpp>

#include "dsp/controls.hpp"
#include "params.hpp"

namespace mastering {

// Original lookahead brickwall limiter.  The detector runs ahead of the audio
// by five milliseconds, so the gain is already down when the peak arrives.
// True-peak detection uses hearth-dsp's 4x interpolator; the audio path stays
// at the host rate.  The delay is always in circuit, including when the module
// is bypassed, so the reported latency never jumps.
class LookaheadLimiter {
public:
  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    lookahead_ = std::max(1, static_cast<int>(0.005f * rate_));
    delayL_.assign(static_cast<size_t>(lookahead_ + 1), 0.0f);
    delayR_.assign(static_cast<size_t>(lookahead_ + 1), 0.0f);
    attackCoeff_ = hdsp::timeCoeff(0.2f, rate_);
    releaseCoeff_ = hdsp::timeCoeff(limiterReleaseMs(controls_.release), rate_);
    reset();
    setControls(controls_);
  }

  void setControls(const LimiterControls& c) noexcept {
    controls_ = c;
    ceiling_ = hdsp::dbToGain(limiterCeilingDb(c.ceiling));
    releaseCoeff_ = hdsp::timeCoeff(limiterReleaseMs(c.release), rate_);
  }

  void reset() noexcept {
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    write_ = 0;
    gain_ = 1.0f;
    grDb_ = 0.0f;
    tpL_.reset();
    tpR_.reset();
  }

  inline void process(float& left, float& right) noexcept {
    float peak = std::max(std::fabs(left), std::fabs(right));
    if (controls_.truePeak) {
      peak = std::max(peak, tpL_.process(left));
      peak = std::max(peak, tpR_.process(right));
    }
    const float target = peak > ceiling_ ? ceiling_ / peak : 1.0f;
    if (target < gain_) {
      gain_ += (target - gain_) * (1.0f - attackCoeff_);
    } else {
      gain_ += (target - gain_) * (1.0f - releaseCoeff_);
    }
    gain_ = std::min(std::max(gain_, 0.0f), 1.0f);
    grDb_ = controls_.enabled ? -hdsp::gainToDb(std::max(gain_, 1e-9f)) : 0.0f;

    delayL_[write_] = left;
    delayR_[write_] = right;
    const size_t size = delayL_.size();
    const size_t read =
        (static_cast<size_t>(write_) + size - static_cast<size_t>(lookahead_)) %
        size;
    float outL = delayL_[read];
    float outR = delayR_[read];
    write_ = (write_ + 1) % static_cast<int>(size);

    if (controls_.enabled) {
      outL *= gain_;
      outR *= gain_;
      outL = std::min(std::max(outL, -ceiling_), ceiling_);
      outR = std::min(std::max(outR, -ceiling_), ceiling_);
    }
    left = outL;
    right = outR;
  }

  int latencySamples() const noexcept { return lookahead_; }
  float gainReductionDb() const noexcept { return grDb_; }

private:
  float rate_ = 48000.0f;
  LimiterControls controls_;
  int lookahead_ = 240;
  std::vector<float> delayL_;
  std::vector<float> delayR_;
  int write_ = 0;
  float ceiling_ = 0.97f;
  float gain_ = 1.0f;
  float grDb_ = 0.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
  hdsp::TruePeakDetector tpL_;
  hdsp::TruePeakDetector tpR_;
};

} // namespace mastering
