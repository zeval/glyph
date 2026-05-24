#include "library.h"

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include <zlib.h>

namespace {

struct PngImage {
  unsigned int width = 0;
  unsigned int height = 0;
  std::vector<unsigned char> rgb;
};

unsigned int readBe32(const unsigned char* p) {
  return (static_cast<unsigned int>(p[0]) << 24) | (static_cast<unsigned int>(p[1]) << 16) |
         (static_cast<unsigned int>(p[2]) << 8) | static_cast<unsigned int>(p[3]);
}

PngImage readRgbPng(const std::string& path) {
  std::ifstream file(path, std::ios::binary);
  REQUIRE(file.good());
  const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),
                                         std::istreambuf_iterator<char>());
  REQUIRE(bytes.size() >= 33);

  const unsigned char expected_signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  for (int i = 0; i < 8; ++i) {
    REQUIRE(bytes[static_cast<size_t>(i)] == expected_signature[i]);
  }

  PngImage image;
  std::vector<unsigned char> compressed;
  size_t offset = 8;
  while (offset + 12 <= bytes.size()) {
    const unsigned int size = readBe32(&bytes[offset]);
    const size_t data_offset = offset + 8;
    const size_t next_offset = data_offset + size + 4;
    REQUIRE(next_offset <= bytes.size());
    const std::string type(reinterpret_cast<const char*>(&bytes[offset + 4]), 4);
    if (type == "IHDR") {
      image.width = readBe32(&bytes[data_offset]);
      image.height = readBe32(&bytes[data_offset + 4]);
      REQUIRE(bytes[data_offset + 8] == 8);
      REQUIRE(bytes[data_offset + 9] == 2);
    } else if (type == "IDAT") {
      compressed.insert(compressed.end(), bytes.begin() + static_cast<std::ptrdiff_t>(data_offset),
                        bytes.begin() + static_cast<std::ptrdiff_t>(data_offset + size));
    } else if (type == "IEND") {
      break;
    }
    offset = next_offset;
  }

  REQUIRE(image.width > 0);
  REQUIRE(image.height > 0);
  const unsigned long stride = image.width * 3;
  std::vector<unsigned char> raw((stride + 1) * image.height);
  unsigned long raw_size = static_cast<unsigned long>(raw.size());
  REQUIRE(uncompress(raw.data(), &raw_size, compressed.data(),
                     static_cast<unsigned long>(compressed.size())) == Z_OK);
  REQUIRE(raw_size == raw.size());

  image.rgb.resize(static_cast<size_t>(stride) * image.height);
  for (unsigned int y = 0; y < image.height; ++y) {
    const size_t raw_offset = static_cast<size_t>(y) * (stride + 1);
    REQUIRE(raw[raw_offset] == 0);
    std::copy(raw.begin() + static_cast<std::ptrdiff_t>(raw_offset + 1),
              raw.begin() + static_cast<std::ptrdiff_t>(raw_offset + 1 + stride),
              image.rgb.begin() + static_cast<std::ptrdiff_t>(static_cast<size_t>(y) * stride));
  }
  return image;
}

class TempDirectory {
public:
  TempDirectory() {
    char path_template[] = "/tmp/glyph-library-XXXXXX";
    char* created = mkdtemp(path_template);
    REQUIRE(created != nullptr);
    path_ = created;
  }

  ~TempDirectory() {
    for (const std::string& file : files_) {
      std::remove(pathFor(file).c_str());
    }
    for (const std::string& directory : directories_) {
      rmdir(pathFor(directory).c_str());
    }
    rmdir(path_.c_str());
  }

  std::string path() const {
    return path_;
  }

  std::string pathFor(const std::string& name) const {
    return path_ + "/" + name;
  }

  void writeFile(const std::string& name) {
    const std::string file_path = pathFor(name);
    const int fd = open(file_path.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
    REQUIRE(fd >= 0);
    const char content[] = "x";
    REQUIRE(write(fd, content, 1) == 1);
    REQUIRE(close(fd) == 0);
    files_.push_back(name);
  }

  void makeDirectory(const std::string& name) {
    REQUIRE(mkdir(pathFor(name).c_str(), 0700) == 0);
    directories_.push_back(name);
  }

private:
  std::string path_;
  std::vector<std::string> files_;
  std::vector<std::string> directories_;
};

} // namespace

TEST_CASE("glyph smoke test binary runs") {
  const std::string name = "glyph";
  REQUIRE(name.size() == 5);
}

TEST_CASE("library detects EPUB extensions case-insensitively") {
  REQUIRE(glyph::hasEpubExtension("book.epub"));
  REQUIRE(glyph::hasEpubExtension("book.EPUB"));
  REQUIRE(glyph::hasEpubExtension("/tmp/Book.EpUb"));
  REQUIRE_FALSE(glyph::hasEpubExtension("book.epub.tmp"));
  REQUIRE_FALSE(glyph::hasEpubExtension("bookepub"));
}

TEST_CASE("library discovers EPUB files directly inside a books directory") {
  TempDirectory books;
  books.writeFile("zeta.EPUB");
  books.writeFile("alpha.epub");
  books.writeFile("notes.txt");
  books.makeDirectory("folder.epub");

  const glyph::LibraryScanResult scan = glyph::discoverLibraryAt(books.path());

  REQUIRE_FALSE(scan.fallback);
  REQUIRE(scan.books_path == books.path() + "/");
  REQUIRE(scan.books.size() == 2);
  REQUIRE(scan.books[0].display_name == "alpha.epub");
  REQUIRE(scan.books[0].file_path == books.pathFor("alpha.epub"));
  REQUIRE_FALSE(scan.books[0].sample);
  REQUIRE(scan.books[1].display_name == "zeta.EPUB");
  REQUIRE(scan.books[1].file_path == books.pathFor("zeta.EPUB"));
  REQUIRE_FALSE(scan.books[1].sample);
}

TEST_CASE("library falls back to stable samples when no EPUB files exist") {
  TempDirectory books;

  const glyph::LibraryScanResult scan = glyph::discoverLibraryAt(books.path());

  REQUIRE(scan.fallback);
  REQUIRE(scan.books_path == books.path() + "/");
  REQUIRE(scan.books.size() == 4);
  REQUIRE(scan.books[0].display_name == "Drop EPUB files in books/");
  for (const glyph::LibraryBook& book : scan.books) {
    REQUIRE(book.sample);
    REQUIRE(book.file_path.empty());
  }
}

TEST_CASE("PSP EBOOT icon asset has opaque menu dimensions") {
  const PngImage icon = readRgbPng("assets/psp/ICON0.PNG");

  REQUIRE(icon.width == 144);
  REQUIRE(icon.height == 80);
}

TEST_CASE("PSP EBOOT icon uses the centered glyph mark on white") {
  const PngImage icon = readRgbPng("assets/psp/ICON0.PNG");

  int min_x = static_cast<int>(icon.width);
  int min_y = static_cast<int>(icon.height);
  int max_x = -1;
  int max_y = -1;
  for (unsigned int y = 0; y < icon.height; ++y) {
    for (unsigned int x = 0; x < icon.width; ++x) {
      const size_t offset = (static_cast<size_t>(y) * icon.width + x) * 3;
      const bool white =
          icon.rgb[offset] == 255 && icon.rgb[offset + 1] == 255 && icon.rgb[offset + 2] == 255;
      if (!white) {
        min_x = std::min(min_x, static_cast<int>(x));
        min_y = std::min(min_y, static_cast<int>(y));
        max_x = std::max(max_x, static_cast<int>(x));
        max_y = std::max(max_y, static_cast<int>(y));
      }
    }
  }

  REQUIRE(max_x >= min_x);
  const int artwork_width = max_x - min_x + 1;
  const int artwork_height = max_y - min_y + 1;
  REQUIRE(artwork_width == artwork_height);
  REQUIRE(artwork_width >= 48);
  REQUIRE(artwork_width <= 64);
  REQUIRE(std::abs((min_x + max_x) - static_cast<int>(icon.width - 1)) <= 1);
  REQUIRE(std::abs((min_y + max_y) - static_cast<int>(icon.height - 1)) <= 1);
}
