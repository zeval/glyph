#ifndef GLYPH_SETTINGS_STORE_H
#define GLYPH_SETTINGS_STORE_H

#include <string>

namespace glyph {

struct AppSettings {
  int scroll_step_mode = 2;
};

std::string defaultSettingsPath();
AppSettings loadAppSettings(const std::string& path);
bool saveAppSettings(const std::string& path, const AppSettings& settings);

int clampScrollStepMode(int mode);
std::string scrollStepModeLabel(int mode);

} // namespace glyph

#endif // GLYPH_SETTINGS_STORE_H
