// Mastering Suite — fixed-order chain and metering
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include <ebur128.h>
#include <hdsp/hdsp.hpp>

#include "dsp/color.hpp"
#include "dsp/compressor.hpp"
#include "dsp/controls.hpp"
#include "dsp/density.hpp"
#include "dsp/eq.hpp"
#include "dsp/limiter.hpp"
#include "params.hpp"

namespace mastering {

class MasteringEngine {
public:
  MasteringEngine() = default;
  MasteringEngine(const MasteringEngine&) = delete;
  MasteringEngine& operator=(const MasteringEngine&) = delete;
  ~MasteringEngine() {
    if (loudness_) {
      ebur128_destroy(&loudness_);
    }
  }

  void prepare(float sampleRate) {
    baseRate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    if (loudness_) {
      ebur128_destroy(&loudness_);
      loudness_ = nullptr;
    }
    loudness_ = ebur128_init(
        2, static_cast<unsigned long>(baseRate_ + 0.5f),
        EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA);
    eq_.prepare(baseRate_);
    comp_.prepare(baseRate_);
    color_.prepare(baseRate_);
    density_.prepare(baseRate_);
    limiter_.prepare(baseRate_);
    spectrum_.prepare(baseRate_);
    inputSmooth_.setTime(15.0f, baseRate_);
    outputSmooth_.setTime(15.0f, baseRate_);
    interleave_.assign(2 * 4096, 0.0f);
    clearOutputState();
    reset();
    setControls(controls_);
    inputSmooth_.snap();
    outputSmooth_.snap();
    color_.snap();
    density_.snap();
  }

  void setControls(const Controls& c) noexcept {
    controls_ = c;
    eq_.setControls(controls_.eq);
    comp_.setControls(controls_.comp);
    color_.setControls(controls_.color);
    density_.setControls(controls_.density);
    inputSmooth_.setTarget(hdsp::dbToGain(densityDb(controls_.input)));
    outputSmooth_.setTarget(hdsp::dbToGain(densityDb(controls_.output)));
  }

  int latencySamples() const noexcept { return limiter_.latencySamples(); }

  const float* spectrumBins() const noexcept { return spectrum_.bins(); }

  // Clears the loudness histogram and meter state.  Called on an explicit
  // user reset, so the small libebur128 re-allocation is acceptable; the
  // per-block audio path never allocates.
  void resetMeters() {
    if (loudness_) {
      ebur128_destroy(&loudness_);
      loudness_ = nullptr;
    }
    loudness_ = ebur128_init(
        2, static_cast<unsigned long>(baseRate_ + 0.5f),
        EBUR128_MODE_M | EBUR128_MODE_S | EBUR128_MODE_I | EBUR128_MODE_LRA);
    meterTpL_.reset();
    meterTpR_.reset();
    spectrum_.reset();
    corrState_ = 0.0f;
    meterCounter_ = 0;
    truePeakMax_ = -120.0f;
    samplePeakMax_ = -120.0f;
    shortTermMax_ = -120.0f;
  }

  void reset() noexcept {
    clearOutputState();
    eq_.reset();
    comp_.reset();
    color_.reset();
    density_.reset();
    limiter_.reset();
    spectrum_.reset();
    meterTpL_.reset();
    meterTpR_.reset();
    inputSmooth_.snap();
    outputSmooth_.snap();
    corrState_ = 0.0f;
    meterCounter_ = 0;
    truePeakMax_ = -120.0f;
    samplePeakMax_ = -120.0f;
    shortTermMax_ = -120.0f;
  }

  void process(float* left, float* right, int frames, Meters& meters) noexcept {
    if (frames <= 0) {
      return;
    }
    LimiterControls lim = controls_.limiter;
    if (!controls_.enabled) {
      lim.enabled = false;
    }
    limiter_.setControls(lim);

    const int dryStart = dryWrite_;
    const int latency = std::min(limiter_.latencySamples(), kMaxDelay - 1);

    float inPeakL = 0.0f;
    float inPeakR = 0.0f;
    float outPeakL = 0.0f;
    float outPeakR = 0.0f;
    float tpL = 0.0f;
    float tpR = 0.0f;
    float compGr = 0.0f;
    float limitGr = 0.0f;
    double sumLR = 0.0;
    double sumL2 = 0.0;
    double sumR2 = 0.0;

    // EQ, compressor, colour and density are per-sample; the limiter is a
    // block stage so it can run at its oversampled rate.
    for (int i = 0; i < frames; ++i) {
      const float rawL = std::isfinite(left[i]) ? left[i] : 0.0f;
      const float rawR = std::isfinite(right[i]) ? right[i] : 0.0f;
      const float dryL = std::min(std::max(rawL, -32.0f), 32.0f);
      const float dryR = std::min(std::max(rawR, -32.0f), 32.0f);
      inPeakL = std::max(inPeakL, std::fabs(dryL));
      inPeakR = std::max(inPeakR, std::fabs(dryR));

      float l = dryL;
      float r = dryR;
      if (controls_.enabled) {
        const float in = inputSmooth_.next();
        l *= in;
        r *= in;
      }
      dryL_[dryWrite_] = l;
      dryR_[dryWrite_] = r;
      dryWrite_ = (dryWrite_ + 1) % kMaxDelay;
      if (controls_.enabled) {
        eq_.process(l, r);
        compGr = std::max(compGr, comp_.process(l, r));
        color_.process(l, r);
        density_.process(l, r);
      }
      left[i] = l;
      right[i] = r;
    }

    limiter_.process(left, right, frames);
    limitGr = limiter_.gainReductionDb();

    for (int i = 0; i < frames; ++i) {
      float l = left[i];
      float r = right[i];
      float out = 1.0f;
      if (controls_.enabled) {
        out = outputSmooth_.next();
        l *= out;
        r *= out;
      }

      // Width and monitoring matrix at the very end of the chain.
      const float mid = 0.5f * (l + r);
      const float side = 0.5f * (l - r) * controls_.width;
      switch (controls_.msMode) {
        case 1:  // M/S encode: mid on the left, side on the right
          l = mid;
          r = side;
          break;
        case 2:  // mid solo
          l = r = mid;
          break;
        case 3:  // side solo
          l = r = side;
          break;
        default:
          l = mid + side;
          r = mid - side;
          break;
      }

      if (controls_.delta) {
        const int dryIndex =
            ((dryStart + i - latency) % kMaxDelay + kMaxDelay) % kMaxDelay;
        l -= dryL_[dryIndex] * out;
        r -= dryR_[dryIndex] * out;
      }

      l = ditherSample(l, 0);
      r = ditherSample(r, 1);

      const float yL = std::isfinite(l) ? l : 0.0f;
      const float yR = std::isfinite(r) ? r : 0.0f;
      left[i] = yL;
      right[i] = yR;
      outPeakL = std::max(outPeakL, std::fabs(yL));
      outPeakR = std::max(outPeakR, std::fabs(yR));
      tpL = std::max(tpL, meterTpL_.process(yL));
      tpR = std::max(tpR, meterTpR_.process(yR));
      sumLR += static_cast<double>(yL) * yR;
      sumL2 += static_cast<double>(yL) * yL;
      sumR2 += static_cast<double>(yR) * yR;
    }

    const auto meterDb = [](float peak) {
      return std::max(hdsp::gainToDb(peak), -120.0f);
    };
    meters.inPeakL = meterDb(inPeakL);
    meters.inPeakR = meterDb(inPeakR);
    meters.outPeakL = meterDb(outPeakL);
    meters.outPeakR = meterDb(outPeakR);
    meters.truePeakL = meterDb(tpL);
    meters.truePeakR = meterDb(tpR);
    meters.compGr = compGr;
    meters.limitGr = limitGr;
    const double denom = std::sqrt(sumL2 * sumR2);
    const float corr = denom > 1e-20
                           ? static_cast<float>(sumLR / denom)
                           : 0.0f;
    corrState_ += (corr - corrState_) * 0.25f;
    meters.correlation = corrState_;

    spectrum_.push(left, right, frames);

    if (loudness_) {
      for (int start = 0; start < frames; start += 4096) {
        const int n = std::min(4096, frames - start);
        for (int i = 0; i < n; ++i) {
          interleave_[static_cast<size_t>(2 * i)] = left[start + i];
          interleave_[static_cast<size_t>(2 * i + 1)] = right[start + i];
        }
        ebur128_add_frames_float(loudness_, interleave_.data(),
                                 static_cast<size_t>(n));
      }
      double momentary = -120.0;
      double shortTerm = -120.0;
      if (ebur128_loudness_momentary(loudness_, &momentary) != EBUR128_SUCCESS ||
          !std::isfinite(momentary)) {
        momentary = -120.0;
      }
      if (ebur128_loudness_shortterm(loudness_, &shortTerm) != EBUR128_SUCCESS ||
          !std::isfinite(shortTerm)) {
        shortTerm = -120.0;
      }
      meters.lufsMomentary = static_cast<float>(momentary);
      meters.lufsShortTerm = static_cast<float>(shortTerm);
      if ((meterCounter_++ % 8) == 0) {
        double integrated = -120.0;
        double lra = 0.0;
        if (ebur128_loudness_global(loudness_, &integrated) != EBUR128_SUCCESS ||
            !std::isfinite(integrated)) {
          integrated = -120.0;
        }
        if (ebur128_loudness_range(loudness_, &lra) != EBUR128_SUCCESS ||
            !std::isfinite(lra)) {
          lra = 0.0;
        }
        meters.lufsIntegrated = static_cast<float>(integrated);
        meters.lra = static_cast<float>(std::max(lra, 0.0));
      }
    }

    truePeakMax_ =
        std::max(truePeakMax_, std::max(meters.truePeakL, meters.truePeakR));
    samplePeakMax_ =
        std::max(samplePeakMax_, std::max(meters.outPeakL, meters.outPeakR));
    if (meters.lufsShortTerm > -100.0f) {
      shortTermMax_ = std::max(shortTermMax_, meters.lufsShortTerm);
    }
    meters.truePeakMax = truePeakMax_;
    meters.samplePeakMax = samplePeakMax_;
    meters.shortTermMax = shortTermMax_;
    meters.plr = (meters.lufsIntegrated > -100.0f && truePeakMax_ > -100.0f)
                     ? std::max(truePeakMax_ - meters.lufsIntegrated, 0.0f)
                     : 0.0f;
    meters.psr = (shortTermMax_ > -100.0f && truePeakMax_ > -100.0f)
                     ? std::max(truePeakMax_ - shortTermMax_, 0.0f)
                     : 0.0f;
    meters.targetLufs = loudnessTargetFor(controls_.targetIndex);
    meters.targetDelta =
        meters.lufsIntegrated > -100.0f
            ? meters.lufsIntegrated - meters.targetLufs
            : 0.0f;
  }

private:
  static constexpr int kMaxDelay = 8192;

  void clearOutputState() noexcept {
    for (int i = 0; i < kMaxDelay; ++i) {
      dryL_[i] = 0.0f;
      dryR_[i] = 0.0f;
    }
    dryWrite_ = 0;
    rngState_ = 0x9e3779b9u;
    shapeErr_[0] = shapeErr_[1] = 0.0f;
  }

  float rng01() noexcept {
    rngState_ ^= rngState_ << 13;
    rngState_ ^= rngState_ >> 17;
    rngState_ ^= rngState_ << 5;
    return static_cast<float>(rngState_ >> 8) * (1.0f / 16777216.0f);
  }

  float ditherSample(float x, int ch) noexcept {
    const int bits = controls_.ditherBits;
    if (bits <= 0) {
      shapeErr_[ch] = 0.0f;
      return x;
    }
    const float step = 1.0f / static_cast<float>(1u << (bits - 1));
    const float feedback = controls_.ditherShape == 1 ? shapeErr_[ch] : 0.0f;
    const float tpdf = (rng01() + rng01() - 1.0f) * step;
    const float target = x + feedback + tpdf;
    const float q = std::round(target / step) * step;
    if (controls_.ditherShape == 1) {
      shapeErr_[ch] = (q - (x + feedback)) * 0.5f;
    } else {
      shapeErr_[ch] = 0.0f;
    }
    return std::min(std::max(q, -1.0f), 1.0f);
  }

  float baseRate_ = 48000.0f;
  Controls controls_;
  MasteringEq eq_;
  BusCompressor comp_;
  ColorStage color_;
  DensityStage density_;
  LookaheadLimiter limiter_;
  hdsp::SpectrumAnalyzer spectrum_;
  hdsp::TruePeakDetector meterTpL_;
  hdsp::TruePeakDetector meterTpR_;
  hdsp::Smoothed inputSmooth_;
  hdsp::Smoothed outputSmooth_;
  ebur128_state* loudness_ = nullptr;
  std::vector<float> interleave_;
  float dryL_[kMaxDelay] = {};
  float dryR_[kMaxDelay] = {};
  int dryWrite_ = 0;
  uint32_t rngState_ = 0x9e3779b9u;
  float shapeErr_[2] = {0.0f, 0.0f};
  float corrState_ = 0.0f;
  float truePeakMax_ = -120.0f;
  float samplePeakMax_ = -120.0f;
  float shortTermMax_ = -120.0f;
  int meterCounter_ = 0;
};

} // namespace mastering
