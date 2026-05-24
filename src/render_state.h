#ifndef GLYPH_RENDER_STATE_H
#define GLYPH_RENDER_STATE_H

#include <string>

namespace glyph {

struct RenderState {
  int screen = 0;
  int selected_book = 0;
  int selected_setting = 0;
  int reader_scroll = 0;
  int reader_line_count = 0;
  bool jump_overlay_open = false;
  int jump_mode = 0;
  int selected_chapter = 0;
  int scroll_step_mode = 0;
  std::string page_entry;
};

inline bool renderStateChanged(const RenderState& before, const RenderState& after) {
  return before.screen != after.screen || before.selected_book != after.selected_book ||
         before.selected_setting != after.selected_setting ||
         before.reader_scroll != after.reader_scroll ||
         before.reader_line_count != after.reader_line_count ||
         before.jump_overlay_open != after.jump_overlay_open ||
         before.jump_mode != after.jump_mode || before.selected_chapter != after.selected_chapter ||
         before.scroll_step_mode != after.scroll_step_mode || before.page_entry != after.page_entry;
}

} // namespace glyph

#endif // GLYPH_RENDER_STATE_H
