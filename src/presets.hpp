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
  PresetValue values[P_COUNT + 1];
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
    {"Pop Punk Master", "User",
     {      {0, 1.0f}, {1, 0.355f}, {2, 0.5f}, {3, 3.0f}, {4, 4.0f}, {5, 1.0f}, {6, 0.665f},
      {7, 0.27f}, {8, 0.71f}, {9, 0.595f}, {10, 0.35f}, {11, 0.82f}, {12, 0.65f}, {13, 0.2f},
      {14, 1.0f}, {15, 0.725f}, {16, 0.0f}, {17, 0.35f}, {18, 0.5f}, {19, 0.3f}, {20, 1.0f},
      {21, 1.0f}, {22, 1.0f}, {23, 1.0f}, {24, 3.0f}, {25, 0.19f}, {26, 0.2f}, {27, 0.55f},
      {28, 1.0f}, {29, 0.5f}, {30, 0.0f}, {31, 1.0f}, {32, 0.425f}, {33, 0.21f}, {34, 0.525f},
      {35, 1.0f}, {36, 0.955f}, {37, 0.5f}, {38, 1.0f}, {39, 0.54f}, {40, 2.0f}, {41, 0.5f},
      {42, 0.58f}, {43, 0.0f}, {44, 2.0f}, {45, 0.0f}, {46, 0.0f}, {47, 0.0f}}},
};

inline constexpr int kPresetCount =
    static_cast<int>(sizeof(kPresets) / sizeof(kPresets[0]));

} // namespace mastering
