// Mastering Suite — DSP tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "dsp/engine.hpp"

using namespace mastering;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}
constexpr float kSr = 48000.0f;
constexpr int kFrames = 256;

void sine(std::vector<float>& l, std::vector<float>& r, int block, float amp,
          float hz) {
  for (int i = 0; i < kFrames; ++i) {
    const float t = static_cast<float>(block * kFrames + i) / kSr;
    l[i] = r[i] = amp * std::sin(2.0f * 3.14159265f * hz * t);
  }
}
} // namespace

int main() {
  // Flat EQ should pass a tone at unity after the limiter lookahead.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    Meters m;
    e.setControls(c);
    std::vector<float> l(kFrames), r(kFrames);
    double inP = 0.0, outP = 0.0;
    for (int b = 0; b < 160; ++b) {
      sine(l, r, b, 0.25f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
      if (b > 20) {
        for (int i = 0; i < kFrames; ++i) {
          outP += static_cast<double>(l[i]) * l[i];
          const float t = static_cast<float>(b * kFrames + i) / kSr;
          const double x = 0.25 * std::sin(2.0 * 3.14159265 * 1000.0 * t);
          inP += x * x;
        }
      }
    }
    const double ratio = std::sqrt(outP / inP);
    check(std::fabs(ratio - 1.0) < 0.03, "flat EQ passes unity");
    check(e.latencySamples() == 240, "limiter reports 5 ms latency");
  }

  // Bus compressor produces gain reduction.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.comp.enabled = true;
    c.comp.threshold = 0.2f;
    c.comp.ratio = 2.0f;
    c.comp.attack = 0.0f;
    c.comp.release = 0.4f;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    float gr = 0.0f;
    for (int b = 0; b < 80; ++b) {
      sine(l, r, b, 0.5f, 500.0f);
      e.process(l.data(), r.data(), kFrames, m);
      gr = std::max(gr, m.compGr);
    }
    check(gr > 1.0f, "bus compressor reports gain reduction");
  }

  // Limiter ceiling, including true-peak mode.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.limiter.enabled = true;
    c.limiter.ceiling = 0.8f;
    c.limiter.truePeak = true;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    float peak = 0.0f;
    for (int b = 0; b < 160; ++b) {
      sine(l, r, b, 1.2f, 700.0f);
      e.process(l.data(), r.data(), kFrames, m);
      if (b > 20) {
        for (float v : l) peak = std::max(peak, std::fabs(v));
      }
    }
    const float ceiling = std::pow(10.0f, limiterCeilingDb(0.8f) * 0.05f);
    check(peak <= ceiling * 1.002f, "limiter holds the ceiling");
    check(m.limitGr > 0.1f, "limiter reports gain reduction");
  }

  // Colour and density stay finite.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.color.enabled = true;
    c.color.model = 4;
    c.color.drive = 0.6f;
    c.density.enabled = true;
    c.density.effect = 0.5f;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    bool finite = true;
    for (int b = 0; b < 120; ++b) {
      sine(l, r, b, 0.3f, 220.0f);
      e.process(l.data(), r.data(), kFrames, m);
      for (float v : l) {
        if (!std::isfinite(v)) finite = false;
      }
    }
    check(finite, "colour and density stay finite");
  }

  // Loudness/spectrum meters populate.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.limiter.enabled = false;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    for (int b = 0; b < 400; ++b) {
      sine(l, r, b, 0.2f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
    }
    check(m.lufsMomentary > -50.0f && m.lufsMomentary < 0.0f,
          "momentary LUFS is populated");
    check(m.lufsIntegrated > -50.0f, "integrated LUFS is populated");
    bool spectrum = false;
    const float* bins = e.spectrumBins();
    for (int i = 0; i < kSpectrumBins; ++i) {
      if (bins[i] > -100.0f) spectrum = true;
    }
    check(spectrum, "FFT spectrum is populated");
  }

  // Meter reset clears the integrated loudness history.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.limiter.enabled = false;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    for (int b = 0; b < 400; ++b) {
      sine(l, r, b, 0.2f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
    }
    check(m.lufsIntegrated > -50.0f, "integrated LUFS before reset");
    e.resetMeters();
    Meters after;
    sine(l, r, 401, 0.2f, 1000.0f);
    e.process(l.data(), r.data(), kFrames, after);
    check(after.lufsIntegrated <= -100.0f, "meter reset clears integrated LUFS");
  }

  // The chain must not fade in when the instance is freshly prepared.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.limiter.enabled = false;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    sine(l, r, 0, 0.25f, 1000.0f);
    e.process(l.data(), r.data(), kFrames, m);
    const double expected =
        0.25 * std::sin(2.0 * 3.141592653589793 * 1000.0 * 10.0 / kSr);
    check(std::fabs(l[250] - expected) < 0.01,
          "fresh instance starts at full gain (no fade-in)");
  }

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
