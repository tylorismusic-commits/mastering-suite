// Mastering Suite — typed parameter values
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "params.hpp"

namespace mastering {

inline std::string lowerText(std::string s) {
  for (char& c : s) {
    c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }
  return s;
}

inline std::string trimText(const std::string& s) {
  const size_t a = s.find_first_not_of(" \t");
  if (a == std::string::npos) {
    return {};
  }
  const size_t b = s.find_last_not_of(" \t");
  return s.substr(a, b - a + 1);
}

inline std::string formatParamValue(uint32_t id, double value) {
  const ParamSpec& s = kParamSpecs[id];
  if (s.labels.count > 0) {
    const int i = std::clamp(static_cast<int>(value + 0.5), 0,
                             s.labels.count - 1);
    return s.labels.names[i];
  }
  char buf[64];
  switch (id) {
    case P_ENABLED:
    case P_EQ_ON:
    case P_COMP_ON:
    case P_COLOR_ON:
    case P_DENS_ON:
    case P_LIMIT_ON:
    case P_COMP_AUTO:
    case P_LIMIT_TRUE_PEAK:
      return value > 0.5 ? "On" : "Off";
    case P_INPUT:
    case P_OUTPUT:
    case P_EQ_LOW_GAIN:
    case P_EQ_MID_GAIN:
    case P_EQ_HIGH_GAIN:
    case P_DENS_INPUT:
    case P_DENS_OUTPUT:
      std::snprintf(buf, sizeof(buf), "%+.1f dB",
                    densityDb(static_cast<float>(value)));
      return buf;
    case P_EQ_LOW_FREQ:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    lowFreqHz(static_cast<float>(value)));
      return buf;
    case P_EQ_MID_FREQ:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    midFreqHz(static_cast<float>(value)));
      return buf;
    case P_EQ_HIGH_FREQ:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    highFreqHz(static_cast<float>(value)));
      return buf;
    case P_EQ_HPF:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    hpfHz(static_cast<float>(value)));
      return buf;
    case P_EQ_MID_Q:
      std::snprintf(buf, sizeof(buf), "%.2f",
                    midQ(static_cast<float>(value)));
      return buf;
    case P_COMP_THRESHOLD:
      std::snprintf(buf, sizeof(buf), "%.1f dB",
                    compThresholdDb(static_cast<float>(value)));
      return buf;
    case P_COMP_ATTACK:
      std::snprintf(buf, sizeof(buf), "%.0f ms",
                    compAttackMs(static_cast<float>(value)));
      return buf;
    case P_COMP_RELEASE:
      std::snprintf(buf, sizeof(buf), "%.0f ms",
                    compReleaseMs(static_cast<float>(value)));
      return buf;
    case P_COMP_KNEE:
      std::snprintf(buf, sizeof(buf), "%.1f dB",
                    compKneeDb(static_cast<float>(value)));
      return buf;
    case P_COMP_SC_HPF:
      std::snprintf(buf, sizeof(buf), "%.0f Hz",
                    compScHpfHz(static_cast<float>(value)));
      return buf;
    case P_COMP_MIX:
    case P_COLOR_DRIVE:
    case P_COLOR_CHARACTER:
    case P_COLOR_TONE:
    case P_COLOR_MIX:
    case P_DENS_EFFECT:
      std::snprintf(buf, sizeof(buf), "%.0f%%", value * 100.0);
      return buf;
    case P_COLOR_LEVEL: {
      const float v = static_cast<float>(value);
      if (v <= 0.002f) {
        return "Off";
      }
      const float db = v <= 0.5f ? -60.0f + 120.0f * v
                                 : 24.0f * (v - 0.5f);
      std::snprintf(buf, sizeof(buf), "%+.1f dB", db);
      return buf;
    }
    case P_LIMIT_CEILING:
      std::snprintf(buf, sizeof(buf), "%.1f dB",
                    limiterCeilingDb(static_cast<float>(value)));
      return buf;
    case P_LIMIT_RELEASE:
      std::snprintf(buf, sizeof(buf), "%.0f ms",
                    limiterReleaseMs(static_cast<float>(value)));
      return buf;
    default:
      std::snprintf(buf, sizeof(buf), "%.2f", value);
      return buf;
  }
}

inline bool parseParamValue(uint32_t id, const std::string& raw, double* out) {
  const ParamSpec& s = kParamSpecs[id];
  const std::string text = lowerText(trimText(raw));
  if (text.empty()) {
    return false;
  }
  for (int i = 0; i < s.labels.count; ++i) {
    if (text == lowerText(s.labels.names[i])) {
      *out = i;
      return true;
    }
  }
  if (text == "on" || text == "true" || text == "yes") {
    *out = 1.0;
    return true;
  }
  if (text == "off" || text == "false" || text == "no") {
    *out = 0.0;
    return true;
  }

  char* end = nullptr;
  const double value = std::strtod(text.c_str(), &end);
  if (end == text.c_str()) {
    return false;
  }
  const std::string suffix = trimText(std::string(end));
  auto hz = [&]() {
    return suffix == "khz" || suffix == "k" ? value * 1000.0 : value;
  };
  auto ms = [&]() {
    return suffix == "s" || suffix == "sec" ? value * 1000.0 : value;
  };
  auto percent = [&]() {
    return suffix == "%" || value > 1.0 ? value / 100.0 : value;
  };

  switch (id) {
    case P_METER:
    case P_MODULE:
    case P_COLOR_CLIP:
      *out = value;
      return true;
    case P_COMP_RATIO: {
      static constexpr double kRatios[3] = {2.0, 4.0, 10.0};
      int best = 0;
      double delta = 1.0e9;
      for (int i = 0; i < 3; ++i) {
        const double d = std::fabs(kRatios[i] - value);
        if (d < delta) {
          delta = d;
          best = i;
        }
      }
      *out = best;
      return true;
    }
    case P_INPUT:
    case P_OUTPUT:
    case P_EQ_LOW_GAIN:
    case P_EQ_MID_GAIN:
    case P_EQ_HIGH_GAIN:
    case P_DENS_INPUT:
    case P_DENS_OUTPUT:
      *out = (value + 12.0) / 24.0;
      return true;
    case P_EQ_LOW_FREQ:
      *out = std::log(std::clamp(hz(), 40.0, 300.0) / 40.0) /
             std::log(7.5);
      return true;
    case P_EQ_MID_FREQ:
      *out = std::log(std::clamp(hz(), 200.0, 5000.0) / 200.0) /
             std::log(25.0);
      return true;
    case P_EQ_HIGH_FREQ:
      *out = std::log(std::clamp(hz(), 2000.0, 8000.0) / 2000.0) /
             std::log(4.0);
      return true;
    case P_EQ_HPF:
      *out = std::log(std::clamp(hz(), 20.0, 120.0) / 20.0) /
             std::log(6.0);
      return true;
    case P_EQ_MID_Q:
      *out = (value - 0.3) / 1.2;
      return true;
    case P_COMP_THRESHOLD:
      *out = (value + 40.0) / 40.0;
      return true;
    case P_COMP_ATTACK:
      *out = std::log(std::clamp(ms(), 1.0, 100.0)) / std::log(100.0);
      return true;
    case P_COMP_RELEASE:
      *out = std::log(std::clamp(ms(), 50.0, 1000.0) / 50.0) /
             std::log(20.0);
      return true;
    case P_COMP_KNEE:
      *out = value / 12.0;
      return true;
    case P_COMP_SC_HPF:
      *out = std::log(std::clamp(hz(), 20.0, 200.0) / 20.0) /
             std::log(10.0);
      return true;
    case P_COMP_MIX:
    case P_COLOR_DRIVE:
    case P_COLOR_CHARACTER:
    case P_COLOR_TONE:
    case P_COLOR_MIX:
    case P_DENS_EFFECT:
      *out = percent();
      return true;
    case P_COLOR_LEVEL:
      if (!suffix.empty() && suffix != "db") {
        return false;
      }
      if (value <= -60.0) {
        *out = 0.0;
      } else if (value <= 0.0) {
        *out = (value + 60.0) / 120.0;
      } else {
        *out = 0.5 + value / 24.0;
      }
      return true;
    case P_LIMIT_CEILING:
      *out = (value + 6.0) / 6.0;
      return true;
    case P_LIMIT_RELEASE:
      *out = std::log(std::clamp(ms(), 20.0, 500.0) / 20.0) /
             std::log(25.0);
      return true;
    default:
      *out = value;
      return true;
  }
}

} // namespace mastering
