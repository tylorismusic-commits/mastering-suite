// Mastering Suite — panel preview
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cairo.h>
#include <hdsp/hdsp.hpp>

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "params.hpp"
#include "ui/panel.hpp"

using namespace mastering;

int main(int argc, char** argv) {
  const std::string fontDir = argc > 2 ? argv[1] : "assets/fonts";
  const std::string outDir = argc > 2 ? argv[2] : "docs";

  Shared shared;
  shared.set(P_EQ_ON, 1.0);
  shared.set(P_EQ_LOW_GAIN, 0.56);
  shared.set(P_EQ_HIGH_GAIN, 0.58);
  shared.set(P_EQ_HIGH_FREQ, 0.68);
  shared.set(P_COMP_ON, 1.0);
  shared.set(P_COMP_MIX, 0.7);
  shared.set(P_COMP_AUTO, 1.0);
  shared.set(P_COLOR_ON, 1.0);
  shared.set(P_COLOR_MODEL, 1.0);
  shared.set(P_COLOR_DRIVE, 0.4);
  shared.set(P_COLOR_MIX, 0.55);
  shared.set(P_DENS_ON, 1.0);
  shared.set(P_DENS_EFFECT, 0.25);
  shared.set(P_LIMIT_ON, 1.0);
  shared.set(P_MODULE, 0.0);
  shared.set(P_METER, 0.0);

  shared.inPeakL.store(-11.0f);
  shared.inPeakR.store(-12.0f);
  shared.outPeakL.store(-13.0f);
  shared.outPeakR.store(-14.0f);
  shared.truePeakL.store(-0.4f);
  shared.truePeakR.store(-0.6f);
  shared.lufsIntegrated.store(-14.2f);
  shared.lufsShortTerm.store(-13.8f);
  shared.lufsMomentary.store(-13.1f);
  shared.lra.store(4.2f);
  shared.correlation.store(0.71f);
  shared.compGr.store(2.4f);
  shared.limitGr.store(1.8f);

  hdsp::SpectrumAnalyzer analyzer;
  analyzer.prepare(48000.0f);
  std::vector<float> l(256), r(256);
  unsigned seed = 987654u;
  for (int b = 0; b < 220; ++b) {
    for (int i = 0; i < 256; ++i) {
      const float t = static_cast<float>(b * 256 + i) / 48000.0f;
      seed = seed * 1664525u + 1013904223u;
      const float noise =
          (static_cast<float>((seed >> 8) & 0xffff) / 32768.0f - 1.0f) * 0.005f;
      l[i] = r[i] =
          0.11f * std::sin(2.0f * hdsp::kPi * 70.0f * t) +
          0.07f * std::sin(2.0f * hdsp::kPi * 330.0f * t) +
          0.05f * std::sin(2.0f * hdsp::kPi * 1400.0f * t) +
          0.035f * std::sin(2.0f * hdsp::kPi * 6200.0f * t) + noise;
    }
    analyzer.push(l.data(), r.data(), 256);
  }
  for (int i = 0; i < kSpectrumBins; ++i) {
    shared.spectrum[i].store(analyzer.bins()[i]);
  }

  Panel panel;
  panel.loadFont(fontDir);
  PanelState state;
  for (int i = 0; i < 24; ++i) panel.tick(shared, state);

  cairo_surface_t* surface =
      cairo_image_surface_create(CAIRO_FORMAT_ARGB32, kPanelWidth, kPanelHeight);
  cairo_t* cr = cairo_create(surface);
  panel.draw(cr, shared, state);
  cairo_destroy(cr);
  const std::string path = outDir + "/mastering-suite.png";
  const cairo_status_t st = cairo_surface_write_to_png(surface, path.c_str());
  cairo_surface_destroy(surface);
  if (st != CAIRO_STATUS_SUCCESS) {
    std::fprintf(stderr, "could not write %s\n", path.c_str());
    return 1;
  }
  std::printf("wrote %s\n", path.c_str());
  return 0;
}
