#ifndef GLYPH_APP_H
#define GLYPH_APP_H

#include <SDL.h>
#include <SDL_ttf.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace glyph {

enum class Screen {
  Browser,
  Reader,
  Settings,
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
    bool readable = false;
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
    bool quit = false;
  };

  void handleEvent(const SDL_Event& event, InputState& input);
  void pollPlatformInput(InputState& input);
  void applyInput(const InputState& input);
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
  int readerLineHeight() const;
  int readerTextWidth() const;
  int readerTextHeight() const;
  int linesPerPage() const;
  int maxReaderScroll() const;
  void render();
  void renderBrowser();
  void renderReader();
  void renderSettings();
  void drawText(const std::string& text, int x, int y, SDL_Color color);
  void drawTextRight(const std::string& text, int right_x, int y, SDL_Color color);
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
  int selected_book_ = 0;
  int selected_setting_ = 0;
  int reader_scroll_ = 0;
  uint32_t previous_platform_buttons_ = 0;

  std::vector<BookEntry> books_;
  std::string books_path_;
  std::string reader_title_;
  std::string reader_status_;
  std::vector<std::string> reader_lines_;
  std::array<std::string, 4> settings_;
};

} // namespace glyph

#endif // GLYPH_APP_H
