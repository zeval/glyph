#ifndef GLYPH_LIBRARY_H
#define GLYPH_LIBRARY_H

#include <string>
#include <vector>

namespace glyph {

struct LibraryBook {
  std::string display_name;
  std::string file_path;
  bool sample = false;
};

struct LibraryScanResult {
  std::string books_path;
  std::vector<LibraryBook> books;
  bool fallback = false;
};

const char* defaultBooksPath();
const char* defaultStorageRootPath();

bool hasEpubExtension(const std::string& path);
std::string displayNameForPath(const std::string& path);
std::vector<LibraryBook> fallbackLibraryBooks();
LibraryScanResult discoverLibrary();
LibraryScanResult discoverLibraryAt(const std::string& books_path);

} // namespace glyph

#endif // GLYPH_LIBRARY_H
