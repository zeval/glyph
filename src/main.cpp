#include "glyph_app.h"

#include <cstdio>

int main(int argc, char** argv) {
  glyph::AppConfig config;
  config.width = 480;
  config.height = 272;
  if (argc > 1 && argv[1] != nullptr) {
    config.initial_book_path = argv[1];
  }

  glyph::App app(config);
  if (!app.init()) {
    std::fprintf(stderr, "glyph: app init failed\n");
    return 1;
  }

  return app.run();
}
