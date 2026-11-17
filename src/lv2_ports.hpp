// Mastering Suite — LV2 port layout
// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <cstdint>

#include "params.hpp"

namespace mastering {

// Audio ports first, then parameters, then the meter-reset trigger and
// read-only meter outputs.
constexpr uint32_t kLv2InL = 0;
constexpr uint32_t kLv2InR = 1;
constexpr uint32_t kLv2OutL = 2;
constexpr uint32_t kLv2OutR = 3;

constexpr uint32_t kLv2FirstParam = 4;
constexpr uint32_t kLv2MeterReset = kLv2FirstParam + static_cast<uint32_t>(P_COUNT);
constexpr uint32_t kLv2FirstMeter = kLv2MeterReset + 1;

enum Lv2Meter : uint32_t {
  kLv2InPeakL = 0,
  kLv2InPeakR,
  kLv2OutPeakL,
  kLv2OutPeakR,
  kLv2TruePeakL,
  kLv2TruePeakR,
  kLv2LufsI,
  kLv2LufsS,
  kLv2LufsM,
  kLv2Lra,
  kLv2Correlation,
  kLv2CompGr,
  kLv2LimitGr,
  kLv2MeterCount
};

constexpr uint32_t kLv2FirstSpectrum =
    kLv2FirstMeter + static_cast<uint32_t>(kLv2MeterCount);
constexpr uint32_t kLv2SpectrumCount = static_cast<uint32_t>(kSpectrumBins);
constexpr uint32_t kLv2SampleRate = kLv2FirstSpectrum + kLv2SpectrumCount;
constexpr uint32_t kLv2Latency = kLv2SampleRate + 1;
constexpr uint32_t kLv2PortCount = kLv2Latency + 1;

static_assert(kLv2MeterReset == 44, "port layout drifted");
static_assert(kLv2FirstMeter == 45, "port layout drifted");
static_assert(kLv2FirstSpectrum == 58, "port layout drifted");
static_assert(kLv2SampleRate == 186, "port layout drifted");
static_assert(kLv2Latency == 187, "port layout drifted");
static_assert(kLv2PortCount == 188, "port layout drifted");

} // namespace mastering
