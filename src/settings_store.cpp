#include "settings_store.h"

#include "library.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace glyph {

namespace {

std::string joinPath(const std::string& directory, const std::string& name) {
  if (directory.empty()) {
    return name;
  }
  const char last = directory[directory.size() - 1];
  if (last == '/' || last == '\\') {
    return directory + name;
  }
  return directory + "/" + name;
}

bool ensureParentDirectory(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos || slash == 0) {
    return true;
  }

  const std::string directory = path.substr(0, slash);
  return mkdir(directory.c_str(), 0700) == 0 || errno == EEXIST;
}

std::string readWholeFile(const std::string& path) {
  std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
  if (!file) {
    return "";
  }

  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

int readJsonInt(const std::string& text, const std::string& key, int fallback) {
  const std::string quoted_key = "\"" + key + "\"";
  const size_t key_pos = text.find(quoted_key);
  if (key_pos == std::string::npos) {
    return fallback;
  }

  const size_t colon = text.find(':', key_pos + quoted_key.size());
  if (colon == std::string::npos) {
    return fallback;
  }

  char* end = nullptr;
  const long value = std::strtol(text.c_str() + colon + 1, &end, 10);
  if (end == text.c_str() + colon + 1) {
    return fallback;
  }
  return static_cast<int>(value);
}

} // namespace

std::string defaultSettingsPath() {
  return joinPath(joinPath(defaultStorageRootPath(), "saves"), "settings.json");
}

AppSettings loadAppSettings(const std::string& path) {
  AppSettings settings;
  const std::string text = readWholeFile(path);
  if (text.empty()) {
    return settings;
  }

  settings.scroll_step_mode =
      clampScrollStepMode(readJsonInt(text, "scroll_step_mode", settings.scroll_step_mode));
  return settings;
}

bool saveAppSettings(const std::string& path, const AppSettings& settings) {
  if (!ensureParentDirectory(path)) {
    return false;
  }

  std::ofstream file(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file) {
    return false;
  }

  file << "{\n"
       << "  \"version\": 1,\n"
       << "  \"scroll_step_mode\": " << clampScrollStepMode(settings.scroll_step_mode) << "\n"
       << "}\n";
  return static_cast<bool>(file);
}

int clampScrollStepMode(int mode) {
  return std::max(0, std::min(3, mode));
}

std::string scrollStepModeLabel(int mode) {
  switch (clampScrollStepMode(mode)) {
  case 0:
    return "1 line";
  case 1:
    return "1/3 page";
  case 2:
    return "1/2 page";
  case 3:
    return "full page";
  }
  return "1/2 page";
}

} // namespace glyph
