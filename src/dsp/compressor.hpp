// Mastering Suite — bus compressor
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/hdsp.hpp>

#include "dsp/controls.hpp"
#include "params.hpp"

namespace mastering {

// Stereo-linked feed-forward bus compressor.  The detector is the louder
// channel, so the stereo image stays put; the side-chain high-pass keeps low
// end from pumping the whole mix.  This is the same gain-computer shape as the
// Comp Suite bus VCA, wrapped with mastering-friendly mix and auto-gain.
class BusCompressor {
public:
  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    attackCoeff_ = hdsp::timeCoeff(20.0f, rate_);
    releaseCoeff_ = hdsp::timeCoeff(200.0f, rate_);
    autoCoeff_ = hdsp::timeCoeff(300.0f, rate_);
    holdSamples_ = static_cast<int>(0.002f * rate_);
    for (auto& f : sc_) {
      f.reset();
    }
    reset();
    setControls(controls_);
  }

  void setControls(const CompControls& c) noexcept {
    const bool wasEnabled = controls_.enabled;
    controls_ = c;
    thresholdDb_ = compThresholdDb(c.threshold);
    ratio_ = compRatio(c.ratio);
    attackCoeff_ = hdsp::timeCoeff(compAttackMs(c.attack), rate_);
    releaseCoeff_ = hdsp::timeCoeff(compReleaseMs(c.release), rate_);
    kneeDb_ = compKneeDb(c.knee);
    const float hp = compScHpfHz(c.scHpf);
    for (auto& f : sc_) {
      f.highPass(hp, 0.7071f, rate_);
    }
    mix_ = clamp01(c.mix);
    if (c.enabled && !wasEnabled) {
      reset();
    }
  }

  // Processes one stereo frame, returns gain reduction in dB.
  inline float process(float& left, float& right) noexcept {
    if (!controls_.enabled) {
      grDb_ = 0.0f;
      return 0.0f;
    }
    const float dryL = left;
    const float dryR = right;
    const float detL = sc_[0].process(dryL);
    const float detR = sc_[1].process(dryR);
    const float level = std::max(std::fabs(detL), std::fabs(detR));
    updateDetector(level);
    const float levelDb = hdsp::gainToDb(std::max(env_, 1.0e-9f));
    const float target = hdsp::gainReductionDb(levelDb, thresholdDb_, ratio_,
                                               kneeDb_);
    // The detector envelope already carries the attack and release; applying
    // them again here would double every time constant.
    grDb_ = std::max(target, 0.0f);

    const float targetAuto = controls_.autoGain ? grDb_ : 0.0f;
    autoDb_ += (targetAuto - autoDb_) * (1.0f - autoCoeff_);
    const float makeup = hdsp::dbToGain(autoDb_);
    const float gain = hdsp::dbToGain(-grDb_) * makeup;

    const float wetL = dryL * gain;
    const float wetR = dryR * gain;
    left = dryL + mix_ * (wetL - dryL);
    right = dryR + mix_ * (wetR - dryR);
    return grDb_;
  }

  void reset() noexcept {
    for (auto& f : sc_) {
      f.reset();
    }
    env_ = 0.0f;
    hold_ = 0;
    grDb_ = 0.0f;
    autoDb_ = 0.0f;
  }

  float gainReductionDb() const noexcept { return grDb_; }

private:
  void updateDetector(float level) noexcept {
    if (level > env_) {
      env_ = attackCoeff_ * env_ + (1.0f - attackCoeff_) * level;
      hold_ = holdSamples_;
    } else if (hold_ > 0) {
      --hold_;
    } else {
      env_ = releaseCoeff_ * env_ + (1.0f - releaseCoeff_) * level;
    }
  }

  float rate_ = 48000.0f;
  CompControls controls_;
  float thresholdDb_ = -20.0f;
  float ratio_ = 4.0f;
  float kneeDb_ = 3.6f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
  float autoCoeff_ = 0.0f;
  float mix_ = 1.0f;
  float env_ = 0.0f;
  float grDb_ = 0.0f;
  float autoDb_ = 0.0f;
  int holdSamples_ = 0;
  int hold_ = 0;
  hdsp::Biquad sc_[2];
};

} // namespace mastering
