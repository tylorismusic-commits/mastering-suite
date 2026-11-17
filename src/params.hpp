// Mastering Suite — parameters
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

#include "dsp/controls.hpp"

namespace mastering {

enum ParamId : uint32_t {
  P_ENABLED = 0,
  P_INPUT,
  P_OUTPUT,
  P_METER,
  P_MODULE,
  P_EQ_ON,
  P_EQ_LOW_GAIN,
  P_EQ_LOW_FREQ,
  P_EQ_MID_GAIN,
  P_EQ_MID_FREQ,
  P_EQ_MID_Q,
  P_EQ_HIGH_GAIN,
  P_EQ_HIGH_FREQ,
  P_EQ_HPF,
  P_COMP_ON,
  P_COMP_THRESHOLD,
  P_COMP_RATIO,
  P_COMP_ATTACK,
  P_COMP_RELEASE,
  P_COMP_KNEE,
  P_COMP_SC_HPF,
  P_COMP_MIX,
  P_COMP_AUTO,
  P_COLOR_ON,
  P_COLOR_MODEL,
  P_COLOR_DRIVE,
  P_COLOR_CHARACTER,
  P_COLOR_TONE,
  P_COLOR_MIX,
  P_COLOR_LEVEL,
  P_COLOR_CLIP,
  P_DENS_ON,
  P_DENS_INPUT,
  P_DENS_EFFECT,
  P_DENS_OUTPUT,
  P_LIMIT_ON,
  P_LIMIT_CEILING,
  P_LIMIT_RELEASE,
  P_LIMIT_TRUE_PEAK,
  P_LIMIT_INPUT,
  P_LIMIT_OS,
  P_EQ_TILT,
  P_COUNT
};

struct Labels {
  const char* const* names = nullptr;
  int count = 0;
};

template <size_t N>
constexpr Labels labels(const char* const (&arr)[N]) noexcept {
  return Labels{arr, static_cast<int>(N)};
}

struct ParamSpec {
  const char* symbol;
  const char* name;
  const char* module;
  double min;
  double max;
  double def;
  bool stepped;
  bool hidden;
  bool toggled;
  Labels labels;
};

inline constexpr const char* kModelLabels[] = {"Valve", "Console", "Bus", "Tape",
                                               "Transformer"};
inline constexpr const char* kClipLabels[] = {"Off", "Soft", "Hard"};
inline constexpr const char* kRatioLabels[] = {"2:1", "4:1", "10:1"};
inline constexpr const char* kOversampleLabels[] = {"Off", "2x", "4x"};
inline constexpr const char* kMeterLabels[] = {"Spectrum", "Loudness", "GR",
                                               "Peaks"};
inline constexpr const char* kModuleLabels[] = {"EQ", "Comp", "Color", "Density",
                                                "Limit"};

inline constexpr ParamSpec kParamSpecs[P_COUNT] = {
    {"power", "Power", "Global", 0.0, 1.0, 1.0, true, false, true, {}},
    {"input", "Input", "Global", 0.0, 1.0, 0.5, false, false, false, {}},
    {"output", "Output", "Global", 0.0, 1.0, 0.5, false, false, false, {}},
    {"meter", "Meter", "Global", 0.0, 3.0, 0.0, true, true, false,
     labels(kMeterLabels)},
    {"module", "Module", "Global", 0.0, 4.0, 0.0, true, true, false,
     labels(kModuleLabels)},

    {"eq_on", "EQ On", "EQ", 0.0, 1.0, 1.0, true, false, true, {}},
    {"eq_low_gain", "Low Gain", "EQ", 0.0, 1.0, 0.5, false, false, false, {}},
    {"eq_low_freq", "Low Freq", "EQ", 0.0, 1.0, 0.35, false, false, false, {}},
    {"eq_mid_gain", "Mid Gain", "EQ", 0.0, 1.0, 0.5, false, false, false, {}},
    {"eq_mid_freq", "Mid Freq", "EQ", 0.0, 1.0, 0.5, false, false, false, {}},
    {"eq_mid_q", "Mid Q", "EQ", 0.0, 1.0, 0.35, false, false, false, {}},
    {"eq_high_gain", "High Gain", "EQ", 0.0, 1.0, 0.5, false, false, false, {}},
    {"eq_high_freq", "High Freq", "EQ", 0.0, 1.0, 0.65, false, false, false, {}},
    {"eq_hpf", "HPF", "EQ", 0.0, 1.0, 0.2, false, false, false, {}},

    {"comp_on", "Comp On", "Compressor", 0.0, 1.0, 0.0, true, false, true, {}},
    {"comp_threshold", "Threshold", "Compressor", 0.0, 1.0, 0.5, false, false,
     false, {}},
    {"comp_ratio", "Ratio", "Compressor", 0.0, 2.0, 1.0, true, false, false,
     labels(kRatioLabels)},
    {"comp_attack", "Attack", "Compressor", 0.0, 1.0, 0.35, false, false, false,
     {}},
    {"comp_release", "Release", "Compressor", 0.0, 1.0, 0.5, false, false,
     false, {}},
    {"comp_knee", "Knee", "Compressor", 0.0, 1.0, 0.3, false, false, false, {}},
    {"comp_sc_hpf", "SC HPF", "Compressor", 0.0, 1.0, 0.2, false, false, false,
     {}},
    {"comp_mix", "Mix", "Compressor", 0.0, 1.0, 1.0, false, false, false, {}},
    {"comp_auto", "Auto Gain", "Compressor", 0.0, 1.0, 0.0, true, false, true,
     {}},

    {"color_on", "Color On", "Color", 0.0, 1.0, 0.0, true, false, true, {}},
    {"color_model", "Model", "Color", 0.0, 4.0, 0.0, true, false, false,
     labels(kModelLabels)},
    {"color_drive", "Drive", "Color", 0.0, 1.0, 0.45, false, false, false, {}},
    {"color_character", "Character", "Color", 0.0, 1.0, 0.5, false, false,
     false, {}},
    {"color_tone", "Tone", "Color", 0.0, 1.0, 0.5, false, false, false, {}},
    {"color_mix", "Mix", "Color", 0.0, 1.0, 1.0, false, false, false, {}},
    {"color_level", "Level", "Color", 0.0, 1.0, 0.5, false, false, false, {}},
    {"color_clip", "Clip", "Color", 0.0, 2.0, 0.0, true, false, false,
     labels(kClipLabels)},

    {"density_on", "Density On", "Density", 0.0, 1.0, 0.0, true, false, true,
     {}},
    {"density_input", "Input", "Density", 0.0, 1.0, 0.5, false, false, false,
     {}},
    {"density_effect", "Density", "Density", 0.0, 1.0, 0.3, false, false, false,
     {}},
    {"density_output", "Output", "Density", 0.0, 1.0, 0.5, false, false, false,
     {}},

    {"limiter_on", "Limiter On", "Limiter", 0.0, 1.0, 1.0, true, false, true,
     {}},
    {"limiter_ceiling", "Ceiling", "Limiter", 0.0, 1.0, 0.95, false, false,
     false, {}},
    {"limiter_release", "Release", "Limiter", 0.0, 1.0, 0.5, false, false,
     false, {}},
    {"limiter_true_peak", "True Peak", "Limiter", 0.0, 1.0, 1.0, true, false,
     true, {}},
    {"limiter_input", "Input Gain", "Limiter", 0.0, 1.0, 0.5, false, false,
     false, {}},
    {"limiter_os", "Oversampling", "Limiter", 0.0, 2.0, 2.0, true, false,
     false, labels(kOversampleLabels)},
    {"eq_tilt", "Tilt", "EQ", 0.0, 1.0, 0.5, false, false, false, {}},
};

static_assert(sizeof(kParamSpecs) / sizeof(kParamSpecs[0]) == P_COUNT,
              "kParamSpecs must cover every parameter id");

// ---------------------------------------------------------------------------
// Mappings shared by the DSP, the host text and the panel.
// ---------------------------------------------------------------------------

inline float clamp01(float v) noexcept {
  return std::min(std::max(v, 0.0f), 1.0f);
}

inline float lowGainDb(float v) noexcept { return -12.0f + 24.0f * clamp01(v); }
inline float midGainDb(float v) noexcept { return -12.0f + 24.0f * clamp01(v); }
inline float highGainDb(float v) noexcept { return -12.0f + 24.0f * clamp01(v); }
inline float lowFreqHz(float v) noexcept {
  return 40.0f * std::pow(7.5f, clamp01(v));
}
inline float midFreqHz(float v) noexcept {
  return 200.0f * std::pow(25.0f, clamp01(v));
}
inline float highFreqHz(float v) noexcept {
  return 2000.0f * std::pow(4.0f, clamp01(v));
}
inline float midQ(float v) noexcept { return 0.3f + 1.2f * clamp01(v); }
inline float hpfHz(float v) noexcept {
  return 20.0f * std::pow(6.0f, clamp01(v));
}
inline float eqTiltDb(float v) noexcept {
  return -6.0f + 12.0f * clamp01(v);
}
inline float compThresholdDb(float v) noexcept {
  return -40.0f + 40.0f * clamp01(v);
}
inline float compRatio(float v) noexcept {
  static constexpr float kRatios[3] = {2.0f, 4.0f, 10.0f};
  const int i = std::min(std::max(static_cast<int>(v + 0.5f), 0), 2);
  return kRatios[i];
}
inline float compAttackMs(float v) noexcept {
  return 1.0f * std::pow(100.0f, clamp01(v));
}
inline float compReleaseMs(float v) noexcept {
  return 50.0f * std::pow(20.0f, clamp01(v));
}
inline float compKneeDb(float v) noexcept { return 12.0f * clamp01(v); }
inline float compScHpfHz(float v) noexcept {
  return 20.0f * std::pow(10.0f, clamp01(v));
}
inline float densityDb(float v) noexcept {
  return -12.0f + 24.0f * clamp01(v);
}
inline float limiterInputDb(float v) noexcept {
  return -12.0f + 24.0f * clamp01(v);
}
inline float limiterCeilingDb(float v) noexcept {
  return -6.0f + 6.0f * clamp01(v);
}
inline float limiterReleaseMs(float v) noexcept {
  return 20.0f * std::pow(50.0f, clamp01(v));
}
inline float colorLevelGain(float v) noexcept {
  v = clamp01(v);
  if (v <= 0.002f) {
    return 0.0f;
  }
  const float db = v <= 0.5f ? -60.0f + 120.0f * v : 24.0f * (v - 0.5f);
  return std::pow(10.0f, db * 0.05f);
}

class Shared {
public:
  Shared() {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      values_[p].store(kParamSpecs[p].def, std::memory_order_relaxed);
      dirty_[p].store(false, std::memory_order_relaxed);
    }
    inPeakL.store(-120.0f);
    inPeakR.store(-120.0f);
    outPeakL.store(-120.0f);
    outPeakR.store(-120.0f);
    truePeakL.store(-120.0f);
    truePeakR.store(-120.0f);
    lufsIntegrated.store(-120.0f);
    lufsShortTerm.store(-120.0f);
    lufsMomentary.store(-120.0f);
    lra.store(0.0f);
    correlation.store(0.0f);
    compGr.store(0.0f);
    limitGr.store(0.0f);
    truePeakMax.store(-120.0f);
    samplePeakMax.store(-120.0f);
    shortTermMax.store(-120.0f);
    plr.store(0.0f);
    psr.store(0.0f);
    for (int i = 0; i < kSpectrumBins; ++i) {
      spectrum[i].store(-120.0f);
    }
  }

  double get(uint32_t p) const noexcept {
    return values_[p].load(std::memory_order_relaxed);
  }

  void set(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
    dirty_[p].store(true, std::memory_order_release);
  }

  void setFromHost(uint32_t p, double v) noexcept {
    if (!std::isfinite(v)) {
      return;
    }
    const ParamSpec& s = kParamSpecs[p];
    v = std::min(std::max(v, s.min), s.max);
    if (s.stepped) {
      v = static_cast<double>(static_cast<int>(v + (v >= 0 ? 0.5 : -0.5)));
    }
    values_[p].store(v, std::memory_order_relaxed);
  }

  bool takeDirty(uint32_t p) noexcept {
    return dirty_[p].exchange(false, std::memory_order_acquire);
  }

  bool isDirty(uint32_t p) const noexcept {
    return dirty_[p].load(std::memory_order_acquire);
  }

  bool anyDirty() const noexcept {
    for (uint32_t p = 0; p < P_COUNT; ++p) {
      if (dirty_[p].load(std::memory_order_acquire)) return true;
    }
    return false;
  }

  void requestMeterReset() noexcept {
    meterReset.store(true, std::memory_order_relaxed);
  }

  bool takeMeterReset() noexcept {
    return meterReset.exchange(false, std::memory_order_acquire);
  }

  std::atomic<bool> meterReset{false};
  std::atomic<float> sampleRate{48000.0f};

  std::atomic<float> inPeakL;
  std::atomic<float> inPeakR;
  std::atomic<float> outPeakL;
  std::atomic<float> outPeakR;
  std::atomic<float> truePeakL;
  std::atomic<float> truePeakR;
  std::atomic<float> lufsIntegrated;
  std::atomic<float> lufsShortTerm;
  std::atomic<float> lufsMomentary;
  std::atomic<float> lra;
  std::atomic<float> correlation;
  std::atomic<float> compGr;
  std::atomic<float> limitGr;
  std::atomic<float> truePeakMax;
  std::atomic<float> samplePeakMax;
  std::atomic<float> shortTermMax;
  std::atomic<float> plr;
  std::atomic<float> psr;
  std::atomic<float> spectrum[kSpectrumBins];

private:
  std::atomic<double> values_[P_COUNT];
  std::atomic<bool> dirty_[P_COUNT];
};

inline Controls controlsFrom(const Shared& s) noexcept {
  Controls c;
  c.enabled = s.get(P_ENABLED) > 0.5;
  c.input = static_cast<float>(s.get(P_INPUT));
  c.output = static_cast<float>(s.get(P_OUTPUT));

  c.eq.enabled = s.get(P_EQ_ON) > 0.5;
  c.eq.lowGain = static_cast<float>(s.get(P_EQ_LOW_GAIN));
  c.eq.lowFreq = static_cast<float>(s.get(P_EQ_LOW_FREQ));
  c.eq.midGain = static_cast<float>(s.get(P_EQ_MID_GAIN));
  c.eq.midFreq = static_cast<float>(s.get(P_EQ_MID_FREQ));
  c.eq.midQ = static_cast<float>(s.get(P_EQ_MID_Q));
  c.eq.highGain = static_cast<float>(s.get(P_EQ_HIGH_GAIN));
  c.eq.highFreq = static_cast<float>(s.get(P_EQ_HIGH_FREQ));
  c.eq.hpf = static_cast<float>(s.get(P_EQ_HPF));
  c.eq.tilt = static_cast<float>(s.get(P_EQ_TILT));

  c.comp.enabled = s.get(P_COMP_ON) > 0.5;
  c.comp.threshold = static_cast<float>(s.get(P_COMP_THRESHOLD));
  c.comp.ratio = static_cast<float>(s.get(P_COMP_RATIO));
  c.comp.attack = static_cast<float>(s.get(P_COMP_ATTACK));
  c.comp.release = static_cast<float>(s.get(P_COMP_RELEASE));
  c.comp.knee = static_cast<float>(s.get(P_COMP_KNEE));
  c.comp.scHpf = static_cast<float>(s.get(P_COMP_SC_HPF));
  c.comp.mix = static_cast<float>(s.get(P_COMP_MIX));
  c.comp.autoGain = s.get(P_COMP_AUTO) > 0.5;

  c.color.enabled = s.get(P_COLOR_ON) > 0.5;
  c.color.model = static_cast<int>(s.get(P_COLOR_MODEL) + 0.5);
  c.color.drive = static_cast<float>(s.get(P_COLOR_DRIVE));
  c.color.character = static_cast<float>(s.get(P_COLOR_CHARACTER));
  c.color.tone = static_cast<float>(s.get(P_COLOR_TONE));
  c.color.mix = static_cast<float>(s.get(P_COLOR_MIX));
  c.color.level = static_cast<float>(s.get(P_COLOR_LEVEL));
  c.color.clip = static_cast<int>(s.get(P_COLOR_CLIP) + 0.5);

  c.density.enabled = s.get(P_DENS_ON) > 0.5;
  c.density.input = static_cast<float>(s.get(P_DENS_INPUT));
  c.density.effect = static_cast<float>(s.get(P_DENS_EFFECT));
  c.density.output = static_cast<float>(s.get(P_DENS_OUTPUT));

  c.limiter.enabled = s.get(P_LIMIT_ON) > 0.5;
  c.limiter.inputGain = static_cast<float>(s.get(P_LIMIT_INPUT));
  c.limiter.ceiling = static_cast<float>(s.get(P_LIMIT_CEILING));
  c.limiter.release = static_cast<float>(s.get(P_LIMIT_RELEASE));
  c.limiter.truePeak = s.get(P_LIMIT_TRUE_PEAK) > 0.5;
  c.limiter.oversample = static_cast<int>(s.get(P_LIMIT_OS) + 0.5);
  return c;
}

} // namespace mastering
