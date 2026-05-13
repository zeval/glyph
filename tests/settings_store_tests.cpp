#include "settings_store.h"

#include <catch2/catch_test_macros.hpp>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

class TempDirectory {
public:
  TempDirectory() {
    char path_template[] = "/tmp/glyph-settings-XXXXXX";
    char* created = mkdtemp(path_template);
    REQUIRE(created != nullptr);
    path_ = created;
  }

  ~TempDirectory() {
    std::remove(pathFor("saves/settings.json").c_str());
    rmdir(pathFor("saves").c_str());
    rmdir(path_.c_str());
  }

  std::string pathFor(const std::string& name) const {
    return path_ + "/" + name;
  }

private:
  std::string path_;
};

} // namespace

TEST_CASE("settings store clamps scroll step mode") {
  REQUIRE(glyph::clampScrollStepMode(-4) == 0);
  REQUIRE(glyph::clampScrollStepMode(2) == 2);
  REQUIRE(glyph::clampScrollStepMode(12) == 3);
  REQUIRE(glyph::scrollStepModeLabel(0) == "1 line");
  REQUIRE(glyph::scrollStepModeLabel(3) == "full page");
}

TEST_CASE("settings store saves scroll step mode") {
  TempDirectory temp;
  const std::string path = temp.pathFor("saves/settings.json");

  glyph::AppSettings settings;
  settings.scroll_step_mode = 1;
  REQUIRE(glyph::saveAppSettings(path, settings));

  const glyph::AppSettings loaded = glyph::loadAppSettings(path);
  REQUIRE(loaded.scroll_step_mode == 1);
}
