// Mastering Suite — CLAP plugin
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/all.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo-xlib.h>
#include <dlfcn.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "dsp/engine.hpp"
#include "params.hpp"
#include "value_text.hpp"
#include "ui/geometry.hpp"
#include "ui/panel.hpp"
#include "version.hpp"

namespace mastering {
namespace {

constexpr const char* kPluginId = "com.tylorismusic.masteringsuite";
constexpr const char* kPluginName = "Mastering Suite";
constexpr const char* kVendor = "tylorismusic";

std::string bundleResourceDir() {
  Dl_info info{};
  if (dladdr(reinterpret_cast<void*>(&bundleResourceDir), &info) &&
      info.dli_fname) {
    std::string path = info.dli_fname;
    const size_t slash = path.rfind('/');
    if (slash != std::string::npos) path = path.substr(0, slash);
    return path + "/MasteringSuite.resources";
  }
  return "MasteringSuite.resources";
}

class Editor {
public:
  explicit Editor(Shared& shared) : shared_(shared) {}

  std::function<void()> requestFlush;

  double scale() const { return scale_.load(std::memory_order_relaxed); }
  UiSize size() const { return sizeForScale(scale()); }

  void resize(double scale) {
    scale_.store(clampScale(scale), std::memory_order_relaxed);
  }

  void resetUiState() {
    resetUi_.store(true, std::memory_order_relaxed);
  }

  bool open(void* parent) {
    std::unique_lock<std::mutex> lock(mutex_);
    if (thread_.joinable()) return opened_;
    parentWindow_ =
        parent ? static_cast<Window>(reinterpret_cast<uintptr_t>(parent)) : 0;
    opened_ = false;
    openFailed_ = false;
    quitting_ = false;
    thread_ = std::thread([this] { run(); });
    cv_.wait_for(lock, std::chrono::seconds(3),
                 [this] { return opened_ || openFailed_; });
    return opened_;
  }

  void close() {
    {
      std::lock_guard<std::mutex> lock(mutex_);
      quitting_ = true;
    }
    if (thread_.joinable()) thread_.join();
    std::lock_guard<std::mutex> lock(mutex_);
    opened_ = false;
  }

  ~Editor() { close(); }

private:
  void run() {
    Display* dpy = XOpenDisplay(nullptr);
    if (!dpy) {
      std::lock_guard<std::mutex> lock(mutex_);
      openFailed_ = true;
      cv_.notify_all();
      return;
    }
    const int screen = DefaultScreen(dpy);
    const Window parent = parentWindow_ ? parentWindow_ : RootWindow(dpy, screen);
    UiSize current = size();

    XSetWindowAttributes attrs{};
    attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                       PointerMotionMask | Button1MotionMask | ButtonMotionMask |
                       KeyPressMask;
    const Window win =
        XCreateWindow(dpy, parent, 0, 0, current.width, current.height, 0,
                      CopyFromParent, InputOutput, CopyFromParent, CWEventMask,
                      &attrs);
    const UiSize minSize = sizeForScale(kMinUiScale);
    const UiSize maxSize = sizeForScale(kMaxUiScale);
    XSizeHints hints{};
    hints.flags = PAspect | PMinSize | PMaxSize;
    hints.min_aspect.x = hints.max_aspect.x = kPanelWidth;
    hints.min_aspect.y = hints.max_aspect.y = kPanelHeight;
    hints.min_width = static_cast<int>(minSize.width);
    hints.min_height = static_cast<int>(minSize.height);
    hints.max_width = static_cast<int>(maxSize.width);
    hints.max_height = static_cast<int>(maxSize.height);
    XSetWMNormalHints(dpy, win, &hints);
    XStoreName(dpy, win, kPluginName);
    XMapRaised(dpy, win);
    XFlush(dpy);
    display_ = dpy;
    window_ = win;

    panel_.loadFont(bundleResourceDir());
    {
      std::lock_guard<std::mutex> lock(mutex_);
      opened_ = true;
      cv_.notify_all();
    }

    XWindowAttributes wa{};
    XGetWindowAttributes(dpy, win, &wa);
    cairo_surface_t* windowSurface = cairo_xlib_surface_create(
        dpy, win, wa.visual, current.width, current.height);
    const cairo_format_t imageFormat =
        wa.depth == 32 ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24;
    cairo_surface_t* backing = nullptr;
    cairo_t* cr = nullptr;
    auto rebuildBacking = [&]() {
      if (cr) {
        cairo_destroy(cr);
        cr = nullptr;
      }
      if (backing) {
        cairo_surface_destroy(backing);
        backing = nullptr;
      }
      backing = cairo_image_surface_create(imageFormat, current.width,
                                           current.height);
      cr = cairo_create(backing);
    };
    rebuildBacking();

    while (true) {
      {
        std::lock_guard<std::mutex> lock(mutex_);
        if (quitting_) break;
      }
      const UiSize want = size();
      if (want.width != current.width || want.height != current.height) {
        current = want;
        XResizeWindow(dpy, win, current.width, current.height);
        cairo_xlib_surface_set_size(windowSurface, current.width, current.height);
        rebuildBacking();
      }
      while (XPending(dpy) > 0) {
        XEvent ev;
        XNextEvent(dpy, &ev);
        handleEvent(ev);
      }
      if (resetUi_.exchange(false, std::memory_order_relaxed)) {
        state_.presetIndex = -1;
        state_.presetDirty = false;
        state_.presetOpen = 0;
        state_.presetScroll = 0;
        state_.inLevelL = state_.inLevelR = -120.0f;
        state_.outLevelL = state_.outLevelR = -120.0f;
        for (int i = 0; i < kSpectrumBins; ++i) state_.spectrum[i] = -120.0f;
      }
      panel_.tick(shared_, state_);
      cairo_save(cr);
      cairo_scale(cr, scale(), scale());
      panel_.draw(cr, shared_, state_);
      cairo_restore(cr);
      cairo_surface_flush(backing);
      cairo_t* xcr = cairo_create(windowSurface);
      cairo_set_source_surface(xcr, backing, 0, 0);
      cairo_paint(xcr);
      cairo_destroy(xcr);
      cairo_surface_flush(windowSurface);
      XFlush(dpy);
      if (requestFlush && shared_.anyDirty()) requestFlush();
      std::this_thread::sleep_for(std::chrono::milliseconds(33));
    }

    if (cr) cairo_destroy(cr);
    if (backing) cairo_surface_destroy(backing);
    if (windowSurface) cairo_surface_destroy(windowSurface);
    display_ = nullptr;
    window_ = 0;
    XDestroyWindow(dpy, win);
    XCloseDisplay(dpy);
  }

  void handleEvent(const XEvent& ev) {
    const double s = scale();
    switch (ev.type) {
      case ButtonPress: {
        if (display_ && window_) {
          XSetInputFocus(display_, window_, RevertToParent, CurrentTime);
          XFlush(display_);
        }
        const double x = ev.xbutton.x / s;
        const double y = ev.xbutton.y / s;
        if (ev.xbutton.button == Button4) {
          panel_.wheel(shared_, state_, x, y, 1.0);
        } else if (ev.xbutton.button == Button5) {
          panel_.wheel(shared_, state_, x, y, -1.0);
        } else if (ev.xbutton.button == Button1) {
          panel_.pointerDown(shared_, state_, x, y);
        } else if (ev.xbutton.button == Button3) {
          panel_.beginTextEntry(shared_, state_, x, y);
        }
        break;
      }
      case ButtonRelease:
        panel_.pointerUp();
        break;
      case KeyPress: {
        char buf[8] = {};
        KeySym sym = 0;
        XKeyEvent key = ev.xkey;
        const int n = XLookupString(&key, buf, sizeof(buf), &sym, nullptr);
        if (sym == XK_Return || sym == XK_KP_Enter) {
          panel_.textKey(shared_, state_, Panel::kTextEnter, 0);
        } else if (sym == XK_Escape) {
          panel_.textKey(shared_, state_, Panel::kTextEscape, 0);
        } else if (sym == XK_BackSpace) {
          panel_.textKey(shared_, state_, Panel::kTextBackspace, 0);
        } else if (n > 0) {
          panel_.textKey(shared_, state_, 0, buf[0]);
        }
        break;
      }
      case MotionNotify: {
        const bool fine = (ev.xmotion.state & ShiftMask) != 0;
        panel_.pointerMove(shared_, state_, ev.xmotion.x / s, ev.xmotion.y / s,
                           fine);
        break;
      }
      default:
        break;
    }
  }

  Shared& shared_;
  PanelState state_;
  Panel panel_;
  std::atomic<double> scale_{1.0};
  std::atomic<bool> resetUi_{false};
  std::mutex mutex_;
  std::condition_variable cv_;
  Window parentWindow_ = 0;
  Display* display_ = nullptr;
  Window window_ = 0;
  bool opened_ = false;
  bool openFailed_ = false;
  bool quitting_ = false;
  std::thread thread_;
};

struct Plugin {
  clap_plugin_t plugin{};
  const clap_host_t* host = nullptr;
  Shared shared;
  MasteringEngine engine;
  Editor editor{shared};
  Meters meters;
  std::vector<float> monoRight;
  std::atomic<uint32_t> reportedLatency{0xffffffffu};
  std::atomic<bool> resetEngine{false};
};

const char* const kFeatures[] = {CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
                                 CLAP_PLUGIN_FEATURE_MASTERING,
                                 CLAP_PLUGIN_FEATURE_EQUALIZER,
                                 CLAP_PLUGIN_FEATURE_COMPRESSOR,
                                 CLAP_PLUGIN_FEATURE_LIMITER,
                                 CLAP_PLUGIN_FEATURE_STEREO, nullptr};

const clap_plugin_descriptor_t kDescriptor = {
    CLAP_VERSION,
    kPluginId,
    kPluginName,
    kVendor,
    "https://github.com/tylorismusic-commits/mastering-suite",
    "",
    "",
    kVersion,
    "Mastering chain: smooth tone EQ, bus compressor, Patina colour, harmonic density and a true-peak limiter, with calibrated loudness metering.",
    kFeatures,
};

void writeName(char* dst, size_t size, const char* src) {
  std::strncpy(dst, src, size - 1);
  dst[size - 1] = '\0';
}

Plugin* self(const clap_plugin_t* p) {
  return static_cast<Plugin*>(p->plugin_data);
}

// --- Parameters -------------------------------------------------------------

uint32_t paramsCount(const clap_plugin_t*) { return P_COUNT; }

bool paramsInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
  if (index >= P_COUNT) return false;
  const ParamSpec& s = kParamSpecs[index];
  std::memset(info, 0, sizeof(*info));
  info->id = index;
  info->flags = CLAP_PARAM_IS_AUTOMATABLE;
  if (s.stepped) info->flags |= CLAP_PARAM_IS_STEPPED;
  if (s.hidden) info->flags |= CLAP_PARAM_IS_HIDDEN;
  writeName(info->name, sizeof(info->name), s.name);
  writeName(info->module, sizeof(info->module), s.module);
  info->min_value = s.min;
  info->max_value = s.max;
  info->default_value = s.def;
  return true;
}

bool paramsValue(const clap_plugin_t* p, clap_id id, double* value) {
  if (id >= P_COUNT) return false;
  *value = self(p)->shared.get(id);
  return true;
}

bool paramsValueToText(const clap_plugin_t*, clap_id id, double value,
                       char* out, uint32_t size) {
  if (id >= P_COUNT) return false;
  const std::string text = formatParamValue(id, value);
  std::snprintf(out, size, "%s", text.c_str());
  return true;
}

bool paramsTextToValue(const clap_plugin_t*, clap_id id, const char* text,
                       double* value) {
  if (!text || id >= P_COUNT) return false;
  return parseParamValue(id, text, value);
}

void applyEvents(Plugin* s, const clap_input_events_t* in) {
  const uint32_t count = in->size(in);
  for (uint32_t i = 0; i < count; ++i) {
    const clap_event_header_t* hdr = in->get(in, i);
    if (hdr->space_id != CLAP_CORE_EVENT_SPACE_ID) continue;
    if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
      const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
      if (pv->param_id < P_COUNT && !s->shared.isDirty(pv->param_id)) {
        s->shared.setFromHost(pv->param_id, pv->value);
      }
    }
  }
}

void emitDirty(Plugin* s, const clap_output_events_t* out) {
  if (!out) return;
  for (uint32_t p = 0; p < P_COUNT; ++p) {
    if (!s->shared.takeDirty(p)) continue;
    clap_event_param_value_t ev{};
    ev.header.size = sizeof(ev);
    ev.header.time = 0;
    ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
    ev.header.type = CLAP_EVENT_PARAM_VALUE;
    ev.param_id = p;
    ev.note_id = -1;
    ev.port_index = -1;
    ev.channel = -1;
    ev.key = -1;
    ev.value = s->shared.get(p);
    out->try_push(out, &ev.header);
  }
}

void paramsFlush(const clap_plugin_t* p, const clap_input_events_t* in,
                 const clap_output_events_t* out) {
  Plugin* s = self(p);
  if (in) applyEvents(s, in);
  emitDirty(s, out);
}

const clap_plugin_params_t kParamsExt = {
    paramsCount, paramsInfo, paramsValue, paramsValueToText, paramsTextToValue,
    paramsFlush,
};

// --- State ------------------------------------------------------------------

constexpr uint32_t kStateMagic = 0x4D535452u; // "MSTR"
constexpr uint32_t kStateVersion = 1;

bool stateSave(const clap_plugin_t* p, const clap_ostream_t* stream) {
  Plugin* s = self(p);
  const uint32_t header[4] = {kStateMagic, kStateVersion, P_COUNT, 0};
  double values[P_COUNT];
  for (uint32_t i = 0; i < P_COUNT; ++i) values[i] = s->shared.get(i);
  const double scale = s->editor.scale();
  const int64_t a = stream->write(stream, header, sizeof(header));
  const int64_t b = stream->write(stream, values, sizeof(values));
  const int64_t c = stream->write(stream, &scale, sizeof(scale));
  return a == static_cast<int64_t>(sizeof(header)) &&
         b == static_cast<int64_t>(sizeof(values)) &&
         c == static_cast<int64_t>(sizeof(scale));
}

bool stateLoad(const clap_plugin_t* p, const clap_istream_t* stream) {
  Plugin* s = self(p);
  uint32_t header[4] = {};
  if (stream->read(stream, header, sizeof(header)) !=
          static_cast<int64_t>(sizeof(header)) ||
      header[0] != kStateMagic || header[1] != kStateVersion ||
      header[2] > 64) {
    return false;
  }
  const uint32_t count = header[2];
  double values[64] = {};
  if (stream->read(stream, values, count * sizeof(double)) !=
      static_cast<int64_t>(count * sizeof(double))) {
    return false;
  }
  for (uint32_t i = 0; i < count && i < P_COUNT; ++i) {
    s->shared.setFromHost(i, values[i]);
  }
  s->resetEngine.store(true, std::memory_order_relaxed);
  s->editor.resetUiState();
  double scale = 0.0;
  if (stream->read(stream, &scale, sizeof(scale)) ==
          static_cast<int64_t>(sizeof(scale)) &&
      scale > 0.0) {
    s->editor.resize(scale);
  }
  return true;
}

const clap_plugin_state_t kStateExt = {stateSave, stateLoad};

// --- Audio ports ------------------------------------------------------------

uint32_t audioPortsCount(const clap_plugin_t*, bool) { return 1; }

bool audioPortsInfo(const clap_plugin_t*, uint32_t index, bool isInput,
                    clap_audio_port_info_t* info) {
  if (index != 0) return false;
  std::memset(info, 0, sizeof(*info));
  info->id = isInput ? 0 : 1;
  info->channel_count = 2;
  info->port_type = CLAP_PORT_STEREO;
  info->in_place_pair = CLAP_INVALID_ID;
  info->flags = CLAP_AUDIO_PORT_IS_MAIN;
  writeName(info->name, sizeof(info->name), isInput ? "Input" : "Output");
  return true;
}

const clap_plugin_audio_ports_t kAudioPortsExt = {audioPortsCount,
                                                  audioPortsInfo};

// --- GUI --------------------------------------------------------------------

bool guiIsApiSupported(const clap_plugin_t*, const char* api, bool floating) {
  return !floating && api && std::strcmp(api, CLAP_WINDOW_API_X11) == 0;
}

bool guiGetPreferredApi(const clap_plugin_t*, const char** api,
                        bool* floating) {
  *api = CLAP_WINDOW_API_X11;
  *floating = false;
  return true;
}

bool guiCreate(const clap_plugin_t*, const char*, bool) { return true; }
void guiDestroy(const clap_plugin_t* p) { self(p)->editor.close(); }
bool guiSetScale(const clap_plugin_t*, double) { return false; }

bool guiGetSize(const clap_plugin_t* p, uint32_t* w, uint32_t* h) {
  const UiSize s = self(p)->editor.size();
  *w = s.width;
  *h = s.height;
  return true;
}

bool guiCanResize(const clap_plugin_t*) { return true; }

bool guiGetResizeHints(const clap_plugin_t*, clap_gui_resize_hints_t* hints) {
  hints->can_resize_horizontally = true;
  hints->can_resize_vertically = true;
  hints->preserve_aspect_ratio = true;
  hints->aspect_ratio_width = kPanelWidth;
  hints->aspect_ratio_height = kPanelHeight;
  return true;
}

bool guiAdjustSize(const clap_plugin_t*, uint32_t* w, uint32_t* h) {
  const UiSize s = snapSize(*w, *h);
  *w = s.width;
  *h = s.height;
  return true;
}

bool guiSetSize(const clap_plugin_t* p, uint32_t w, uint32_t h) {
  self(p)->editor.resize(scaleForRequest(w, h));
  return true;
}

bool guiSetParent(const clap_plugin_t* p, const clap_window_t* window) {
  return self(p)->editor.open(
      window->x11 ? reinterpret_cast<void*>(static_cast<uintptr_t>(window->x11))
                  : nullptr);
}

bool guiSetTransient(const clap_plugin_t*, const clap_window_t*) { return false; }
void guiSuggestTitle(const clap_plugin_t*, const char*) {}
bool guiShow(const clap_plugin_t*) { return true; }
bool guiHide(const clap_plugin_t*) { return true; }

const clap_plugin_gui_t kGuiExt = {
    guiIsApiSupported, guiGetPreferredApi, guiCreate,    guiDestroy,
    guiSetScale,       guiGetSize,         guiCanResize, guiGetResizeHints,
    guiAdjustSize,     guiSetSize,         guiSetParent, guiSetTransient,
    guiSuggestTitle,   guiShow,            guiHide,
};

uint32_t latencyGet(const clap_plugin_t* p) {
  return static_cast<uint32_t>(self(p)->engine.latencySamples());
}

const clap_plugin_latency_t kLatencyExt = {latencyGet};

// --- Lifecycle --------------------------------------------------------------

bool pluginInit(const clap_plugin_t*) { return true; }
void pluginDestroy(const clap_plugin_t* p) { delete self(p); }

bool pluginActivate(const clap_plugin_t* p, double sampleRate,
                    uint32_t minFrames, uint32_t maxFrames) {
  Plugin* s = self(p);
  (void)minFrames;
  s->shared.sampleRate.store(static_cast<float>(sampleRate),
                             std::memory_order_relaxed);
  s->monoRight.assign(std::max<uint32_t>(maxFrames, 512), 0.0f);
  s->engine.prepare(static_cast<float>(sampleRate));
  const uint32_t latency = static_cast<uint32_t>(s->engine.latencySamples());
  if (latency != s->reportedLatency.exchange(latency)) {
    if (s->host && s->host->get_extension) {
      const auto* ext = static_cast<const clap_host_latency_t*>(
          s->host->get_extension(s->host, CLAP_EXT_LATENCY));
      if (ext && ext->changed) ext->changed(s->host);
    }
  }
  return true;
}

void pluginDeactivate(const clap_plugin_t*) {}
bool pluginStartProcessing(const clap_plugin_t*) { return true; }
void pluginStopProcessing(const clap_plugin_t*) {}
void pluginReset(const clap_plugin_t* p) { self(p)->engine.reset(); }

void resetMeterAtomics(Shared& shared) {
  shared.inPeakL.store(-120.0f);
  shared.inPeakR.store(-120.0f);
  shared.outPeakL.store(-120.0f);
  shared.outPeakR.store(-120.0f);
  shared.truePeakL.store(-120.0f);
  shared.truePeakR.store(-120.0f);
  shared.lufsIntegrated.store(-120.0f);
  shared.lufsShortTerm.store(-120.0f);
  shared.lufsMomentary.store(-120.0f);
  shared.lra.store(0.0f);
  shared.correlation.store(0.0f);
  shared.compGr.store(0.0f);
  shared.limitGr.store(0.0f);
  shared.truePeakMax.store(-120.0f);
  shared.samplePeakMax.store(-120.0f);
  shared.shortTermMax.store(-120.0f);
  shared.plr.store(0.0f);
  shared.psr.store(0.0f);
  for (int i = 0; i < kSpectrumBins; ++i) {
    shared.spectrum[i].store(-120.0f);
  }
}

clap_process_status pluginProcess(const clap_plugin_t* p,
                                  const clap_process_t* proc) {
  Plugin* s = self(p);
  const uint32_t frames = proc->frames_count;
  if (proc->in_events) applyEvents(s, proc->in_events);
  emitDirty(s, proc->out_events);
  if (proc->audio_inputs_count == 0 || proc->audio_outputs_count == 0) {
    return CLAP_PROCESS_CONTINUE;
  }
  const clap_audio_buffer_t& inBuf = proc->audio_inputs[0];
  const clap_audio_buffer_t& outBuf = proc->audio_outputs[0];
  if (!outBuf.data32 || !inBuf.data32 || !outBuf.data32[0] ||
      !inBuf.data32[0]) {
    return CLAP_PROCESS_CONTINUE;
  }
  float* left = outBuf.data32[0];
  const float* inL = inBuf.data32[0];
  const float* inR =
      inBuf.channel_count > 1 && inBuf.data32[1] ? inBuf.data32[1] : inL;
  const bool monoOut = outBuf.channel_count < 2 || !outBuf.data32[1];
  float* right = monoOut ? s->monoRight.data() : outBuf.data32[1];
  if (!right || (monoOut && s->monoRight.size() < frames)) {
    return CLAP_PROCESS_CONTINUE;
  }

  if (s->resetEngine.exchange(false, std::memory_order_relaxed)) {
    s->engine.reset();
    s->engine.resetMeters();
    resetMeterAtomics(s->shared);
  }
  s->engine.setControls(controlsFrom(s->shared));
  if (s->shared.takeMeterReset()) {
    s->engine.resetMeters();
    resetMeterAtomics(s->shared);
  }
  if (frames > 0) {
    if (monoOut) {
      if (left != inL) std::memcpy(left, inL, frames * sizeof(float));
      std::memcpy(right, inR, frames * sizeof(float));
      s->engine.process(left, right, static_cast<int>(frames), s->meters);
    } else {
      if (left != inL) std::memcpy(left, inL, frames * sizeof(float));
      if (right != inR) std::memcpy(right, inR, frames * sizeof(float));
      s->engine.process(left, right, static_cast<int>(frames), s->meters);
    }
    s->shared.inPeakL.store(s->meters.inPeakL);
    s->shared.inPeakR.store(s->meters.inPeakR);
    s->shared.outPeakL.store(s->meters.outPeakL);
    s->shared.outPeakR.store(s->meters.outPeakR);
    s->shared.truePeakL.store(s->meters.truePeakL);
    s->shared.truePeakR.store(s->meters.truePeakR);
    s->shared.lufsIntegrated.store(s->meters.lufsIntegrated);
    s->shared.lufsShortTerm.store(s->meters.lufsShortTerm);
    s->shared.lufsMomentary.store(s->meters.lufsMomentary);
    s->shared.lra.store(s->meters.lra);
    s->shared.correlation.store(s->meters.correlation);
    s->shared.compGr.store(s->meters.compGr);
    s->shared.limitGr.store(s->meters.limitGr);
    s->shared.truePeakMax.store(s->meters.truePeakMax);
    s->shared.samplePeakMax.store(s->meters.samplePeakMax);
    s->shared.shortTermMax.store(s->meters.shortTermMax);
    s->shared.plr.store(s->meters.plr);
    s->shared.psr.store(s->meters.psr);
    const float* spectrum = s->engine.spectrumBins();
    for (int i = 0; i < kSpectrumBins; ++i) {
      s->shared.spectrum[i].store(spectrum[i]);
    }
  }
  return CLAP_PROCESS_CONTINUE;
}

const void* pluginGetExtension(const clap_plugin_t*, const char* id) {
  if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &kParamsExt;
  if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &kStateExt;
  if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &kAudioPortsExt;
  if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &kGuiExt;
  if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &kLatencyExt;
  return nullptr;
}

void pluginOnMainThread(const clap_plugin_t*) {}

Plugin* createPlugin(const clap_host_t* host) {
  Plugin* s = new Plugin();
  s->host = host;
  s->plugin.desc = &kDescriptor;
  s->plugin.plugin_data = s;
  s->plugin.init = pluginInit;
  s->plugin.destroy = pluginDestroy;
  s->plugin.activate = pluginActivate;
  s->plugin.deactivate = pluginDeactivate;
  s->plugin.start_processing = pluginStartProcessing;
  s->plugin.stop_processing = pluginStopProcessing;
  s->plugin.reset = pluginReset;
  s->plugin.process = pluginProcess;
  s->plugin.get_extension = pluginGetExtension;
  s->plugin.on_main_thread = pluginOnMainThread;
  s->engine.prepare(48000.0f);
  s->editor.requestFlush = [s] {
    if (s->host && s->host->request_process) s->host->request_process(s->host);
  };
  return s;
}

uint32_t factoryCount(const clap_plugin_factory_t*) { return 1; }
const clap_plugin_descriptor_t* factoryDescriptor(const clap_plugin_factory_t*,
                                                  uint32_t index) {
  return index == 0 ? &kDescriptor : nullptr;
}

const clap_plugin_t* factoryCreate(const clap_plugin_factory_t*,
                                   const clap_host_t* host, const char* id) {
  if (std::strcmp(id, kPluginId) != 0) return nullptr;
  return &createPlugin(host)->plugin;
}

const clap_plugin_factory_t kFactory = {factoryCount, factoryDescriptor,
                                        factoryCreate};

bool entryInit(const char*) { return true; }
void entryDeinit() {}
const void* entryGetFactory(const char* factoryId) {
  return std::strcmp(factoryId, CLAP_PLUGIN_FACTORY_ID) == 0 ? &kFactory
                                                             : nullptr;
}

} // namespace
} // namespace mastering

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION,
    mastering::entryInit,
    mastering::entryDeinit,
    mastering::entryGetFactory,
};
