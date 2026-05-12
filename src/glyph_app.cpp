#include "glyph_app.h"

#include "epub.h"
#include "library.h"
#include "text_layout.h"

#include <SDL_image.h>

#include <algorithm>
#include <cstdio>

#if defined(GLYPH_PLATFORM_PSP)
#include <pspctrl.h>
#endif

namespace glyph {

namespace {

constexpr SDL_Color kBg = {12, 14, 16, 255};
constexpr SDL_Color kPanel = {24, 28, 32, 255};
constexpr SDL_Color kPanelHi = {38, 46, 54, 255};
constexpr SDL_Color kText = {232, 230, 222, 255};
constexpr SDL_Color kMuted = {148, 156, 160, 255};
constexpr SDL_Color kAccent = {98, 164, 168, 255};
constexpr SDL_Color kWarn = {204, 166, 92, 255};

constexpr uint32_t kHoldThresholdMs = 320;
constexpr uint32_t kScrollRepeatMs = 90;
#if defined(GLYPH_PLATFORM_PSP)
constexpr int kUiFontSize = 15;
#else
constexpr int kUiFontSize = 14;
#endif

const char* screenName(Screen screen) {
  switch (screen) {
  case Screen::Browser:
    return "Library";
  case Screen::Reader:
    return "Reader";
  case Screen::Settings:
    return "Settings";
  }
  return "glyph";
}

std::string authorsLine(const std::vector<std::string>& authors) {
  if (authors.empty()) {
    return "Unknown author";
  }
  std::string out = authors[0];
  for (size_t i = 1; i < authors.size(); ++i) {
    out += ", " + authors[i];
  }
  return out;
}

} // namespace

App::App(AppConfig config) : config_(config) {
  settings_ = {
      "Theme: dark",
      "Font: Atkinson",
      "Reader mode: page + scroll",
      std::string("Storage: ") + defaultStorageRootPath(),
  };

  refreshLibrary();
  setReaderText("No book open", "Open an EPUB from the library.",
                std::string("Drop DRM-free EPUB files in ") + defaultBooksPath() +
                    " and open one from the library. The PoC supports simple text EPUBs.");
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
  running_ = true;
  return true;
}

int App::run() {
  while (running_) {
    InputState input;
    SDL_Event event;
    while (SDL_PollEvent(&event) != 0) {
      handleEvent(event, input);
    }

    pollPlatformInput(input);
    applyInput(input);
    render();
    SDL_Delay(16);
  }

  return 0;
}

void App::shutdown() {
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

void App::handleEvent(const SDL_Event& event, InputState& input) {
  if (event.type == SDL_QUIT) {
    input.quit = true;
    return;
  }

  if (event.type != SDL_KEYDOWN || event.key.repeat != 0) {
    return;
  }

  const SDL_Keycode key = event.key.keysym.sym;
  switch (key) {
  case SDLK_ESCAPE:
    input.back = true;
    break;
  case SDLK_RETURN:
  case SDLK_KP_ENTER:
    input.accept = true;
    break;
  case SDLK_UP:
    input.up = true;
    break;
  case SDLK_DOWN:
    input.down = true;
    break;
  case SDLK_LEFT:
    input.left = true;
    break;
  case SDLK_RIGHT:
    input.right = true;
    break;
  case SDLK_q:
  case SDLK_PAGEUP:
    input.shoulder_l_click = true;
    break;
  case SDLK_e:
  case SDLK_PAGEDOWN:
    input.shoulder_r_click = true;
    break;
  case SDLK_s:
    input.menu = true;
    break;
  case SDLK_t:
    input.toc = true;
    break;
  case SDLK_b:
    input.bookmark = true;
    break;
  case SDLK_TAB:
    input.status = true;
    break;
  default:
    break;
  }
}

void App::pollPlatformInput(InputState& input) {
#if defined(GLYPH_PLATFORM_PSP)
  SceCtrlData pad = {};
  sceCtrlSetSamplingMode(PSP_CTRL_MODE_ANALOG);
  sceCtrlReadBufferPositive(&pad, 1);

  const uint32_t buttons = pad.Buttons;
  const uint32_t pressed = buttons & ~previous_platform_buttons_;

  input.up = input.up || ((pressed & PSP_CTRL_UP) != 0);
  input.down = input.down || ((pressed & PSP_CTRL_DOWN) != 0);
  input.left = input.left || ((pressed & PSP_CTRL_LEFT) != 0);
  input.right = input.right || ((pressed & PSP_CTRL_RIGHT) != 0);
  input.accept = input.accept || ((pressed & PSP_CTRL_CROSS) != 0);
  input.back = input.back || ((pressed & PSP_CTRL_CIRCLE) != 0);
  input.menu = input.menu || ((pressed & PSP_CTRL_START) != 0);
  input.toc = input.toc || ((pressed & PSP_CTRL_TRIANGLE) != 0);
  input.bookmark = input.bookmark || ((pressed & PSP_CTRL_SQUARE) != 0);
  input.status = input.status || ((pressed & PSP_CTRL_SELECT) != 0);
  const bool current_l = (buttons & PSP_CTRL_LTRIGGER) != 0;
  const bool current_r = (buttons & PSP_CTRL_RTRIGGER) != 0;
  input.shoulder_l_click = input.shoulder_l_click || ((pressed & PSP_CTRL_LTRIGGER) != 0);
  input.shoulder_r_click = input.shoulder_r_click || ((pressed & PSP_CTRL_RTRIGGER) != 0);
  input.shoulder_l_down = input.shoulder_l_down || current_l;
  input.shoulder_r_down = input.shoulder_r_down || current_r;
  previous_platform_buttons_ = buttons;
#else
  const uint8_t* keys = SDL_GetKeyboardState(nullptr);
  input.shoulder_l_down =
      input.shoulder_l_down || keys[SDL_SCANCODE_Q] || keys[SDL_SCANCODE_PAGEUP];
  input.shoulder_r_down =
      input.shoulder_r_down || keys[SDL_SCANCODE_E] || keys[SDL_SCANCODE_PAGEDOWN];
#endif
}

void App::applyInput(const InputState& input) {
  if (input.quit) {
    running_ = false;
  }

  if (screen_ == Screen::Reader) {
    const uint32_t now_ms = SDL_GetTicks();
    updateShoulderHold(input.shoulder_l_down, input.shoulder_r_down, now_ms);
  } else {
    left_hold_started_ms_ = 0;
    right_hold_started_ms_ = 0;
  }

  if (input.menu) {
    screen_ = Screen::Settings;
  }
  if (input.toc) {
    screen_ = Screen::Browser;
  }

  switch (screen_) {
  case Screen::Browser:
    if (input.down) {
      const int last_book = books_.empty() ? 0 : static_cast<int>(books_.size()) - 1;
      selected_book_ = std::min<int>(selected_book_ + 1, last_book);
    }
    if (input.up) {
      selected_book_ = std::max(0, selected_book_ - 1);
    }
    if ((input.accept || input.right || input.shoulder_r_click) && !books_.empty()) {
      openSelectedBook();
    }
    if (input.back || input.quit) {
      running_ = false;
    }
    break;

  case Screen::Reader:
    if (input.right || input.shoulder_r_click) {
      reader_scroll_ = std::min(maxReaderScroll(), reader_scroll_ + linesPerPage());
    }
    if (input.left || input.shoulder_l_click) {
      reader_scroll_ = std::max(0, reader_scroll_ - linesPerPage());
    }
    if (input.down) {
      reader_scroll_ = std::min(maxReaderScroll(), reader_scroll_ + 1);
    }
    if (input.up) {
      reader_scroll_ = std::max(0, reader_scroll_ - 1);
    }
    if (input.back) {
      screen_ = Screen::Browser;
    }
    break;

  case Screen::Settings:
    if (input.down) {
      selected_setting_ =
          std::min<int>(selected_setting_ + 1, static_cast<int>(settings_.size()) - 1);
    }
    if (input.up) {
      selected_setting_ = std::max(0, selected_setting_ - 1);
    }
    if (input.back || input.menu) {
      screen_ = Screen::Reader;
    }
    break;
  }
}

void App::updateShoulderHold(bool left_down, bool right_down, uint32_t now_ms) {
  if (left_down) {
    if (left_hold_started_ms_ == 0) {
      left_hold_started_ms_ = now_ms;
    }
    if (now_ms - left_hold_started_ms_ >= kHoldThresholdMs &&
        now_ms - last_scroll_step_ms_ >= kScrollRepeatMs) {
      reader_scroll_ = std::max(0, reader_scroll_ - 1);
      last_scroll_step_ms_ = now_ms;
    }
  } else {
    left_hold_started_ms_ = 0;
  }

  if (right_down) {
    if (right_hold_started_ms_ == 0) {
      right_hold_started_ms_ = now_ms;
    }
    if (now_ms - right_hold_started_ms_ >= kHoldThresholdMs &&
        now_ms - last_scroll_step_ms_ >= kScrollRepeatMs) {
      reader_scroll_ = std::min(maxReaderScroll(), reader_scroll_ + 1);
      last_scroll_step_ms_ = now_ms;
    }
  } else {
    right_hold_started_ms_ = 0;
  }
}

void App::refreshLibrary() {
  books_.clear();

  const LibraryScanResult library = discoverLibrary();
  books_path_ = library.books_path;
  for (const LibraryBook& discovered : library.books) {
    BookEntry book;
    book.path = discovered.file_path;
    book.title = discovered.display_name;
    book.subtitle = discovered.sample ? "No books discovered" : discovered.file_path;

    if (!book.path.empty()) {
      EpubDocument document;
      if (document.open(book.path)) {
        book.readable = true;
        book.title = document.book().metadata.title;
        book.subtitle = authorsLine(document.book().metadata.authors);
      } else {
        book.subtitle = document.error();
      }
    }
    books_.push_back(book);
  }
  selected_book_ = std::min<int>(selected_book_, static_cast<int>(books_.size()) - 1);
}

void App::openSelectedBook() {
  if (selected_book_ < 0 || selected_book_ >= static_cast<int>(books_.size())) {
    return;
  }

  const BookEntry& entry = books_[static_cast<size_t>(selected_book_)];
  if (entry.path.empty()) {
    setReaderText("Library empty", entry.subtitle,
                  "Create a books directory next to the host binary or use "
                  "ef0:/PSP/GAME/glyph/books/ on PSP Go.");
    screen_ = Screen::Reader;
    return;
  }

  openBookPath(entry.path);
}

void App::openBookPath(const std::string& path) {
  EpubDocument document;
  if (!document.open(path)) {
    setReaderText(displayNameForPath(path), "Could not open EPUB", document.error());
    screen_ = Screen::Reader;
    return;
  }

  EpubTextResult text = document.readAllReadableSpineText();
  if (!text.ok) {
    setReaderText(document.book().metadata.title, "Could not read EPUB text", text.error);
    screen_ = Screen::Reader;
    return;
  }

  setReaderText(document.book().metadata.title, authorsLine(document.book().metadata.authors),
                text.text);
  screen_ = Screen::Reader;
}

void App::setReaderText(const std::string& title, const std::string& status,
                        const std::string& text) {
  reader_title_ = title;
  reader_status_ = status;
  reader_lines_ = wrapReaderText(text);
  if (reader_lines_.empty()) {
    reader_lines_.push_back("No readable text.");
  }
  reader_scroll_ = 0;
}

std::vector<std::string> App::wrapReaderText(const std::string& text) const {
  std::vector<std::string> lines;

  TextLayoutConfig layout_config;
  layout_config.viewport_width = config_.width;
  layout_config.viewport_height = 100000;
  layout_config.margin_left = 0;
  layout_config.margin_top = 0;
  layout_config.margin_right = 0;
  layout_config.margin_bottom = 0;
  layout_config.average_char_width = 7;
  if (font_ != nullptr) {
    int sample_width = 0;
    int sample_height = 0;
    if (TTF_SizeUTF8(font_, "abcdefghijklmnopqrstuvwxyz", &sample_width, &sample_height) == 0) {
      layout_config.average_char_width = std::max(1, (sample_width + 25) / 26);
    }
  }
  layout_config.line_height = font_ != nullptr ? std::max(12, TTF_FontLineSkip(font_)) : 17;
  layout_config.paragraph_spacing = layout_config.line_height / 2;
  layout_config.blank_line_height = layout_config.line_height;

  const TextLayout layout = paginatePlainText(text, layout_config);
  for (const TextLayoutPage& page : layout.pages) {
    int next_y = 0;
    for (const TextLayoutLine& line : page.lines) {
      while (line.y - next_y >= layout_config.line_height) {
        lines.emplace_back();
        next_y += layout_config.line_height;
      }
      lines.push_back(line.text);
      next_y = line.y + layout_config.line_height;
    }
  }

  while (!lines.empty() && lines.back().empty()) {
    lines.pop_back();
  }
  return lines;
}

int App::linesPerPage() const {
  const int line_height = font_ != nullptr ? std::max(12, TTF_FontLineSkip(font_)) : 17;
  const int usable_height = std::max(24, config_.height - 74);
  return std::max(1, usable_height / line_height);
}

int App::maxReaderScroll() const {
  if (reader_lines_.empty()) {
    return 0;
  }
  return std::max(0, static_cast<int>(reader_lines_.size()) - linesPerPage());
}

void App::render() {
  SDL_RenderSetClipRect(renderer_, nullptr);
  SDL_RenderSetViewport(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, kBg.r, kBg.g, kBg.b, kBg.a);
  SDL_RenderClear(renderer_);
  fillRect(0, 0, config_.width, config_.height, kBg);

  fillRect(0, 0, config_.width, 24, kPanel);
  drawText("glyph", 8, 5, kAccent);
  drawTextRight(screenName(screen_), config_.width - 8, 5, kMuted);

  switch (screen_) {
  case Screen::Browser:
    renderBrowser();
    break;
  case Screen::Reader:
    renderReader();
    break;
  case Screen::Settings:
    renderSettings();
    break;
  }

  SDL_RenderPresent(renderer_);
}

void App::renderBrowser() {
  drawText(books_path_, 8, 32, kMuted);

  int y = 54;
  for (int i = 0; i < static_cast<int>(books_.size()); ++i) {
    const bool selected = i == selected_book_;
    if (selected) {
      fillRect(6, y - 3, config_.width - 12, 34, kPanelHi);
      strokeRect(6, y - 3, config_.width - 12, 34, kAccent);
    }
    const BookEntry& book = books_[static_cast<size_t>(i)];
    drawText(book.title, 14, y, selected ? kText : kMuted);
    drawText(book.subtitle, 18, y + 15, book.readable ? kMuted : kWarn);
    y += 38;
    if (y > config_.height - 42) {
      break;
    }
  }

  drawText("Cross/Enter open  Circle/Esc back  Start/S settings", 8, config_.height - 18, kMuted);
}

void App::renderReader() {
  strokeRect(8, 34, config_.width - 16, config_.height - 62, kPanelHi);
  drawText(reader_title_, 18, 44, kAccent);
  drawText(reader_status_, 18, 62, kMuted);

  const int line_height = font_ != nullptr ? std::max(12, TTF_FontLineSkip(font_)) : 17;
  const int lines_per_page = linesPerPage();
  int y = 84;
  for (int i = 0; i < lines_per_page; ++i) {
    const int line_index = reader_scroll_ + i;
    if (line_index >= static_cast<int>(reader_lines_.size())) {
      break;
    }
    drawText(reader_lines_[static_cast<size_t>(line_index)], 18, y, kText);
    y += line_height;
  }

  const int total_pages =
      std::max(1, (static_cast<int>(reader_lines_.size()) + lines_per_page - 1) / lines_per_page);
  const int current_page = std::min(total_pages, (reader_scroll_ / lines_per_page) + 1);
  drawText("L/Q prev  R/E next  hold L/R scroll", 12, config_.height - 20, kMuted);
  drawTextRight("page " + std::to_string(current_page) + "/" + std::to_string(total_pages),
                config_.width - 14, config_.height - 20, kMuted);
}

void App::renderSettings() {
  drawText("Settings", 8, 32, kAccent);

  int y = 58;
  for (int i = 0; i < static_cast<int>(settings_.size()); ++i) {
    const bool selected = i == selected_setting_;
    if (selected) {
      fillRect(6, y - 3, config_.width - 12, 22, kPanelHi);
    }
    drawText(settings_[static_cast<size_t>(i)], 14, y, selected ? kText : kMuted);
    y += 24;
  }

  drawText("Circle/Esc or Start/S closes", 8, config_.height - 18, kMuted);
}

void App::drawText(const std::string& text, int x, int y, SDL_Color color) {
  if (font_ == nullptr || text.empty()) {
    return;
  }

  SDL_Surface* surface = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
  if (surface == nullptr) {
    return;
  }

  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
  if (texture == nullptr) {
    SDL_FreeSurface(surface);
    return;
  }

  SDL_Rect dst = {x, y, surface->w, surface->h};
  SDL_FreeSurface(surface);
  SDL_RenderCopy(renderer_, texture, nullptr, &dst);
  SDL_DestroyTexture(texture);
}

void App::drawTextRight(const std::string& text, int right_x, int y, SDL_Color color) {
  if (font_ == nullptr || text.empty()) {
    return;
  }

  int w = 0;
  int h = 0;
  if (TTF_SizeUTF8(font_, text.c_str(), &w, &h) != 0) {
    return;
  }
  drawText(text, right_x - w, y, color);
}

void App::fillRect(int x, int y, int w, int h, SDL_Color color) {
  SDL_Rect rect = {x, y, w, h};
  SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
  SDL_RenderFillRect(renderer_, &rect);
}

void App::strokeRect(int x, int y, int w, int h, SDL_Color color) {
  SDL_Rect rect = {x, y, w, h};
  SDL_SetRenderDrawColor(renderer_, color.r, color.g, color.b, color.a);
  SDL_RenderDrawRect(renderer_, &rect);
}

bool App::loadFont() {
  const char* candidates[] = {
      "ef0:/PSP/GAME/glyph/assets/fonts/AtkinsonHyperlegibleNext-Regular.ttf",
      "ms0:/PSP/GAME/glyph/assets/fonts/AtkinsonHyperlegibleNext-Regular.ttf",
      "ef0:/PSP/GAME/glyph/assets/fonts/AtkinsonHyperlegibleNext[wght].ttf",
      "ms0:/PSP/GAME/glyph/assets/fonts/AtkinsonHyperlegibleNext[wght].ttf",
      "assets/fonts/AtkinsonHyperlegibleNext-Regular.ttf",
      "assets/fonts/AtkinsonHyperlegibleNext[wght].ttf",
      "assets/fonts/AtkinsonHyperlegible-Regular.ttf",
      "assets/fonts/Atkinson-Hyperlegible-Regular-102.ttf",
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
