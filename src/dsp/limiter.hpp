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

// Lookahead brickwall limiter with limiter-only 2x/4x oversampling.  The
// detector and gain act on the oversampled signal; the audio is returned to
// the host rate through the matching half-band filter.  Latency is kept
// constant across oversampling modes by padding the base-rate paths, so the
// host never has to chase a moving latency while a session is playing.
class LookaheadLimiter {
public:
  static constexpr int kMaxBlock = 1024;
  static constexpr int kMaxFactor = 4;
  static constexpr int kMaxFilterLatency = 45;

  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    baseLookahead_ = std::max(1, static_cast<int>(0.005f * rate_));
    const int maxLook = baseLookahead_ * kMaxFactor;
    delayL_.assign(static_cast<size_t>(maxLook + 1), 0.0f);
    delayR_.assign(static_cast<size_t>(maxLook + 1), 0.0f);
    osL_.assign(static_cast<size_t>(kMaxBlock * kMaxFactor), 0.0f);
    osR_.assign(static_cast<size_t>(kMaxBlock * kMaxFactor), 0.0f);
    padL_.assign(static_cast<size_t>(kMaxFilterLatency + 1), 0.0f);
    padR_.assign(static_cast<size_t>(kMaxFilterLatency + 1), 0.0f);
    osFactor_ = 0;
    configureOversampling(controls_.oversample);
    setControls(controls_);
    inputSmooth_.setTarget(hdsp::dbToGain(limiterInputDb(controls_.inputGain)));
    inputSmooth_.snap();
  }

  void setControls(const LimiterControls& c) noexcept {
    const bool wasEnabled = controls_.enabled;
    controls_ = c;
    const int os = std::min(std::max(c.oversample, 0), 2);
    if (os != osSetting_) {
      configureOversampling(os);
    }
    // A small internal margin covers the half-band filter's overshoot on the
    // downsampled output, so the delivered true peak sits at or below the
    // ceiling the user sees.
    ceiling_ = hdsp::dbToGain(limiterCeilingDb(c.ceiling) - 0.05f);
    inputSmooth_.setTarget(hdsp::dbToGain(limiterInputDb(c.inputGain)));
    releaseCoeff_ =
        hdsp::timeCoeff(limiterReleaseMs(c.release), rate_ * osFactor_);
    attackCoeff_ = hdsp::timeCoeff(0.2f, rate_ * osFactor_);
    if (c.enabled && !wasEnabled) {
      gain_ = 1.0f;
      grDb_ = 0.0f;
      tpL_.reset();
      tpR_.reset();
    }
  }

  void reset() noexcept {
    std::fill(delayL_.begin(), delayL_.end(), 0.0f);
    std::fill(delayR_.begin(), delayR_.end(), 0.0f);
    std::fill(padL_.begin(), padL_.end(), 0.0f);
    std::fill(padR_.begin(), padR_.end(), 0.0f);
    write_ = 0;
    padWrite_ = 0;
    gain_ = 1.0f;
    grDb_ = 0.0f;
    inputSmooth_.snap();
    tpL_.reset();
    tpR_.reset();
  }

  // Processes up to any host block size; the oversampler works in 1024-frame
  // chunks internally.
  inline void process(float* left, float* right, int frames) noexcept {
    grDb_ = 0.0f;
    for (int offset = 0; offset < frames; offset += kMaxBlock) {
      const int n = std::min(kMaxBlock, frames - offset);
      if (osFactor_ == 1) {
        processSamples(left + offset, right + offset, n);
        applyPad(left + offset, right + offset, n);
      } else {
        os_.up(left + offset, right + offset, n, osL_.data(), osR_.data());
        processSamples(osL_.data(), osR_.data(), n * osFactor_);
        os_.down(osL_.data(), osR_.data(), n, left + offset, right + offset);
        applyPad(left + offset, right + offset, n);
      }
    }
  }

  // Total latency in base-rate samples: lookahead + half-band filter +
  // compensation pad.  It is independent of the oversampling setting.
  int latencySamples() const noexcept {
    return baseLookahead_ + kMaxFilterLatency;
  }
  float gainReductionDb() const noexcept { return grDb_; }
  int oversampleFactor() const noexcept { return osFactor_; }

private:
  void configureOversampling(int setting) noexcept {
    osSetting_ = std::min(std::max(setting, 0), 2);
    osFactor_ = 1 << (osSetting_ == 2 ? 2 : osSetting_);
    os_.prepare(osFactor_);
    lookahead_ = std::max(1, baseLookahead_ * osFactor_);
    delaySize_ = static_cast<size_t>(lookahead_ + 1);
    padDelay_ = kMaxFilterLatency - os_.latencySamples();
    if (padDelay_ < 0) padDelay_ = 0;
    if (padDelay_ > kMaxFilterLatency) padDelay_ = kMaxFilterLatency;
    inputSmooth_.setTime(15.0f, rate_ * osFactor_);
    attackCoeff_ = hdsp::timeCoeff(0.2f, rate_ * osFactor_);
    releaseCoeff_ =
        hdsp::timeCoeff(limiterReleaseMs(controls_.release), rate_ * osFactor_);
    reset();
  }

  inline void processSamples(float* l, float* r, int n) noexcept {
    for (int i = 0; i < n; ++i) {
      const float drive = inputSmooth_.next();
      const float inGain = controls_.enabled ? drive : 1.0f;
      const float inL = l[i] * inGain;
      const float inR = r[i] * inGain;
      float peak = std::max(std::fabs(inL), std::fabs(inR));
      if (osFactor_ == 1 && controls_.truePeak) {
        peak = std::max(peak, tpL_.process(inL));
        peak = std::max(peak, tpR_.process(inR));
      }
      const float target = peak > ceiling_ ? ceiling_ / peak : 1.0f;
      if (target < gain_) {
        gain_ += (target - gain_) * (1.0f - attackCoeff_);
      } else {
        gain_ += (target - gain_) * (1.0f - releaseCoeff_);
      }
      gain_ = std::min(std::max(gain_, 0.0f), 1.0f);
      if (controls_.enabled) {
        grDb_ = std::max(grDb_, -hdsp::gainToDb(std::max(gain_, 1e-9f)));
      }

      delayL_[write_] = inL;
      delayR_[write_] = inR;
      const size_t read = (write_ + delaySize_ - static_cast<size_t>(lookahead_)) %
                          delaySize_;
      float outL = delayL_[read];
      float outR = delayR_[read];
      write_ = (write_ + 1) % static_cast<int>(delaySize_);

      if (controls_.enabled) {
        outL = std::min(std::max(outL * gain_, -ceiling_), ceiling_);
        outR = std::min(std::max(outR * gain_, -ceiling_), ceiling_);
      }
      l[i] = outL;
      r[i] = outR;
    }
  }

  inline void applyPad(float* l, float* r, int n) noexcept {
    if (padDelay_ <= 0) {
      return;
    }
    const size_t size = padL_.size();
    for (int i = 0; i < n; ++i) {
      padL_[padWrite_] = l[i];
      padR_[padWrite_] = r[i];
      const size_t read =
          (padWrite_ + size - static_cast<size_t>(padDelay_)) % size;
      l[i] = padL_[read];
      r[i] = padR_[read];
      padWrite_ = (padWrite_ + 1) % static_cast<int>(size);
    }
  }

  float rate_ = 48000.0f;
  LimiterControls controls_;
  hdsp::Oversampler os_;
  int osSetting_ = -1;
  int osFactor_ = 4;
  int baseLookahead_ = 240;
  int lookahead_ = 960;
  size_t delaySize_ = 961;
  int padDelay_ = 0;
  std::vector<float> delayL_;
  std::vector<float> delayR_;
  std::vector<float> osL_;
  std::vector<float> osR_;
  std::vector<float> padL_;
  std::vector<float> padR_;
  int write_ = 0;
  int padWrite_ = 0;
  float ceiling_ = 0.97f;
  float gain_ = 1.0f;
  float grDb_ = 0.0f;
  float attackCoeff_ = 0.0f;
  float releaseCoeff_ = 0.0f;
  hdsp::Smoothed inputSmooth_;
  hdsp::TruePeakDetector tpL_;
  hdsp::TruePeakDetector tpR_;
};

} // namespace mastering
