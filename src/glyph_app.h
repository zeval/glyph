#ifndef GLYPH_APP_H
#define GLYPH_APP_H

#include <SDL.h>
#include <SDL_ttf.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glyph {

struct BookProgress;
struct AppSettings;

enum class Screen {
  Browser,
  Reader,
  Settings,
};

enum class JumpMode {
  Chapters,
  Page,
};

struct AppConfig {
  int width = 480;
  int height = 272;
  std::string initial_book_path;
};

class App {
public:
  explicit App(AppConfig config);
  ~App();

  App(const App&) = delete;
  App& operator=(const App&) = delete;

  bool init();
  int run();
  void shutdown();

private:
  struct BookEntry {
    std::string title;
    std::string subtitle;
    std::string path;
    std::string cover_image_path;
    std::string progress_label;
    int progress_percent = 0;
    bool readable = false;
    bool has_progress = false;
  };

  struct InputState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool accept = false;
    bool back = false;
    bool menu = false;
    bool toc = false;
    bool bookmark = false;
    bool status = false;
    bool shoulder_l_click = false;
    bool shoulder_r_click = false;
    bool delete_digit = false;
    int digit = -1;
    bool quit = false;
  };

  struct TextTextureEntry {
    std::string text;
    SDL_Color color = {0, 0, 0, 0};
    SDL_Texture* texture = nullptr;
    int width = 0;
    int height = 0;
    uint32_t last_used = 0;
  };

  struct ReaderChapter {
    std::string title;
    int start_line = 0;
    int end_line = 0;
    size_t spine_index = 0;
  };

  void handleEvent(const SDL_Event& event, InputState& input);
  void pollPlatformInput(InputState& input);
  void applyInput(const InputState& input);
  void saveCurrentProgress();
  void updateBookProgress(const BookProgress& progress);
  void openSettings();
  void adjustSelectedSetting(int delta);
  void updateSettingsLabels();
  void openJumpOverlay();
  void closeJumpOverlay();
  void applyJumpOverlayInput(const InputState& input);
  void syncSelectedChapterToScroll();
  int currentPageNumber() const;
  int totalPageCount() const;
  int pageEntryValue() const;
  void jumpToPage(int page);
  void jumpToChapter(int chapter_index);
  void pageReaderForward();
  void pageReaderBackward();
  void stepOrPageReaderForward();
  void stepOrPageReaderBackward();
  void refreshLibrary();
  void openSelectedBook();
  void openBookPath(const std::string& path);
  void setReaderText(const std::string& title, const std::string& status, const std::string& text);
  void clearCoverTexture();
  void updateSelectedCover();
  void drawCoverPreview(int x, int y, int w, int h);
  std::vector<std::string> wrapReaderText(const std::string& text) const;
  std::string currentChapterTitle() const;
  std::string readerProgressText() const;
  int firstVisibleBook() const;
  int visibleBookCount() const;
  int readerLineHeight() const;
  int readerTextWidth() const;
  int readerTextHeight() const;
  int linesPerPage() const;
  int readerScrollStep() const;
  int maxReaderScroll() const;
  void prepareRenderResources();
  void prepareText(const std::string& text, SDL_Color color);
  SDL_Texture* textTextureFor(const std::string& text, SDL_Color color, int& width, int& height);
  void clearTextCache();
  void render();
  void renderBrowser();
  void renderReader();
  void renderSettings();
  void renderJumpOverlay();
  void drawText(const std::string& text, int x, int y, SDL_Color color);
  void drawTextRight(const std::string& text, int right_x, int y, SDL_Color color);
  void drawTextClipped(const std::string& text, int x, int y, int max_width, SDL_Color color);
  void fillRect(int x, int y, int w, int h, SDL_Color color);
  void strokeRect(int x, int y, int w, int h, SDL_Color color);
  bool loadFont();

  AppConfig config_;
  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  TTF_Font* font_ = nullptr;
  SDL_Texture* cover_texture_ = nullptr;
  int cover_texture_book_ = -1;
  int cover_texture_width_ = 0;
  int cover_texture_height_ = 0;
  bool running_ = false;
  bool needs_render_ = true;
  Screen screen_ = Screen::Browser;
  Screen settings_return_screen_ = Screen::Browser;
  bool jump_overlay_open_ = false;
  JumpMode jump_mode_ = JumpMode::Chapters;
  int selected_book_ = 0;
  int selected_setting_ = 0;
  int selected_chapter_ = 0;
  int reader_scroll_ = 0;
  int scroll_step_mode_ = 2;
  uint32_t previous_platform_buttons_ = 0;
  std::string page_entry_;

  std::vector<BookEntry> books_;
  std::string books_path_;
  std::string reader_title_;
  std::string reader_status_;
  std::string reader_book_path_;
  std::string reader_text_;
  std::vector<std::string> reader_lines_;
  std::vector<ReaderChapter> reader_chapters_;
  std::vector<TextTextureEntry> text_cache_;
  uint32_t text_cache_tick_ = 0;
  std::array<std::string, 8> settings_;
};

} // namespace glyph

#endif // GLYPH_APP_H
