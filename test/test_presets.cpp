// Mastering Suite - user preset tests
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>

#include "params.hpp"
#include "presets.hpp"
#include "ui/panel.hpp"

using namespace mastering;

namespace {
int gFailures = 0;
void check(bool ok, const char* what) {
  std::printf("%s  %s\n", ok ? "PASS" : "FAIL", what);
  if (!ok) ++gFailures;
}
} // namespace

int main() {
  namespace fs = std::filesystem;
  const fs::path root = fs::temp_directory_path() / "mastering-preset-test";
  fs::remove_all(root);
  fs::create_directories(root / "config");
  fs::create_directories(root / "home");
  const std::string oldXdg =
      std::getenv("XDG_CONFIG_HOME") ? std::getenv("XDG_CONFIG_HOME") : "";
  const std::string oldHome =
      std::getenv("HOME") ? std::getenv("HOME") : "";
  setenv("XDG_CONFIG_HOME", (root / "config").string().c_str(), 1);
  setenv("HOME", (root / "home").string().c_str(), 1);

  Shared shared;
  PanelState state;
  Panel panel;
  shared.set(P_EQ_LOW_GAIN, 0.63);
  const double saved = shared.get(P_EQ_LOW_GAIN);
  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 100, 450);
  check(state.savePreset, "SAVE opens the name field");
  for (char c : std::string("My Master")) panel.textKey(shared, state, 0, c);
  panel.textKey(shared, state, Panel::kTextEnter, 0);
  check(state.userPresets.size() == 1, "user preset is written");
  check(state.presetIndex == kPresetCount, "saved preset becomes active");
  shared.set(P_EQ_LOW_GAIN, 0.1);
  panel.applyPreset(shared, state, state.presetIndex);
  check(std::fabs(shared.get(P_EQ_LOW_GAIN) - saved) < 1e-9,
        "user preset recalls every parameter");

  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 100, 480);
  for (char c : std::string("Renamed")) panel.textKey(shared, state, 0, c);
  panel.textKey(shared, state, Panel::kTextEnter, 0);
  check(state.userPresets.size() == 1 &&
            state.userPresets[0].name == "Renamed",
        "user preset renames");

  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 180, 450);
  check(state.userPresets.size() == 2, "DUPLICATE creates a preset");

  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 250, 480);
  const fs::path exported =
      userPresetExportDir() / (std::string("Renamed copy") + kUserPresetExt);
  check(fs::exists(exported), "EXPORT writes a preset file");

  panel.handlePresetClick(shared, state, 72 + 165, 32);
  panel.handlePresetClick(shared, state, 180, 480);
  panel.handlePresetClick(shared, state, 180, 480);
  check(state.userPresets.size() == 1, "DELETE removes the preset");
  check(panel.importDroppedFile(shared, state, exported.string()),
        "IMPORT accepts a dropped preset file");
  check(state.userPresets.size() == 2, "import adds a preset");

  if (!oldXdg.empty()) setenv("XDG_CONFIG_HOME", oldXdg.c_str(), 1);
  else unsetenv("XDG_CONFIG_HOME");
  if (!oldHome.empty()) setenv("HOME", oldHome.c_str(), 1);
  else unsetenv("HOME");
  fs::remove_all(root);
  std::printf("%d failure(s)\n", gFailures);
  return gFailures == 0 ? 0 : 1;
}
