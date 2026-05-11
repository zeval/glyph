#include "glyph_app.h"

#include <cstdio>

int main(int, char**) {
  glyph::App app({480, 272});
  if (!app.init()) {
    std::fprintf(stderr, "glyph: app init failed\n");
    return 1;
  }

  return app.run();
}
