#ifndef GLYPH_CLI_H
#define GLYPH_CLI_H

#include "glyph_snapshot.h"

#include <string>

namespace glyph {

enum class LaunchMode {
  Run,
  Snapshot,
  Help,
};

struct LaunchOptions {
  LaunchMode mode = LaunchMode::Run;
  int width = 480;
  int height = 272;
  std::string initial_book_path;
  SnapshotKind snapshot_kind = SnapshotKind::Browser;
  std::string snapshot_output_path;
};

struct CliParseResult {
  bool ok = false;
  LaunchOptions options;
  std::string error;
};

CliParseResult parseCommandLine(int argc, char** argv);
const char* usageText();

} // namespace glyph

#endif // GLYPH_CLI_H
