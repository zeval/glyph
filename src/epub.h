#ifndef GLYPH_EPUB_H
#define GLYPH_EPUB_H

#include "zip_archive.h"

#include <cstddef>
#include <string>
#include <vector>

namespace glyph {

struct EpubMetadata {
  std::string title;
  std::vector<std::string> authors;
  std::string language;
  std::string identifier;
};

struct EpubManifestItem {
  std::string id;
  std::string href;
  std::string media_type;
  std::string properties;
};

struct EpubSpineItem {
  std::string idref;
  std::string href;
  std::string media_type;
  bool linear = true;
};

struct EpubBook {
  std::string file_path;
  std::string package_path;
  EpubMetadata metadata;
  std::vector<EpubManifestItem> manifest;
  std::vector<EpubSpineItem> spine;
  std::vector<std::string> warnings;
};

struct EpubTextResult {
  bool ok = false;
  std::string error;
  std::string text;
  size_t spine_index = 0;
};

class EpubDocument {
public:
  bool open(const std::string& path);

  const std::string& error() const;
  const EpubBook& book() const;
  EpubTextResult readSpineText(size_t spine_index) const;
  EpubTextResult readFirstReadableSpineText() const;
  EpubTextResult readAllReadableSpineText() const;

private:
  ZipArchive archive_;
  EpubBook book_;
  std::string error_;
};

std::string extractXhtmlText(const std::string& xhtml);

} // namespace glyph

#endif // GLYPH_EPUB_H
