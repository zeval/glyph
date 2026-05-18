#include "glyph_app.h"

#include "library.h"
#include "settings_store.h"

#include <SDL_image.h>

#include <cstdio>

namespace glyph {

namespace {

#if defined(GLYPH_PLATFORM_PSP)
constexpr int kUiFontSize = 15;
#else
constexpr int kUiFontSize = 14;
#endif

} // namespace

App::App(AppConfig config) : config_(config) {
  const AppSettings settings = loadAppSettings(defaultSettingsPath());
  scroll_step_mode_ = clampScrollStepMode(settings.scroll_step_mode);
  updateSettingsLabels();

  refreshLibrary();
  setReaderText("No book open", "Open an EPUB from the library.",
                std::string("Drop DRM-free EPUB files in ") + defaultBooksPath() +
                    " and open one from the library. This build supports simple text EPUBs.");
  if (!config_.initial_book_path.empty()) {
    openBookPath(config_.initial_book_path);
  }
}

App::~App() {
  shutdown();
}

bool App::init() {
#if defined(GLYPH_PLATFORM_PSP)
  SDL_SetHint(SDL_HINT_RENDER_DRIVER, "software");
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "0");
#endif

  if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
    std::fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
    return false;
  }

  const int image_flags = IMG_INIT_PNG | IMG_INIT_JPG;
  if ((IMG_Init(image_flags) & image_flags) != image_flags) {
    std::fprintf(stderr, "IMG_Init warning: %s\n", IMG_GetError());
  }

  if (TTF_Init() != 0) {
    std::fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
    return false;
  }

  window_ = SDL_CreateWindow("glyph", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, config_.width,
                             config_.height, SDL_WINDOW_SHOWN);
  if (window_ == nullptr) {
    std::fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
    return false;
  }

#if defined(GLYPH_PLATFORM_PSP)
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
#else
  renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
#endif
  if (renderer_ == nullptr) {
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_SOFTWARE);
  }
  if (renderer_ == nullptr) {
    std::fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
    return false;
  }

  SDL_RenderSetLogicalSize(renderer_, config_.width, config_.height);
  SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
  loadFont();
  if (!reader_text_.empty()) {
    reader_lines_ = wrapReaderText(reader_text_);
  }
  running_ = true;
  needs_render_ = true;
  return true;
}

int App::run() {
  while (running_) {
    InputState input;
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      if (event.type == SDL_WINDOWEVENT) {
        needs_render_ = true;
      }
      handleEvent(event, input);
    }

    const Screen previous_screen = screen_;
    const int previous_book = selected_book_;
    const int previous_setting = selected_setting_;
    const int previous_scroll = reader_scroll_;
    const size_t previous_line_count = reader_lines_.size();

    pollPlatformInput(input);
    applyInput(input);

    if (screen_ != previous_screen || selected_book_ != previous_book ||
        selected_setting_ != previous_setting || reader_scroll_ != previous_scroll ||
        reader_lines_.size() != previous_line_count) {
      needs_render_ = true;
    }

    if (needs_render_) {
      render();
      needs_render_ = false;
    }

    SDL_Delay(16);
  }

  return 0;
}

void App::shutdown() {
  clearTextCache();
  clearCoverTexture();
  if (font_ != nullptr) {
    TTF_CloseFont(font_);
    font_ = nullptr;
  }
  if (renderer_ != nullptr) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_ != nullptr) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  TTF_Quit();
  IMG_Quit();
  SDL_Quit();
}

bool App::loadFont() {
  const char* candidates[] = {
      "ef0:/PSP/GAME/glyph/assets/fonts/AtkinsonHyperlegibleNext-Regular.ttf",
      "ms0:/PSP/GAME/glyph/assets/fonts/AtkinsonHyperlegibleNext-Regular.ttf",
      "assets/fonts/AtkinsonHyperlegibleNext-Regular.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "/System/Library/Fonts/Supplemental/Arial Unicode.ttf",
      "/System/Library/Fonts/Supplemental/Arial.ttf",
  };

  for (const char* path : candidates) {
    font_ = TTF_OpenFont(path, kUiFontSize);
    if (font_ != nullptr) {
      TTF_SetFontHinting(font_, TTF_HINTING_NORMAL);
      return true;
    }
  }

  std::fprintf(stderr, "No usable TTF font found; text rendering disabled.\n");
  return false;
}

} // namespace glyph
