#include "glyph_app.h"

#include "epub.h"
#if defined(GLYPH_PLATFORM_HOST)
#include "png_writer.h"
#endif
#include "progress.h"
#include "settings_store.h"

#include <SDL_image.h>

#include <algorithm>
#include <string>

#if defined(GLYPH_PLATFORM_PSP)
#include <pspdisplay.h>
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

constexpr int kTopBarHeight = 24;
constexpr int kReaderTextX = 10;
constexpr int kReaderTextY = kTopBarHeight + 6;
constexpr int kReaderTextBottomPadding = 4;
constexpr int kBrowserListWidth = 282;
constexpr int kBrowserRowHeight = 42;
constexpr int kBrowserFooterHeight = 50;
constexpr size_t kTextTextureCacheLimit = 160;

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

bool sameColor(SDL_Color a, SDL_Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

} // namespace

void App::clearCoverTexture() {
  if (cover_texture_ != nullptr) {
    SDL_DestroyTexture(cover_texture_);
    cover_texture_ = nullptr;
  }
  cover_texture_width_ = 0;
  cover_texture_height_ = 0;
}

void App::updateSelectedCover() {
  if (cover_texture_book_ == selected_book_) {
    return;
  }

  clearCoverTexture();
  cover_texture_book_ = selected_book_;
  if (selected_book_ < 0 || selected_book_ >= static_cast<int>(books_.size())) {
    return;
  }

  const BookEntry& book = books_[static_cast<size_t>(selected_book_)];
  if (book.path.empty() || book.cover_image_path.empty()) {
    return;
  }

  EpubDocument document;
  if (!document.open(book.path)) {
    return;
  }

  ZipReadResult resource = document.readResource(book.cover_image_path);
  if (!resource.ok || resource.bytes.empty() || resource.bytes.size() > 4u * 1024u * 1024u) {
    return;
  }

  SDL_RWops* rw =
      SDL_RWFromConstMem(resource.bytes.data(), static_cast<int>(resource.bytes.size()));
  if (rw == nullptr) {
    return;
  }

  SDL_Surface* surface = IMG_Load_RW(rw, 1);
  if (surface == nullptr) {
    return;
  }

  cover_texture_ = SDL_CreateTextureFromSurface(renderer_, surface);
  if (cover_texture_ != nullptr) {
    cover_texture_width_ = surface->w;
    cover_texture_height_ = surface->h;
    SDL_SetTextureBlendMode(cover_texture_, SDL_BLENDMODE_BLEND);
  }
  SDL_FreeSurface(surface);
}

void App::drawCoverPreview(int x, int y, int w, int h) {
  fillRect(x, y, w, h, kPanel);
  strokeRect(x, y, w, h, kPanelHi);

  if (cover_texture_ == nullptr || cover_texture_width_ <= 0 || cover_texture_height_ <= 0) {
    drawText("No cover", x + 18, y + h / 2 - 8, kMuted);
    return;
  }

  const int inset = 8;
  const int max_w = std::max(1, w - inset * 2);
  const int max_h = std::max(1, h - inset * 2);
  const double scale = std::min(static_cast<double>(max_w) / cover_texture_width_,
                                static_cast<double>(max_h) / cover_texture_height_);
  const int dst_w = std::max(1, static_cast<int>(cover_texture_width_ * scale));
  const int dst_h = std::max(1, static_cast<int>(cover_texture_height_ * scale));
  SDL_Rect dst = {x + (w - dst_w) / 2, y + (h - dst_h) / 2, dst_w, dst_h};
  SDL_RenderCopy(renderer_, cover_texture_, nullptr, &dst);
}

int App::readerLineHeight() const {
  return font_ != nullptr ? std::max(12, TTF_FontLineSkip(font_)) : 17;
}

int App::readerTextWidth() const {
  return std::max(1, config_.width - (kReaderTextX * 2));
}

int App::readerTextHeight() const {
  const int text_bottom = config_.height - kReaderTextBottomPadding;
  return std::max(readerLineHeight(), text_bottom - kReaderTextY);
}

int App::linesPerPage() const {
  return std::max(1, readerTextHeight() / readerLineHeight());
}

int App::readerScrollStep() const {
  const int page_lines = linesPerPage();
  switch (clampScrollStepMode(scroll_step_mode_)) {
  case 0:
    return 1;
  case 1:
    return std::max(2, page_lines / 3);
  case 2:
    return std::max(2, page_lines / 2);
  case 3:
    return page_lines;
  }
  return std::max(2, page_lines / 2);
}

int App::maxReaderScroll() const {
  if (reader_lines_.empty()) {
    return 0;
  }
  return std::max(0, static_cast<int>(reader_lines_.size()) - linesPerPage());
}

int App::visibleBookCount() const {
  return std::max(1, (config_.height - kBrowserFooterHeight - 28) / kBrowserRowHeight + 1);
}

int App::firstVisibleBook() const {
  const int count = static_cast<int>(books_.size());
  const int visible_count = visibleBookCount();
  if (count <= visible_count) {
    return 0;
  }

  const int centered = selected_book_ - (visible_count / 2);
  return std::min(count - visible_count, std::max(0, centered));
}

void App::prepareRenderResources() {
  if (screen_ != Screen::Reader) {
    prepareText(screenName(screen_), kMuted);
  }

  switch (screen_) {
  case Screen::Browser: {
    updateSelectedCover();
    prepareText(books_path_, kMuted);
    prepareText("No cover", kMuted);
    prepareText("Cross open   Start settings", kMuted);
    prepareText("D-pad choose   Circle quit", kMuted);
    const int first = firstVisibleBook();
    const int last = std::min(static_cast<int>(books_.size()), first + visibleBookCount());
    for (int i = first; i < last; ++i) {
      prepareText(books_[static_cast<size_t>(i)].title, i == selected_book_ ? kText : kMuted);
      prepareText(books_[static_cast<size_t>(i)].subtitle,
                  books_[static_cast<size_t>(i)].readable ? kMuted : kWarn);
      prepareText(books_[static_cast<size_t>(i)].progress_label, kMuted);
    }
    break;
  }
  case Screen::Reader: {
    const int line_count = linesPerPage();
    prepareText(reader_title_, kAccent);
    prepareText(currentChapterTitle(), kMuted);
    prepareText(readerProgressText(), kMuted);
    for (int i = 0; i < line_count; ++i) {
      const int line_index = reader_scroll_ + i;
      if (line_index >= static_cast<int>(reader_lines_.size())) {
        break;
      }
      prepareText(reader_lines_[static_cast<size_t>(line_index)], kText);
    }
    if (jump_overlay_open_) {
      prepareText("Jump", kAccent);
      prepareText(jump_mode_ == JumpMode::Chapters ? "Chapters" : "Page", kText);
      prepareText("Left/Right mode  Cross jump  Circle close", kMuted);
      prepareText("Page " + std::to_string(pageEntryValue()) + "/" +
                      std::to_string(totalPageCount()),
                  kText);
      prepareText("Type digits on host; Up/Down adjusts", kMuted);
      const int chapter_count = static_cast<int>(reader_chapters_.size());
      const int visible_count = 7;
      const int first = std::max(
          0, std::min(selected_chapter_ - visible_count / 2, chapter_count - visible_count));
      const int last = std::min(chapter_count, first + visible_count);
      for (int i = first; i < last; ++i) {
        prepareText(reader_chapters_[static_cast<size_t>(i)].title,
                    i == selected_chapter_ ? kText : kMuted);
      }
    }
    break;
  }
  case Screen::Settings:
    prepareText("Settings", kAccent);
    prepareText("Circle/Esc or Start/S closes", kMuted);
    for (int i = 0; i < static_cast<int>(settings_.size()); ++i) {
      prepareText(settings_[static_cast<size_t>(i)], i == selected_setting_ ? kText : kMuted);
    }
    break;
  }
}

void App::prepareText(const std::string& text, SDL_Color color) {
  int width = 0;
  int height = 0;
  textTextureFor(text, color, width, height);
}

SDL_Texture* App::textTextureFor(const std::string& text, SDL_Color color, int& width,
                                 int& height) {
  width = 0;
  height = 0;
  if (font_ == nullptr || text.empty()) {
    return nullptr;
  }

  ++text_cache_tick_;
  for (TextTextureEntry& entry : text_cache_) {
    if (entry.text == text && sameColor(entry.color, color)) {
      entry.last_used = text_cache_tick_;
      width = entry.width;
      height = entry.height;
      return entry.texture;
    }
  }

  SDL_Surface* surface = TTF_RenderUTF8_Blended(font_, text.c_str(), color);
  if (surface == nullptr) {
    return nullptr;
  }

  SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer_, surface);
  if (texture == nullptr) {
    SDL_FreeSurface(surface);
    return nullptr;
  }
  SDL_SetTextureBlendMode(texture, SDL_BLENDMODE_BLEND);

  while (text_cache_.size() >= kTextTextureCacheLimit) {
    auto oldest = text_cache_.begin();
    for (auto it = text_cache_.begin(); it != text_cache_.end(); ++it) {
      if (it->last_used < oldest->last_used) {
        oldest = it;
      }
    }
    SDL_DestroyTexture(oldest->texture);
    text_cache_.erase(oldest);
  }

  TextTextureEntry entry;
  entry.text = text;
  entry.color = color;
  entry.texture = texture;
  entry.width = surface->w;
  entry.height = surface->h;
  entry.last_used = text_cache_tick_;
  width = entry.width;
  height = entry.height;
  SDL_FreeSurface(surface);
  text_cache_.push_back(entry);
  return text_cache_.back().texture;
}

void App::clearTextCache() {
  for (TextTextureEntry& entry : text_cache_) {
    if (entry.texture != nullptr) {
      SDL_DestroyTexture(entry.texture);
      entry.texture = nullptr;
    }
  }
  text_cache_.clear();
}

void App::render() {
  renderFrame(true);
}

void App::renderFrame(bool present) {
  prepareRenderResources();

  SDL_RenderSetClipRect(renderer_, nullptr);
  SDL_RenderSetViewport(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, kBg.r, kBg.g, kBg.b, kBg.a);
  SDL_RenderClear(renderer_);

  if (screen_ != Screen::Reader) {
    fillRect(0, 0, config_.width, kTopBarHeight, kPanel);
    drawBrandMark(8, 4, 2, 1, kAccent);
    drawTextRight(screenName(screen_), config_.width - 8, 5, kMuted);
  }

  switch (screen_) {
  case Screen::Browser:
    renderBrowser();
    break;
  case Screen::Reader:
    renderReader();
    if (jump_overlay_open_) {
      renderJumpOverlay();
    }
    break;
  case Screen::Settings:
    renderSettings();
    break;
  }

  if (present) {
#if defined(GLYPH_PLATFORM_PSP)
    sceDisplayWaitVblankStart();
#endif
    SDL_RenderPresent(renderer_);
  }
}

bool App::renderSnapshot(SnapshotKind kind, const std::string& output_path) {
#if defined(GLYPH_PLATFORM_HOST)
  closeJumpOverlay();
  switch (kind) {
  case SnapshotKind::Browser:
    screen_ = Screen::Browser;
    break;
  case SnapshotKind::Settings:
    settings_return_screen_ = Screen::Browser;
    selected_setting_ = 0;
    screen_ = Screen::Settings;
    break;
  case SnapshotKind::Reader:
    screen_ = Screen::Reader;
    break;
  case SnapshotKind::ReaderSelect:
    screen_ = Screen::Reader;
    openJumpOverlay();
    break;
  }

  renderFrame(false);
  std::vector<unsigned char> pixels(static_cast<size_t>(config_.width) * config_.height * 3);
  if (SDL_RenderReadPixels(renderer_, nullptr, SDL_PIXELFORMAT_RGB24, pixels.data(),
                           config_.width * 3) != 0) {
    return false;
  }
  return writeRgbPng(output_path, config_.width, config_.height, pixels);
#else
  (void)kind;
  (void)output_path;
  return false;
#endif
}

void App::renderBrowser() {
  drawText(books_path_, 8, 32, kMuted);

  const int cover_x = kBrowserListWidth + 18;
  const int cover_y = 42;
  const int cover_w = config_.width - cover_x - 14;
  const int cover_h = config_.height - cover_y - 42;
  drawCoverPreview(cover_x, cover_y, cover_w, cover_h);

  SDL_Rect list_clip = {0, 28, kBrowserListWidth + 2, config_.height - kBrowserFooterHeight};
  SDL_RenderSetClipRect(renderer_, &list_clip);

  const int first = firstVisibleBook();
  const int last = std::min(static_cast<int>(books_.size()), first + visibleBookCount());
  int y = 54;
  for (int i = first; i < last; ++i) {
    const bool selected = i == selected_book_;
    if (selected) {
      fillRect(6, y - 3, kBrowserListWidth - 12, 38, kPanelHi);
      strokeRect(6, y - 3, kBrowserListWidth - 12, 38, kAccent);
    }
    const BookEntry& book = books_[static_cast<size_t>(i)];
    drawText(book.title, 14, y, selected ? kText : kMuted);
    drawTextClipped(book.subtitle, 18, y + 15, book.has_progress ? 172 : 250,
                    book.readable ? kMuted : kWarn);
    if (book.has_progress) {
      drawTextRight(book.progress_label, kBrowserListWidth - 14, y + 15, kMuted);
      const int bar_x = 18;
      const int bar_y = y + 31;
      const int bar_w = kBrowserListWidth - 36;
      fillRect(bar_x, bar_y, bar_w, 3, kPanel);
      fillRect(bar_x, bar_y, std::max(1, (bar_w * book.progress_percent) / 100), 3, kAccent);
    }
    y += kBrowserRowHeight;
  }

  SDL_RenderSetClipRect(renderer_, nullptr);
  drawText("Cross open   Start settings", 8, config_.height - 34, kMuted);
  drawText("D-pad choose   Circle quit", 8, config_.height - 18, kMuted);
}

void App::renderReader() {
  fillRect(0, 0, config_.width, kTopBarHeight, kPanel);
  fillRect(0, kTopBarHeight, config_.width, 1, kPanelHi);
  drawBrandMark(8, 4, 2, 1, kAccent);
  drawTextClipped(reader_title_, 25, 5, 157, kAccent);
  drawTextClipped(currentChapterTitle(), 188, 5, 178, kMuted);
  drawTextRight(readerProgressText(), config_.width - 8, 5, kMuted);

  const int line_height = readerLineHeight();
  const int lines_per_page = linesPerPage();
  SDL_Rect text_clip = {kReaderTextX, kReaderTextY, readerTextWidth(), readerTextHeight()};
  SDL_RenderSetClipRect(renderer_, &text_clip);

  int y = kReaderTextY;
  for (int i = 0; i < lines_per_page; ++i) {
    const int line_index = reader_scroll_ + i;
    if (line_index >= static_cast<int>(reader_lines_.size())) {
      break;
    }
    drawText(reader_lines_[static_cast<size_t>(line_index)], kReaderTextX, y, kText);
    y += line_height;
  }
  SDL_RenderSetClipRect(renderer_, nullptr);
}

void App::renderJumpOverlay() {
  const int x = 44;
  const int y = 38;
  const int w = config_.width - 88;
  const int h = config_.height - 76;
  fillRect(x, y, w, h, {16, 19, 22, 246});
  strokeRect(x, y, w, h, kAccent);

  drawText("Jump", x + 10, y + 8, kAccent);
  drawTextRight(jump_mode_ == JumpMode::Chapters ? "Chapters" : "Page", x + w - 10, y + 8, kText);
  fillRect(x + 1, y + 30, w - 2, 1, kPanelHi);

  if (jump_mode_ == JumpMode::Chapters) {
    const int chapter_count = static_cast<int>(reader_chapters_.size());
    if (chapter_count <= 0) {
      drawText("No chapter list available", x + 12, y + 48, kMuted);
    } else {
      const int visible_count = 7;
      const int first = std::max(
          0, std::min(selected_chapter_ - visible_count / 2, chapter_count - visible_count));
      const int last = std::min(chapter_count, first + visible_count);
      int row_y = y + 42;
      for (int i = first; i < last; ++i) {
        const bool selected = i == selected_chapter_;
        if (selected) {
          fillRect(x + 8, row_y - 3, w - 16, 20, kPanelHi);
        }
        drawTextClipped(std::to_string(i + 1) + ". " +
                            reader_chapters_[static_cast<size_t>(i)].title,
                        x + 14, row_y, w - 28, selected ? kText : kMuted);
        row_y += 22;
      }
    }
  } else {
    const std::string page_value =
        page_entry_.empty() ? std::to_string(currentPageNumber()) : page_entry_;
    drawText("Page", x + 16, y + 54, kMuted);
    fillRect(x + 70, y + 46, 96, 28, kPanel);
    strokeRect(x + 70, y + 46, 96, 28, kPanelHi);
    drawText(page_value, x + 82, y + 53, kText);
    drawText("/ " + std::to_string(totalPageCount()), x + 176, y + 54, kMuted);
    drawText("Host digits type page number", x + 16, y + 92, kMuted);
    drawText("Up/Down +/-1  L/R +/-10", x + 16, y + 114, kMuted);
  }

  fillRect(x + 1, y + h - 27, w - 2, 1, kPanelHi);
  drawText("Left/Right mode  Cross jump  Circle close", x + 10, y + h - 19, kMuted);
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

  drawText("Left/Right adjust  Circle/Esc closes", 8, config_.height - 18, kMuted);
}

void App::drawText(const std::string& text, int x, int y, SDL_Color color) {
  int width = 0;
  int height = 0;
  SDL_Texture* texture = textTextureFor(text, color, width, height);
  if (texture == nullptr) {
    return;
  }

  SDL_Rect dst = {x, y, width, height};
  SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void App::drawTextRight(const std::string& text, int right_x, int y, SDL_Color color) {
  int width = 0;
  int height = 0;
  SDL_Texture* texture = textTextureFor(text, color, width, height);
  if (texture == nullptr) {
    return;
  }

  SDL_Rect dst = {right_x - width, y, width, height};
  SDL_RenderCopy(renderer_, texture, nullptr, &dst);
}

void App::drawTextClipped(const std::string& text, int x, int y, int max_width, SDL_Color color) {
  if (max_width <= 0) {
    return;
  }

  SDL_Rect previous_clip = {};
  const SDL_bool had_clip = SDL_RenderIsClipEnabled(renderer_);
  if (had_clip == SDL_TRUE) {
    SDL_RenderGetClipRect(renderer_, &previous_clip);
  }

  SDL_Rect clip = {x, y, max_width, config_.height - y};
  if (had_clip == SDL_TRUE) {
    const int left = std::max(clip.x, previous_clip.x);
    const int top = std::max(clip.y, previous_clip.y);
    const int right = std::min(clip.x + clip.w, previous_clip.x + previous_clip.w);
    const int bottom = std::min(clip.y + clip.h, previous_clip.y + previous_clip.h);
    clip = {left, top, std::max(0, right - left), std::max(0, bottom - top)};
  }

  SDL_RenderSetClipRect(renderer_, &clip);
  drawText(text, x, y, color);
  if (had_clip == SDL_TRUE) {
    SDL_RenderSetClipRect(renderer_, &previous_clip);
  } else {
    SDL_RenderSetClipRect(renderer_, nullptr);
  }
}

void App::drawBrandMark(int x, int y, int cell, int gap, SDL_Color color) {
  const int step = cell + gap;
  const int pixels[][2] = {
      {1, 0}, {2, 0}, {0, 1}, {3, 1}, {0, 2}, {3, 2}, {0, 3},
      {1, 3}, {2, 3}, {3, 3}, {3, 4}, {0, 5}, {1, 5}, {2, 5},
  };

  for (const auto& pixel : pixels) {
    fillRect(x + pixel[0] * step, y + pixel[1] * step, cell, cell, color);
  }
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

} // namespace glyph
