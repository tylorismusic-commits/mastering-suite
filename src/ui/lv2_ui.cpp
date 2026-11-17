// Mastering Suite — LV2 UI (X11 + Cairo)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// The host owns the widget size.  On every allocation we letterbox the
// 1040x640 panel into whatever we are given and redraw.  We never call
// XResizeWindow or the host's ui_resize in response to a configure event:
// suil turns those into another GTK allocation, which arrives back as another
// configure event, and the window flickers and eventually wedges Ardour.
//
// Redraws are throttled to ~30 Hz from the idle interface.  Hosts call idle
// from a GTK idle handler, which can run thousands of times a second; drawing
// every call starves the host's main loop.

#include <lv2/core/lv2.h>
#include <lv2/ui/ui.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <cairo-xlib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <string>

#include "lv2_ports.hpp"
#include "params.hpp"
#include "ui/geometry.hpp"
#include "ui/panel.hpp"

namespace {

constexpr const char* kUiUri =
    "https://github.com/tylorismusic-commits/mastering-suite#ui";
constexpr auto kFrameInterval = std::chrono::milliseconds(33);
constexpr auto kResetPulse = std::chrono::milliseconds(150);

struct Ui {
  LV2UI_Write_Function write = nullptr;
  LV2UI_Controller controller = nullptr;

  mastering::Shared shared;
  mastering::Panel panel;
  mastering::PanelState state;

  Display* dpy = nullptr;
  Window win = 0;
  cairo_surface_t* windowSurface = nullptr;
  cairo_surface_t* backing = nullptr;
  cairo_t* cr = nullptr;
  cairo_format_t format = CAIRO_FORMAT_RGB24;

  uint32_t width = mastering::kPanelWidth;
  uint32_t height = mastering::kPanelHeight;

  bool dirty = true;
  bool resetPulseState = false;
  std::chrono::steady_clock::time_point resetPulseUntil{};
  std::chrono::steady_clock::time_point lastFrame{};

  // Largest uniform scale that fits the panel into the host allocation.
  double layoutScale() const {
    const double sx = static_cast<double>(width) / mastering::kPanelWidth;
    const double sy = static_cast<double>(height) / mastering::kPanelHeight;
    return std::min(sx, sy);
  }

  double offsetX() const {
    return (static_cast<double>(width) -
            mastering::kPanelWidth * layoutScale()) *
           0.5;
  }

  double offsetY() const {
    return (static_cast<double>(height) -
            mastering::kPanelHeight * layoutScale()) *
           0.5;
  }

  void rebuildSurfaces() {
    if (cr) {
      cairo_destroy(cr);
      cr = nullptr;
    }
    if (backing) {
      cairo_surface_destroy(backing);
      backing = nullptr;
    }
    backing = cairo_image_surface_create(format, static_cast<int>(width),
                                         static_cast<int>(height));
    cr = cairo_create(backing);
    if (windowSurface) {
      cairo_xlib_surface_set_size(windowSurface, width, height);
    }
  }

  void configure(uint32_t w, uint32_t h) {
    if (w == 0 || h == 0 || (w == width && h == height)) {
      return;
    }
    width = w;
    height = h;
    rebuildSurfaces();
    dirty = true;
  }

  void handleEvent(const XEvent& ev) {
    const double s = layoutScale();
    const double ox = offsetX();
    const double oy = offsetY();
    auto toLogicalX = [&](int x) { return (x - ox) / s; };
    auto toLogicalY = [&](int y) { return (y - oy) / s; };

    switch (ev.type) {
      case ConfigureNotify:
        configure(static_cast<uint32_t>(ev.xconfigure.width),
                  static_cast<uint32_t>(ev.xconfigure.height));
        break;
      case Expose:
        if (ev.xexpose.count == 0) {
          draw();
        }
        break;
      case ButtonPress: {
        if (dpy && win) {
          XSetInputFocus(dpy, win, RevertToParent, CurrentTime);
          XFlush(dpy);
        }
        if (ev.xbutton.button == Button4) {
          panel.wheel(shared, state, toLogicalX(ev.xbutton.x),
                      toLogicalY(ev.xbutton.y), 1.0);
        } else if (ev.xbutton.button == Button5) {
          panel.wheel(shared, state, toLogicalX(ev.xbutton.x),
                      toLogicalY(ev.xbutton.y), -1.0);
        } else if (ev.xbutton.button == Button1) {
          panel.pointerDown(shared, state, toLogicalX(ev.xbutton.x),
                            toLogicalY(ev.xbutton.y));
        } else if (ev.xbutton.button == Button3) {
          panel.beginTextEntry(shared, state, toLogicalX(ev.xbutton.x),
                               toLogicalY(ev.xbutton.y));
        }
        dirty = true;
        break;
      }
      case ButtonRelease:
        panel.pointerUp();
        dirty = true;
        break;
      case KeyPress: {
        char buf[8] = {};
        KeySym sym = 0;
        XKeyEvent key = ev.xkey;
        const int n = XLookupString(&key, buf, sizeof(buf), &sym, nullptr);
        if (sym == XK_Return || sym == XK_KP_Enter) {
          panel.textKey(shared, state, mastering::Panel::kTextEnter, 0);
        } else if (sym == XK_Escape) {
          panel.textKey(shared, state, mastering::Panel::kTextEscape, 0);
        } else if (sym == XK_BackSpace) {
          panel.textKey(shared, state, mastering::Panel::kTextBackspace, 0);
        } else if (n > 0) {
          panel.textKey(shared, state, 0, buf[0]);
        }
        dirty = true;
        break;
      }
      case MotionNotify: {
        const bool fine = (ev.xmotion.state & ShiftMask) != 0;
        panel.pointerMove(shared, state, toLogicalX(ev.xmotion.x),
                          toLogicalY(ev.xmotion.y), fine);
        dirty = true;
        break;
      }
      default:
        break;
    }
  }

  void writeReset(float value) {
    if (write) {
      write(controller, mastering::kLv2MeterReset, sizeof(float), 0, &value);
    }
  }

  void draw() {
    if (!cr || !windowSurface || width < 8 || height < 8) {
      return;
    }

    // Push changed controls to the host.
    for (uint32_t i = 0; i < mastering::P_COUNT; ++i) {
      if (!shared.takeDirty(i)) {
        continue;
      }
      const float v = static_cast<float>(shared.get(i));
      if (write) {
        write(controller, mastering::kLv2FirstParam + i, sizeof(float), 0, &v);
      }
    }

    // The meter reset is a pulse: hold the trigger high across a few host
    // cycles so it survives write coalescing, then release it.
    const auto now = std::chrono::steady_clock::now();
    if (shared.takeMeterReset()) {
      resetPulseUntil = now + kResetPulse;
    }
    const bool pulse = now < resetPulseUntil;
    if (pulse != resetPulseState) {
      writeReset(pulse ? 1.0f : 0.0f);
      resetPulseState = pulse;
    }

    // Letterbox: dark surround, panel centred and uniformly scaled.
    cairo_save(cr);
    cairo_set_source_rgb(cr, 0.043, 0.047, 0.051);
    cairo_paint(cr);
    cairo_translate(cr, offsetX(), offsetY());
    cairo_scale(cr, layoutScale(), layoutScale());
    cairo_rectangle(cr, 0, 0, mastering::kPanelWidth, mastering::kPanelHeight);
    cairo_clip(cr);
    panel.tick(shared, state);
    panel.draw(cr, shared, state);
    cairo_restore(cr);
    cairo_surface_flush(backing);

    cairo_t* xcr = cairo_create(windowSurface);
    cairo_set_source_surface(xcr, backing, 0, 0);
    cairo_paint(xcr);
    cairo_destroy(xcr);
    // Required: cairo buffers the composite for Xlib surfaces.
    cairo_surface_flush(windowSurface);
    XFlush(dpy);
  }

  int idle() {
    while (dpy && XPending(dpy) > 0) {
      XEvent ev;
      XNextEvent(dpy, &ev);
      handleEvent(ev);
    }
    const auto now = std::chrono::steady_clock::now();
    if (dirty || now - lastFrame >= kFrameInterval ||
        resetPulseState != (now < resetPulseUntil)) {
      draw();
      dirty = false;
      lastFrame = now;
    }
    return 0;
  }
};

LV2UI_Handle uiInstantiate(const LV2UI_Descriptor*, const char* /*pluginUri*/,
                           const char* bundlePath, LV2UI_Write_Function write,
                           LV2UI_Controller controller, LV2UI_Widget* widget,
                           const LV2_Feature* const* features) {
  void* parent = nullptr;
  const LV2UI_Resize* hostResize = nullptr;
  const float* scaleHint = nullptr;
  for (const LV2_Feature* const* f = features; f && *f; ++f) {
    if (!std::strcmp((*f)->URI, LV2_UI__parent)) {
      parent = (*f)->data;
    } else if (!std::strcmp((*f)->URI, LV2_UI__resize)) {
      hostResize = static_cast<const LV2UI_Resize*>(f[0]->data);
    } else if (!std::strcmp((*f)->URI, LV2_UI__scaleFactor)) {
      scaleHint = static_cast<const float*>(f[0]->data);
    }
  }

  auto* ui = new Ui();
  ui->write = write;
  ui->controller = controller;

  ui->dpy = XOpenDisplay(nullptr);
  if (!ui->dpy) {
    delete ui;
    return nullptr;
  }

  const int screen = DefaultScreen(ui->dpy);
  const double initialScale =
      scaleHint && *scaleHint > 0.0f ? mastering::clampScale(*scaleHint) : 1.0;
  const mastering::UiSize initial = mastering::sizeForScale(initialScale);
  ui->width = initial.width;
  ui->height = initial.height;

  const Window parentWindow =
      parent ? static_cast<Window>(reinterpret_cast<uintptr_t>(parent))
             : RootWindow(ui->dpy, screen);

  XSetWindowAttributes attrs{};
  attrs.background_pixel = BlackPixel(ui->dpy, screen);
  attrs.event_mask = ExposureMask | ButtonPressMask | ButtonReleaseMask |
                     PointerMotionMask | Button1MotionMask | ButtonMotionMask |
                     KeyPressMask | StructureNotifyMask;
  ui->win = XCreateWindow(ui->dpy, parentWindow, 0, 0, ui->width, ui->height,
                          0, CopyFromParent, InputOutput, CopyFromParent,
                          CWBackPixel | CWEventMask, &attrs);
  XStoreName(ui->dpy, ui->win, "Mastering Suite");
  XMapRaised(ui->dpy, ui->win);

  // suil derives the host widget's minimum and maximum size from the X11
  // size hints.  Without PMinSize the minimum becomes the current window size,
  // so the plugin window could only ever grow.
  {
    const mastering::UiSize minSize =
        mastering::sizeForScale(mastering::kMinUiScale);
    const mastering::UiSize maxSize =
        mastering::sizeForScale(mastering::kMaxUiScale);
    XSizeHints hints{};
    hints.flags = PMinSize | PMaxSize | PBaseSize | PWinGravity;
    hints.min_width = static_cast<int>(minSize.width);
    hints.min_height = static_cast<int>(minSize.height);
    hints.max_width = static_cast<int>(maxSize.width);
    hints.max_height = static_cast<int>(maxSize.height);
    hints.base_width = mastering::kPanelWidth;
    hints.base_height = mastering::kPanelHeight;
    hints.win_gravity = NorthWestGravity;
    XSetWMNormalHints(ui->dpy, ui->win, &hints);
  }

  ui->format = DefaultDepth(ui->dpy, screen) == 32 ? CAIRO_FORMAT_ARGB32
                                                   : CAIRO_FORMAT_RGB24;
  ui->windowSurface = cairo_xlib_surface_create(
      ui->dpy, ui->win, DefaultVisual(ui->dpy, screen), ui->width, ui->height);
  ui->rebuildSurfaces();
  XFlush(ui->dpy);

  std::string dir = bundlePath ? bundlePath : "";
  ui->panel.loadFont(dir);

  // Ask for the logical size once, so a host that sizes the window from the
  // widget's preferred size opens us at 1040x640.  After this the host owns
  // the size and we only follow allocations.
  if (hostResize && hostResize->ui_resize) {
    hostResize->ui_resize(hostResize->handle, static_cast<int>(ui->width),
                          static_cast<int>(ui->height));
  }

  *widget = reinterpret_cast<LV2UI_Widget>(static_cast<uintptr_t>(ui->win));
  return ui;
}

void uiCleanup(LV2UI_Handle handle) {
  auto* ui = static_cast<Ui*>(handle);
  if (ui->cr) {
    cairo_destroy(ui->cr);
  }
  if (ui->backing) {
    cairo_surface_destroy(ui->backing);
  }
  if (ui->windowSurface) {
    cairo_surface_destroy(ui->windowSurface);
  }
  if (ui->dpy) {
    XDestroyWindow(ui->dpy, ui->win);
    XCloseDisplay(ui->dpy);
  }
  delete ui;
}

void uiPortEvent(LV2UI_Handle handle, uint32_t port, uint32_t size,
                 uint32_t format, const void* buffer) {
  auto* ui = static_cast<Ui*>(handle);
  if (format != 0 || size != sizeof(float) || !buffer) {
    return;
  }
  const float v = *static_cast<const float*>(buffer);
  if (port >= mastering::kLv2FirstParam &&
      port < mastering::kLv2FirstParam + mastering::P_COUNT) {
    ui->shared.setFromHost(port - mastering::kLv2FirstParam,
                           static_cast<double>(v));
    ui->dirty = true;
    return;
  }
  if (port >= mastering::kLv2FirstMeter &&
      port < mastering::kLv2FirstMeter + mastering::kLv2MeterCount) {
    switch (port - mastering::kLv2FirstMeter) {
      case mastering::kLv2InPeakL:
        ui->shared.inPeakL.store(v);
        break;
      case mastering::kLv2InPeakR:
        ui->shared.inPeakR.store(v);
        break;
      case mastering::kLv2OutPeakL:
        ui->shared.outPeakL.store(v);
        break;
      case mastering::kLv2OutPeakR:
        ui->shared.outPeakR.store(v);
        break;
      case mastering::kLv2TruePeakL:
        ui->shared.truePeakL.store(v);
        break;
      case mastering::kLv2TruePeakR:
        ui->shared.truePeakR.store(v);
        break;
      case mastering::kLv2LufsI:
        ui->shared.lufsIntegrated.store(v);
        break;
      case mastering::kLv2LufsS:
        ui->shared.lufsShortTerm.store(v);
        break;
      case mastering::kLv2LufsM:
        ui->shared.lufsMomentary.store(v);
        break;
      case mastering::kLv2Lra:
        ui->shared.lra.store(v);
        break;
      case mastering::kLv2Correlation:
        ui->shared.correlation.store(v);
        break;
      case mastering::kLv2CompGr:
        ui->shared.compGr.store(v);
        break;
      case mastering::kLv2LimitGr:
        ui->shared.limitGr.store(v);
        break;
      case mastering::kLv2TruePeakMax:
        ui->shared.truePeakMax.store(v);
        break;
      case mastering::kLv2SamplePeakMax:
        ui->shared.samplePeakMax.store(v);
        break;
      case mastering::kLv2ShortTermMax:
        ui->shared.shortTermMax.store(v);
        break;
      case mastering::kLv2Plr:
        ui->shared.plr.store(v);
        break;
      case mastering::kLv2Psr:
        ui->shared.psr.store(v);
        break;
      default:
        break;
    }
    ui->dirty = true;
    return;
  }
  if (port >= mastering::kLv2FirstSpectrum &&
      port < mastering::kLv2FirstSpectrum + mastering::kLv2SpectrumCount) {
    const uint32_t bin = port - mastering::kLv2FirstSpectrum;
    ui->shared.spectrum[bin].store(v, std::memory_order_relaxed);
    ui->dirty = true;
    return;
  }
  if (port == mastering::kLv2SampleRate && v > 0.0f) {
    ui->shared.sampleRate.store(v, std::memory_order_relaxed);
    ui->dirty = true;
    return;
  }
  // Meter reset and latency are for the host, not the panel.
}

int uiIdle(LV2UI_Handle handle) {
  return static_cast<Ui*>(handle)->idle();
}

const LV2UI_Idle_Interface kIdleInterface = {uiIdle};

const void* uiExtensionData(const char* uri) {
  if (uri && !std::strcmp(uri, LV2_UI__idleInterface)) {
    return &kIdleInterface;
  }
  return nullptr;
}

const LV2UI_Descriptor kUiDescriptor = {kUiUri, uiInstantiate, uiCleanup,
                                        uiPortEvent, uiExtensionData};

} // namespace

LV2_SYMBOL_EXPORT const LV2UI_Descriptor* lv2ui_descriptor(uint32_t index) {
  return index == 0 ? &kUiDescriptor : nullptr;
}
