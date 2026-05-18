#include "glyph_app.h"

#include "epub.h"
#include "library.h"
#include "progress.h"
#include "text_layout.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace glyph {

namespace {

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

std::vector<std::string> App::wrapReaderText(const std::string& text) const {
  std::vector<std::string> lines;

  TextLayoutConfig layout_config;
  layout_config.viewport_width = readerTextWidth();
  layout_config.viewport_height = 100000;
  layout_config.margin_left = 0;
  layout_config.margin_top = 0;
  layout_config.margin_right = 0;
  layout_config.margin_bottom = 0;
  layout_config.average_char_width = 8;

  if (font_ != nullptr) {
    int sample_width = 0;
    int sample_height = 0;
    const char sample[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    if (TTF_SizeUTF8(font_, sample, &sample_width, &sample_height) == 0) {
      const int sample_chars = static_cast<int>(sizeof(sample) - 1);
      layout_config.average_char_width =
          std::max(7, std::min(8, sample_width / std::max(1, sample_chars)));
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

} // namespace glyph
