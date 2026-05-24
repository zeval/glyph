#include "glyph_app.h"
#include "glyph_cli.h"

#include <SDL.h>

#include <cstdio>

int main(int argc, char** argv) {
  const glyph::CliParseResult parsed = glyph::parseCommandLine(argc, argv);
  if (!parsed.ok) {
    std::fprintf(stderr, "glyph: %s\n%s", parsed.error.c_str(), glyph::usageText());
    return 2;
  }
  if (parsed.options.mode == glyph::LaunchMode::Help) {
    std::printf("%s", glyph::usageText());
    return 0;
  }

  glyph::AppConfig config;
  config.width = parsed.options.width;
  config.height = parsed.options.height;
  config.initial_book_path = parsed.options.initial_book_path;

  if (parsed.options.mode == glyph::LaunchMode::Snapshot) {
    SDL_setenv("SDL_VIDEODRIVER", "dummy", 0);
  }

  glyph::App app(config);
  if (!app.init()) {
    std::fprintf(stderr, "glyph: app init failed\n");
    return 1;
  }

  if (parsed.options.mode == glyph::LaunchMode::Snapshot) {
    if (!app.renderSnapshot(parsed.options.snapshot_kind, parsed.options.snapshot_output_path)) {
      std::fprintf(stderr, "glyph: snapshot failed: %s\n",
                   parsed.options.snapshot_output_path.c_str());
      return 1;
    }
    return 0;
  }

  return app.run();
}
