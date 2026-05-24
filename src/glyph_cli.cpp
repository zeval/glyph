#include "glyph_cli.h"

namespace glyph {

namespace {

bool parseSnapshotKind(const std::string& value, SnapshotKind& kind) {
  if (value == "browser") {
    kind = SnapshotKind::Browser;
    return true;
  }
  if (value == "settings") {
    kind = SnapshotKind::Settings;
    return true;
  }
  if (value == "reader") {
    kind = SnapshotKind::Reader;
    return true;
  }
  if (value == "reader-select") {
    kind = SnapshotKind::ReaderSelect;
    return true;
  }
  return false;
}

bool requireValue(int argc, int index, const std::string& option, std::string& error) {
  if (index + 1 < argc) {
    return true;
  }
  error = option + " requires a value";
  return false;
}

} // namespace

CliParseResult parseCommandLine(int argc, char** argv) {
  CliParseResult result;
  result.ok = true;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i] != nullptr ? argv[i] : "";
    if (arg == "--help" || arg == "-h") {
      result.options.mode = LaunchMode::Help;
      continue;
    }
    if (arg == "--snapshot") {
      if (!requireValue(argc, i, arg, result.error)) {
        result.ok = false;
        return result;
      }
      SnapshotKind kind = SnapshotKind::Browser;
      const std::string value = argv[++i] != nullptr ? argv[i] : "";
      if (!parseSnapshotKind(value, kind)) {
        result.ok = false;
        result.error = "unknown snapshot screen: " + value;
        return result;
      }
      result.options.mode = LaunchMode::Snapshot;
      result.options.snapshot_kind = kind;
      continue;
    }
    if (arg == "--out") {
      if (!requireValue(argc, i, arg, result.error)) {
        result.ok = false;
        return result;
      }
      result.options.snapshot_output_path = argv[++i] != nullptr ? argv[i] : "";
      continue;
    }
    if (arg == "--book") {
      if (!requireValue(argc, i, arg, result.error)) {
        result.ok = false;
        return result;
      }
      result.options.initial_book_path = argv[++i] != nullptr ? argv[i] : "";
      continue;
    }
    if (!arg.empty() && arg[0] == '-') {
      result.ok = false;
      result.error = "unknown option: " + arg;
      return result;
    }
    result.options.initial_book_path = arg;
  }

  if (result.options.mode == LaunchMode::Snapshot && result.options.snapshot_output_path.empty()) {
    result.ok = false;
    result.error = "--snapshot requires --out";
  }

  return result;
}

const char* usageText() {
  return "usage: glyph [book.epub]\n"
         "       glyph --snapshot browser|settings|reader|reader-select --out path.png "
         "[--book book.epub]\n";
}

} // namespace glyph
