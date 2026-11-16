// Mastering Suite — Comp Suite-style panel
// SPDX-License-Identifier: GPL-3.0-or-later
#include "ui/panel.hpp"

#include "dsp/eq.hpp"
#include "presets.hpp"

#include <cairo-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace mastering {

namespace {

struct Rgb {
  double r, g, b;
};

struct Skin {
  const char* caption;
  Rgb bodyTop, bodyBottom;
  Rgb legend, legendDim;
  Rgb headerBg;
  Rgb tabOff, tabOn, tabTextOff, tabTextOn;
  Rgb meterFrame, meterFace, meterNeedle, meterScale;
  Rgb knobCap, knobCapEdge, knobSkirt, knobIndicator;
  bool knurled;
  Rgb btnOff, btnOn, btnText, btnTextOn;
  Rgb screw;
};

// Neutral mastering finish, based on Comp Suite's Clean/VCA faceplate.
const Skin kSkin = {
    "Mastering chain  -  EQ / Comp / Color / Density / Limit",
    {0.22, 0.27, 0.36}, {0.15, 0.19, 0.27},
    {0.92, 0.93, 0.91}, {0.64, 0.68, 0.72},
    {0.11, 0.14, 0.20},
    {0.20, 0.24, 0.31}, {0.19, 0.33, 0.50}, {0.72, 0.78, 0.86},
    {0.96, 0.97, 0.98},
    {0.10, 0.13, 0.18}, {0.73, 0.73, 0.70}, {0.12, 0.12, 0.12},
    {0.25, 0.25, 0.25},
    {0.76, 0.78, 0.81}, {0.45, 0.48, 0.53}, {0.14, 0.17, 0.22},
    {0.08, 0.09, 0.11},
    false,
    {0.10, 0.11, 0.13}, {0.19, 0.33, 0.50}, {0.72, 0.78, 0.86},
    {0.96, 0.97, 0.98},
    {0.40, 0.42, 0.45}};

template <size_t N>
std::vector<const char*> sc(const char* const (&labels)[N]) {
  return std::vector<const char*>(labels, labels + N);
}

const char* const kScaleGain[] = {"-12", "0", "+12"};
const char* const kScaleFreqLow[] = {"40", "", "", "", "300"};
const char* const kScaleFreqMid[] = {"200", "", "", "", "5k"};
const char* const kScaleFreqHigh[] = {"2k", "", "", "", "8k"};
const char* const kScaleQ[] = {"WIDE", "", "", "", "TIGHT"};
const char* const kScaleHpf[] = {"20", "", "", "", "120"};
const char* const kScaleDrive[] = {"0", "2", "4", "6", "8", "10"};
const char* const kScaleCharacter[] = {"SOFT", "", "", "", "HARD"};
const char* const kScaleTone[] = {"DARK", "", "", "", "BRIGHT"};
const char* const kScaleMix[] = {"DRY", "", "", "", "WET"};
const char* const kScaleLevel[] = {"OFF", "-30", "0", "+6", "+12"};
const char* const kScaleThreshold[] = {"-40", "", "", "", "0"};
const char* const kScaleAttack[] = {"1", "", "", "", "100"};
const char* const kScaleRelease[] = {"50", "", "", "", "1k"};
const char* const kScaleKnee[] = {"0", "", "", "", "12"};
const char* const kScaleScHpf[] = {"20", "", "", "", "200"};
const char* const kScaleDensity[] = {"0", "", "", "", "100"};
const char* const kScaleCeiling[] = {"-6", "", "", "", "0"};
const char* const kScaleLimitRelease[] = {"20", "", "", "", "500"};

constexpr double kPi = 3.14159265358979323846;

constexpr double kPresetX = 72.0;
constexpr double kPresetY = 14.0;
constexpr double kPresetW = 330.0;
constexpr double kPresetH = 36.0;
constexpr double kListY = 56.0;
constexpr double kListRow = 22.0;
constexpr double kListVisible = 17.0;

double knobAngle(double f) { return (135.0 + 270.0 * f) * kPi / 180.0; }
double clamp01(double v) { return std::min(std::max(v, 0.0), 1.0); }

void roundRect(cairo_t* cr, double x, double y, double w, double h, double r) {
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + w - r, y + r, r, -kPi / 2, 0);
  cairo_arc(cr, x + w - r, y + h - r, r, 0, kPi / 2);
  cairo_arc(cr, x + r, y + h - r, r, kPi / 2, kPi);
  cairo_arc(cr, x + r, y + r, r, kPi, 3 * kPi / 2);
  cairo_close_path(cr);
}

void setRgb(cairo_t* cr, Rgb c, double a = 1.0) {
  cairo_set_source_rgba(cr, c.r, c.g, c.b, a);
}

Rgb mix(Rgb a, Rgb b, double t) {
  return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t,
          a.b + (b.b - a.b) * t};
}

double freqToX(double freq, double x, double w) {
  const double lo = std::log(20.0);
  const double hi = std::log(20000.0);
  const double t = (std::log(std::clamp(freq, 20.0, 20000.0)) - lo) / (hi - lo);
  return x + w * t;
}

uint32_t moduleOnParam(int module) {
  static const uint32_t kOn[5] = {P_EQ_ON, P_COMP_ON, P_COLOR_ON, P_DENS_ON,
                                  P_LIMIT_ON};
  return kOn[std::clamp(module, 0, 4)];
}

bool isModuleOnParam(uint32_t param) {
  return param == P_EQ_ON || param == P_COMP_ON || param == P_COLOR_ON ||
         param == P_DENS_ON || param == P_LIMIT_ON;
}

const std::vector<int>& presetOrder() {
  static const std::vector<int> order = [] {
    std::vector<int> v;
    for (int i = 0; i < kPresetCount; ++i) v.push_back(i);
    std::stable_sort(v.begin(), v.end(), [](int a, int b) {
      const int c = std::strcmp(kPresets[a].category, kPresets[b].category);
      if (c != 0) return c < 0;
      return std::strcmp(kPresets[a].name, kPresets[b].name) < 0;
    });
    return v;
  }();
  return order;
}

} // namespace

Panel::Panel() {
  grain_ = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 64, 64);
  unsigned char* data = cairo_image_surface_get_data(grain_);
  const int stride = cairo_image_surface_get_stride(grain_);
  unsigned int seed = 22222u;
  for (int y = 0; y < 64; ++y) {
    for (int x = 0; x < 64; ++x) {
      seed = seed * 1103515245u + 12345u;
      const unsigned v = (seed >> 16) & 0xff;
      const unsigned a = (v < 128) ? 10u : 0u;
      const unsigned c = (v < 128) ? 0u : 255u;
      const unsigned p = (c * a + 127u) / 255u;
      unsigned char* px = data + y * stride + x * 4;
      px[0] = px[1] = px[2] = static_cast<unsigned char>(p);
      px[3] = static_cast<unsigned char>(a);
    }
  }
  cairo_surface_mark_dirty(grain_);
  fallbackFont_ = cairo_toy_font_face_create(
      "sans-serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  font_ = fallbackFont_;
}

Panel::~Panel() {
  if (grain_) cairo_surface_destroy(grain_);
  if (font_ && font_ != fallbackFont_) cairo_font_face_destroy(font_);
  if (fallbackFont_) cairo_font_face_destroy(fallbackFont_);
  if (ftFace_) FT_Done_Face(static_cast<FT_Face>(ftFace_));
  if (ft_) FT_Done_FreeType(static_cast<FT_Library>(ft_));
}

void Panel::loadFont(const std::string& dir) {
  if (ft_) return;
  std::string path = dir;
  if (!path.empty() && path.back() != '/') path += '/';
  path += "Jost-ExtraBold.ttf";
  FT_Library lib = nullptr;
  if (FT_Init_FreeType(&lib) != 0) return;
  FT_Face face = nullptr;
  if (FT_New_Face(lib, path.c_str(), 0, &face) != 0) {
    FT_Done_FreeType(lib);
    return;
  }
  cairo_font_face_t* font = cairo_ft_font_face_create_for_ft_face(face, 0);
  if (cairo_font_face_status(font) != CAIRO_STATUS_SUCCESS) {
    cairo_font_face_destroy(font);
    FT_Done_Face(face);
    FT_Done_FreeType(lib);
    return;
  }
  ft_ = lib;
  ftFace_ = face;
  font_ = font;
}

void Panel::text(cairo_t* cr, const char* s, double x, double y, double size,
                 int align, double r, double g, double b) const {
  cairo_set_font_face(cr, font_);
  cairo_set_font_size(cr, size);
  cairo_set_source_rgb(cr, r, g, b);
  cairo_text_extents_t e;
  cairo_text_extents(cr, s, &e);
  // Centre and right-align on the ink extents, not the advance width, so
  // labels sit optically centred over their controls.
  double tx = x - e.x_bearing;
  if (align == 1) tx = x - e.x_bearing - e.width * 0.5;
  else if (align == 2) tx = x - e.x_bearing - e.width;
  cairo_move_to(cr, tx, y);
  cairo_show_text(cr, s);
}

void Panel::layout(const Shared& shared) {
  widgets_.clear();
  const int module = std::clamp(
      static_cast<int>(shared.get(P_MODULE) + 0.5), 0, 4);

  auto knob = [&](uint32_t param, double x, double y, double r,
                  const char* label, std::vector<const char*> scale,
                  bool below = false) {
    Widget k{};
    k.kind = Kind::Knob;
    k.param = param;
    k.x = x;
    k.y = y;
    k.r = r;
    k.label = label;
    k.scale = std::move(scale);
    k.labelBelow = below;
    widgets_.push_back(k);
  };
  auto pill = [&](uint32_t param, double x, double y, double w, double h,
                  const char* label) {
    Widget p{};
    p.kind = Kind::Pill;
    p.param = param;
    p.x = x;
    p.y = y;
    p.w = w;
    p.h = h;
    p.label = label;
    widgets_.push_back(p);
  };
  auto cycle = [&](uint32_t param, double x, double y, double w, double h,
                   std::vector<const char*> labels) {
    Widget c{};
    c.kind = Kind::Cycle;
    c.param = param;
    c.x = x;
    c.y = y;
    c.w = w;
    c.h = h;
    c.scale = std::move(labels);
    widgets_.push_back(c);
  };
  auto option = [&](uint32_t param, double value, double x, double y,
                    double w, double h, const char* label) {
    Widget o{};
    o.kind = Kind::Option;
    o.param = param;
    o.value = value;
    o.x = x;
    o.y = y;
    o.w = w;
    o.h = h;
    o.label = label;
    widgets_.push_back(o);
  };

  {
    Widget p{};
    p.kind = Kind::Power;
    p.param = P_ENABLED;
    p.x = 34;
    p.y = 30;
    p.r = 14;
    widgets_.push_back(p);

    Widget r{};
    r.kind = Kind::MeterReset;
    r.x = 940;
    r.y = 12;
    r.w = 70;
    r.h = 38;
    r.label = "RESET";
    widgets_.push_back(r);
  }

  static const char* kTabs[5] = {"EQ", "COMP", "COLOR", "DENSITY", "LIMIT"};
  const double tabW = (980.0 - 4.0 * 8.0) / 5.0;
  for (int i = 0; i < 5; ++i) {
    Widget t{};
    t.kind = Kind::ModuleTab;
    t.param = P_MODULE;
    t.value = i;
    t.x = 30 + i * (tabW + 8.0);
    t.y = 70;
    t.w = tabW;
    t.h = 40;
    t.label = kTabs[i];
    widgets_.push_back(t);

    Widget led{};
    led.kind = Kind::ModulePower;
    led.param = moduleOnParam(i);
    led.value = i;
    led.x = t.x + t.w - 24;
    led.y = t.y + 13;
    led.w = 14;
    led.h = 14;
    widgets_.push_back(led);
  }

  static const char* kMeterTabs[3] = {"SPECTRUM", "LOUDNESS", "GR"};
  const double meterTabW = 78.0;
  for (int i = 0; i < 3; ++i) {
    Widget m{};
    m.kind = Kind::MeterTab;
    m.param = P_METER;
    m.value = i;
    m.x = 192 + i * (meterTabW + 5.0);
    m.y = 130;
    m.w = meterTabW;
    m.h = 20;
    m.label = kMeterTabs[i];
    widgets_.push_back(m);
  }

  if (module == 0) {
    knob(P_EQ_LOW_GAIN, 220, 455, 30, "LOW GAIN", sc(kScaleGain));
    knob(P_EQ_LOW_FREQ, 400, 455, 30, "LOW FREQ", sc(kScaleFreqLow));
    knob(P_EQ_MID_GAIN, 580, 455, 30, "MID GAIN", sc(kScaleGain));
    knob(P_EQ_MID_FREQ, 760, 455, 30, "MID FREQ", sc(kScaleFreqMid));
    knob(P_EQ_MID_Q, 220, 560, 26, "MID Q", sc(kScaleQ));
    knob(P_EQ_HIGH_GAIN, 400, 560, 26, "HIGH GAIN", sc(kScaleGain));
    knob(P_EQ_HIGH_FREQ, 580, 560, 26, "HIGH FREQ", sc(kScaleFreqHigh));
    knob(P_EQ_HPF, 760, 560, 26, "HPF", sc(kScaleHpf));
  } else if (module == 1) {
    knob(P_COMP_THRESHOLD, 220, 455, 30, "THRESHOLD", sc(kScaleThreshold));
    knob(P_COMP_RATIO, 400, 455, 30, "RATIO", sc(kRatioLabels));
    knob(P_COMP_ATTACK, 580, 455, 30, "ATTACK", sc(kScaleAttack));
    knob(P_COMP_RELEASE, 760, 455, 30, "RELEASE", sc(kScaleRelease));
    knob(P_COMP_KNEE, 220, 560, 26, "KNEE", sc(kScaleKnee));
    knob(P_COMP_SC_HPF, 400, 560, 26, "SC HPF", sc(kScaleScHpf));
    knob(P_COMP_MIX, 580, 560, 26, "MIX", sc(kScaleMix));
    pill(P_COMP_AUTO, 700, 548, 120, 24, "AUTO GAIN");
  } else if (module == 2) {
    cycle(P_COLOR_MODEL, 680, 348, 120, 26, sc(kModelLabels));
    knob(P_COLOR_DRIVE, 300, 455, 30, "DRIVE", sc(kScaleDrive));
    knob(P_COLOR_CHARACTER, 520, 455, 30, "CHARACTER", sc(kScaleCharacter));
    knob(P_COLOR_TONE, 740, 455, 30, "TONE", sc(kScaleTone));
    knob(P_COLOR_MIX, 300, 560, 26, "MIX", sc(kScaleMix));
    knob(P_COLOR_LEVEL, 520, 560, 26, "LEVEL", sc(kScaleLevel));
    option(P_COLOR_CLIP, 0, 660, 548, 52, 26, "OFF");
    option(P_COLOR_CLIP, 1, 716, 548, 52, 26, "SOFT");
    option(P_COLOR_CLIP, 2, 772, 548, 52, 26, "HARD");
  } else if (module == 3) {
    knob(P_DENS_INPUT, 300, 480, 38, "IN TRIM", sc(kScaleGain));
    knob(P_DENS_EFFECT, 520, 480, 40, "DENSITY", sc(kScaleDensity));
    knob(P_DENS_OUTPUT, 740, 480, 38, "OUT TRIM", sc(kScaleGain));
  } else {
    knob(P_LIMIT_CEILING, 380, 480, 38, "CEILING", sc(kScaleCeiling));
    knob(P_LIMIT_RELEASE, 660, 480, 38, "RELEASE", sc(kScaleLimitRelease));
    pill(P_LIMIT_TRUE_PEAK, 460, 570, 120, 26, "TRUE PEAK");
  }

  pill(moduleOnParam(module), 875, 348, 90, 26, "ON");
  knob(P_INPUT, 920, 455, 26, "INPUT", sc(kScaleGain));
  knob(P_OUTPUT, 920, 560, 26, "OUTPUT", sc(kScaleGain));
}

void Panel::draw(cairo_t* cr, Shared& shared, const PanelState& state) {
  layout(shared);
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.05, 0.06, 0.075);
  cairo_paint(cr);

  drawHeader(cr, shared, state);
  drawBody(cr);
  drawDisplay(cr, shared, state, 0);
  drawLeds(cr, shared);
  drawLevelMeter(cr, 44, 150, 26, 380, state.inLevelL, state.inLevelR, -60.0,
                 0.0, false);
  drawLevelMeter(cr, 974, 150, 26, 380, state.outLevelL, state.outLevelR,
                 -60.0, 3.0, true);

  static const char* const kCaptions[5] = {
      "Broad shelves, a wide mid bell and a gentle high-pass.",
      "Stereo-linked bus compressor with side-chain filter and mix.",
      "Single-band Patina colour: five models, drive, tone and level.",
      "Harmonic density and weight without adding fizz.",
      "Five-millisecond lookahead with a true-peak ceiling."};
  const int module =
      std::clamp(static_cast<int>(shared.get(P_MODULE) + 0.5), 0, 4);
  text(cr, kCaptions[module], 190, 366, 11.5, 0, 0.72, 0.76, 0.80);
  if (module == 2) {
    text(cr, "CLIP", 742, 540, 9.5, 1, 0.72, 0.76, 0.80);
  }

  for (const Widget& w : widgets_) drawWidget(cr, w, shared, state, 0);
  if (state.presetOpen) drawPresetList(cr, shared, state);
  text(cr, kSkin.caption, 520, 622, 12, 1, 0.78, 0.80, 0.82);
  cairo_restore(cr);
}

void Panel::drawHeader(cairo_t* cr, const Shared& shared,
                       const PanelState& state) const {
  cairo_pattern_t* bar = cairo_pattern_create_linear(0, 0, 0, 62);
  cairo_pattern_add_color_stop_rgb(bar, 0.0, kSkin.headerBg.r * 1.35,
                                   kSkin.headerBg.g * 1.35,
                                   kSkin.headerBg.b * 1.35);
  cairo_pattern_add_color_stop_rgb(bar, 1.0, kSkin.headerBg.r * 0.75,
                                   kSkin.headerBg.g * 0.75,
                                   kSkin.headerBg.b * 0.75);
  cairo_rectangle(cr, 0, 0, kPanelWidth, 62);
  cairo_set_source(cr, bar);
  cairo_fill(cr);
  cairo_pattern_destroy(bar);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.08);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 0, 0.5);
  cairo_line_to(cr, kPanelWidth, 0.5);
  cairo_stroke(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
  cairo_move_to(cr, 0, 61.5);
  cairo_line_to(cr, kPanelWidth, 61.5);
  cairo_stroke(cr);
  text(cr, "MASTERING SUITE", 520, 38, 18, 1, 0.82, 0.85, 0.84);

  roundRect(cr, 760, 12, 170, 38, 5);
  setRgb(cr, mix(kSkin.headerBg, {0, 0, 0}, 0.32));
  cairo_fill(cr);
  setRgb(cr, mix(kSkin.headerBg, {1, 1, 1}, 0.10), 0.5);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 760.5, 12.5, 169, 37, 5);
  cairo_stroke(cr);
  text(cr, "INTEGRATED", 772, 26, 8.5, 0, kSkin.legendDim.r,
       kSkin.legendDim.g, kSkin.legendDim.b);
  char lufs[32];
  std::snprintf(lufs, sizeof(lufs), "%.1f LUFS", state.lufsI);
  text(cr, lufs, 918, 45, 16, 2, 0.90, 0.92, 0.90);

  drawPresetBar(cr, shared, state);
}

void Panel::drawBody(cairo_t* cr) const {
  cairo_pattern_t* pat = cairo_pattern_create_linear(0, 122, 0, 604);
  cairo_pattern_add_color_stop_rgb(pat, 0.0, kSkin.bodyTop.r, kSkin.bodyTop.g,
                                   kSkin.bodyTop.b);
  cairo_pattern_add_color_stop_rgb(pat, 1.0, kSkin.bodyBottom.r,
                                   kSkin.bodyBottom.g, kSkin.bodyBottom.b);
  roundRect(cr, 16, 122, 1008, 482, 6);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  cairo_pattern_destroy(pat);

  if (grain_) {
    cairo_pattern_t* grain = cairo_pattern_create_for_surface(grain_);
    cairo_pattern_set_extend(grain, CAIRO_EXTEND_REPEAT);
    cairo_save(cr);
    cairo_rectangle(cr, 17, 123, 1006, 480);
    cairo_clip(cr);
    cairo_set_source(cr, grain);
    cairo_paint(cr);
    cairo_restore(cr);
    cairo_pattern_destroy(grain);
  }

  setRgb(cr, mix(kSkin.bodyTop, {1, 1, 1}, 0.10), 0.6);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 16.5, 122.5, 1007, 481, 6);
  cairo_stroke(cr);
  setRgb(cr, mix(kSkin.bodyBottom, {0, 0, 0}, 0.45), 0.6);
  roundRect(cr, 15.5, 121.5, 1009, 483, 6);
  cairo_stroke(cr);

  const double screwX[2] = {28.0, 1012.0};
  const double screwY[2] = {134.0, 590.0};
  for (int sy = 0; sy < 2; ++sy) {
    for (int sx = 0; sx < 2; ++sx) {
      const double x = screwX[sx];
      const double y = screwY[sy];
      cairo_pattern_t* metal = cairo_pattern_create_radial(
          x - 2.0, y - 2.0, 0.5, x, y, 6.0);
      const Rgb hi = mix(kSkin.screw, {1, 1, 1}, 0.38);
      const Rgb lo = mix(kSkin.screw, {0, 0, 0}, 0.55);
      cairo_pattern_add_color_stop_rgb(metal, 0.0, hi.r, hi.g, hi.b);
      cairo_pattern_add_color_stop_rgb(metal, 0.7, kSkin.screw.r,
                                       kSkin.screw.g, kSkin.screw.b);
      cairo_pattern_add_color_stop_rgb(metal, 1.0, lo.r, lo.g, lo.b);
      cairo_arc(cr, x, y, 5.4, 0, 2 * kPi);
      cairo_set_source(cr, metal);
      cairo_fill(cr);
      cairo_pattern_destroy(metal);
      cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.55);
      cairo_set_line_width(cr, 1.2);
      cairo_move_to(cr, x - 3.2, y + 2.0);
      cairo_line_to(cr, x + 3.2, y - 2.0);
      cairo_stroke(cr);
      cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.35);
      cairo_arc(cr, x, y, 5.4, 0, 2 * kPi);
      cairo_stroke(cr);
    }
  }
}

void Panel::drawDisplay(cairo_t* cr, const Shared& shared,
                        const PanelState& state, int /*model*/) const {
  const double fx = 182, fy = 122, fw = 678, fh = 210;

  // Recessed bezel, matching the Comp Suite display inset.
  cairo_pattern_t* bezel =
      cairo_pattern_create_linear(0, fy - 8, 0, fy + fh + 8);
  cairo_pattern_add_color_stop_rgb(bezel, 0.0, 0.24, 0.27, 0.32);
  cairo_pattern_add_color_stop_rgb(bezel, 1.0, 0.08, 0.09, 0.11);
  roundRect(cr, fx - 8, fy - 8, fw + 16, fh + 16, 7);
  cairo_set_source(cr, bezel);
  cairo_fill(cr);
  cairo_pattern_destroy(bezel);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.62);
  cairo_set_line_width(cr, 3.0);
  roundRect(cr, fx - 5, fy - 5, fw + 10, fh + 10, 5);
  cairo_stroke(cr);
  cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0.10);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, fx - 7.5, fy - 7.5, fw + 15, fh + 15, 7);
  cairo_stroke(cr);

  roundRect(cr, fx, fy, fw, fh, 3);
  cairo_pattern_t* face = cairo_pattern_create_linear(0, fy, 0, fy + fh);
  cairo_pattern_add_color_stop_rgb(face, 0.0, kSkin.meterFace.r,
                                   kSkin.meterFace.g, kSkin.meterFace.b);
  cairo_pattern_add_color_stop_rgb(face, 1.0, kSkin.meterFace.r * 0.86,
                                   kSkin.meterFace.g * 0.86,
                                   kSkin.meterFace.b * 0.86);
  cairo_set_source(cr, face);
  cairo_fill(cr);
  cairo_pattern_destroy(face);

  const int mode =
      std::clamp(static_cast<int>(shared.get(P_METER) + 0.5), 0, 2);
  if (mode == 0) {
    drawSpectrum(cr, shared, state, 0, fx, fy, fw, fh, true);
  } else if (mode == 1) {
    drawLoudness(cr, state, 0, fx, fy, fw, fh);
  } else {
    drawGr(cr, state, 0, fx, fy, fw, fh);
  }
}

void Panel::drawSpectrum(cairo_t* cr, const Shared& shared,
                         const PanelState& state, int /*model*/, double x,
                         double y, double w, double h, bool curve) const {
  constexpr double kMinDb = -78.0;
  constexpr double kMaxDb = 0.0;
  const double top = y + 34.0;
  const double bottom = y + h - 22.0;
  const double left = x + 38.0;
  const double right = x + w - 12.0;
  const double plotW = right - left;
  const double plotH = bottom - top;

  bool any = false;
  cairo_new_path(cr);
  cairo_move_to(cr, left, bottom);
  for (int i = 0; i < kSpectrumBins; ++i) {
    const double t = static_cast<double>(i) / (kSpectrumBins - 1);
    const double f = 20.0 * std::pow(1000.0, t);
    const double db =
        std::clamp(static_cast<double>(state.spectrum[i]), kMinDb, kMaxDb);
    const double px = freqToX(f, left, plotW);
    const double py = bottom - (db - kMinDb) / (kMaxDb - kMinDb) * plotH;
    cairo_line_to(cr, px, py);
    if (db > kMinDb + 2.0) any = true;
  }
  cairo_line_to(cr, right, bottom);
  cairo_close_path(cr);
  if (any) {
    setRgb(cr, kSkin.meterNeedle, 0.13);
    cairo_fill_preserve(cr);
    setRgb(cr, mix(kSkin.meterNeedle, {1, 1, 1}, 0.20), 0.85);
    cairo_set_line_width(cr, 1.4);
    cairo_stroke(cr);
  }
  cairo_new_path(cr);

  setRgb(cr, kSkin.meterScale, 0.16);
  cairo_set_line_width(cr, 1.0);
  for (int i = 1; i < 4; ++i) {
    const double py = top + plotH * i / 4.0;
    cairo_move_to(cr, left, py);
    cairo_line_to(cr, right, py);
    cairo_stroke(cr);
  }
  static const double kGridFreq[3] = {100.0, 1000.0, 10000.0};
  for (double f : kGridFreq) {
    const double px = freqToX(f, left, plotW);
    cairo_move_to(cr, px, top);
    cairo_line_to(cr, px, bottom);
    cairo_stroke(cr);
  }

  cairo_new_path(cr);
  static const char* const kDbLabels[4] = {"0", "-24", "-48", "-72"};
  for (int i = 0; i < 4; ++i) {
    const double py = top + plotH * i / 3.0;
    text(cr, kDbLabels[i], left - 8, py + 3, 8.5, 2, kSkin.meterScale.r,
         kSkin.meterScale.g, kSkin.meterScale.b);
  }

  static const char* const kFreqLabels[5] = {"20", "100", "1k", "10k", "20k"};
  static const double kFreqTicks[5] = {20.0, 100.0, 1000.0, 10000.0, 20000.0};
  for (int i = 0; i < 5; ++i) {
    const double px = freqToX(kFreqTicks[i], left, plotW);
    const int align = i == 0 ? 0 : (i == 4 ? 2 : 1);
    text(cr, kFreqLabels[i], px, bottom + 14, 8.5, align,
         kSkin.meterScale.r, kSkin.meterScale.g, kSkin.meterScale.b);
  }

  if (curve) {
    MasteringEq eq;
    const float rate = shared.sampleRate.load(std::memory_order_relaxed);
    eq.prepare(rate > 0.0f ? rate : 48000.0f);
    eq.setControls(controlsFrom(shared).eq);
    setRgb(cr, {0.92, 0.58, 0.22}, 0.95);
    cairo_set_line_width(cr, 1.8);
    const double mid = (top + bottom) * 0.5;
    const double span = plotH * 0.42 / 12.0;
    bool first = true;
    for (int i = 0; i <= 180; ++i) {
      const double t = i / 180.0;
      const double f = 20.0 * std::pow(1000.0, t);
      const double db = std::clamp(eq.gainDbAt(f), -12.0, 12.0);
      const double px = freqToX(f, left, plotW);
      const double py = mid - db * span;
      if (first) {
        cairo_move_to(cr, px, py);
        first = false;
      } else {
        cairo_line_to(cr, px, py);
      }
    }
    cairo_stroke(cr);
    text(cr, "EQ", right, y + 18, 10, 2, 0.92, 0.58, 0.22);
  }
}

void Panel::drawLoudness(cairo_t* cr, const PanelState& state,
                         int /*model*/, double x, double y, double w,
                         double h) const {
  const double col1 = x + w * 0.2;
  const double col2 = x + w * 0.5;
  const double col3 = x + w * 0.8;
  text(cr, "INTEGRATED", col1, y + 48, 10, 1, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  text(cr, "SHORT-TERM", col2, y + 48, 10, 1, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  text(cr, "MOMENTARY", col3, y + 48, 10, 1, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  char buf[64];
  std::snprintf(buf, sizeof(buf), "%.1f", state.lufsI);
  text(cr, buf, col1, y + 94, 38, 1, 0.10, 0.10, 0.10);
  std::snprintf(buf, sizeof(buf), "%.1f", state.lufsS);
  text(cr, buf, col2, y + 94, 38, 1, 0.10, 0.10, 0.10);
  std::snprintf(buf, sizeof(buf), "%.1f", state.lufsM);
  text(cr, buf, col3, y + 94, 38, 1, 0.10, 0.10, 0.10);
  text(cr, "LUFS", col1, y + 110, 10, 1, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  text(cr, "LUFS", col2, y + 110, 10, 1, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  text(cr, "LUFS", col3, y + 110, 10, 1, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);

  const double barX = x + 30;
  const double barW = w - 60;
  const double barY = y + 130;
  const double vals[3] = {state.lufsI, state.lufsS, state.lufsM};
  for (int i = 0; i < 3; ++i) {
    const double v = std::clamp((vals[i] + 30.0) / 30.0, 0.0, 1.0);
    const double by = barY + i * 16;
    roundRect(cr, barX, by, barW, 6, 2);
    setRgb(cr, mix(kSkin.meterFace, {0, 0, 0}, 0.45));
    cairo_fill(cr);
    roundRect(cr, barX, by, barW * v, 6, 2);
    setRgb(cr, {0.27, 0.82, 0.52}, 0.9);
    cairo_fill(cr);
  }

  std::snprintf(buf, sizeof(buf), "TRUE PEAK  %.2f dBTP", state.truePeak);
  text(cr, buf, x + 30, y + h - 34, 12, 0, 0.10, 0.10, 0.10);
  std::snprintf(buf, sizeof(buf), "LRA  %.1f LU     CORRELATION  %+.2f",
                state.lra, state.correlation);
  text(cr, buf, x + 30, y + h - 14, 12, 0, 0.10, 0.10, 0.10);
}

void Panel::drawGr(cairo_t* cr, const PanelState& state, int /*model*/,
                   double x, double y, double w, double h) const {
  const double x0 = x + 30;
  const double x1 = x + w - 30;
  char buf[64];
  text(cr, "COMPRESSOR", x0, y + 78, 12, 0, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  const double c = std::clamp(state.compGr / 12.0, 0.0, 1.0);
  roundRect(cr, x0, y + 88, x1 - x0, 12, 3);
  setRgb(cr, mix(kSkin.meterFace, {0, 0, 0}, 0.45));
  cairo_fill(cr);
  roundRect(cr, x0, y + 88, (x1 - x0) * c, 12, 3);
  setRgb(cr, {0.92, 0.58, 0.22}, 0.92);
  cairo_fill(cr);
  std::snprintf(buf, sizeof(buf), "%.1f dB", state.compGr);
  text(cr, buf, x1, y + 78, 11, 2, 0.10, 0.10, 0.10);

  text(cr, "LIMITER", x0, y + 132, 12, 0, kSkin.meterScale.r,
       kSkin.meterScale.g, kSkin.meterScale.b);
  const double l = std::clamp(state.limitGr / 12.0, 0.0, 1.0);
  roundRect(cr, x0, y + 142, x1 - x0, 12, 3);
  setRgb(cr, mix(kSkin.meterFace, {0, 0, 0}, 0.45));
  cairo_fill(cr);
  roundRect(cr, x0, y + 142, (x1 - x0) * l, 12, 3);
  setRgb(cr, {0.85, 0.30, 0.24}, 0.92);
  cairo_fill(cr);
  std::snprintf(buf, sizeof(buf), "%.1f dB", state.limitGr);
  text(cr, buf, x1, y + 132, 11, 2, 0.10, 0.10, 0.10);

  std::snprintf(buf, sizeof(buf), "TRUE PEAK  %.2f dBTP    CORRELATION  %+.2f",
                state.truePeak, state.correlation);
  text(cr, buf, x0, y + h - 20, 12, 0, 0.10, 0.10, 0.10);
}

void Panel::drawLeds(cairo_t* cr, const Shared& shared) const {
  roundRect(cr, 874, 144, 26, 186, 3);
  setRgb(cr, {0.04, 0.05, 0.06}, 0.75);
  cairo_fill(cr);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.5);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, 874.5, 144.5, 25, 185, 3);
  cairo_stroke(cr);

  const float db = std::max(shared.inPeakL.load(), shared.inPeakR.load());
  const int lit = std::clamp(static_cast<int>(std::lround((db + 60.0f) / 5.0f)),
                             0, 12);
  for (int i = 0; i < 12; ++i) {
    const double y = 152 + i * 14;
    Rgb c;
    if (i < 4) c = {0.27, 0.88, 0.42};
    else if (i < 8) c = {0.96, 0.82, 0.29};
    else c = {1.0, 0.30, 0.24};
    roundRect(cr, 879, y, 16, 10, 2);
    if (i < lit) setRgb(cr, c);
    else setRgb(cr, mix(c, {0.05, 0.05, 0.05}, 0.80));
    cairo_fill(cr);
  }
}

void Panel::drawLevelMeter(cairo_t* cr, double x, double y, double w, double h,
                           double dbL, double dbR, double minDb, double maxDb,
                           bool labelsRight) const {
  roundRect(cr, x, y, w, h, 2);
  setRgb(cr, kSkin.meterFrame);
  cairo_fill(cr);
  const double l = clamp01((dbL - minDb) / (maxDb - minDb));
  const double r = clamp01((dbR - minDb) / (maxDb - minDb));
  constexpr double inset = 4.0;
  constexpr double gap = 3.0;
  const double barW = (w - 2.0 * inset - gap) * 0.5;
  const double barTop = y + 5.0;
  const double barH = h - 10.0;
  const double greenTop = clamp01((-12.0 - minDb) / (maxDb - minDb));
  const double yellowTop = clamp01((-3.0 - minDb) / (maxDb - minDb));
  for (int ch = 0; ch < 2; ++ch) {
    const double lv = ch == 0 ? l : r;
    const double bx = x + inset + ch * (barW + gap);
    roundRect(cr, bx, barTop, barW, barH, 1);
    setRgb(cr, mix(kSkin.meterFace, {0, 0, 0}, 0.55));
    cairo_fill(cr);

    auto zone = [&](Rgb colour, double from, double to) {
      if (to <= from) return;
      setRgb(cr, colour, 0.88);
      cairo_rectangle(cr, bx, barTop + barH - to * barH, barW,
                      (to - from) * barH);
      cairo_fill(cr);
    };
    const double greenEnd = std::min(lv, greenTop);
    zone({0.27, 0.88, 0.42}, 0.0, greenEnd);
    if (lv > greenTop) {
      const double yellowEnd = std::min(lv, yellowTop);
      zone({0.96, 0.82, 0.29}, greenTop, yellowEnd);
      if (lv > yellowTop) zone({1.0, 0.30, 0.24}, yellowTop, lv);
    }
  }
  static const char* const kInputLabels[] = {"0",   "-6",  "-12", "-24",
                                             "-36", "-48", "-60"};
  static const double kInputValues[] = {0, -6, -12, -24, -36, -48, -60};
  static const char* const kOutputLabels[] = {"+3",  "0",   "-6",  "-12",
                                              "-24", "-36", "-48", "-60"};
  static const double kOutputValues[] = {3, 0, -6, -12, -24, -36, -48, -60};
  const bool outputScale = maxDb > 0.5;
  const char* const* labels = outputScale ? kOutputLabels : kInputLabels;
  const double* values = outputScale ? kOutputValues : kInputValues;
  const int labelCount = outputScale ? 8 : 7;
  for (int i = 0; i < labelCount; ++i) {
    const double fy = y + 6 + (h - 12) * (values[i] - maxDb) / (minDb - maxDb);
    if (labelsRight) {
      text(cr, labels[i], x + w + 7, fy + 4, 9, 0, kSkin.legendDim.r,
           kSkin.legendDim.g, kSkin.legendDim.b);
    } else {
      text(cr, labels[i], x - 7, fy + 4, 9, 2, kSkin.legendDim.r,
           kSkin.legendDim.g, kSkin.legendDim.b);
    }
  }
}

void Panel::drawWidget(cairo_t* cr, const Widget& w, const Shared& shared,
                       const PanelState& state, int /*model*/) const {
  switch (w.kind) {
    case Kind::Power: {
      // Shared Comp Suite power button, unchanged.
      const bool on = shared.get(P_ENABLED) > 0.5;
      cairo_arc(cr, w.x, w.y, w.r, 0, 2 * kPi);
      setRgb(cr, {0.12, 0.12, 0.12});
      cairo_fill(cr);
      cairo_arc(cr, w.x, w.y, w.r - 1.5, 0, 2 * kPi);
      setRgb(cr, on ? Rgb{0.36, 0.86, 0.46} : Rgb{0.42, 0.42, 0.42});
      cairo_set_line_width(cr, 2.2);
      cairo_new_path(cr);
      cairo_arc(cr, w.x, w.y, 7, -kPi / 2 + 0.6, -kPi / 2 + 2 * kPi - 0.6);
      cairo_stroke(cr);
      cairo_move_to(cr, w.x, w.y - 9);
      cairo_line_to(cr, w.x, w.y - 1);
      cairo_stroke(cr);
      break;
    }
    case Kind::Knob:
      drawKnob(cr, w, shared.get(w.param), 0);
      break;
    case Kind::MeterTab: {
      const bool lit = std::fabs(shared.get(P_METER) - w.value) < 1e-3;
      roundRect(cr, w.x, w.y, w.w, w.h, 3);
      setRgb(cr, lit ? kSkin.tabOn : mix(kSkin.meterFace, {1, 1, 1}, 0.06));
      cairo_fill(cr);
      setRgb(cr, mix(lit ? kSkin.tabOn : kSkin.meterFrame, {0, 0, 0}, 0.35),
             0.7);
      cairo_set_line_width(cr, 1.0);
      roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 3);
      cairo_stroke(cr);
      const Rgb tc = lit ? kSkin.tabTextOn : Rgb{0.18, 0.21, 0.25};
      text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 3.2, 8.5, 1, tc.r,
           tc.g, tc.b);
      break;
    }
    case Kind::ModuleTab: {
      const bool lit = std::fabs(shared.get(P_MODULE) - w.value) < 1e-3;
      roundRect(cr, w.x, w.y, w.w, w.h, 4);
      setRgb(cr, lit ? kSkin.tabOn : kSkin.tabOff);
      cairo_fill(cr);
      setRgb(cr, mix(lit ? kSkin.tabOn : kSkin.tabOff, {0, 0, 0}, 0.35), 0.7);
      cairo_set_line_width(cr, 1.0);
      roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 4);
      cairo_stroke(cr);
      const Rgb tc = lit ? kSkin.tabTextOn : kSkin.tabTextOff;
      text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 5, 12, 1, tc.r,
           tc.g, tc.b);
      break;
    }
    case Kind::ModulePower: {
      const bool on = shared.get(w.param) > 0.5;
      roundRect(cr, w.x, w.y, w.w, w.h, 2);
      setRgb(cr, on ? Rgb{0.34, 0.86, 0.48} : Rgb{0.18, 0.20, 0.24});
      cairo_fill(cr);
      setRgb(cr, {0, 0, 0}, 0.4);
      cairo_set_line_width(cr, 1.0);
      roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 2);
      cairo_stroke(cr);
      break;
    }
    case Kind::MeterReset: {
      roundRect(cr, w.x, w.y, w.w, w.h, 5);
      setRgb(cr, kSkin.btnOff);
      cairo_fill(cr);
      setRgb(cr, {0.0, 0.0, 0.0}, 0.35);
      cairo_set_line_width(cr, 1.0);
      roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1, w.h - 1, 5);
      cairo_stroke(cr);
      text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 3.5, 9.5, 1,
           kSkin.btnText.r, kSkin.btnText.g, kSkin.btnText.b);
      break;
    }
    case Kind::Option: {
      const int value = static_cast<int>(std::lround(shared.get(w.param)));
      drawPill(cr, w, value == static_cast<int>(w.value), 0);
      break;
    }
    case Kind::Pill:
      if (isModuleOnParam(w.param)) {
        Widget copy = w;
        copy.label = shared.get(w.param) > 0.5 ? "ON" : "OFF";
        drawPill(cr, copy, shared.get(w.param) > 0.5, 0);
      } else {
        drawPill(cr, w, shared.get(w.param) > 0.5, 0);
      }
      break;
    case Kind::Cycle: {
      const int count = static_cast<int>(w.scale.size());
      const int v =
          std::clamp(static_cast<int>(shared.get(w.param) + 0.5), 0,
                     std::max(count - 1, 0));
      Widget copy = w;
      copy.label = count > 0 ? w.scale[v] : "";
      drawPill(cr, copy, true, 0);
      break;
    }
  }
  (void)state;
}

void Panel::drawKnob(cairo_t* cr, const Widget& w, double value,
                     int /*model*/) const {
  const double cx = w.x, cy = w.y, r = w.r;

  setRgb(cr, kSkin.legendDim, 0.55);
  cairo_set_line_width(cr, 1.0);
  for (int i = 0; i <= 10; ++i) {
    const double a = knobAngle(i / 10.0);
    cairo_move_to(cr, cx + std::cos(a) * r * 1.10, cy + std::sin(a) * r * 1.10);
    cairo_line_to(cr, cx + std::cos(a) * r * 1.19, cy + std::sin(a) * r * 1.19);
    cairo_stroke(cr);
  }

  const size_t n = w.scale.size();
  for (size_t i = 0; i < n; ++i) {
    const double f = n > 1 ? static_cast<double>(i) / (n - 1) : 0.0;
    const double a = knobAngle(f);
    text(cr, w.scale[i], cx + std::cos(a) * (r * w.scaleRadius),
         cy + std::sin(a) * (r * w.scaleRadius) + 4, 10, 1, kSkin.legend.r,
         kSkin.legend.g, kSkin.legend.b);
  }

  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.18);
  cairo_arc(cr, cx + 0.5, cy + 1.5, r * 1.03, 0, 2 * kPi);
  cairo_fill(cr);

  cairo_pattern_t* body = cairo_pattern_create_linear(0, cy - r, 0, cy + r);
  const Rgb top = mix(kSkin.knobSkirt, {1, 1, 1}, 0.16);
  const Rgb bot = mix(kSkin.knobSkirt, {0, 0, 0}, 0.30);
  cairo_pattern_add_color_stop_rgb(body, 0.0, top.r, top.g, top.b);
  cairo_pattern_add_color_stop_rgb(body, 1.0, bot.r, bot.g, bot.b);
  cairo_arc(cr, cx, cy, r, 0, 2 * kPi);
  cairo_set_source(cr, body);
  cairo_fill(cr);
  cairo_pattern_destroy(body);
  cairo_set_line_width(cr, 1.0);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.45);
  cairo_arc(cr, cx, cy, r - 0.5, 0, 2 * kPi);
  cairo_stroke(cr);

  cairo_pattern_t* cap = cairo_pattern_create_radial(
      cx - r * 0.35, cy - r * 0.40, r * 0.05, cx, cy, r * 0.98);
  const Rgb hi = mix(kSkin.knobCap, {1, 1, 1}, 0.24);
  cairo_pattern_add_color_stop_rgb(cap, 0.0, hi.r, hi.g, hi.b);
  cairo_pattern_add_color_stop_rgb(cap, 0.75, kSkin.knobCap.r, kSkin.knobCap.g,
                                   kSkin.knobCap.b);
  cairo_pattern_add_color_stop_rgb(cap, 1.0, kSkin.knobCapEdge.r,
                                   kSkin.knobCapEdge.g, kSkin.knobCapEdge.b);
  cairo_arc(cr, cx, cy, r * 0.82, 0, 2 * kPi);
  cairo_set_source(cr, cap);
  cairo_fill(cr);
  cairo_pattern_destroy(cap);

  if (kSkin.knurled) {
    cairo_set_line_width(cr, 0.8);
    cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.22);
    for (int k = 0; k < 60; ++k) {
      const double a = k * 2 * kPi / 60.0;
      cairo_move_to(cr, cx + std::cos(a) * r * 0.86,
                    cy + std::sin(a) * r * 0.86);
      cairo_line_to(cr, cx + std::cos(a) * r * 0.99,
                    cy + std::sin(a) * r * 0.99);
      cairo_stroke(cr);
    }
  }

  const ParamSpec& spec = kParamSpecs[w.param];
  const double span = spec.max - spec.min;
  const double frac =
      span > 0.0 ? (std::min(value, spec.max) - spec.min) / span : 0.0;
  const double a = knobAngle(clamp01(frac));
  cairo_set_line_width(cr, 2.0);
  setRgb(cr, kSkin.knobIndicator);
  cairo_move_to(cr, cx + std::cos(a) * r * 0.26, cy + std::sin(a) * r * 0.26);
  cairo_line_to(cr, cx + std::cos(a) * r * 0.78, cy + std::sin(a) * r * 0.78);
  cairo_stroke(cr);

  const double labelY = w.labelBelow ? cy + r * 1.44 + 20 : cy - r * 1.44 - 18;
  text(cr, w.label, cx, labelY, 10.5, 1, kSkin.legend.r, kSkin.legend.g,
       kSkin.legend.b);
}

void Panel::drawPill(cairo_t* cr, const Widget& w, bool on,
                     int /*model*/) const {
  roundRect(cr, w.x, w.y, w.w, w.h, 5);
  setRgb(cr, on ? kSkin.btnOn : kSkin.btnOff);
  cairo_fill(cr);
  setRgb(cr, {0.0, 0.0, 0.0}, 0.35);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, w.x + 0.5, w.y + 0.5, w.w - 1.0, w.h - 1.0, 5);
  cairo_stroke(cr);
  const Rgb tc = on ? kSkin.btnTextOn : kSkin.btnText;
  text(cr, w.label, w.x + w.w * 0.5, w.y + w.h * 0.5 + 3.5, 9.5, 1, tc.r,
       tc.g, tc.b);
}

void Panel::tick(const Shared& shared, PanelState& state) const {
  // Short fall-off so the meters stay readable at 30 fps without losing the
  // transient that just went through.
  auto peakHold = [](float current, float incoming, float fall) {
    return std::max(incoming, current - fall);
  };
  state.inLevelL = peakHold(state.inLevelL, shared.inPeakL.load(), 0.9f);
  state.inLevelR = peakHold(state.inLevelR, shared.inPeakR.load(), 0.9f);
  state.outLevelL = peakHold(state.outLevelL, shared.outPeakL.load(), 0.9f);
  state.outLevelR = peakHold(state.outLevelR, shared.outPeakR.load(), 0.9f);
  state.lufsI = shared.lufsIntegrated.load();
  state.lufsS = shared.lufsShortTerm.load();
  state.lufsM = shared.lufsMomentary.load();
  state.truePeak =
      peakHold(state.truePeak,
               std::max(shared.truePeakL.load(), shared.truePeakR.load()), 0.2f);
  state.lra = shared.lra.load();
  state.correlation = shared.correlation.load();
  state.compGr = peakHold(state.compGr, shared.compGr.load(), 0.5f);
  state.limitGr = peakHold(state.limitGr, shared.limitGr.load(), 0.5f);
  for (int i = 0; i < kSpectrumBins; ++i) {
    const float target = shared.spectrum[i].load();
    state.spectrum[i] += (target - state.spectrum[i]) * 0.45f;
  }
}

bool Panel::hitWidget(const Widget& w, double x, double y) const {
  switch (w.kind) {
    case Kind::Knob:
    case Kind::Power: {
      const double dx = x - w.x, dy = y - w.y;
      return dx * dx + dy * dy <= w.r * w.r * 1.3;
    }
    default:
      return x >= w.x && x <= w.x + w.w && y >= w.y && y <= w.y + w.h;
  }
}

bool Panel::pointerDown(Shared& shared, PanelState& state, double x,
                        double y) {
  if (handlePresetClick(shared, state, x, y)) return true;
  for (const Widget& w : widgets_) {
    if (!hitWidget(w, x, y)) continue;
    switch (w.kind) {
      case Kind::Knob:
        dragParam_ = static_cast<int>(w.param);
        dragStartY_ = y;
        dragStartValue_ = shared.get(w.param);
        return false;
      case Kind::Power:
        shared.set(P_ENABLED, shared.get(P_ENABLED) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::ModuleTab:
      case Kind::MeterTab:
      case Kind::Cycle:
        if (w.kind == Kind::Cycle) {
          const int count = static_cast<int>(w.scale.size());
          const int v = static_cast<int>(shared.get(w.param) + 0.5);
          shared.set(w.param, static_cast<double>((v + 1) % count));
        } else {
          shared.set(w.param, w.value);
        }
        state.presetDirty = true;
        return true;
      case Kind::ModulePower:
      case Kind::Pill:
        shared.set(w.param, shared.get(w.param) > 0.5 ? 0.0 : 1.0);
        state.presetDirty = true;
        return true;
      case Kind::Option:
        shared.set(w.param, w.value);
        state.presetDirty = true;
        return true;
      case Kind::MeterReset:
        shared.requestMeterReset();
        return true;
    }
  }
  return false;
}

bool Panel::pointerMove(Shared& shared, PanelState& state, double x,
                        double y, bool fine) {
  (void)x;
  if (dragParam_ < 0) return false;
  const uint32_t param = static_cast<uint32_t>(dragParam_);
  const ParamSpec& spec = kParamSpecs[param];
  const double perPixel = (spec.max - spec.min) / (fine ? 800.0 : 200.0);
  const double v = dragStartValue_ + (dragStartY_ - y) * perPixel;
  shared.set(param, v);
  state.presetDirty = true;
  return true;
}

void Panel::pointerUp() { dragParam_ = -1; }

bool Panel::wheel(Shared& shared, PanelState& state, double x, double y,
                  double dy) {
  if (state.presetOpen && x >= kPresetX && x <= kPresetX + kPresetW &&
      y >= kListY && y <= kListY + kListVisible * kListRow + 10) {
    int rows = kPresetCount;
    const char* last = nullptr;
    for (int i : presetOrder()) {
      if (!last || std::strcmp(last, kPresets[i].category) != 0) {
        last = kPresets[i].category;
        ++rows;
      }
    }
    const int maxScroll = std::max(0, rows - static_cast<int>(kListVisible));
    state.presetScroll = std::min(
        maxScroll, std::max(0, state.presetScroll - static_cast<int>(dy)));
    return true;
  }
  for (const Widget& w : widgets_) {
    if (w.kind == Kind::Knob && hitWidget(w, x, y)) {
      const ParamSpec& spec = kParamSpecs[w.param];
      if (spec.stepped) {
        shared.set(w.param, shared.get(w.param) - (dy > 0 ? 1.0 : -1.0));
      } else {
        shared.set(w.param,
                   shared.get(w.param) - dy * (spec.max - spec.min) * 0.01);
      }
      state.presetDirty = true;
      return true;
    }
  }
  return false;
}

void Panel::applyPreset(Shared& shared, PanelState& state, int index) const {
  if (index < 0 || index >= kPresetCount) return;
  for (uint32_t p = 0; p < P_COUNT; ++p) {
    shared.set(p, kParamSpecs[p].def);
  }
  const Preset& p = kPresets[index];
  for (const PresetValue& v : p.values) {
    if (v.param >= 0 && v.param < static_cast<int>(P_COUNT)) {
      shared.set(static_cast<uint32_t>(v.param), v.value);
    }
  }
  state.presetIndex = index;
  state.presetDirty = false;
  state.presetOpen = 0;
}

void Panel::drawPresetBar(cairo_t* cr, const Shared& shared,
                          const PanelState& state) const {
  roundRect(cr, kPresetX, kPresetY, kPresetW, kPresetH, 5);
  setRgb(cr, mix(kSkin.headerBg, {1, 1, 1}, 0.12));
  cairo_fill(cr);
  setRgb(cr, mix(kSkin.headerBg, {0, 0, 0}, 0.35));
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kPresetX + 0.5, kPresetY + 0.5, kPresetW - 1, kPresetH - 1, 5);
  cairo_stroke(cr);

  text(cr, "<", kPresetX + 18, kPresetY + 23, 14, 1, kSkin.tabTextOn.r,
       kSkin.tabTextOn.g, kSkin.tabTextOn.b);
  text(cr, ">", kPresetX + kPresetW - 18, kPresetY + 23, 14, 1,
       kSkin.tabTextOn.r, kSkin.tabTextOn.g, kSkin.tabTextOn.b);
  char label[96];
  if (state.presetIndex >= 0 && state.presetIndex < kPresetCount) {
    std::snprintf(label, sizeof(label), "%s%s", kPresets[state.presetIndex].name,
                  state.presetDirty ? " *" : "");
  } else {
    std::snprintf(label, sizeof(label), "Init%s", state.presetDirty ? " *" : "");
  }
  text(cr, label, kPresetX + kPresetW * 0.5, kPresetY + 23, 13, 1,
       kSkin.tabTextOn.r, kSkin.tabTextOn.g, kSkin.tabTextOn.b);
  (void)shared;
}

void Panel::drawPresetList(cairo_t* cr, const Shared& shared,
                           const PanelState& state) const {
  const double h = kListVisible * kListRow + 10;
  roundRect(cr, kPresetX, kListY, kPresetW, h, 5);
  cairo_set_source_rgba(cr, 0.05, 0.06, 0.07, 0.97);
  cairo_fill(cr);
  setRgb(cr, kSkin.tabOn, 0.7);
  cairo_set_line_width(cr, 1.0);
  roundRect(cr, kPresetX + 0.5, kListY + 0.5, kPresetW - 1, h - 1, 5);
  cairo_stroke(cr);

  cairo_save(cr);
  roundRect(cr, kPresetX + 1, kListY + 1, kPresetW - 2, h - 2, 4);
  cairo_clip(cr);

  const char* lastCategory = nullptr;
  double y = kListY + 6 - state.presetScroll * kListRow;
  for (int i : presetOrder()) {
    const Preset& p = kPresets[i];
    if (!lastCategory || std::strcmp(lastCategory, p.category) != 0) {
      lastCategory = p.category;
      text(cr, p.category, kPresetX + 14, y + 15, 11, 0, 0.52, 0.66, 0.80);
      y += kListRow;
      if (y > kListY + h) break;
    }
    if (y + kListRow > kListY && y < kListY + h) {
      if (i == state.presetIndex) {
        roundRect(cr, kPresetX + 6, y + 1, kPresetW - 12, kListRow - 2, 3);
        setRgb(cr, kSkin.tabOn, 0.85);
        cairo_fill(cr);
      }
      text(cr, p.name, kPresetX + 26, y + 15, 12.5, 0,
           i == state.presetIndex ? 1.0 : 0.86,
           i == state.presetIndex ? 1.0 : 0.88,
           i == state.presetIndex ? 1.0 : 0.90);
      text(cr, p.category, kPresetX + kPresetW - 16, y + 15, 9.5, 2,
           i == state.presetIndex ? 0.88 : 0.56,
           i == state.presetIndex ? 0.92 : 0.60,
           i == state.presetIndex ? 0.96 : 0.66);
    }
    y += kListRow;
  }
  cairo_restore(cr);
  (void)shared;
}

bool Panel::handlePresetClick(Shared& shared, PanelState& state, double x,
                              double y) {
  if (y >= kPresetY && y <= kPresetY + kPresetH) {
    if (x >= kPresetX && x <= kPresetX + 36) {
      const int next = state.presetIndex <= 0 ? 0 : state.presetIndex - 1;
      applyPreset(shared, state, next);
      return true;
    }
    if (x >= kPresetX + kPresetW - 36 && x <= kPresetX + kPresetW) {
      const int next = std::min(kPresetCount - 1, state.presetIndex + 1);
      applyPreset(shared, state, next);
      return true;
    }
    if (x > kPresetX + 36 && x < kPresetX + kPresetW - 36) {
      state.presetOpen = state.presetOpen ? 0 : 1;
      if (state.presetOpen) state.presetScroll = 0;
      return true;
    }
  }
  if (!state.presetOpen) return false;
  const double h = kListVisible * kListRow + 10;
  if (x < kPresetX || x > kPresetX + kPresetW || y < kListY || y > kListY + h) {
    state.presetOpen = 0;
    return true;
  }
  double rowY = kListY + 6 - state.presetScroll * kListRow;
  const char* lastCategory = nullptr;
  for (int i : presetOrder()) {
    if (!lastCategory || std::strcmp(lastCategory, kPresets[i].category) != 0) {
      lastCategory = kPresets[i].category;
      rowY += kListRow;
    }
    if (y >= rowY && y < rowY + kListRow) {
      applyPreset(shared, state, i);
      return true;
    }
    rowY += kListRow;
  }
  return true;
}

} // namespace mastering
