// Mastering Suite — DSP controls
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

namespace mastering {

enum class ColorModel : int {
  Valve = 0,
  Console = 1,
  Bus = 2,
  Tape = 3,
  Transformer = 4,
  Count = 5,
};

constexpr int kSpectrumBins = 128;
constexpr int kModuleCount = 5;

struct EqControls {
  bool enabled = true;
  float lowGain = 0.5f;
  float lowFreq = 0.35f;
  float midGain = 0.5f;
  float midFreq = 0.5f;
  float midQ = 0.35f;
  float highGain = 0.5f;
  float highFreq = 0.65f;
  float hpf = 0.2f;
};

struct CompControls {
  bool enabled = false;
  float threshold = 0.5f;
  float ratio = 1.0f;
  float attack = 0.35f;
  float release = 0.5f;
  float knee = 0.3f;
  float scHpf = 0.2f;
  float mix = 1.0f;
  bool autoGain = false;
};

struct ColorControls {
  bool enabled = false;
  int model = 0;
  float drive = 0.45f;
  float character = 0.5f;
  float tone = 0.5f;
  float mix = 1.0f;
  float level = 0.5f;
  int clip = 0; // Off / Soft / Hard
};

struct DensityControls {
  bool enabled = false;
  float input = 0.5f;
  float effect = 0.3f;
  float output = 0.5f;
};

struct LimiterControls {
  bool enabled = true;
  float ceiling = 0.95f;
  float release = 0.5f;
  bool truePeak = true;
};

struct Controls {
  bool enabled = true;
  float input = 0.5f;
  float output = 0.5f;
  EqControls eq;
  CompControls comp;
  ColorControls color;
  DensityControls density;
  LimiterControls limiter;
};

struct Meters {
  float inPeakL = -120.0f;
  float inPeakR = -120.0f;
  float outPeakL = -120.0f;
  float outPeakR = -120.0f;
  float truePeakL = -120.0f;
  float truePeakR = -120.0f;
  float lufsIntegrated = -120.0f;
  float lufsShortTerm = -120.0f;
  float lufsMomentary = -120.0f;
  float lra = 0.0f;
  float correlation = 0.0f;
  float compGr = 0.0f;
  float limitGr = 0.0f;
  float spectrum[kSpectrumBins] = {};
};

} // namespace mastering
