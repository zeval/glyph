#ifndef GLYPH_TEXT_LAYOUT_H
#define GLYPH_TEXT_LAYOUT_H

#include <string>
#include <vector>

namespace glyph {

struct TextLayoutConfig {
  int viewport_width = 480;
  int viewport_height = 272;
  int margin_left = 18;
  int margin_top = 18;
  int margin_right = 18;
  int margin_bottom = 18;
  int average_char_width = 8;
  int line_height = 16;
  int paragraph_spacing = 6;
  int blank_line_height = 16;
};

struct TextLayoutLine {
  std::string text;
  int x = 0;
  int y = 0;
};

struct TextLayoutPage {
  std::vector<TextLayoutLine> lines;
};

struct TextLayout {
  std::vector<TextLayoutPage> pages;
  int content_width = 0;
  int content_height = 0;
  int columns_per_line = 0;
};

TextLayout paginatePlainText(const std::string& text, const TextLayoutConfig& config);

} // namespace glyph

#endif // GLYPH_TEXT_LAYOUT_H
