#include "text_layout.h"

#include <algorithm>
#include <cctype>
#include <cstddef>

namespace glyph {

namespace {

enum class BlockKind {
  Paragraph,
  Blank,
};

struct TextBlock {
  BlockKind kind = BlockKind::Paragraph;
  std::string text;
};

struct SanitizedConfig {
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
  int content_width = 444;
  int content_height = 236;
  int columns_per_line = 55;
};

int positiveOrDefault(int value, int fallback) {
  return value > 0 ? value : fallback;
}

int nonNegative(int value) {
  return std::max(0, value);
}

SanitizedConfig sanitizeConfig(const TextLayoutConfig& config) {
  SanitizedConfig sanitized;
  sanitized.viewport_width = positiveOrDefault(config.viewport_width, 480);
  sanitized.viewport_height = positiveOrDefault(config.viewport_height, 272);
  sanitized.margin_left = nonNegative(config.margin_left);
  sanitized.margin_top = nonNegative(config.margin_top);
  sanitized.margin_right = nonNegative(config.margin_right);
  sanitized.margin_bottom = nonNegative(config.margin_bottom);
  sanitized.average_char_width = positiveOrDefault(config.average_char_width, 8);
  sanitized.line_height = positiveOrDefault(config.line_height, 16);
  sanitized.paragraph_spacing = nonNegative(config.paragraph_spacing);
  sanitized.blank_line_height = nonNegative(config.blank_line_height);

  sanitized.content_width =
      std::max(1, sanitized.viewport_width - sanitized.margin_left - sanitized.margin_right);
  sanitized.content_height =
      std::max(sanitized.line_height,
               sanitized.viewport_height - sanitized.margin_top - sanitized.margin_bottom);
  sanitized.columns_per_line = std::max(1, sanitized.content_width / sanitized.average_char_width);
  return sanitized;
}

bool isAsciiWhitespace(char ch) {
  return std::isspace(static_cast<unsigned char>(ch)) != 0;
}

bool isLineBlank(const std::string& line) {
  return std::all_of(line.begin(), line.end(), isAsciiWhitespace);
}

std::string normalizeWhitespace(const std::string& text) {
  std::string normalized;
  bool pending_space = false;

  for (const char ch : text) {
    if (isAsciiWhitespace(ch)) {
      pending_space = !normalized.empty();
      continue;
    }
    if (pending_space) {
      normalized.push_back(' ');
      pending_space = false;
    }
    normalized.push_back(ch);
  }

  return normalized;
}

std::vector<TextBlock> parseBlocks(const std::string& text) {
  std::vector<TextBlock> blocks;
  std::string current_line;
  bool previous_was_cr = false;

  auto pushLine = [&blocks](const std::string& line) {
    if (isLineBlank(line)) {
      blocks.push_back({BlockKind::Blank, ""});
      return;
    }

    const std::string normalized = normalizeWhitespace(line);
    if (!normalized.empty()) {
      blocks.push_back({BlockKind::Paragraph, normalized});
    }
  };

  for (const char ch : text) {
    if (ch == '\r') {
      pushLine(current_line);
      current_line.clear();
      previous_was_cr = true;
      continue;
    }
    if (ch == '\n') {
      if (!previous_was_cr) {
        pushLine(current_line);
        current_line.clear();
      }
      previous_was_cr = false;
      continue;
    }
    previous_was_cr = false;
    current_line.push_back(ch);
  }

  if (!current_line.empty()) {
    pushLine(current_line);
  }

  while (!blocks.empty() && blocks.back().kind == BlockKind::Blank) {
    blocks.pop_back();
  }

  return blocks;
}

std::size_t utf8CodePointLength(unsigned char lead) {
  if ((lead & 0x80U) == 0U) {
    return 1;
  }
  if ((lead & 0xE0U) == 0xC0U) {
    return 2;
  }
  if ((lead & 0xF0U) == 0xE0U) {
    return 3;
  }
  if ((lead & 0xF8U) == 0xF0U) {
    return 4;
  }
  return 1;
}

std::size_t nextUtf8Offset(const std::string& text, std::size_t offset) {
  const std::size_t length = utf8CodePointLength(static_cast<unsigned char>(text[offset]));
  if (offset + length > text.size()) {
    return offset + 1;
  }
  return offset + length;
}

std::size_t columnCount(const std::string& text) {
  std::size_t columns = 0;
  for (std::size_t i = 0; i < text.size(); i = nextUtf8Offset(text, i)) {
    ++columns;
  }
  return columns;
}

std::vector<std::string> splitLongWord(const std::string& word, int columns_per_line) {
  std::vector<std::string> chunks;
  const std::size_t max_columns = static_cast<std::size_t>(columns_per_line);

  std::size_t start = 0;
  std::size_t cursor = 0;
  std::size_t columns = 0;
  while (cursor < word.size()) {
    if (columns == max_columns) {
      chunks.push_back(word.substr(start, cursor - start));
      start = cursor;
      columns = 0;
    }
    cursor = nextUtf8Offset(word, cursor);
    ++columns;
  }

  if (start < word.size()) {
    chunks.push_back(word.substr(start));
  }

  return chunks;
}

std::vector<std::string> wrapParagraph(const std::string& paragraph, int columns_per_line) {
  std::vector<std::string> lines;
  std::string current_line;
  std::string current_word;

  auto flushLine = [&lines, &current_line]() {
    if (!current_line.empty()) {
      lines.push_back(current_line);
      current_line.clear();
    }
  };

  auto appendWord = [&current_line, &flushLine, &lines, columns_per_line](const std::string& word) {
    if (word.empty()) {
      return;
    }

    const std::size_t word_columns = columnCount(word);
    const std::size_t max_columns = static_cast<std::size_t>(columns_per_line);
    if (word_columns > max_columns) {
      flushLine();
      const std::vector<std::string> chunks = splitLongWord(word, columns_per_line);
      for (std::size_t i = 0; i < chunks.size(); ++i) {
        const std::string& chunk = chunks[i];
        if (i + 1 == chunks.size() && columnCount(chunk) < max_columns) {
          current_line = chunk;
        } else {
          lines.push_back(chunk);
        }
      }
      return;
    }

    if (current_line.empty()) {
      current_line = word;
      return;
    }

    if (columnCount(current_line) + 1 + word_columns <= max_columns) {
      current_line.push_back(' ');
      current_line.append(word);
      return;
    }

    flushLine();
    current_line = word;
  };

  for (const char ch : paragraph) {
    if (ch == ' ') {
      appendWord(current_word);
      current_word.clear();
      continue;
    }
    current_word.push_back(ch);
  }
  appendWord(current_word);
  flushLine();

  return lines;
}

class Paginator {
public:
  explicit Paginator(SanitizedConfig config) : config_(config), current_y_(config.margin_top) {
    layout_.content_width = config_.content_width;
    layout_.content_height = config_.content_height;
    layout_.columns_per_line = config_.columns_per_line;
    layout_.pages.push_back({});
  }

  void addLine(const std::string& text) {
    const int bottom_y = config_.margin_top + config_.content_height;
    if (current_y_ + config_.line_height > bottom_y) {
      if (!currentPage().lines.empty()) {
        beginPage();
      } else {
        current_y_ = config_.margin_top;
      }
    }

    currentPage().lines.push_back({text, config_.margin_left, current_y_});
    current_y_ += config_.line_height;
  }

  void addVerticalSpace(int height) {
    if (height <= 0) {
      return;
    }

    const int bottom_y = config_.margin_top + config_.content_height;
    if (current_y_ + height > bottom_y && !currentPage().lines.empty()) {
      beginPage();
      return;
    }

    current_y_ = std::min(bottom_y, current_y_ + height);
  }

  TextLayout finish() {
    return layout_;
  }

private:
  TextLayoutPage& currentPage() {
    return layout_.pages.back();
  }

  void beginPage() {
    layout_.pages.push_back({});
    current_y_ = config_.margin_top;
  }

  SanitizedConfig config_;
  TextLayout layout_;
  int current_y_ = 0;
};

bool nextBlockIsParagraph(const std::vector<TextBlock>& blocks, std::size_t index) {
  return index + 1 < blocks.size() && blocks[index + 1].kind == BlockKind::Paragraph;
}

} // namespace

TextLayout paginatePlainText(const std::string& text, const TextLayoutConfig& config) {
  const SanitizedConfig sanitized = sanitizeConfig(config);
  Paginator paginator(sanitized);
  const std::vector<TextBlock> blocks = parseBlocks(text);

  for (std::size_t i = 0; i < blocks.size(); ++i) {
    const TextBlock& block = blocks[i];
    if (block.kind == BlockKind::Blank) {
      paginator.addVerticalSpace(sanitized.blank_line_height);
      continue;
    }

    const std::vector<std::string> lines = wrapParagraph(block.text, sanitized.columns_per_line);
    for (const std::string& line : lines) {
      paginator.addLine(line);
    }
    if (nextBlockIsParagraph(blocks, i)) {
      paginator.addVerticalSpace(sanitized.paragraph_spacing);
    }
  }

  return paginator.finish();
}

} // namespace glyph
