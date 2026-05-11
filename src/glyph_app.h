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
    bool shoulder_l_down = false;
    bool shoulder_r_down = false;
    bool quit = false;
  };

  void handleEvent(const SDL_Event& event, InputState& input);
  void pollPlatformInput(InputState& input);
  void applyInput(const InputState& input);
  void updateShoulderHold(bool left_down, bool right_down, uint32_t now_ms);
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
  bool running_ = false;
  Screen screen_ = Screen::Browser;
  int selected_book_ = 0;
  int selected_setting_ = 0;
  int reader_page_ = 1;
  int reader_scroll_ = 0;
  uint32_t left_hold_started_ms_ = 0;
  uint32_t right_hold_started_ms_ = 0;
  uint32_t last_scroll_step_ms_ = 0;
  bool previous_platform_l_ = false;
  bool previous_platform_r_ = false;

  std::vector<std::string> books_;
  std::array<std::string, 4> settings_;
};

} // namespace glyph

#endif // GLYPH_APP_H
