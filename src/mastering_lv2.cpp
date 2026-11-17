// Mastering Suite — LV2 plugin
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Fixed mastering chain with the same DSP as the CLAP build.  Control ports
// mirror the CLAP parameter ids: port = kLv2FirstParam + ParamId.

#include <lv2/core/lv2.h>

#include <algorithm>
#include <cstring>

#include "dsp/engine.hpp"
#include "lv2_ports.hpp"
#include "params.hpp"

namespace {

constexpr const char* kPluginUri =
    "https://github.com/tylorismusic-commits/mastering-suite";

struct Plugin {
  float sampleRate = 48000.0f;
  float lastPort[mastering::P_COUNT] = {};
  bool portSeen[mastering::P_COUNT] = {};
  bool resetSeen = false;
  mastering::Shared shared;
  mastering::MasteringEngine engine;
  mastering::Meters meters;
  float* ports[mastering::kLv2PortCount] = {};
};

LV2_Handle instantiate(const LV2_Descriptor*, double rate, const char*,
                       const LV2_Feature* const*) {
  auto* p = new Plugin();
  p->sampleRate = static_cast<float>(rate > 0.0 ? rate : 48000.0);
  p->shared.sampleRate.store(p->sampleRate, std::memory_order_relaxed);
  p->engine.prepare(p->sampleRate);
  p->engine.setControls(mastering::controlsFrom(p->shared));
  return p;
}

void connectPort(LV2_Handle handle, uint32_t port, void* data) {
  auto* p = static_cast<Plugin*>(handle);
  if (port < mastering::kLv2PortCount) {
    p->ports[port] = static_cast<float*>(data);
  }
}

void activate(LV2_Handle handle) {
  auto* p = static_cast<Plugin*>(handle);
  p->engine.reset();
  p->engine.resetMeters();
  p->resetSeen = false;
  if (p->ports[mastering::kLv2SampleRate]) {
    *p->ports[mastering::kLv2SampleRate] = p->sampleRate;
  }
  if (p->ports[mastering::kLv2Latency]) {
    *p->ports[mastering::kLv2Latency] =
        static_cast<float>(p->engine.latencySamples());
  }
}

void deactivate(LV2_Handle) {}

void run(LV2_Handle handle, uint32_t frames) {
  auto* p = static_cast<Plugin*>(handle);

  // A rising edge on the UI reset trigger clears the loudness history.  The
  // UI holds the trigger high for 150 ms and then releases it, so a host that
  // coalesces UI writes still delivers an edge.
  float* resetPort = p->ports[mastering::kLv2MeterReset];
  const bool reset = resetPort && *resetPort > 0.5f;
  if (reset && !p->resetSeen) {
    p->engine.resetMeters();
  }
  p->resetSeen = reset;

  // Pull host parameter values in.  Apply a port on its first appearance and
  // then only when it changes, so a cached host value cannot undo an edit made
  // in the editor.
  for (uint32_t i = 0; i < mastering::P_COUNT; ++i) {
    float* v = p->ports[mastering::kLv2FirstParam + i];
    if (!v) {
      continue;
    }
    if (!p->portSeen[i] || *v != p->lastPort[i]) {
      p->shared.setFromHost(i, static_cast<double>(*v));
      p->lastPort[i] = *v;
      p->portSeen[i] = true;
    }
  }
  p->engine.setControls(mastering::controlsFrom(p->shared));

  float* inL = p->ports[mastering::kLv2InL];
  float* inR = p->ports[mastering::kLv2InR];
  float* outL = p->ports[mastering::kLv2OutL];
  float* outR = p->ports[mastering::kLv2OutR];
  if (!inL) {
    inL = outL;
  }
  if (!inR) {
    inR = inL;
  }

  if (inL && outL) {
    if (!outR) {
      // Mono host output: process the left channel and discard the right.
      float scratch[512];
      for (uint32_t offset = 0; offset < frames; offset += 512) {
        const uint32_t n = std::min<uint32_t>(512, frames - offset);
        if (outL + offset != inL + offset) {
          std::memcpy(outL + offset, inL + offset, n * sizeof(float));
        }
        std::memcpy(scratch, inR + offset, n * sizeof(float));
        p->engine.process(outL + offset, scratch, static_cast<int>(n),
                          p->meters);
      }
    } else {
      if (outL != inL) {
        std::memcpy(outL, inL, frames * sizeof(float));
      }
      if (outR != inR) {
        std::memcpy(outR, inR, frames * sizeof(float));
      }
      p->engine.process(outL, outR, static_cast<int>(frames), p->meters);
    }
  }

  auto writeMeter = [&](mastering::Lv2Meter meter, float value) {
    float* v =
        p->ports[mastering::kLv2FirstMeter + static_cast<uint32_t>(meter)];
    if (v) {
      *v = value;
    }
  };
  writeMeter(mastering::kLv2InPeakL, p->meters.inPeakL);
  writeMeter(mastering::kLv2InPeakR, p->meters.inPeakR);
  writeMeter(mastering::kLv2OutPeakL, p->meters.outPeakL);
  writeMeter(mastering::kLv2OutPeakR, p->meters.outPeakR);
  writeMeter(mastering::kLv2TruePeakL, p->meters.truePeakL);
  writeMeter(mastering::kLv2TruePeakR, p->meters.truePeakR);
  writeMeter(mastering::kLv2LufsI, p->meters.lufsIntegrated);
  writeMeter(mastering::kLv2LufsS, p->meters.lufsShortTerm);
  writeMeter(mastering::kLv2LufsM, p->meters.lufsMomentary);
  writeMeter(mastering::kLv2Lra, p->meters.lra);
  writeMeter(mastering::kLv2Correlation, p->meters.correlation);
  writeMeter(mastering::kLv2CompGr, p->meters.compGr);
  writeMeter(mastering::kLv2LimitGr, p->meters.limitGr);
  writeMeter(mastering::kLv2TruePeakMax, p->meters.truePeakMax);
  writeMeter(mastering::kLv2SamplePeakMax, p->meters.samplePeakMax);
  writeMeter(mastering::kLv2ShortTermMax, p->meters.shortTermMax);
  writeMeter(mastering::kLv2Plr, p->meters.plr);
  writeMeter(mastering::kLv2Psr, p->meters.psr);

  const float* spectrum = p->engine.spectrumBins();
  for (uint32_t i = 0; i < mastering::kLv2SpectrumCount; ++i) {
    float* v = p->ports[mastering::kLv2FirstSpectrum + i];
    if (v) {
      *v = spectrum[i];
    }
  }
  if (p->ports[mastering::kLv2SampleRate]) {
    *p->ports[mastering::kLv2SampleRate] = p->sampleRate;
  }
  if (p->ports[mastering::kLv2Latency]) {
    *p->ports[mastering::kLv2Latency] =
        static_cast<float>(p->engine.latencySamples());
  }
}

void cleanup(LV2_Handle handle) { delete static_cast<Plugin*>(handle); }

const LV2_Descriptor kDescriptor = {
    kPluginUri,
    instantiate,
    connectPort,
    activate,
    run,
    deactivate,
    cleanup,
    nullptr,
};

} // namespace

LV2_SYMBOL_EXPORT const LV2_Descriptor* lv2_descriptor(uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}
