// Mastering Suite — Patina colour stage
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>

#include <hdsp/hdsp.hpp>

#include "dsp/controls.hpp"
#include "params.hpp"

namespace mastering {

// Static transfer for the colour display.
inline float colorShape(int model, float drive, float character, float x) {
  const float k = 1.0f + 5.0f * clamp01(drive);
  float bias = 0.10f + 0.20f * clamp01(character);
  switch (static_cast<ColorModel>(model)) {
    case ColorModel::Console:
      bias = 0.04f + 0.10f * character;
      break;
    case ColorModel::Bus:
      bias = 0.02f + 0.08f * character;
      break;
    case ColorModel::Tape:
      bias = 0.06f + 0.12f * character;
      break;
    case ColorModel::Transformer:
      bias = 0.14f + 0.22f * character;
      break;
    default:
      break;
  }
  const float tb = std::tanh(bias);
  const float norm = std::max(k * (1.0f - tb * tb), 1.0e-6f);
  return (std::tanh(k * x + bias) - tb) / norm;
}

// Single-band extraction of Patina's model core: drive, model shaping, tone,
// mix and output level.  No crossovers or band-count UI live here.
class ColorStage {
public:
  void prepare(float sampleRate) noexcept {
    rate_ = sampleRate > 0.0f ? sampleRate : 48000.0f;
    driveSmooth_.setTime(15.0f, rate_);
    mixSmooth_.setTime(10.0f, rate_);
    levelSmooth_.setTime(15.0f, rate_);
    enableSmooth_.setTime(8.0f, rate_);
    reset();
    setFilters();
  }

  void setControls(const ColorControls& c) noexcept {
    controls_ = c;
    model_ = std::min(std::max(c.model, 0), static_cast<int>(ColorModel::Count) - 1);
    driveSmooth_.setTarget(hdsp::dbToGain(-12.0f + 24.0f * clamp01(c.drive)));
    mixSmooth_.setTarget(clamp01(c.mix));
    levelSmooth_.setTarget(colorLevelGain(c.level));
    enableSmooth_.setTarget(c.enabled ? 1.0f : 0.0f);
    setFilters();
  }

  void snap() noexcept {
    driveSmooth_.snap();
    mixSmooth_.snap();
    levelSmooth_.snap();
    enableSmooth_.snap();
  }

  void reset() noexcept {
    for (int ch = 0; ch < 2; ++ch) {
      pre_[ch].reset();
      tone_[ch].reset();
      lp_[ch].reset();
      hp_[ch].reset();
      shaper_[ch].reset();
    }
  }

  inline void process(float& left, float& right) noexcept {
    const float drive = driveSmooth_.next();
    const float mix = mixSmooth_.next();
    const float level = levelSmooth_.next();
    const float gate = enableSmooth_.next();

    const float dryL = left;
    const float dryR = right;
    const float k = 1.0f + 5.0f * clamp01(controls_.drive);
    const float trim = hdsp::dbToGain(kModelTrimDb[model_]);

    float xL = pre_[0].process(dryL * drive);
    float xR = pre_[1].process(dryR * drive);
    xL = shaper_[0].process(xL, k, biasFor(0));
    xR = shaper_[1].process(xR, k, biasFor(1));
    xL = tone_[0].process(xL);
    xR = tone_[1].process(xR);
    if (model_ == static_cast<int>(ColorModel::Tape)) {
      xL = lp_[0].process(xL);
      xR = lp_[1].process(xR);
    }
    xL = hp_[0].process(xL);
    xR = hp_[1].process(xR);
    xL *= trim;
    xR *= trim;

    const float wetL = (dryL + mix * (xL - dryL)) * level;
    const float wetR = (dryR + mix * (xR - dryR)) * level;
    left = dryL + gate * (wetL - dryL);
    right = dryR + gate * (wetR - dryR);
  }

private:
  void setFilters() noexcept {
    const float tone = clamp01(controls_.tone);
    const float character = clamp01(controls_.character);
    const float high = (tone - 0.5f) * 12.0f;
    const float low = (0.5f - tone) * 8.0f;
    for (int ch = 0; ch < 2; ++ch) {
      float preHz = 160.0f;
      float preDb = low * 0.35f;
      switch (static_cast<ColorModel>(model_)) {
        case ColorModel::Console:
          preHz = 150.0f;
          preDb += 1.5f * character;
          break;
        case ColorModel::Bus:
          preHz = 120.0f;
          preDb += 1.0f * character;
          break;
        case ColorModel::Tape:
          preHz = 90.0f;
          preDb += 4.0f * character;
          break;
        case ColorModel::Transformer:
          preHz = 60.0f;
          preDb += 6.0f * character;
          break;
        default:
          preDb += 1.2f * character;
          break;
      }
      pre_[ch].lowShelf(preHz, preDb, 0.7f, rate_);
      tone_[ch].highShelf(3500.0f + 1500.0f * tone, high, 0.7f, rate_);
      if (model_ == static_cast<int>(ColorModel::Tape)) {
        lp_[ch].lowPass(12000.0f + 6000.0f * tone, 0.7071f, rate_);
      } else {
        lp_[ch].lowPass(21000.0f, 0.7071f, rate_);
      }
      hp_[ch].highPass(model_ == static_cast<int>(ColorModel::Transformer) ? 22.0f
                                                                          : 10.0f,
                       0.7071f, rate_);
    }
  }

  float biasFor(int ch) const noexcept {
    const float character = clamp01(controls_.character);
    float bias = 0.10f + 0.20f * character;
    switch (static_cast<ColorModel>(model_)) {
      case ColorModel::Console:
        bias = 0.04f + 0.10f * character +
               (ch == 0 ? 0.02f : -0.02f) * character;
        break;
      case ColorModel::Bus:
        bias = 0.02f + 0.08f * character;
        break;
      case ColorModel::Tape:
        bias = 0.06f + 0.12f * character;
        break;
      case ColorModel::Transformer:
        bias = 0.14f + 0.22f * character;
        break;
      default:
        break;
    }
    return bias;
  }

  static constexpr float kModelTrimDb[5] = {0.0f, -1.0f, -1.5f, -1.0f,
                                            -1.0f};

  float rate_ = 48000.0f;
  int model_ = 0;
  ColorControls controls_;
  hdsp::Smoothed driveSmooth_;
  hdsp::Smoothed mixSmooth_;
  hdsp::Smoothed levelSmooth_;
  hdsp::Smoothed enableSmooth_;
  hdsp::Biquad pre_[2];
  hdsp::Biquad tone_[2];
  hdsp::Biquad lp_[2];
  hdsp::Biquad hp_[2];
  hdsp::AsymShaper shaper_[2];
};

} // namespace mastering
