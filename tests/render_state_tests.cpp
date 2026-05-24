#include "render_state.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("render state is unchanged for identical values") {
  glyph::RenderState before;
  glyph::RenderState after;

  REQUIRE_FALSE(glyph::renderStateChanged(before, after));
}

TEST_CASE("render state changes when reader jump overlay opens") {
  glyph::RenderState before;
  glyph::RenderState after = before;
  after.jump_overlay_open = true;

  REQUIRE(glyph::renderStateChanged(before, after));
}

TEST_CASE("render state changes when reader jump overlay content changes") {
  glyph::RenderState before;
  glyph::RenderState after = before;
  after.jump_mode = 1;

  REQUIRE(glyph::renderStateChanged(before, after));

  after = before;
  after.selected_chapter = 4;

  REQUIRE(glyph::renderStateChanged(before, after));

  after = before;
  after.page_entry = "12";

  REQUIRE(glyph::renderStateChanged(before, after));
}
