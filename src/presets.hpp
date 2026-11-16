// Mastering Suite — factory presets
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "params.hpp"

namespace mastering {

struct PresetValue {
  int16_t param = -1;
  float value = 0.0f;
};

struct Preset {
  const char* name;
  const char* category;
  PresetValue values[20];
};

inline constexpr Preset kPresets[] = {
    {"Clean Master", "Mastering",
     {{P_EQ_ON, 1}, {P_COMP_ON, 0}, {P_COLOR_ON, 0}, {P_DENS_ON, 0},
      {P_LIMIT_ON, 1}, {P_LIMIT_CEILING, 0.95f}, {-1, 0}}},
    {"Gentle Glue", "Mix Bus",
     {{P_EQ_ON, 1}, {P_EQ_HIGH_GAIN, 0.53f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.56f}, {P_COMP_RATIO, 1},
      {P_COMP_ATTACK, 0.42f}, {P_COMP_RELEASE, 0.56f}, {P_COMP_KNEE, 0.4f},
      {P_COMP_MIX, 0.6f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 0}, {P_DENS_ON, 0}, {P_LIMIT_ON, 1},
      {P_LIMIT_CEILING, 0.95f}, {-1, 0}}},
    {"Streaming Ready", "Mastering",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.52f}, {P_EQ_HIGH_GAIN, 0.56f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.6f}, {P_COMP_RATIO, 1},
      {P_COMP_ATTACK, 0.3f}, {P_COMP_RELEASE, 0.5f}, {P_COMP_KNEE, 0.35f},
      {P_COMP_MIX, 0.75f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 1}, {P_COLOR_DRIVE, 0.38f},
      {P_COLOR_CHARACTER, 0.45f}, {P_COLOR_TONE, 0.55f}, {P_COLOR_MIX, 0.55f},
      {P_COLOR_LEVEL, 0.5f}, {P_DENS_ON, 1}, {P_DENS_EFFECT, 0.25f}}},
    {"CD Loud", "Mastering",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.54f}, {P_EQ_HIGH_GAIN, 0.55f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.45f}, {P_COMP_RATIO, 2},
      {P_COMP_ATTACK, 0.25f}, {P_COMP_RELEASE, 0.45f}, {P_COMP_KNEE, 0.3f},
      {P_COMP_MIX, 0.8f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 3}, {P_COLOR_DRIVE, 0.52f},
      {P_COLOR_CHARACTER, 0.5f}, {P_COLOR_TONE, 0.45f}, {P_COLOR_MIX, 1.0f},
      {P_COLOR_LEVEL, 0.49f}, {P_DENS_ON, 1}, {P_DENS_EFFECT, 0.45f}}},
    {"Tape Master", "Character",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.56f}, {P_EQ_HIGH_GAIN, 0.53f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.55f}, {P_COMP_RATIO, 1},
      {P_COMP_ATTACK, 0.4f}, {P_COMP_RELEASE, 0.55f}, {P_COMP_KNEE, 0.4f},
      {P_COMP_MIX, 0.65f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 3}, {P_COLOR_DRIVE, 0.56f},
      {P_COLOR_CHARACTER, 0.5f}, {P_COLOR_TONE, 0.45f}, {P_COLOR_MIX, 1.0f},
      {P_COLOR_LEVEL, 0.49f}, {P_DENS_ON, 0}, {P_DENS_EFFECT, 0.25f}}},
    {"Iron Weight", "Character",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.61f}, {P_EQ_LOW_FREQ, 0.2f},
      {P_EQ_HIGH_GAIN, 0.48f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.52f}, {P_COMP_RATIO, 1},
      {P_COMP_ATTACK, 0.35f}, {P_COMP_RELEASE, 0.55f}, {P_COMP_KNEE, 0.35f},
      {P_COMP_MIX, 0.6f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 4}, {P_COLOR_DRIVE, 0.62f},
      {P_COLOR_CHARACTER, 0.72f}, {P_COLOR_TONE, 0.35f}, {P_COLOR_MIX, 1.0f},
      {P_COLOR_LEVEL, 0.48f}, {P_DENS_ON, 1}}},
    {"Air & Glue", "Mastering",
     {{P_EQ_ON, 1}, {P_EQ_HIGH_GAIN, 0.63f}, {P_EQ_HIGH_FREQ, 0.72f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.58f}, {P_COMP_RATIO, 1},
      {P_COMP_ATTACK, 0.38f}, {P_COMP_RELEASE, 0.52f}, {P_COMP_KNEE, 0.45f},
      {P_COMP_MIX, 0.55f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 1}, {P_COLOR_DRIVE, 0.34f},
      {P_COLOR_CHARACTER, 0.35f}, {P_COLOR_TONE, 0.65f}, {P_COLOR_MIX, 0.45f},
      {P_COLOR_LEVEL, 0.5f}, {P_DENS_ON, 0}, {P_LIMIT_CEILING, 0.95f}}},
    {"Density Push", "Character",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.53f}, {P_EQ_HIGH_GAIN, 0.54f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.5f}, {P_COMP_RATIO, 1},
      {P_COMP_ATTACK, 0.3f}, {P_COMP_RELEASE, 0.5f}, {P_COMP_KNEE, 0.3f},
      {P_COMP_MIX, 0.7f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 2}, {P_COLOR_DRIVE, 0.4f},
      {P_COLOR_CHARACTER, 0.45f}, {P_COLOR_TONE, 0.5f}, {P_COLOR_MIX, 0.5f},
      {P_COLOR_LEVEL, 0.5f}, {P_DENS_ON, 1}, {P_DENS_EFFECT, 0.65f}}},
    {"Transparent Limit", "Mastering",
     {{P_EQ_ON, 0}, {P_COMP_ON, 0}, {P_COLOR_ON, 0}, {P_DENS_ON, 0},
      {P_LIMIT_ON, 1}, {P_LIMIT_CEILING, 0.95f}, {P_LIMIT_TRUE_PEAK, 1},
      {-1, 0}}},
    {"Club Master", "Mastering",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.57f}, {P_EQ_HIGH_GAIN, 0.55f},
      {P_COMP_ON, 1}, {P_COMP_THRESHOLD, 0.42f}, {P_COMP_RATIO, 2},
      {P_COMP_ATTACK, 0.25f}, {P_COMP_RELEASE, 0.42f}, {P_COMP_KNEE, 0.25f},
      {P_COMP_MIX, 0.85f}, {P_COMP_AUTO, 1},
      {P_COLOR_ON, 1}, {P_COLOR_MODEL, 3}, {P_COLOR_DRIVE, 0.55f},
      {P_COLOR_CHARACTER, 0.55f}, {P_COLOR_TONE, 0.42f}, {P_COLOR_MIX, 0.9f},
      {P_COLOR_LEVEL, 0.48f}, {P_DENS_ON, 1}, {P_DENS_EFFECT, 0.55f}}},
    {"Low Warmth", "EQ",
     {{P_EQ_ON, 1}, {P_EQ_LOW_GAIN, 0.62f}, {P_EQ_LOW_FREQ, 0.28f},
      {P_EQ_HIGH_GAIN, 0.48f}, {P_COMP_ON, 0}, {P_COLOR_ON, 0},
      {P_DENS_ON, 0}, {P_LIMIT_ON, 1}, {P_LIMIT_CEILING, 0.95f}, {-1, 0}}},
};

inline constexpr int kPresetCount =
    static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace mastering
