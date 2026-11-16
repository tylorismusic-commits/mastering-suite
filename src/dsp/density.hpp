// Mastering Suite — harmonic density stage
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/hdsp.hpp>

#include "dsp/controls.hpp"
#include "params.hpp"

namespace mastering {

// Original harmonic-density stage.  It leans on the ADAA shaper's asymmetric
// bias to add even and odd harmonics, then blends that density back over the
// dry signal.  This is not an OInflator clone and uses no proprietary curve.
class DensityStage {
public:
  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    inputSmooth_.setTime(12.0f, rate_);
    effectSmooth_.setTime(20.0f, rate_);
    outputSmooth_.setTime(12.0f, rate_);
    enableSmooth_.setTime(8.0f, rate_);
    for (auto& h : hp_) {
      h.highPass(15.0f, 0.7071f, rate_);
      h.reset();
    }
    reset();
  }

  void setControls(const DensityControls& c) noexcept {
    controls_ = c;
    inputSmooth_.setTarget(hdsp::dbToGain(densityDb(c.input)));
    effectSmooth_.setTarget(clamp01(c.effect));
    outputSmooth_.setTarget(hdsp::dbToGain(densityDb(c.output)));
    enableSmooth_.setTarget(c.enabled ? 1.0f : 0.0f);
  }

  void snap() noexcept {
    inputSmooth_.snap();
    effectSmooth_.snap();
    outputSmooth_.snap();
    enableSmooth_.snap();
  }

  void reset() noexcept {
    for (auto& s : shaper_) {
      s.reset();
    }
    for (auto& h : hp_) {
      h.reset();
    }
  }

  inline void process(float& left, float& right) noexcept {
    const float in = inputSmooth_.next();
    const float effect = effectSmooth_.next();
    const float out = outputSmooth_.next();
    const float gate = enableSmooth_.next();

    const float dryL = left;
    const float dryR = right;
    const float k = 1.0f + 4.0f * effect;
    const float bias = 0.02f + 0.18f * effect;

    float wetL = shaper_[0].process(dryL * in, k, bias);
    float wetR = shaper_[1].process(dryR * in, k, bias);
    wetL = hp_[0].process(wetL) * out;
    wetR = hp_[1].process(wetR) * out;

    const float yL = dryL + effect * (wetL - dryL);
    const float yR = dryR + effect * (wetR - dryR);
    left = dryL + gate * (yL - dryL);
    right = dryR + gate * (yR - dryR);
  }

  float amount() const noexcept { return controls_.effect; }

private:
  float rate_ = 48000.0f;
  DensityControls controls_;
  hdsp::Smoothed inputSmooth_;
  hdsp::Smoothed effectSmooth_;
  hdsp::Smoothed outputSmooth_;
  hdsp::Smoothed enableSmooth_;
  hdsp::AsymShaper shaper_[2];
  hdsp::Biquad hp_[2];
};

} // namespace mastering
