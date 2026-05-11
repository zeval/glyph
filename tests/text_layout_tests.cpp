#include "text_layout.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {

glyph::TextLayoutConfig testConfig() {
  glyph::TextLayoutConfig config;
  config.viewport_width = 10;
  config.viewport_height = 100;
  config.margin_left = 0;
  config.margin_top = 0;
  config.margin_right = 0;
  config.margin_bottom = 0;
  config.average_char_width = 1;
  config.line_height = 10;
  config.paragraph_spacing = 0;
  config.blank_line_height = 10;
  return config;
}

} // namespace

TEST_CASE("plain text pagination wraps words to configured columns") {
  const glyph::TextLayout layout = glyph::paginatePlainText("one two three four", testConfig());

  REQUIRE(layout.columns_per_line == 10);
  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 2);
  CHECK(layout.pages[0].lines[0].text == "one two");
  CHECK(layout.pages[0].lines[0].y == 0);
  CHECK(layout.pages[0].lines[1].text == "three four");
  CHECK(layout.pages[0].lines[1].y == 10);
}

TEST_CASE("plain text pagination splits words longer than a line") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 5;

  const glyph::TextLayout layout = glyph::paginatePlainText("abcdefghij k", config);

  REQUIRE(layout.columns_per_line == 5);
  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 3);
  CHECK(layout.pages[0].lines[0].text == "abcde");
  CHECK(layout.pages[0].lines[1].text == "fghij");
  CHECK(layout.pages[0].lines[2].text == "k");
}

TEST_CASE("plain text pagination continues after a partial long word chunk") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 5;

  const glyph::TextLayout layout = glyph::paginatePlainText("abcdef gh", config);

  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 2);
  CHECK(layout.pages[0].lines[0].text == "abcde");
  CHECK(layout.pages[0].lines[1].text == "f gh");
}

TEST_CASE("plain text pagination preserves blank lines as vertical space") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 20;
  config.blank_line_height = 10;

  const glyph::TextLayout layout = glyph::paginatePlainText("alpha\n\nbeta", config);

  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 2);
  CHECK(layout.pages[0].lines[0].text == "alpha");
  CHECK(layout.pages[0].lines[0].y == 0);
  CHECK(layout.pages[0].lines[1].text == "beta");
  CHECK(layout.pages[0].lines[1].y == 20);
}

TEST_CASE("plain text pagination applies spacing between adjacent paragraphs") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 20;
  config.paragraph_spacing = 5;

  const glyph::TextLayout layout = glyph::paginatePlainText("alpha\nbeta", config);

  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 2);
  CHECK(layout.pages[0].lines[0].y == 0);
  CHECK(layout.pages[0].lines[1].y == 15);
}

TEST_CASE("plain text pagination starts a new page at the bottom boundary") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 20;
  config.viewport_height = 25;

  const glyph::TextLayout layout = glyph::paginatePlainText("one\ntwo\nthree", config);

  REQUIRE(layout.pages.size() == 2);
  REQUIRE(layout.pages[0].lines.size() == 2);
  REQUIRE(layout.pages[1].lines.size() == 1);
  CHECK(layout.pages[0].lines[0].text == "one");
  CHECK(layout.pages[0].lines[1].text == "two");
  CHECK(layout.pages[1].lines[0].text == "three");
  CHECK(layout.pages[1].lines[0].y == 0);
}

TEST_CASE("plain text pagination does not place first text below the viewport") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 20;
  config.viewport_height = 15;

  const glyph::TextLayout layout = glyph::paginatePlainText("\n\nalpha", config);

  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 1);
  CHECK(layout.pages[0].lines[0].text == "alpha");
  CHECK(layout.pages[0].lines[0].y == 0);
}

TEST_CASE("plain text pagination normalizes CRLF and paragraph whitespace") {
  glyph::TextLayoutConfig config = testConfig();
  config.viewport_width = 20;

  const glyph::TextLayout layout =
      glyph::paginatePlainText("alpha   beta\r\ngamma\t delta\romega", config);

  REQUIRE(layout.pages.size() == 1);
  REQUIRE(layout.pages[0].lines.size() == 3);
  CHECK(layout.pages[0].lines[0].text == "alpha beta");
  CHECK(layout.pages[0].lines[1].text == "gamma delta");
  CHECK(layout.pages[0].lines[2].text == "omega");
}

TEST_CASE("plain text pagination returns one empty page for empty input") {
  const glyph::TextLayout layout = glyph::paginatePlainText("", testConfig());

  REQUIRE(layout.pages.size() == 1);
  CHECK(layout.pages[0].lines.empty());
}
