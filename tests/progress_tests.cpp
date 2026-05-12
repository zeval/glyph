#include "progress.h"

#include <catch2/catch_test_macros.hpp>

#include <sys/stat.h>
#include <unistd.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

class TempDirectory {
public:
  TempDirectory() {
    char path_template[] = "/tmp/glyph-progress-XXXXXX";
    char* created = mkdtemp(path_template);
    REQUIRE(created != nullptr);
    path_ = created;
  }

  ~TempDirectory() {
    std::remove(pathFor("saves/progress.json").c_str());
    rmdir(pathFor("saves").c_str());
    rmdir(path_.c_str());
  }

  std::string pathFor(const std::string& name) const {
    return path_ + "/" + name;
  }

private:
  std::string path_;
};

} // namespace

TEST_CASE("progress computes page summaries") {
  glyph::BookProgress progress;
  progress.file_path = "book.epub";
  progress.reader_scroll = 26;
  progress.total_lines = 100;
  progress.lines_per_page = 13;

  REQUIRE(glyph::progressCurrentPage(progress) == 3);
  REQUIRE(glyph::progressTotalPages(progress) == 8);
  REQUIRE(glyph::progressPercent(progress) == 37);
  REQUIRE(glyph::progressSummary(progress) == "p 3/8 37%");
}

TEST_CASE("progress saves and updates JSON records") {
  TempDirectory temp;
  const std::string path = temp.pathFor("saves/progress.json");

  glyph::BookProgress first;
  first.file_path = "/books/one.epub";
  first.reader_scroll = 7;
  first.total_lines = 91;
  first.lines_per_page = 13;
  REQUIRE(glyph::saveBookProgress(path, first));

  glyph::BookProgress second;
  second.file_path = "/books/two \"quoted\".epub";
  second.reader_scroll = 14;
  second.total_lines = 120;
  second.lines_per_page = 12;
  REQUIRE(glyph::saveBookProgress(path, second));

  first.reader_scroll = 21;
  REQUIRE(glyph::saveBookProgress(path, first));

  const std::vector<glyph::BookProgress> loaded = glyph::loadProgressFile(path);
  REQUIRE(loaded.size() == 2);

  const glyph::BookProgress loaded_first = glyph::findBookProgress(loaded, first.file_path);
  REQUIRE(loaded_first.reader_scroll == 21);
  REQUIRE(loaded_first.total_lines == 91);
  REQUIRE(loaded_first.lines_per_page == 13);

  const glyph::BookProgress loaded_second = glyph::findBookProgress(loaded, second.file_path);
  REQUIRE(loaded_second.reader_scroll == 14);
  REQUIRE(loaded_second.file_path == second.file_path);
}
