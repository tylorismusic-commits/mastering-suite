// Mastering Suite — LV2 UI host test (X11 + Xvfb)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Loads mastering_suite_ui.so directly, embeds it in a parent window and
// drives it through the idle interface the way a real host does.  Covers the
// resize handshake, meter/spectrum port events, UI-to-host parameter writes
// and the meter-reset pulse.

#include <dlfcn.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <thread>
#include <vector>

#include "lv2_ports.hpp"

namespace {

int gFailures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    ++gFailures;
  }
}

struct HostResize {
  LV2UI_Resize api{};
  int lastWidth = 0;
  int lastHeight = 0;
  int calls = 0;
};

int onResize(void* handle, int width, int height) {
  auto* host = static_cast<HostResize*>(handle);
  host->lastWidth = width;
  host->lastHeight = height;
  ++host->calls;
  return 0;
}

struct Written {
  uint32_t port = 0;
  float value = 0.0f;
};

struct HostWrite {
  std::vector<Written> events;
};

void onWrite(LV2UI_Controller controller, uint32_t port, uint32_t bufferSize,
             uint32_t, const void* buffer) {
  if (!controller || !buffer || bufferSize != sizeof(float)) {
    return;
  }
  auto* host = static_cast<HostWrite*>(controller);
  host->events.push_back(
      {port, *static_cast<const float*>(buffer)});
}

using DescriptorFn = const LV2UI_Descriptor* (*)(uint32_t);

void sendButton(Display* dpy, Window window, int x, int y, bool press) {
  XEvent ev{};
  ev.type = press ? ButtonPress : ButtonRelease;
  ev.xbutton.display = dpy;
  ev.xbutton.window = window;
  ev.xbutton.root = RootWindow(dpy, DefaultScreen(dpy));
  ev.xbutton.x = x;
  ev.xbutton.y = y;
  ev.xbutton.button = Button1;
  ev.xbutton.same_screen = True;
  XSendEvent(dpy, window, False, 0, &ev);
  XSync(dpy, False);
}

void sendMotion(Display* dpy, Window window, int x, int y, bool down) {
  XEvent ev{};
  ev.type = MotionNotify;
  ev.xmotion.display = dpy;
  ev.xmotion.window = window;
  ev.xmotion.root = RootWindow(dpy, DefaultScreen(dpy));
  ev.xmotion.x = x;
  ev.xmotion.y = y;
  ev.xmotion.state = down ? Button1Mask : 0;
  ev.xmotion.same_screen = True;
  XSendEvent(dpy, window, False, 0, &ev);
  XSync(dpy, False);
}

bool wrotePort(const HostWrite& host, uint32_t port, float value) {
  for (const Written& e : host.events) {
    if (e.port == port && e.value == value) {
      return true;
    }
  }
  return false;
}

} // namespace

int main(int argc, char** argv) {
  const std::string bundle =
      argc > 1 ? argv[1] : "build/lv2/mastering-suite.lv2";
  const std::string library = bundle + "/mastering_suite_ui.so";

  void* module = dlopen(library.c_str(), RTLD_NOW | RTLD_LOCAL);
  if (!module) {
    std::printf("FAIL  cannot load %s: %s\n", library.c_str(), dlerror());
    return 1;
  }
  check(true, "UI library loads");

  auto descriptor =
      reinterpret_cast<DescriptorFn>(dlsym(module, "lv2ui_descriptor"));
  check(descriptor != nullptr, "lv2ui_descriptor is exported");
  if (!descriptor) {
    return 1;
  }

  const LV2UI_Descriptor* ui = descriptor(0);
  check(ui != nullptr, "UI descriptor index 0 exists");
  if (!ui) {
    return 1;
  }

  Display* dpy = XOpenDisplay(nullptr);
  if (!dpy) {
    std::printf("FAIL  cannot open display (run under xvfb-run)\n");
    return 1;
  }

  const int screen = DefaultScreen(dpy);
  Window parent = XCreateSimpleWindow(dpy, RootWindow(dpy, screen), 0, 0,
                                      1200, 800, 0, 0, 0);
  XMapWindow(dpy, parent);
  XFlush(dpy);

  HostResize host;
  host.api.handle = &host;
  host.api.ui_resize = &onResize;
  HostWrite writes;
  float scale = 1.0f;

  LV2_Feature parentFeature{LV2_UI__parent,
                            reinterpret_cast<void*>(
                                static_cast<uintptr_t>(parent))};
  LV2_Feature resizeFeature{LV2_UI__resize, &host.api};
  LV2_Feature scaleFeature{LV2_UI__scaleFactor, &scale};
  const LV2_Feature* features[] = {&parentFeature, &resizeFeature,
                                   &scaleFeature, nullptr};

  LV2UI_Widget widget = nullptr;
  LV2UI_Handle handle = ui->instantiate(
      ui, "https://github.com/tylorismusic-commits/mastering-suite",
      (bundle + "/").c_str(), &onWrite, &writes, &widget, features);
  check(handle != nullptr, "UI instantiates with a parent window");
  if (!handle) {
    return 1;
  }
  check(widget != nullptr, "UI returns an X11 widget");
  check(host.calls > 0, "UI asks the host for its preferred size");
  check(host.lastWidth == 1040 && host.lastHeight == 640,
        "preferred size is the 1040x640 logical layout");

  const auto* idle = static_cast<const LV2UI_Idle_Interface*>(
      ui->extension_data(LV2_UI__idleInterface));
  check(idle != nullptr, "UI provides the idle interface");
  check(ui->extension_data("http://lv2plug.in/plugins/not-a-thing") == nullptr,
        "unknown extension data is refused");

  const Window window =
      static_cast<Window>(reinterpret_cast<uintptr_t>(widget));

  {
    XSizeHints hints{};
    long supplied = 0;
    const bool ok = XGetWMNormalHints(dpy, window, &hints, &supplied) != 0;
    check(ok && (hints.flags & PMinSize) != 0 && hints.min_width == 520 &&
              hints.min_height == 320,
          "PMinSize hint advertises the 0.5x minimum");
    check(ok && (hints.flags & PMaxSize) != 0 && hints.max_width == 2600 &&
              hints.max_height == 1600,
          "PMaxSize hint advertises the 2.5x maximum");
  }

  bool idled = true;
  if (idle) {
    for (int i = 0; i < 5; ++i) {
      if (idle->idle(handle) != 0) {
        idled = false;
      }
    }
  }
  check(idled, "idle() returns 0 and draws without crashing");

  // Feed meter, spectrum and latency values the way the host would.
  if (idle) {
    auto meter = [&](mastering::Lv2Meter m, float v) {
      ui->port_event(handle,
                     mastering::kLv2FirstMeter + static_cast<uint32_t>(m),
                     sizeof(float), 0, &v);
    };
    meter(mastering::kLv2InPeakL, -10.0f);
    meter(mastering::kLv2InPeakR, -11.0f);
    meter(mastering::kLv2OutPeakL, -12.0f);
    meter(mastering::kLv2OutPeakR, -13.0f);
    meter(mastering::kLv2TruePeakL, -0.4f);
    meter(mastering::kLv2TruePeakR, -0.6f);
    meter(mastering::kLv2LufsI, -14.2f);
    meter(mastering::kLv2LufsS, -13.8f);
    meter(mastering::kLv2LufsM, -13.1f);
    meter(mastering::kLv2Lra, 4.2f);
    meter(mastering::kLv2Correlation, 0.71f);
    meter(mastering::kLv2CompGr, 2.4f);
    meter(mastering::kLv2LimitGr, 1.8f);
    const float bin = -20.0f;
    ui->port_event(handle, mastering::kLv2FirstSpectrum + 12, sizeof(float), 0,
                   &bin);
    const float latency = 240.0f;
    ui->port_event(handle, mastering::kLv2Latency, sizeof(float), 0, &latency);
    idle->idle(handle);
  }

  // Clicking RESET must produce a meter-reset pulse on the hidden port, then
  // release it so the next click re-arms the edge.
  if (idle) {
    writes.events.clear();
    sendButton(dpy, window, 975, 30, true);
    sendButton(dpy, window, 975, 30, false);
    idle->idle(handle);
    check(wrotePort(writes, mastering::kLv2MeterReset, 1.0f),
          "RESET writes a high meter-reset pulse");
    std::this_thread::sleep_for(std::chrono::milliseconds(220));
    idle->idle(handle);
    check(wrotePort(writes, mastering::kLv2MeterReset, 0.0f),
          "meter-reset pulse is released");
  }

  // Dragging LOW GAIN must write the parameter port back to the host.
  if (idle) {
    writes.events.clear();
    sendButton(dpy, window, 250, 455, true);
    sendMotion(dpy, window, 250, 405, true);
    sendButton(dpy, window, 250, 405, false);
    idle->idle(handle);
    bool wroteEq = false;
    for (const Written& e : writes.events) {
      if (e.port == mastering::kLv2FirstParam + mastering::P_EQ_LOW_GAIN) {
        wroteEq = true;
      }
    }
    check(wroteEq, "dragging a knob writes its parameter to the host");
  }

  // The host owns the size.  Adopting it must not bounce a resize back
  // through the host, or suil re-allocates and the window flickers/loops.
  if (idle) {
    host.calls = 0;
    XResizeWindow(dpy, window, 1600, 800);
    XSync(dpy, False);
    for (int i = 0; i < 5; ++i) {
      idle->idle(handle);
    }
    XWindowAttributes attrs{};
    XGetWindowAttributes(dpy, window, &attrs);
    char label[128];
    std::snprintf(label, sizeof(label),
                  "growing follows the host allocation (got %dx%d)",
                  attrs.width, attrs.height);
    check(attrs.width == 1600 && attrs.height == 800, label);
    check(host.calls == 0, "growing does not call back into the host resize");

    XResizeWindow(dpy, window, 700, 420);
    XSync(dpy, False);
    for (int i = 0; i < 5; ++i) {
      idle->idle(handle);
    }
    XGetWindowAttributes(dpy, window, &attrs);
    std::snprintf(label, sizeof(label),
                  "shrinking follows the host allocation (got %dx%d)",
                  attrs.width, attrs.height);
    check(attrs.width == 700 && attrs.height == 420, label);
    check(host.calls == 0, "shrinking does not call back into the host resize");
  }

  // A degenerate allocation must not crash or wedge the idle loop.
  if (idle) {
    XResizeWindow(dpy, window, 40, 20);
    XSync(dpy, False);
    for (int i = 0; i < 5; ++i) {
      idle->idle(handle);
    }
    check(true, "degenerate allocation is survivable");
  }

  // The idle caller runs from the host's main loop, so it must be cheap when
  // nothing changed.  500 calls should be dominated by the 33 ms throttle.
  if (idle) {
    const auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < 500; ++i) {
      idle->idle(handle);
    }
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    char label[128];
    std::snprintf(label, sizeof(label),
                  "500 idle calls stay throttled (%lld ms)",
                  static_cast<long long>(elapsed.count()));
    check(elapsed.count() < 500, label);
  }

  ui->cleanup(handle);
  XDestroyWindow(dpy, parent);
  XCloseDisplay(dpy);
  dlclose(module);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
