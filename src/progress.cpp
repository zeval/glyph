#include "progress.h"

#include "library.h"

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <sys/stat.h>

namespace glyph {

namespace {

std::string joinPath(const std::string& directory, const std::string& name) {
  if (directory.empty()) {
    return name;
  }
  const char last = directory[directory.size() - 1];
  if (last == '/' || last == '\\') {
    return directory + name;
  }
  return directory + "/" + name;
}

bool ensureParentDirectory(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos || slash == 0) {
    return true;
  }

  const std::string directory = path.substr(0, slash);
  if (mkdir(directory.c_str(), 0700) == 0 || errno == EEXIST) {
    return true;
  }
  return false;
}

std::string readWholeFile(const std::string& path) {
  std::ifstream file(path.c_str(), std::ios::in | std::ios::binary);
  if (!file) {
    return "";
  }

  std::ostringstream out;
  out << file.rdbuf();
  return out.str();
}

void appendHexByte(std::string& out, unsigned char value) {
  constexpr char kHex[] = "0123456789abcdef";
  out.push_back('\\');
  out.push_back('u');
  out.push_back('0');
  out.push_back('0');
  out.push_back(kHex[(value >> 4U) & 0x0FU]);
  out.push_back(kHex[value & 0x0FU]);
}

std::string escapeJsonString(const std::string& value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const unsigned char ch : value) {
    switch (ch) {
    case '\\':
      out += "\\\\";
      break;
    case '"':
      out += "\\\"";
      break;
    case '\n':
      out += "\\n";
      break;
    case '\r':
      out += "\\r";
      break;
    case '\t':
      out += "\\t";
      break;
    default:
      if (ch < 0x20U) {
        appendHexByte(out, ch);
      } else {
        out.push_back(static_cast<char>(ch));
      }
      break;
    }
  }
  return out;
}

int hexValue(char ch) {
  if (ch >= '0' && ch <= '9') {
    return ch - '0';
  }
  if (ch >= 'a' && ch <= 'f') {
    return ch - 'a' + 10;
  }
  if (ch >= 'A' && ch <= 'F') {
    return ch - 'A' + 10;
  }
  return -1;
}

class JsonReader {
public:
  explicit JsonReader(const std::string& text) : text_(text) {}

  std::vector<BookProgress> readProgress() {
    std::vector<BookProgress> entries;
    if (!consume('{')) {
      return entries;
    }

    while (true) {
      skipSpace();
      if (consume('}')) {
        break;
      }

      std::string key;
      if (!readString(key) || !consume(':')) {
        return {};
      }

      if (key == "books") {
        if (!readBooks(entries)) {
          return {};
        }
      } else if (!skipValue()) {
        return {};
      }

      skipSpace();
      if (consume('}')) {
        break;
      }
      if (!consume(',')) {
        return {};
      }
    }
    return entries;
  }

private:
  void skipSpace() {
    while (pos_ < text_.size() && std::isspace(static_cast<unsigned char>(text_[pos_])) != 0) {
      ++pos_;
    }
  }

  bool consume(char expected) {
    skipSpace();
    if (pos_ >= text_.size() || text_[pos_] != expected) {
      return false;
    }
    ++pos_;
    return true;
  }

  bool readString(std::string& out) {
    skipSpace();
    if (pos_ >= text_.size() || text_[pos_] != '"') {
      return false;
    }
    ++pos_;
    out.clear();

    while (pos_ < text_.size()) {
      const char ch = text_[pos_++];
      if (ch == '"') {
        return true;
      }
      if (ch != '\\') {
        out.push_back(ch);
        continue;
      }
      if (pos_ >= text_.size()) {
        return false;
      }

      const char escaped = text_[pos_++];
      switch (escaped) {
      case '"':
      case '\\':
      case '/':
        out.push_back(escaped);
        break;
      case 'n':
        out.push_back('\n');
        break;
      case 'r':
        out.push_back('\r');
        break;
      case 't':
        out.push_back('\t');
        break;
      case 'u':
        if (!readAsciiUnicodeEscape(out)) {
          return false;
        }
        break;
      default:
        return false;
      }
    }
    return false;
  }

  bool readAsciiUnicodeEscape(std::string& out) {
    if (pos_ + 4 > text_.size()) {
      return false;
    }

    int value = 0;
    for (int i = 0; i < 4; ++i) {
      const int nibble = hexValue(text_[pos_++]);
      if (nibble < 0) {
        return false;
      }
      value = (value << 4) | nibble;
    }
    if (value > 0 && value <= 0x7f) {
      out.push_back(static_cast<char>(value));
    }
    return true;
  }

  bool readInt(int& out) {
    skipSpace();
    if (pos_ >= text_.size()) {
      return false;
    }

    char* end = nullptr;
    const long value = std::strtol(text_.c_str() + pos_, &end, 10);
    if (end == text_.c_str() + pos_) {
      return false;
    }
    pos_ = static_cast<size_t>(end - text_.c_str());
    out = static_cast<int>(std::max<long>(0, value));
    return true;
  }

  bool readBooks(std::vector<BookProgress>& entries) {
    if (!consume('[')) {
      return false;
    }

    while (true) {
      skipSpace();
      if (consume(']')) {
        return true;
      }

      BookProgress progress;
      if (!readBook(progress)) {
        return false;
      }
      if (!progress.file_path.empty()) {
        entries.push_back(progress);
      }

      skipSpace();
      if (consume(']')) {
        return true;
      }
      if (!consume(',')) {
        return false;
      }
    }
  }

  bool readBook(BookProgress& progress) {
    if (!consume('{')) {
      return false;
    }

    while (true) {
      skipSpace();
      if (consume('}')) {
        return true;
      }

      std::string key;
      if (!readString(key) || !consume(':')) {
        return false;
      }

      if (key == "path") {
        if (!readString(progress.file_path)) {
          return false;
        }
      } else if (key == "reader_scroll") {
        if (!readInt(progress.reader_scroll)) {
          return false;
        }
      } else if (key == "total_lines") {
        if (!readInt(progress.total_lines)) {
          return false;
        }
      } else if (key == "lines_per_page") {
        if (!readInt(progress.lines_per_page)) {
          return false;
        }
      } else if (!skipValue()) {
        return false;
      }

      skipSpace();
      if (consume('}')) {
        return true;
      }
      if (!consume(',')) {
        return false;
      }
    }
  }

  bool skipValue() {
    skipSpace();
    if (pos_ >= text_.size()) {
      return false;
    }

    if (text_[pos_] == '"') {
      std::string ignored;
      return readString(ignored);
    }
    if (text_[pos_] == '{') {
      ++pos_;
      while (true) {
        skipSpace();
        if (consume('}')) {
          return true;
        }
        std::string key;
        if (!readString(key) || !consume(':') || !skipValue()) {
          return false;
        }
        skipSpace();
        if (consume('}')) {
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }
    if (text_[pos_] == '[') {
      ++pos_;
      while (true) {
        skipSpace();
        if (consume(']')) {
          return true;
        }
        if (!skipValue()) {
          return false;
        }
        skipSpace();
        if (consume(']')) {
          return true;
        }
        if (!consume(',')) {
          return false;
        }
      }
    }

    while (pos_ < text_.size() && text_[pos_] != ',' && text_[pos_] != '}' && text_[pos_] != ']') {
      ++pos_;
    }
    return true;
  }

  const std::string& text_;
  size_t pos_ = 0;
};

bool writeProgressFile(const std::string& path, const std::vector<BookProgress>& entries) {
  if (!ensureParentDirectory(path)) {
    return false;
  }

  std::ofstream file(path.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
  if (!file) {
    return false;
  }

  file << "{\n  \"version\": 1,\n  \"books\": [\n";
  for (size_t i = 0; i < entries.size(); ++i) {
    const BookProgress& entry = entries[i];
    file << "    {\"path\":\"" << escapeJsonString(entry.file_path)
         << "\",\"reader_scroll\":" << std::max(0, entry.reader_scroll)
         << ",\"total_lines\":" << std::max(0, entry.total_lines)
         << ",\"lines_per_page\":" << std::max(1, entry.lines_per_page) << "}";
    if (i + 1 < entries.size()) {
      file << ",";
    }
    file << "\n";
  }
  file << "  ]\n}\n";
  return static_cast<bool>(file);
}

} // namespace

std::string defaultProgressPath() {
  return joinPath(joinPath(defaultStorageRootPath(), "saves"), "progress.json");
}

std::vector<BookProgress> loadProgressFile(const std::string& path) {
  const std::string text = readWholeFile(path);
  if (text.empty()) {
    return {};
  }
  return JsonReader(text).readProgress();
}

bool saveBookProgress(const std::string& path, const BookProgress& progress) {
  if (progress.file_path.empty()) {
    return false;
  }

  std::vector<BookProgress> entries = loadProgressFile(path);
  bool updated = false;
  for (BookProgress& entry : entries) {
    if (entry.file_path == progress.file_path) {
      entry = progress;
      updated = true;
      break;
    }
  }
  if (!updated) {
    entries.push_back(progress);
  }
  return writeProgressFile(path, entries);
}

BookProgress findBookProgress(const std::vector<BookProgress>& entries,
                              const std::string& file_path) {
  for (const BookProgress& entry : entries) {
    if (entry.file_path == file_path) {
      return entry;
    }
  }
  return {};
}

int progressTotalPages(const BookProgress& progress) {
  if (progress.total_lines <= 0 || progress.lines_per_page <= 0) {
    return 0;
  }
  return std::max(1,
                  (progress.total_lines + progress.lines_per_page - 1) / progress.lines_per_page);
}

int progressCurrentPage(const BookProgress& progress) {
  const int total_pages = progressTotalPages(progress);
  if (total_pages <= 0 || progress.lines_per_page <= 0) {
    return 0;
  }
  const int page = (std::max(0, progress.reader_scroll) / progress.lines_per_page) + 1;
  return std::min(total_pages, std::max(1, page));
}

int progressPercent(const BookProgress& progress) {
  const int total_pages = progressTotalPages(progress);
  if (total_pages <= 0) {
    return 0;
  }
  return std::min(100, (progressCurrentPage(progress) * 100) / total_pages);
}

std::string progressSummary(const BookProgress& progress) {
  const int total_pages = progressTotalPages(progress);
  if (total_pages <= 0) {
    return "";
  }

  return "p " + std::to_string(progressCurrentPage(progress)) + "/" + std::to_string(total_pages) +
         " " + std::to_string(progressPercent(progress)) + "%";
}

} // namespace glyph
