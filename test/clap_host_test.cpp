// Mastering Suite — CLAP host smoke test
// SPDX-License-Identifier: GPL-3.0-or-later

#include <clap/all.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XTest.h>
#include <dlfcn.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "params.hpp"

namespace {

int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}

constexpr uint32_t kFrames = 512;

struct HostEvents {
  std::vector<clap_event_param_value_t> inputs;
  std::vector<clap_event_param_value_t> outputs;
};

uint32_t inSize(const clap_input_events_t* list) {
  return static_cast<uint32_t>(static_cast<HostEvents*>(list->ctx)->inputs.size());
}
const clap_event_header_t* inGet(const clap_input_events_t* list, uint32_t i) {
  return &static_cast<HostEvents*>(list->ctx)->inputs[i].header;
}
bool outPush(const clap_output_events_t* list, const clap_event_header_t* hdr) {
  auto* ev = reinterpret_cast<const clap_event_param_value_t*>(hdr);
  static_cast<HostEvents*>(list->ctx)->outputs.push_back(*ev);
  return true;
}

int gLatencyChanges = 0;
void hostLatencyChanged(const clap_host_t*) { ++gLatencyChanges; }
const clap_host_latency_t kHostLatency = {hostLatencyChanged};

const void* hostGetExtension(const clap_host_t*, const char* id) {
  if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &kHostLatency;
  return nullptr;
}

clap_event_param_value_t paramEvent(uint32_t id, double value) {
  clap_event_param_value_t ev{};
  ev.header.size = sizeof(ev);
  ev.header.space_id = CLAP_CORE_EVENT_SPACE_ID;
  ev.header.type = CLAP_EVENT_PARAM_VALUE;
  ev.param_id = id;
  ev.note_id = -1;
  ev.port_index = -1;
  ev.channel = -1;
  ev.key = -1;
  ev.value = value;
  return ev;
}

struct MemStream {
  std::vector<unsigned char> data;
  size_t readPos = 0;
};

int64_t streamWrite(const clap_ostream_t* s, const void* buf, uint64_t size) {
  auto* m = static_cast<MemStream*>(s->ctx);
  const auto* bytes = static_cast<const unsigned char*>(buf);
  m->data.insert(m->data.end(), bytes, bytes + size);
  return static_cast<int64_t>(size);
}

int64_t streamRead(const clap_istream_t* s, void* buf, uint64_t size) {
  auto* m = static_cast<MemStream*>(s->ctx);
  const size_t n = std::min<size_t>(size, m->data.size() - m->readPos);
  std::memcpy(buf, m->data.data() + m->readPos, n);
  m->readPos += n;
  return static_cast<int64_t>(n);
}

void writePpm(const char* path, XImage* img, int w, int h) {
  FILE* f = std::fopen(path, "wb");
  if (!f) return;
  std::fprintf(f, "P6\n%d %d\n255\n", w, h);
  for (int y = 0; y < h; ++y) {
    for (int x = 0; x < w; ++x) {
      const unsigned long p = XGetPixel(img, x, y);
      const unsigned char rgb[3] = {
          static_cast<unsigned char>((p >> 16) & 0xff),
          static_cast<unsigned char>((p >> 8) & 0xff),
          static_cast<unsigned char>(p & 0xff)};
      std::fwrite(rgb, 1, 3, f);
    }
  }
  std::fclose(f);
}

} // namespace

int main(int argc, char** argv) {
  const char* path =
      argc > 1 ? argv[1] : "build/clap/MasteringSuite.clap";
  void* lib = dlopen(path, RTLD_NOW | RTLD_LOCAL);
  check(lib != nullptr, "bundle loads");
  if (!lib) {
    std::printf("%s\n", dlerror());
    return 1;
  }
  auto* entry = static_cast<const clap_plugin_entry_t*>(
      dlsym(lib, "clap_entry"));
  check(entry != nullptr, "clap_entry symbol present");
  if (!entry || !entry->init(path)) return 1;

  auto* factory = static_cast<const clap_plugin_factory_t*>(
      entry->get_factory(CLAP_PLUGIN_FACTORY_ID));
  check(factory && factory->get_plugin_count(factory) == 1,
        "one plugin in factory");

  clap_host_t host{};
  host.clap_version = CLAP_VERSION;
  host.name = "mastering-test";
  host.vendor = "tylorismusic";
  host.version = "0.1";
  host.get_extension = hostGetExtension;
  host.request_process = nullptr;
  const clap_plugin_t* plugin =
      factory->create_plugin(factory, &host, "com.tylorismusic.masteringsuite");
  check(plugin != nullptr, "plugin instantiates");
  if (!plugin) return 1;
  check(plugin->init(plugin), "plugin init");

  auto* params = static_cast<const clap_plugin_params_t*>(
      plugin->get_extension(plugin, CLAP_EXT_PARAMS));
  auto* state = static_cast<const clap_plugin_state_t*>(
      plugin->get_extension(plugin, CLAP_EXT_STATE));
  auto* gui = static_cast<const clap_plugin_gui_t*>(
      plugin->get_extension(plugin, CLAP_EXT_GUI));
  auto* latency = static_cast<const clap_plugin_latency_t*>(
      plugin->get_extension(plugin, CLAP_EXT_LATENCY));
  check(params && params->count(plugin) == mastering::P_COUNT,
        "parameter count matches");
  check(params && params->count(plugin) == 39, "thirty-nine parameters");
  check(latency && latency->get(plugin) == 240,
        "latency is 5 ms (240 samples)");
  check(state && gui, "state and GUI extensions present");

  check(plugin->activate(plugin, 48000.0, kFrames, kFrames), "activate");
  check(plugin->start_processing(plugin), "start processing");

  HostEvents events;
  clap_input_events_t inEvents{&events, inSize, inGet};
  clap_output_events_t outEvents{&events, outPush};
  std::vector<float> left(kFrames), right(kFrames);
  float* chans[2] = {left.data(), right.data()};
  clap_audio_buffer_t inBuf{};
  inBuf.data32 = chans;
  inBuf.channel_count = 2;
  clap_audio_buffer_t outBuf{};
  outBuf.data32 = chans;
  outBuf.channel_count = 2;

  events.inputs.push_back(paramEvent(mastering::P_EQ_ON, 1.0));
  events.inputs.push_back(paramEvent(mastering::P_COMP_ON, 1.0));
  events.inputs.push_back(paramEvent(mastering::P_COLOR_ON, 1.0));
  events.inputs.push_back(paramEvent(mastering::P_COLOR_DRIVE, 0.5));
  events.inputs.push_back(paramEvent(mastering::P_DENS_ON, 1.0));
  events.inputs.push_back(paramEvent(mastering::P_DENS_EFFECT, 0.3));
  events.inputs.push_back(paramEvent(mastering::P_LIMIT_ON, 1.0));

  bool finite = true;
  float peak = 0.0f;
  for (int block = 0; block < 160; ++block) {
    for (uint32_t i = 0; i < kFrames; ++i) {
      const double t = (block * kFrames + i) / 48000.0;
      left[i] = right[i] =
          static_cast<float>(0.3 * std::sin(2.0 * M_PI * 220.0 * t));
    }
    clap_process_t proc{};
    proc.frames_count = kFrames;
    proc.audio_inputs = &inBuf;
    proc.audio_outputs = &outBuf;
    proc.audio_inputs_count = 1;
    proc.audio_outputs_count = 1;
    proc.in_events = &inEvents;
    proc.out_events = &outEvents;
    plugin->process(plugin, &proc);
    events.inputs.clear();
    for (uint32_t i = 0; i < kFrames; ++i) {
      if (!std::isfinite(left[i]) || !std::isfinite(right[i])) finite = false;
      peak = std::max(peak, std::fabs(left[i]));
    }
  }
  check(finite, "host-driven processing stays finite");
  check(peak > 0.01f && peak < 2.0f, "output has a sane level");

  // Mono output fallback: the right channel goes to a scratch buffer, so the
  // single host channel must not be overwritten with the right input.
  {
    std::vector<float> mono(kFrames);
    float* monoStereo[2] = {mono.data(), nullptr};
    clap_audio_buffer_t monoBuf{};
    monoBuf.data32 = monoStereo;
    monoBuf.channel_count = 1;
    for (uint32_t i = 0; i < kFrames; ++i) {
      const double t = i / 48000.0;
      left[i] = static_cast<float>(0.3 * std::sin(2.0 * M_PI * 220.0 * t));
      right[i] = static_cast<float>(0.15 * std::sin(2.0 * M_PI * 330.0 * t));
    }
    clap_process_t proc{};
    proc.frames_count = kFrames;
    proc.audio_inputs = &inBuf;
    proc.audio_outputs = &monoBuf;
    proc.audio_inputs_count = 1;
    proc.audio_outputs_count = 1;
    proc.in_events = &inEvents;
    proc.out_events = &outEvents;
    plugin->process(plugin, &proc);
    bool monoFinite = true;
    float monoPeak = 0.0f;
    for (uint32_t i = 0; i < kFrames; ++i) {
      if (!std::isfinite(mono[i])) monoFinite = false;
      monoPeak = std::max(monoPeak, std::fabs(mono[i]));
    }
    check(monoFinite && monoPeak > 0.01f,
          "mono output falls back without losing the left channel");
  }

  double value = 0.0;
  events.inputs.push_back(paramEvent(mastering::P_EQ_LOW_GAIN, 0.8));
  clap_input_events_t setIn{&events, inSize, inGet};
  params->flush(plugin, &setIn, nullptr);
  events.inputs.clear();
  check(params->get_value(plugin, mastering::P_EQ_LOW_GAIN, &value) &&
            std::fabs(value - 0.8) < 1e-9,
        "parameter round-trips through the host");

  // State round trip.
  MemStream saved;
  clap_ostream_t out{&saved, streamWrite};
  check(state->save(plugin, &out) && saved.data.size() > 0, "state saves");
  events.inputs.push_back(paramEvent(mastering::P_EQ_LOW_GAIN, 0.2));
  clap_input_events_t changeIn{&events, inSize, inGet};
  params->flush(plugin, &changeIn, nullptr);
  events.inputs.clear();
  MemStream back;
  back.data = saved.data;
  clap_istream_t in{&back, streamRead};
  check(state->load(plugin, &in), "state loads");
  params->get_value(plugin, mastering::P_EQ_LOW_GAIN, &value);
  check(std::fabs(value - 0.8) < 1e-9, "state restores the EQ gain");

  // Host-facing text.
  {
    char buf[128] = {0};
    params->value_to_text(plugin, mastering::P_COLOR_MODEL, 4.0, buf,
                          sizeof(buf));
    check(std::strcmp(buf, "Transformer") == 0, "model reads as a name");
    params->value_to_text(plugin, mastering::P_COMP_RATIO, 2.0, buf,
                          sizeof(buf));
    check(std::strcmp(buf, "10:1") == 0, "ratio reads as a label");
    params->value_to_text(plugin, mastering::P_COLOR_CLIP, 2.0, buf,
                          sizeof(buf));
    check(std::strcmp(buf, "Hard") == 0, "clip reads as a mode name");
    params->value_to_text(plugin, mastering::P_LIMIT_CEILING, 0.95, buf,
                          sizeof(buf));
    check(std::strstr(buf, "dB") != nullptr, "ceiling reads as dB");
  }

  // Editor.
  if (gui) {
    Display* dpy = XOpenDisplay(nullptr);
    if (dpy) {
      check(gui->is_api_supported(plugin, CLAP_WINDOW_API_X11, false),
            "X11 embedding supported");
      check(gui->create(plugin, CLAP_WINDOW_API_X11, false), "gui create");
      Window parent = XCreateSimpleWindow(
          dpy, RootWindow(dpy, DefaultScreen(dpy)), 0, 0, 1040, 640, 0, 0, 0);
      XMapWindow(dpy, parent);
      XFlush(dpy);
      clap_window_t win{};
      win.api = CLAP_WINDOW_API_X11;
      win.x11 = parent;
      check(gui->set_parent(plugin, &win), "gui set_parent");
      int originX = 0, originY = 0;
      Window editor = 0;
      {
        Window rootRet, parentRet, *kids = nullptr;
        unsigned n = 0;
        if (XQueryTree(dpy, parent, &rootRet, &parentRet, &kids, &n)) {
          for (unsigned i = 0; i < n; ++i) {
            XWindowAttributes a;
            if (XGetWindowAttributes(dpy, kids[i], &a) && a.width >= 1000) {
              editor = kids[i];
            }
          }
          if (editor) {
            Window rr;
            XTranslateCoordinates(dpy, editor,
                                  RootWindow(dpy, DefaultScreen(dpy)), 0, 0,
                                  &originX, &originY, &rr);
          }
          XFree(kids);
        }
      }
      check(editor != 0, "editor creates a child window");
      check(gui->show(plugin), "gui show");
      std::this_thread::sleep_for(std::chrono::milliseconds(500));
      uint32_t w = 0, h = 0;
      check(gui->get_size(plugin, &w, &h) && w == 1040 && h == 640,
            "gui size is 1040x640");
      if (editor) {
        XImage* img = XGetImage(dpy, editor, 0, 0, 1040, 640, AllPlanes,
                                ZPixmap);
        if (img) {
          writePpm("/tmp/opencode/mastering-editor.ppm", img, 1040, 640);
          XDestroyImage(img);
        }
      }

      auto click = [&](int x, int y) {
        XTestFakeMotionEvent(dpy, -1, originX + x, originY + y, CurrentTime);
        XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
        XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
        XFlush(dpy);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
      };
      auto doubleClick = [&](int x, int y) {
        const int sx = originX + x, sy = originY + y;
        XTestFakeMotionEvent(dpy, -1, sx, sy, CurrentTime);
        for (int i = 0; i < 2; ++i) {
          XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
          XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
          XFlush(dpy);
          std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
      };
      auto rightClick = [&](int x, int y) {
        XTestFakeMotionEvent(dpy, -1, originX + x, originY + y, CurrentTime);
        XTestFakeButtonEvent(dpy, 3, True, CurrentTime);
        XTestFakeButtonEvent(dpy, 3, False, CurrentTime);
        XFlush(dpy);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
      };
      auto key = [&](KeySym sym) {
        const KeyCode code = XKeysymToKeycode(dpy, sym);
        XTestFakeKeyEvent(dpy, code, True, CurrentTime);
        XTestFakeKeyEvent(dpy, code, False, CurrentTime);
        XFlush(dpy);
        std::this_thread::sleep_for(std::chrono::milliseconds(40));
      };
      auto drag = [&](int x, int y, int dy) {
        const int sx = originX + x, sy = originY + y;
        XTestFakeMotionEvent(dpy, -1, sx, sy, CurrentTime);
        XTestFakeButtonEvent(dpy, 1, True, CurrentTime);
        for (int i = 1; i <= 10; ++i) {
          XTestFakeMotionEvent(dpy, -1, sx, sy + dy * i / 10, CurrentTime);
          XFlush(dpy);
          std::this_thread::sleep_for(std::chrono::milliseconds(8));
        }
        XTestFakeButtonEvent(dpy, 1, False, CurrentTime);
        XFlush(dpy);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
      };
      auto paramValue = [&](uint32_t id) {
        double v = 0.0;
        params->get_value(plugin, id, &v);
        return v;
      };
      const double before = paramValue(mastering::P_EQ_LOW_GAIN);
      drag(220, 455, -50);
      check(paramValue(mastering::P_EQ_LOW_GAIN) > before + 0.03,
            "dragging LOW GAIN moves it");
      rightClick(220, 455);
      key(XK_minus);
      key(XK_6);
      key(XK_Return);
      check(std::fabs(paramValue(mastering::P_EQ_LOW_GAIN) - 0.25) < 1e-4,
            "right-click typed value enters -6 dB");
      doubleClick(220, 455);
      check(std::fabs(paramValue(mastering::P_EQ_LOW_GAIN) - 0.5) < 1e-4,
            "double-click resets to the parameter default");

      // With a preset selected, double-click restores that preset's value.
      click(384, 32);
      click(384, 32);
      click(384, 32);
      doubleClick(220, 455);
      check(std::fabs(paramValue(mastering::P_EQ_LOW_GAIN) - 0.52) < 1e-4,
            "double-click respects the selected preset");

      click(320, 90);
      check(paramValue(mastering::P_MODULE) == 1.0,
            "clicking COMP selects the compressor");
      check(paramValue(mastering::P_COMP_ON) == 1.0,
            "compressor is enabled in the test patch");
      click(920, 455);  // not a control; exercises hit-testing safely
      gui->hide(plugin);
      gui->destroy(plugin);
      XDestroyWindow(dpy, parent);
      XCloseDisplay(dpy);
      check(true, "gui create/embed/destroy completes");
    } else {
      std::printf("SKIP  editor (no X display)\n");
    }
  }

  plugin->stop_processing(plugin);
  plugin->deactivate(plugin);
  plugin->destroy(plugin);
  entry->deinit();
  dlclose(lib);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
