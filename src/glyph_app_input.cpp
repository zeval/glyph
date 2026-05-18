#include "glyph_app.h"

#include "library.h"
#include "progress.h"
#include "settings_store.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#if defined(GLYPH_PLATFORM_PSP)
#include <pspctrl.h>
#endif

namespace glyph {

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
  case SDLK_BACKSPACE:
    input.delete_digit = true;
    break;
  case SDLK_0:
  case SDLK_1:
  case SDLK_2:
  case SDLK_3:
  case SDLK_4:
  case SDLK_5:
  case SDLK_6:
  case SDLK_7:
  case SDLK_8:
  case SDLK_9:
    input.digit = static_cast<int>(key - SDLK_0);
    break;
  case SDLK_KP_0:
  case SDLK_KP_1:
  case SDLK_KP_2:
  case SDLK_KP_3:
  case SDLK_KP_4:
  case SDLK_KP_5:
  case SDLK_KP_6:
  case SDLK_KP_7:
  case SDLK_KP_8:
  case SDLK_KP_9:
    input.digit = static_cast<int>(key - SDLK_KP_0);
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
    if (screen_ == Screen::Settings) {
      screen_ = settings_return_screen_;
    } else {
      openSettings();
    }
    return;
  }
  if (input.toc && screen_ != Screen::Reader) {
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
    if (jump_overlay_open_) {
      applyJumpOverlayInput(input);
      break;
    }
    if (input.toc || input.status) {
      openJumpOverlay();
      break;
    }
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
      closeJumpOverlay();
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
    if (input.left) {
      adjustSelectedSetting(-1);
    }
    if (input.right || input.accept) {
      adjustSelectedSetting(1);
    }
    if (input.back) {
      screen_ = settings_return_screen_;
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

void App::openSettings() {
  closeJumpOverlay();
  settings_return_screen_ = screen_;
  selected_setting_ = 0;
  screen_ = Screen::Settings;
}

void App::adjustSelectedSetting(int delta) {
  if (selected_setting_ != 2) {
    return;
  }

  scroll_step_mode_ = clampScrollStepMode(scroll_step_mode_ + delta);
  AppSettings settings;
  settings.scroll_step_mode = scroll_step_mode_;
  saveAppSettings(defaultSettingsPath(), settings);
  updateSettingsLabels();
}

void App::updateSettingsLabels() {
  settings_ = {
      "Theme: dark",
      "Font: Atkinson",
      "Bumper step: " + scrollStepModeLabel(scroll_step_mode_),
      "Triangle: jump to chapter/page",
      "Select: jump to chapter/page",
      "D-pad: page/line navigation",
      "Circle: save progress and return",
      std::string("Storage: ") + defaultStorageRootPath(),
  };
}

void App::openJumpOverlay() {
  if (reader_lines_.empty()) {
    return;
  }
  syncSelectedChapterToScroll();
  jump_mode_ = JumpMode::Chapters;
  page_entry_.clear();
  jump_overlay_open_ = true;
}

void App::closeJumpOverlay() {
  jump_overlay_open_ = false;
  page_entry_.clear();
}

void App::applyJumpOverlayInput(const InputState& input) {
  if (input.back || input.toc) {
    closeJumpOverlay();
    return;
  }
  if (input.left || input.right || input.status) {
    jump_mode_ = jump_mode_ == JumpMode::Chapters ? JumpMode::Page : JumpMode::Chapters;
  }

  if (jump_mode_ == JumpMode::Chapters) {
    const int chapter_count = static_cast<int>(reader_chapters_.size());
    if (input.down && chapter_count > 0) {
      selected_chapter_ = std::min(chapter_count - 1, selected_chapter_ + 1);
    }
    if (input.up && chapter_count > 0) {
      selected_chapter_ = std::max(0, selected_chapter_ - 1);
    }
    if (input.shoulder_r_click && chapter_count > 0) {
      selected_chapter_ = std::min(chapter_count - 1, selected_chapter_ + 5);
    }
    if (input.shoulder_l_click && chapter_count > 0) {
      selected_chapter_ = std::max(0, selected_chapter_ - 5);
    }
    if (input.accept && chapter_count > 0) {
      jumpToChapter(selected_chapter_);
      closeJumpOverlay();
    }
    return;
  }

  int page = pageEntryValue();
  if (input.digit >= 0 && page_entry_.size() < 4) {
    if (page_entry_ == "0") {
      page_entry_.clear();
    }
    page_entry_.push_back(static_cast<char>('0' + input.digit));
  }
  if (input.delete_digit && !page_entry_.empty()) {
    page_entry_.pop_back();
  }
  if (input.up) {
    page = std::max(1, page + 1);
    page_entry_ = std::to_string(std::min(totalPageCount(), page));
  }
  if (input.down) {
    page = std::max(1, page - 1);
    page_entry_ = std::to_string(page);
  }
  if (input.shoulder_r_click) {
    page_entry_ = std::to_string(std::min(totalPageCount(), page + 10));
  }
  if (input.shoulder_l_click) {
    page_entry_ = std::to_string(std::max(1, page - 10));
  }
  if (input.accept) {
    jumpToPage(pageEntryValue());
    closeJumpOverlay();
  }
}

void App::syncSelectedChapterToScroll() {
  selected_chapter_ = 0;
  for (int i = 0; i < static_cast<int>(reader_chapters_.size()); ++i) {
    if (reader_scroll_ >= reader_chapters_[static_cast<size_t>(i)].start_line) {
      selected_chapter_ = i;
    }
  }
}

int App::currentPageNumber() const {
  return std::min(totalPageCount(), (reader_scroll_ / linesPerPage()) + 1);
}

int App::totalPageCount() const {
  if (reader_lines_.empty()) {
    return 1;
  }
  const int page_lines = linesPerPage();
  return std::max(1, (static_cast<int>(reader_lines_.size()) + page_lines - 1) / page_lines);
}

int App::pageEntryValue() const {
  if (page_entry_.empty()) {
    return currentPageNumber();
  }
  const int page = std::atoi(page_entry_.c_str());
  return std::max(1, std::min(totalPageCount(), page));
}

void App::jumpToPage(int page) {
  const int clamped_page = std::max(1, std::min(totalPageCount(), page));
  reader_scroll_ = std::min(maxReaderScroll(), (clamped_page - 1) * linesPerPage());
  syncSelectedChapterToScroll();
}

void App::jumpToChapter(int chapter_index) {
  if (chapter_index < 0 || chapter_index >= static_cast<int>(reader_chapters_.size())) {
    return;
  }
  reader_scroll_ =
      std::min(maxReaderScroll(),
               std::max(0, reader_chapters_[static_cast<size_t>(chapter_index)].start_line));
  selected_chapter_ = chapter_index;
}

void App::pageReaderForward() {
  reader_scroll_ = std::min(maxReaderScroll(), reader_scroll_ + linesPerPage());
}

void App::pageReaderBackward() {
  reader_scroll_ = std::max(0, reader_scroll_ - linesPerPage());
}

void App::stepOrPageReaderForward() {
  const int page_lines = linesPerPage();
  const int scroll_step = readerScrollStep();
  const int max_scroll = maxReaderScroll();
  if (reader_scroll_ >= max_scroll) {
    return;
  }

  const int page_offset = reader_scroll_ % page_lines;
  if (page_offset + scroll_step < page_lines) {
    reader_scroll_ = std::min(max_scroll, reader_scroll_ + scroll_step);
    return;
  }

  reader_scroll_ = std::min(max_scroll, reader_scroll_ + page_lines - page_offset);
}

void App::stepOrPageReaderBackward() {
  if (reader_scroll_ <= 0) {
    return;
  }

  const int page_lines = linesPerPage();
  const int scroll_step = readerScrollStep();
  const int page_offset = reader_scroll_ % page_lines;
  if (page_offset > 0) {
    reader_scroll_ -= std::min(page_offset, scroll_step);
    return;
  }

  pageReaderBackward();
}

} // namespace glyph
