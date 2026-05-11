#ifndef GLYPH_ZIP_ARCHIVE_H
#define GLYPH_ZIP_ARCHIVE_H

#include <cstdint>
#include <string>
#include <vector>

namespace glyph {

struct ZipReadResult {
  bool ok = false;
  std::string error;
  std::vector<uint8_t> bytes;
};

struct ZipEntry {
  std::string name;
  uint16_t method = 0;
  uint32_t compressed_size = 0;
  uint32_t uncompressed_size = 0;
  uint32_t local_header_offset = 0;
};

class ZipArchive {
public:
  bool open(const std::string& path);

  const std::string& error() const;
  const std::vector<ZipEntry>& entries() const;
  bool hasFile(const std::string& name) const;
  ZipReadResult readFile(const std::string& name) const;

private:
  const ZipEntry* findEntry(const std::string& name) const;
  bool parseCentralDirectory();

  std::vector<uint8_t> data_;
  std::vector<ZipEntry> entries_;
  std::string error_;
};

std::string bytesToString(const std::vector<uint8_t>& bytes);

} // namespace glyph

#endif // GLYPH_ZIP_ARCHIVE_H
