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
    check(e.latencySamples() == 285, "4x limiter reports 285 samples latency");
  }

  // Latency follows the host sample rate.
  {
    MasteringEngine e;
    e.prepare(44100.0f);
    check(e.latencySamples() == 265, "44.1 kHz latency is 265 samples");
    e.prepare(96000.0f);
    check(e.latencySamples() == 525, "96 kHz latency is 525 samples");
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    sine(l, r, 0, 0.2f, 1000.0f);
    e.process(l.data(), r.data(), kFrames, m);
    bool finite = true;
    for (float v : l) {
      if (!std::isfinite(v)) finite = false;
    }
    check(finite, "96 kHz processing stays finite");
  }

  // The oversampling selector does not move the reported latency.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    for (int os = 0; os <= 2; ++os) {
      c.limiter.oversample = os;
      e.setControls(c);
      sine(l, r, 0, 0.2f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
      check(e.latencySamples() == 285,
            "limiter latency stays constant across oversampling");
      bool finite = true;
      for (float v : l) {
        if (!std::isfinite(v)) finite = false;
      }
      check(finite, "oversampled limiter stays finite");
    }
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

  // Limiter input gain drives into the ceiling; the bypass path stays unity.
  {
    auto run = [](bool enabled, float gainNorm, float& peak) {
      MasteringEngine e;
      e.prepare(kSr);
      Controls c;
      c.eq.enabled = false;
      c.comp.enabled = false;
      c.color.enabled = false;
      c.density.enabled = false;
      c.limiter.enabled = enabled;
      c.limiter.ceiling = 0.5f;   // -3 dB
      c.limiter.truePeak = true;
      c.limiter.inputGain = gainNorm;
      e.setControls(c);
      Meters m;
      std::vector<float> l(kFrames), r(kFrames);
      peak = 0.0f;
      for (int b = 0; b < 160; ++b) {
        sine(l, r, b, 0.2f, 1000.0f);
        e.process(l.data(), r.data(), kFrames, m);
        if (b > 80) {
          for (float v : l) peak = std::max(peak, std::fabs(v));
        }
      }
    };
    float unity = 0.0f;
    float driven = 0.0f;
    float bypassed = 0.0f;
    run(true, 0.5f, unity);
    run(true, 1.0f, driven);
    run(false, 1.0f, bypassed);
    const float ceiling = std::pow(10.0f, limiterCeilingDb(0.5f) * 0.05f);
    check(std::fabs(unity - 0.2f) < 0.01f,
          "limiter at 0 dB input leaves headroom alone");
    check(driven > 0.6f && driven <= ceiling * 1.002f,
          "limiter input gain drives into the ceiling");
    check(std::fabs(bypassed - 0.2f) < 0.01f,
          "bypassed limiter ignores its input gain");
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

  // Colour Drive is level-compensated by the internal auto-gain.
  {
    auto colourRms = [](float drive) {
      MasteringEngine e;
      e.prepare(kSr);
      Controls c;
      c.eq.enabled = false;
      c.comp.enabled = false;
      c.density.enabled = false;
      c.limiter.enabled = false;
      c.color.enabled = true;
      c.color.model = 0;
      c.color.drive = drive;
      c.color.character = 0.5f;
      c.color.tone = 0.5f;
      c.color.mix = 1.0f;
      c.color.level = 0.5f;
      e.setControls(c);
      Meters m;
      std::vector<float> l(kFrames), r(kFrames);
      double sum = 0.0;
      int count = 0;
      for (int b = 0; b < 400; ++b) {
        sine(l, r, b, 0.1f, 1000.0f);
        e.process(l.data(), r.data(), kFrames, m);
        if (b > 200) {
          for (float v : l) {
            sum += static_cast<double>(v) * v;
            ++count;
          }
        }
      }
      return std::sqrt(sum / count);
    };
    const double inputRms = 0.1 / std::sqrt(2.0);
    for (float drive : {0.0f, 0.45f, 1.0f}) {
      const double rms = colourRms(drive);
      const double delta = 20.0 * std::log10(rms / inputRms);
      check(std::fabs(delta) < 2.0,
            "colour drive stays within 2 dB of unity");
    }
  }

  // Compressor auto gain compensates the mix, not just the wet path.
  {
    auto compRms = [](bool autoGain) {
      MasteringEngine e;
      e.prepare(kSr);
      Controls c;
      c.eq.enabled = false;
      c.color.enabled = false;
      c.density.enabled = false;
      c.limiter.enabled = false;
      c.comp.enabled = true;
      c.comp.threshold = 0.2f;
      c.comp.ratio = 2.0f;
      c.comp.attack = 0.1f;
      c.comp.release = 0.4f;
      c.comp.mix = 1.0f;
      c.comp.autoGain = autoGain;
      e.setControls(c);
      Meters m;
      std::vector<float> l(kFrames), r(kFrames);
      double sum = 0.0;
      int count = 0;
      for (int b = 0; b < 240; ++b) {
        sine(l, r, b, 0.5f, 500.0f);
        e.process(l.data(), r.data(), kFrames, m);
        if (b > 120) {
          for (float v : l) {
            sum += static_cast<double>(v) * v;
            ++count;
          }
        }
      }
      return std::sqrt(sum / count);
    };
    const double noAuto = compRms(false);
    const double autoGain = compRms(true);
    check(autoGain > noAuto * 1.05,
          "compressor auto gain restores level after compression");
    check(std::fabs(20.0 * std::log10(autoGain / (0.5 / std::sqrt(2.0)))) <
              1.5,
          "compressor auto gain lands near unity");
  }

  // Colour-stage soft and hard clipping.
  {
    auto runClip = [](int clip, float& peak) {
      MasteringEngine e;
      e.prepare(kSr);
      Controls c;
      c.eq.enabled = false;
      c.comp.enabled = false;
      c.density.enabled = false;
      c.limiter.enabled = false;
      c.color.enabled = true;
      c.color.model = 0;
      c.color.drive = 0.45f;
      c.color.character = 0.5f;
      c.color.tone = 0.5f;
      c.color.mix = 1.0f;
      c.color.level = 0.5f;
      c.color.clip = clip;
      e.setControls(c);
      Meters m;
      std::vector<float> l(kFrames), r(kFrames);
      peak = 0.0f;
      bool finite = true;
      for (int b = 0; b < 80; ++b) {
        sine(l, r, b, 1.0f, 220.0f);
        e.process(l.data(), r.data(), kFrames, m);
        for (float v : l) {
          if (!std::isfinite(v)) finite = false;
          if (b > 20) peak = std::max(peak, std::fabs(v));
        }
      }
      return finite;
    };

    float softPeak = 0.0f;
    float hardPeak = 0.0f;
    const bool softFinite = runClip(1, softPeak);
    const bool hardFinite = runClip(2, hardPeak);
    check(softFinite && hardFinite, "colour clip modes stay finite");
    check(hardPeak <= 0.2005f && hardPeak > 0.19f,
          "hard clip stops at its ceiling");
    check(softPeak > hardPeak && softPeak <= 0.2505f,
          "soft clip rounds the ceiling without passing it");
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
    check(m.truePeakMax > -50.0f, "true-peak max hold is populated");
    check(m.samplePeakMax > -50.0f, "sample-peak max hold is populated");
    check(m.shortTermMax > -50.0f, "short-term max hold is populated");
    check(m.plr > 0.0f, "PLR is populated");
    check(m.psr > 0.0f, "PSR is populated");
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
    std::vector<float> l(512), r(512);
    for (int i = 0; i < 512; ++i) {
      l[i] = r[i] = 0.25f * std::sin(
          2.0f * 3.141592653589793f * 1000.0f * i / kSr);
    }
    e.process(l.data(), r.data(), 512, m);
    const double expected =
        0.25 * std::sin(2.0 * 3.141592653589793 * 1000.0 * 10.0 / kSr);
    check(std::fabs(l[295] - expected) < 0.01,
          "fresh instance starts at full gain (no fade-in)");
  }

  // EBU R128 calibration: a full-scale 1 kHz sine in both channels reads
  // about 0 LUFS (+3 dB from two channels, plus K-weighting).
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.eq.enabled = false;
    c.comp.enabled = false;
    c.color.enabled = false;
    c.density.enabled = false;
    c.limiter.enabled = false;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    for (int b = 0; b < 1000; ++b) {
      sine(l, r, b, 1.0f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
    }
    check(std::fabs(m.lufsIntegrated) < 0.5f,
          "integrated LUFS is calibrated on a full-scale stereo sine");
    check(m.truePeakL > -1.0f && m.truePeakL < 1.5f,
          "true-peak meter tracks a full-scale sine");
  }


  // Output width and the M/S monitor matrix.
  {
    auto run = [&](float width, int mode, float& outL, float& outR) {
      MasteringEngine e;
      e.prepare(kSr);
      Controls c;
      c.limiter.enabled = false;
      c.width = width;
      c.msMode = mode;
      e.setControls(c);
      Meters m;
      std::vector<float> l(kFrames), r(kFrames);
      for (int b = 0; b < 8; ++b) {
        for (int i = 0; i < kFrames; ++i) {
          const float t = static_cast<float>(b * kFrames + i) / kSr;
          l[i] = 0.5f * std::sin(2.0f * 3.14159265f * 200.0f * t);
          r[i] = 0.0f;
        }
        e.process(l.data(), r.data(), kFrames, m);
      }
      outL = l[kFrames - 1];
      outR = r[kFrames - 1];
    };
    float l = 0.0f, r = 0.0f;
    run(0.0f, 0, l, r);
    check(std::fabs(l - r) < 1.0e-4, "zero width collapses to mono");
    run(2.0f, 0, l, r);
    check(l > 0.4f && r < -0.1f, "double width widens the stereo image");
    run(1.0f, 2, l, r);
    check(std::fabs(l - r) < 1.0e-4, "mid monitor outputs the centre only");
    run(1.0f, 3, l, r);
    check(std::fabs(l - r) < 1.0e-4 && std::fabs(l) > 0.1f,
          "side monitor outputs the difference only");
  }

  // Delta audition subtracts the latency-aligned dry signal.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.delta = true;
    c.eq.enabled = false;
    c.comp.enabled = false;
    c.color.enabled = false;
    c.density.enabled = false;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    float residual = 0.0f;
    for (int b = 0; b < 40; ++b) {
      sine(l, r, b, 0.1f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
      if (b > 20) {
        for (int i = 0; i < kFrames; ++i) {
          residual = std::max(residual, std::fabs(l[i]));
          residual = std::max(residual, std::fabs(r[i]));
        }
      }
    }
    check(residual < 0.001f, "delta is silent for a transparent chain");
  }

  // 16-bit dither lands on the quantiser grid.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.limiter.enabled = false;
    c.ditherBits = 16;
    c.ditherShape = 1;
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    for (int b = 0; b < 8; ++b) {
      for (int i = 0; i < kFrames; ++i) {
        l[i] = r[i] = 0.123456f;
      }
      e.process(l.data(), r.data(), kFrames, m);
    }
    bool onGrid = true;
    for (int i = 0; i < kFrames; ++i) {
      const double scaled = static_cast<double>(l[i]) * 32768.0;
      if (std::fabs(scaled - std::round(scaled)) > 1.0e-3) onGrid = false;
    }
    check(onGrid, "16-bit dither quantises to the 32768-step grid");
  }

  // Streaming loudness target and match offset.
  {
    MasteringEngine e;
    e.prepare(kSr);
    Controls c;
    c.limiter.enabled = false;
    c.targetIndex = 0;  // -9 LUFS
    e.setControls(c);
    Meters m;
    std::vector<float> l(kFrames), r(kFrames);
    for (int b = 0; b < 800; ++b) {
      sine(l, r, b, 0.5f, 1000.0f);
      e.process(l.data(), r.data(), kFrames, m);
    }
    check(std::fabs(m.targetLufs + 9.0f) < 1.0e-4,
          "target loudness follows the selector");
    check(std::fabs(m.targetDelta - (m.lufsIntegrated - m.targetLufs)) < 1.0e-3,
          "target match reports the delta to the measured loudness");
  }

  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
