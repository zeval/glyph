#include "glyph_app.h"

#include "epub.h"
#include "library.h"
#include "progress.h"
#include "text_layout.h"

#include <SDL_image.h>

#include <algorithm>
#include <cctype>
#include <cstdio>

#if defined(GLYPH_PLATFORM_PSP)
#include <pspctrl.h>
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
constexpr size_t kTextTextureCacheLimit = 160;
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

bool sameColor(SDL_Color a, SDL_Color b) {
  return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

std::string trimAndCollapse(const std::string& value) {
  std::string out;
  bool pending_space = false;
  for (const char ch : value) {
    if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
      pending_space = !out.empty();
      continue;
    }
    if (pending_space) {
      out.push_back(' ');
      pending_space = false;
    }
    out.push_back(ch);
  }
  return out;
}

std::string shortenText(const std::string& value, size_t max_chars) {
  if (value.size() <= max_chars) {
    return value;
  }
  if (max_chars <= 3) {
    return value.substr(0, max_chars);
  }
  return value.substr(0, max_chars - 3) + "...";
}

std::string firstReadableLine(const std::string& text) {
  std::string line;
  for (size_t i = 0; i <= text.size(); ++i) {
    const char ch = i < text.size() ? text[i] : '\n';
    if (ch == '\r' || ch == '\n') {
      const std::string trimmed = trimAndCollapse(line);
      if (!trimmed.empty()) {
        return shortenText(trimmed, 42);
      }
      line.clear();
      continue;
    }
    line.push_back(ch);
  }
  return "";
}

std::string chapterTitleFor(const EpubSpineItem& spine, const std::string& text,
                            int chapter_number) {
  const std::string line = firstReadableLine(text);
  if (!line.empty()) {
    return line;
  }
  const std::string name = displayNameForPath(spine.href);
  if (!name.empty()) {
    return shortenText(name, 42);
  }
  return "Chapter " + std::to_string(chapter_number);
}

} // namespace

App::App(AppConfig config) : config_(config) {
  settings_ = {
      "Theme: dark",
      "Font: Atkinson",
      "Bumpers: scroll + edge page",
      "D-pad: page/line navigation",
      "Circle: save progress and return",
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
  input.shoulder_l_click = input.shoulder_l_click || ((pressed & PSP_CTRL_LTRIGGER) != 0);
  input.shoulder_r_click = input.shoulder_r_click || ((pressed & PSP_CTRL_RTRIGGER) != 0);
  previous_platform_buttons_ = buttons;
#else
  (void)input;
#endif
}

void App::applyInput(const InputState& input) {
  if (input.quit) {
    if (screen_ == Screen::Reader) {
      saveCurrentProgress();
    }
    running_ = false;
  }

  if (input.menu) {
    screen_ = Screen::Settings;
  }
  if (input.toc) {
    if (screen_ == Screen::Reader) {
      saveCurrentProgress();
    }
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
    if (input.right) {
      pageReaderForward();
    }
    if (input.left) {
      pageReaderBackward();
    }
    if (input.shoulder_r_click) {
      stepOrPageReaderForward();
    }
    if (input.shoulder_l_click) {
      stepOrPageReaderBackward();
    }
    if (input.down) {
      reader_scroll_ = std::min(maxReaderScroll(), reader_scroll_ + 1);
    }
    if (input.up) {
      reader_scroll_ = std::max(0, reader_scroll_ - 1);
    }
    if (input.back) {
      saveCurrentProgress();
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

void App::saveCurrentProgress() {
  if (reader_book_path_.empty() || reader_lines_.empty()) {
    return;
  }

  BookProgress progress;
  progress.file_path = reader_book_path_;
  progress.reader_scroll = std::min(maxReaderScroll(), std::max(0, reader_scroll_));
  progress.total_lines = static_cast<int>(reader_lines_.size());
  progress.lines_per_page = linesPerPage();
  if (saveBookProgress(defaultProgressPath(), progress)) {
    updateBookProgress(progress);
  }
}

void App::updateBookProgress(const BookProgress& progress) {
  for (BookEntry& book : books_) {
    if (book.path == progress.file_path) {
      book.has_progress = true;
      book.progress_label = progressSummary(progress);
      book.progress_percent = progressPercent(progress);
      return;
    }
  }
}

void App::pageReaderForward() {
  reader_scroll_ = std::min(maxReaderScroll(), reader_scroll_ + linesPerPage());
}

void App::pageReaderBackward() {
  reader_scroll_ = std::max(0, reader_scroll_ - linesPerPage());
}

void App::stepOrPageReaderForward() {
  const int page_lines = linesPerPage();
  const int max_scroll = maxReaderScroll();
  if (reader_scroll_ >= max_scroll) {
    return;
  }

  const int page_offset = reader_scroll_ % page_lines;
  if (page_offset + 1 < page_lines) {
    reader_scroll_ = std::min(max_scroll, reader_scroll_ + 1);
    return;
  }

  reader_scroll_ = std::min(max_scroll, reader_scroll_ + page_lines - page_offset);
}

void App::stepOrPageReaderBackward() {
  if (reader_scroll_ <= 0) {
    return;
  }

  const int page_lines = linesPerPage();
  const int page_offset = reader_scroll_ % page_lines;
  if (page_offset > 0) {
    --reader_scroll_;
    return;
  }

  pageReaderBackward();
}

void App::refreshLibrary() {
  books_.clear();

  const LibraryScanResult library = discoverLibrary();
  const std::vector<BookProgress> progress_entries = loadProgressFile(defaultProgressPath());
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
        book.cover_image_path = document.book().cover_image_path;
      } else {
        book.subtitle = document.error();
      }
      const BookProgress progress = findBookProgress(progress_entries, book.path);
      if (!progress.file_path.empty() && progressTotalPages(progress) > 0) {
        book.has_progress = true;
        book.progress_label = progressSummary(progress);
        book.progress_percent = progressPercent(progress);
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

  reader_title_ = document.book().metadata.title;
  reader_status_ = authorsLine(document.book().metadata.authors);
  reader_book_path_ = path;
  reader_text_.clear();
  reader_lines_.clear();
  reader_chapters_.clear();
  reader_scroll_ = 0;

  EpubTextResult last_result;
  int chapter_number = 1;
  for (size_t i = 0; i < document.book().spine.size(); ++i) {
    EpubTextResult text = document.readSpineText(i);
    if (!text.ok) {
      last_result = text;
      continue;
    }

    if (!reader_lines_.empty()) {
      reader_lines_.emplace_back();
      reader_text_ += "\n\n";
    }

    ReaderChapter chapter;
    chapter.title =
        chapterTitleFor(document.book().spine[i], text.text, static_cast<int>(chapter_number));
    chapter.start_line = static_cast<int>(reader_lines_.size());
    chapter.spine_index = i;

    std::vector<std::string> chapter_lines = wrapReaderText(text.text);
    reader_lines_.insert(reader_lines_.end(), chapter_lines.begin(), chapter_lines.end());
    reader_text_ += text.text;

    chapter.end_line = static_cast<int>(reader_lines_.size());
    reader_chapters_.push_back(chapter);
    ++chapter_number;
  }

  if (reader_lines_.empty()) {
    const std::string error =
        last_result.error.empty() ? "EPUB spine did not contain readable text" : last_result.error;
    setReaderText(document.book().metadata.title, "Could not read EPUB text", error);
    screen_ = Screen::Reader;
    return;
  }

  const BookProgress progress =
      findBookProgress(loadProgressFile(defaultProgressPath()), reader_book_path_);
  if (progress.file_path == reader_book_path_ && progressTotalPages(progress) > 0) {
    reader_scroll_ = std::min(maxReaderScroll(), std::max(0, progress.reader_scroll));
  }
  screen_ = Screen::Reader;
}

void App::setReaderText(const std::string& title, const std::string& status,
                        const std::string& text) {
  reader_title_ = title;
  reader_status_ = status;
  reader_book_path_.clear();
  reader_text_ = text;
  reader_chapters_.clear();
  reader_lines_ = wrapReaderText(text);
  if (reader_lines_.empty()) {
    reader_lines_.push_back("No readable text.");
  }
  reader_scroll_ = 0;
}

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

std::vector<std::string> App::wrapReaderText(const std::string& text) const {
  std::vector<std::string> lines;

  TextLayoutConfig layout_config;
  layout_config.viewport_width = readerTextWidth();
  layout_config.viewport_height = 100000;
  layout_config.margin_left = 0;
  layout_config.margin_top = 0;
  layout_config.margin_right = 0;
  layout_config.margin_bottom = 0;
  layout_config.average_char_width = 6;

  if (font_ != nullptr) {
    int sample_width = 0;
    int sample_height = 0;
    const char sample[] = "abcdefghijklmnopqrstuvwxyzabcdefghijklmnopqrstuvwxyz     ";
    if (TTF_SizeUTF8(font_, sample, &sample_width, &sample_height) == 0) {
      const int sample_chars = static_cast<int>(sizeof(sample) - 1);
      layout_config.average_char_width =
          std::max(5, std::min(7, sample_width / std::max(1, sample_chars)));
    }
  }
  layout_config.line_height = readerLineHeight();
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

int App::maxReaderScroll() const {
  if (reader_lines_.empty()) {
    return 0;
  }
  return std::max(0, static_cast<int>(reader_lines_.size()) - linesPerPage());
}

std::string App::currentChapterTitle() const {
  if (reader_chapters_.empty()) {
    return reader_status_;
  }

  const int line = std::min(static_cast<int>(reader_lines_.size()), std::max(0, reader_scroll_));
  const ReaderChapter* best = &reader_chapters_.front();
  for (const ReaderChapter& chapter : reader_chapters_) {
    if (line >= chapter.start_line && line < chapter.end_line) {
      return chapter.title;
    }
    if (line >= chapter.start_line) {
      best = &chapter;
    }
  }
  return best->title;
}

std::string App::readerProgressText() const {
  BookProgress progress;
  progress.file_path = reader_book_path_;
  progress.reader_scroll = reader_scroll_;
  progress.total_lines = static_cast<int>(reader_lines_.size());
  progress.lines_per_page = linesPerPage();
  return progressSummary(progress);
}

int App::visibleBookCount() const {
  return std::max(1, (config_.height - 54) / kBrowserRowHeight + 1);
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
    prepareText("glyph", kAccent);
    prepareText(screenName(screen_), kMuted);
  }

  switch (screen_) {
  case Screen::Browser: {
    updateSelectedCover();
    prepareText(books_path_, kMuted);
    prepareText("No cover", kMuted);
    prepareText("Cross/Enter open  Circle/Esc back  Start/S settings", kMuted);
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
  prepareRenderResources();

  SDL_RenderSetClipRect(renderer_, nullptr);
  SDL_RenderSetViewport(renderer_, nullptr);
  SDL_SetRenderDrawColor(renderer_, kBg.r, kBg.g, kBg.b, kBg.a);
  SDL_RenderClear(renderer_);

  if (screen_ != Screen::Reader) {
    fillRect(0, 0, config_.width, kTopBarHeight, kPanel);
    drawText("glyph", 8, 5, kAccent);
    drawTextRight(screenName(screen_), config_.width - 8, 5, kMuted);
  }

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

#if defined(GLYPH_PLATFORM_PSP)
  sceDisplayWaitVblankStart();
#endif
  SDL_RenderPresent(renderer_);
}

void App::renderBrowser() {
  drawText(books_path_, 8, 32, kMuted);

  const int cover_x = kBrowserListWidth + 18;
  const int cover_y = 42;
  const int cover_w = config_.width - cover_x - 14;
  const int cover_h = config_.height - cover_y - 42;
  drawCoverPreview(cover_x, cover_y, cover_w, cover_h);

  SDL_Rect list_clip = {0, 28, kBrowserListWidth + 2, config_.height - 54};
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
  drawText("Cross/Enter open  Circle/Esc back  Start/S settings", 8, config_.height - 18, kMuted);
}

void App::renderReader() {
  fillRect(0, 0, config_.width, kTopBarHeight, kPanel);
  fillRect(0, kTopBarHeight, config_.width, 1, kPanelHi);
  drawTextClipped(reader_title_, 8, 5, 174, kAccent);
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
