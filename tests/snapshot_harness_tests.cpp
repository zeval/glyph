#include "glyph_cli.h"
#include "png_writer.h"

#include <catch2/catch_test_macros.hpp>

#include <fstream>
#include <string>
#include <vector>

namespace {

unsigned int readBe32(const unsigned char* p) {
  return (static_cast<unsigned int>(p[0]) << 24) | (static_cast<unsigned int>(p[1]) << 16) |
         (static_cast<unsigned int>(p[2]) << 8) | static_cast<unsigned int>(p[3]);
}

} // namespace

TEST_CASE("snapshot CLI parses named screens and output path") {
  const char* argv[] = {"glyph",
                        "--snapshot",
                        "reader-select",
                        "--book",
                        "book.epub",
                        "--out",
                        "build/ui/reader-select.png"};

  const glyph::CliParseResult parsed = glyph::parseCommandLine(7, const_cast<char**>(argv));

  REQUIRE(parsed.ok);
  REQUIRE(parsed.options.mode == glyph::LaunchMode::Snapshot);
  REQUIRE(parsed.options.snapshot_kind == glyph::SnapshotKind::ReaderSelect);
  REQUIRE(parsed.options.initial_book_path == "book.epub");
  REQUIRE(parsed.options.snapshot_output_path == "build/ui/reader-select.png");
}

TEST_CASE("snapshot CLI requires an output path") {
  const char* argv[] = {"glyph", "--snapshot", "browser"};

  const glyph::CliParseResult parsed = glyph::parseCommandLine(3, const_cast<char**>(argv));

  REQUIRE_FALSE(parsed.ok);
  REQUIRE(parsed.error.find("--out") != std::string::npos);
}

TEST_CASE("PNG writer emits RGB PNG dimensions") {
  const std::string output_path = "/tmp/glyph-png-writer-test.png";
  const std::vector<unsigned char> pixels = {
      255, 0, 0, 0, 255, 0, 0, 0, 255, 255, 255, 255,
  };

  REQUIRE(glyph::writeRgbPng(output_path, 2, 2, pixels));

  std::ifstream file(output_path, std::ios::binary);
  REQUIRE(file.good());
  unsigned char header[26] = {};
  file.read(reinterpret_cast<char*>(header), sizeof(header));
  REQUIRE(file.gcount() == static_cast<std::streamsize>(sizeof(header)));
  REQUIRE(readBe32(&header[16]) == 2);
  REQUIRE(readBe32(&header[20]) == 2);
  REQUIRE(header[24] == 8);
  REQUIRE(header[25] == 2);
  std::remove(output_path.c_str());
}
