// Mastering Suite — LV2 host smoke test using lilv.
// SPDX-License-Identifier: GPL-3.0-or-later

#include <lv2/core/lv2.h>
#include <lilv/lilv.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>
#include <vector>

#include "lv2_ports.hpp"
#include "params.hpp"
#include "presets.hpp"

namespace {

constexpr const char* kPluginUri =
    "https://github.com/tylorismusic-commits/mastering-suite";

int gFailures = 0;

void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) {
    ++gFailures;
  }
}

LilvNode* uri(LilvWorld* w, const char* s) { return lilv_new_uri(w, s); }

} // namespace

int main(int argc, char** argv) {
  std::string bundle = argc > 1 ? argv[1] : "build/lv2/mastering-suite.lv2";
  if (bundle.empty() || bundle.back() != '/') {
    bundle += '/';
  }
  LilvWorld* world = lilv_world_new();
  LilvNode* pluginUri = uri(world, kPluginUri);
  lilv_world_load_bundle(world, lilv_new_file_uri(world, nullptr, bundle.c_str()));
  const LilvPlugins* plugins = lilv_world_get_all_plugins(world);
  const LilvPlugin* plugin = lilv_plugins_get_by_uri(plugins, pluginUri);
  check(plugin != nullptr, "bundle loads and plugin is found");
  if (!plugin) {
    return 1;
  }

  {
    LilvNode* name = lilv_plugin_get_name(plugin);
    const char* n = name ? lilv_node_as_string(name) : "";
    char label[128];
    std::snprintf(label, sizeof(label),
                  "plugin is named 'Mastering Suite' (got '%s')", n);
    check(std::strcmp(n, "Mastering Suite") == 0, label);
    if (name) lilv_node_free(name);
  }

  const LilvUIs* uis = lilv_plugin_get_uis(plugin);
  check(lilv_uis_size(uis) == 1, "one X11 UI declared");

  const uint32_t nPorts = lilv_plugin_get_num_ports(plugin);
  char portCountLabel[192];
  std::snprintf(portCountLabel, sizeof(portCountLabel),
                "%u ports declared (%u expected: 4 audio + %u params + reset + "
                "%u meters + %u spectrum + latency), got %u",
                mastering::kLv2PortCount, mastering::kLv2PortCount,
                static_cast<unsigned>(mastering::P_COUNT),
                static_cast<unsigned>(mastering::kLv2MeterCount),
                mastering::kLv2SpectrumCount, nPorts);
  check(nPorts == mastering::kLv2PortCount, portCountLabel);

  // Every parameter symbol must survive into the TTL, in order, because the
  // port index is part of the saved-session format.
  {
    bool symbolsOk = true;
    for (uint32_t id = 0; id < mastering::P_COUNT; ++id) {
      const LilvPort* p = lilv_plugin_get_port_by_index(
          plugin, mastering::kLv2FirstParam + id);
      const LilvNode* sym = p ? lilv_port_get_symbol(plugin, p) : nullptr;
      const char* got = sym ? lilv_node_as_string(sym) : "(missing)";
      if (std::strcmp(got, mastering::kParamSpecs[id].symbol) != 0) {
        std::printf("  port %u symbol '%s', expected '%s'\n",
                    mastering::kLv2FirstParam + id, got,
                    mastering::kParamSpecs[id].symbol);
        symbolsOk = false;
      }
    }
    check(symbolsOk, "parameter symbols match the port order");
  }

  // Every control port's default must sit inside its declared range.
  {
    std::vector<float> mins(nPorts), maxs(nPorts), defs(nPorts);
    lilv_plugin_get_port_ranges_float(plugin, mins.data(), maxs.data(),
                                      defs.data());
    LilvNode* audioPort = uri(world, LV2_CORE__AudioPort);
    bool rangesOk = true;
    for (uint32_t i = 0; i < nPorts; ++i) {
      const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
      if (lilv_port_is_a(plugin, port, audioPort)) {
        continue;
      }
      if (!(mins[i] <= maxs[i]) || defs[i] < mins[i] || defs[i] > maxs[i]) {
        std::printf("  port %u range broken: min %g max %g default %g\n", i,
                    mins[i], maxs[i], defs[i]);
        rangesOk = false;
      }
    }
    lilv_node_free(audioPort);
    check(rangesOk, "port ranges and defaults are consistent");
  }

  // ---------------------------------------------------------------------
  // Presets: present, attached, complete and every symbol resolves.
  // ---------------------------------------------------------------------
  {
    LilvNode* rdfType =
        uri(world, "http://www.w3.org/1999/02/22-rdf-syntax-ns#type");
    LilvNode* presetClass =
        uri(world, "http://lv2plug.in/ns/ext/presets#Preset");
    LilvNode* bankClass =
        uri(world, "http://lv2plug.in/ns/ext/presets#Bank");
    LilvNode* appliesTo = uri(world, "http://lv2plug.in/ns/lv2core#appliesTo");
    LilvNode* lv2Port = uri(world, "http://lv2plug.in/ns/lv2core#port");
    LilvNode* lv2Symbol = uri(world, "http://lv2plug.in/ns/lv2core#symbol");
    LilvNode* psetValue =
        uri(world, "http://lv2plug.in/ns/ext/presets#value");

    LilvNodes* presets =
        lilv_world_find_nodes(world, nullptr, rdfType, presetClass);
    LilvNodes* banks =
        lilv_world_find_nodes(world, nullptr, rdfType, bankClass);

    char countLabel[128];
    const unsigned found =
        presets ? static_cast<unsigned>(lilv_nodes_size(presets)) : 0u;
    std::snprintf(countLabel, sizeof(countLabel),
                  "%d presets declared (got %u)", mastering::kPresetCount,
                  found);
    check(static_cast<int>(found) == mastering::kPresetCount, countLabel);

    std::set<std::string> validSymbols;
    for (uint32_t id = 0; id < mastering::P_COUNT; ++id) {
      validSymbols.insert(mastering::kParamSpecs[id].symbol);
    }

    bool attached = true;
    bool symbolsResolve = true;
    bool complete = true;
    int index = 0;
    LILV_FOREACH(nodes, it, presets) {
      const LilvNode* preset = lilv_nodes_get(presets, it);
      LilvNode* target = lilv_world_get(world, preset, appliesTo, nullptr);
      if (!target || !lilv_node_equals(target, pluginUri)) {
        std::printf("  preset %d does not apply to this plugin\n", index);
        attached = false;
      }
      if (target) lilv_node_free(target);

      LilvNodes* ports =
          lilv_world_find_nodes(world, preset, lv2Port, nullptr);
      const unsigned values =
          ports ? static_cast<unsigned>(lilv_nodes_size(ports)) : 0u;
      if (values != mastering::P_COUNT) {
        std::printf("  preset %d has %u values, expected %u\n", index, values,
                    static_cast<unsigned>(mastering::P_COUNT));
        complete = false;
      }
      LILV_FOREACH(nodes, pit, ports) {
        const LilvNode* port = lilv_nodes_get(ports, pit);
        LilvNode* sym = lilv_world_get(world, port, lv2Symbol, nullptr);
        LilvNode* val = lilv_world_get(world, port, psetValue, nullptr);
        const char* s = sym ? lilv_node_as_string(sym) : "";
        if (validSymbols.find(s) == validSymbols.end()) {
          std::printf("  preset %d uses unknown symbol '%s'\n", index, s);
          symbolsResolve = false;
        }
        if (!val) {
          std::printf("  preset %d symbol '%s' has no value\n", index, s);
          symbolsResolve = false;
        }
        if (sym) lilv_node_free(sym);
        if (val) lilv_node_free(val);
      }
      lilv_nodes_free(ports);
      ++index;
    }
    check(attached, "every preset applies to this plugin");
    check(symbolsResolve, "every preset port symbol resolves to a parameter");
    check(complete, "every preset carries the full parameter state");
    check(banks && lilv_nodes_size(banks) >= 1, "preset banks are declared");

    lilv_nodes_free(presets);
    lilv_nodes_free(banks);
    lilv_node_free(rdfType);
    lilv_node_free(presetClass);
    lilv_node_free(bankClass);
    lilv_node_free(appliesTo);
    lilv_node_free(lv2Port);
    lilv_node_free(lv2Symbol);
    lilv_node_free(psetValue);
  }

  std::vector<float> values(nPorts, 0.0f);
  for (uint32_t i = 0; i < nPorts; ++i) {
    const LilvPort* port = lilv_plugin_get_port_by_index(plugin, i);
    LilvNode* def = lilv_port_get(plugin, port, uri(world, LV2_CORE__default));
    values[i] = def ? lilv_node_as_float(def) : 0.0f;
    if (def) lilv_node_free(def);
  }

  LilvInstance* inst = lilv_plugin_instantiate(plugin, 48000.0, nullptr);
  check(inst != nullptr, "plugin instantiates");
  if (!inst) {
    return 1;
  }

  const uint32_t frames = 256;
  std::vector<float> inL(frames), inR(frames), outL(frames), outR(frames);
  std::vector<float*> audio = {inL.data(), inR.data(), outL.data(),
                               outR.data()};
  for (uint32_t i = 0; i < 4; ++i) {
    lilv_instance_connect_port(inst, i, audio[i]);
  }
  for (uint32_t i = 4; i < nPorts; ++i) {
    lilv_instance_connect_port(inst, i, &values[i]);
  }

  auto setParam = [&](uint32_t id, float v) {
    values[mastering::kLv2FirstParam + id] = v;
  };

  setParam(mastering::P_EQ_ON, 1.0f);
  setParam(mastering::P_COMP_ON, 1.0f);
  setParam(mastering::P_COMP_THRESHOLD, 0.35f);
  setParam(mastering::P_COMP_RATIO, 1.0f);
  setParam(mastering::P_COLOR_ON, 1.0f);
  setParam(mastering::P_COLOR_DRIVE, 0.5f);
  setParam(mastering::P_COLOR_MIX, 0.6f);
  setParam(mastering::P_DENS_ON, 1.0f);
  setParam(mastering::P_DENS_EFFECT, 0.3f);
  setParam(mastering::P_LIMIT_ON, 1.0f);
  setParam(mastering::P_LIMIT_CEILING, 0.9f);

  lilv_instance_activate(inst);
  float peak = 0.0f;
  bool finite = true;
  for (int block = 0; block < 160; ++block) {
    for (uint32_t i = 0; i < frames; ++i) {
      const float t = static_cast<float>(block * frames + i) / 48000.0f;
      inL[i] = inR[i] = 0.3f * std::sin(2.0f * 3.14159265f * 220.0f * t);
    }
    lilv_instance_run(inst, frames);
    for (uint32_t i = 0; i < frames; ++i) {
      if (!std::isfinite(outL[i]) || !std::isfinite(outR[i])) {
        finite = false;
      }
      peak = std::max(peak, std::fabs(outL[i]));
    }
  }
  check(finite, "LV2 processing stays finite");
  check(peak > 0.01f && peak < 2.0f, "LV2 output has a sane level");

  const uint32_t meters = mastering::kLv2FirstMeter;
  check(values[meters + mastering::kLv2OutPeakL] > -60.0f,
        "output peak meter reports activity");
  check(values[meters + mastering::kLv2TruePeakL] > -60.0f,
        "true-peak meter reports activity");
  check(values[meters + mastering::kLv2LufsI] > -60.0f,
        "integrated LUFS port reports activity");
  check(values[meters + mastering::kLv2Correlation] > 0.9f,
        "correlation reports a coherent stereo signal");
  check(values[meters + mastering::kLv2TruePeakMax] > -60.0f,
        "true-peak max port reports activity");
  check(values[meters + mastering::kLv2SamplePeakMax] > -60.0f,
        "sample-peak max port reports activity");
  check(values[meters + mastering::kLv2Plr] > 0.0f,
        "PLR port reports activity");
  check(values[meters + mastering::kLv2Psr] > 0.0f,
        "PSR port reports activity");
  bool spectrum = false;
  for (uint32_t i = 0; i < mastering::kLv2SpectrumCount; ++i) {
    if (values[mastering::kLv2FirstSpectrum + i] > -110.0f) {
      spectrum = true;
    }
  }
  check(spectrum, "spectrum ports report activity");
  check(values[mastering::kLv2SampleRate] == 48000.0f,
        "sample-rate port reports the host rate");
  check(values[mastering::kLv2Latency] == 285.0f,
        "latency port reports the 4x limiter latency");

  // Rising-edge meter reset clears the integrated history.
  values[mastering::kLv2MeterReset] = 1.0f;
  lilv_instance_run(inst, frames);
  values[mastering::kLv2MeterReset] = 0.0f;
  check(values[meters + mastering::kLv2LufsI] <= -100.0f,
        "meter reset clears integrated LUFS");

  // Mono output fallback: disconnect the right output and check the left
  // channel still carries processed audio.
  {
    lilv_instance_connect_port(inst, mastering::kLv2OutR, nullptr);
    for (uint32_t i = 0; i < frames; ++i) {
      const float t = static_cast<float>(i) / 48000.0f;
      inL[i] = 0.25f * std::sin(2.0f * 3.14159265f * 220.0f * t);
      inR[i] = 0.12f * std::sin(2.0f * 3.14159265f * 330.0f * t);
    }
    lilv_instance_run(inst, frames);
    bool monoFinite = true;
    float monoPeak = 0.0f;
    for (uint32_t i = 0; i < frames; ++i) {
      if (!std::isfinite(outL[i])) monoFinite = false;
      monoPeak = std::max(monoPeak, std::fabs(outL[i]));
    }
    check(monoFinite && monoPeak > 0.01f,
          "mono LV2 output falls back without losing the left channel");
  }

  lilv_instance_deactivate(inst);
  lilv_instance_free(inst);
  lilv_node_free(pluginUri);
  lilv_world_free(world);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
