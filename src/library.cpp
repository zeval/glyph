#include "library.h"

#include <algorithm>
#include <dirent.h>
#include <memory>
#include <sys/stat.h>

namespace glyph {

namespace {

char lowerAscii(char value) {
  if (value >= 'A' && value <= 'Z') {
    return static_cast<char>(value - 'A' + 'a');
  }
  return value;
}

std::string lowerAsciiCopy(const std::string& value) {
  std::string lowered = value;
  std::transform(lowered.begin(), lowered.end(), lowered.begin(), lowerAscii);
  return lowered;
}

std::string ensureTrailingSlash(const std::string& path) {
  if (path.empty()) {
    return "./";
  }

  const char last = path[path.size() - 1];
  if (last == '/' || last == '\\') {
    return path;
  }
  return path + "/";
}

std::string joinPath(const std::string& directory, const std::string& name) {
  return ensureTrailingSlash(directory) + name;
}

bool isRegularFile(const std::string& path) {
  struct stat info = {};
  if (stat(path.c_str(), &info) != 0) {
    return false;
  }
  return S_ISREG(info.st_mode);
}

struct DirectoryCloser {
  void operator()(DIR* directory) const {
    if (directory != nullptr) {
      closedir(directory);
    }
  }
};

bool libraryBookLess(const LibraryBook& left, const LibraryBook& right) {
  const std::string left_name = lowerAsciiCopy(left.display_name);
  const std::string right_name = lowerAsciiCopy(right.display_name);
  if (left_name == right_name) {
    return left.display_name < right.display_name;
  }
  return left_name < right_name;
}

} // namespace

const char* defaultBooksPath() {
#if defined(GLYPH_PLATFORM_PSP)
  return "ef0:/PSP/GAME/glyph/books/";
#else
  return "./books/";
#endif
}

const char* defaultStorageRootPath() {
#if defined(GLYPH_PLATFORM_PSP)
  return "ef0:/PSP/GAME/glyph/";
#else
  return "./";
#endif
}

bool hasEpubExtension(const std::string& path) {
  constexpr char kExtension[] = ".epub";
  constexpr size_t kExtensionLength = sizeof(kExtension) - 1;

  if (path.size() < kExtensionLength) {
    return false;
  }

  const size_t offset = path.size() - kExtensionLength;
  for (size_t i = 0; i < kExtensionLength; ++i) {
    if (lowerAscii(path[offset + i]) != kExtension[i]) {
      return false;
    }
  }
  return true;
}

std::string displayNameForPath(const std::string& path) {
  const size_t slash = path.find_last_of("/\\");
  if (slash == std::string::npos || slash + 1 >= path.size()) {
    return path;
  }
  return path.substr(slash + 1);
}

std::vector<LibraryBook> fallbackLibraryBooks() {
  return {
      {"Drop EPUB files in books/", "", true},
      {"A Study in Scarlet.epub", "", true},
      {"Pride and Prejudice.epub", "", true},
      {"The Time Machine.epub", "", true},
  };
}

LibraryScanResult discoverLibrary() {
  return discoverLibraryAt(defaultBooksPath());
}

LibraryScanResult discoverLibraryAt(const std::string& books_path) {
  LibraryScanResult result;
  result.books_path = ensureTrailingSlash(books_path);

  std::unique_ptr<DIR, DirectoryCloser> directory(opendir(result.books_path.c_str()));
  if (directory != nullptr) {
    while (true) {
      struct dirent* entry = readdir(directory.get());
      if (entry == nullptr) {
        break;
      }

      const std::string name = entry->d_name;
      if (!hasEpubExtension(name)) {
        continue;
      }

      const std::string file_path = joinPath(result.books_path, name);
      if (!isRegularFile(file_path)) {
        continue;
      }

      result.books.push_back({displayNameForPath(file_path), file_path, false});
    }
  }

  std::sort(result.books.begin(), result.books.end(), libraryBookLess);
  if (result.books.empty()) {
    result.books = fallbackLibraryBooks();
    result.fallback = true;
  }

  return result;
}

} // namespace glyph
