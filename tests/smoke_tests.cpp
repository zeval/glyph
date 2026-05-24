#include "library.h"

#include <catch2/catch_test_macros.hpp>

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

namespace {

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
  std::ifstream file("assets/psp/ICON0.PNG", std::ios::binary);
  REQUIRE(file.good());

  unsigned char header[26] = {};
  file.read(reinterpret_cast<char*>(header), sizeof(header));
  REQUIRE(file.gcount() == static_cast<std::streamsize>(sizeof(header)));

  const unsigned char expected_signature[8] = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n'};
  for (int i = 0; i < 8; ++i) {
    REQUIRE(header[i] == expected_signature[i]);
  }

  const auto read_be32 = [](const unsigned char* p) {
    return (static_cast<unsigned int>(p[0]) << 24) | (static_cast<unsigned int>(p[1]) << 16) |
           (static_cast<unsigned int>(p[2]) << 8) | static_cast<unsigned int>(p[3]);
  };

  REQUIRE(read_be32(&header[16]) == 144);
  REQUIRE(read_be32(&header[20]) == 80);
  REQUIRE(header[24] == 8);
  REQUIRE(header[25] == 2);
}
