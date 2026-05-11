#include "zip_archive.h"

#include <catch2/catch_test_macros.hpp>
#include <zlib.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace {

void appendLe16(std::vector<uint8_t>& out, uint16_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
}

void appendLe32(std::vector<uint8_t>& out, uint32_t value) {
  out.push_back(static_cast<uint8_t>(value & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 8u) & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 16u) & 0xffu));
  out.push_back(static_cast<uint8_t>((value >> 24u) & 0xffu));
}

void appendBytes(std::vector<uint8_t>& out, const std::string& text) {
  out.insert(out.end(), text.begin(), text.end());
}

std::vector<uint8_t> deflateRaw(const std::string& text) {
  std::vector<uint8_t> compressed(compressBound(text.size()));

  z_stream stream = {};
  stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(text.data()));
  stream.avail_in = static_cast<uInt>(text.size());
  stream.next_out = compressed.data();
  stream.avail_out = static_cast<uInt>(compressed.size());

  REQUIRE(deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED, -MAX_WBITS, 8,
                       Z_DEFAULT_STRATEGY) == Z_OK);
  REQUIRE(deflate(&stream, Z_FINISH) == Z_STREAM_END);
  REQUIRE(deflateEnd(&stream) == Z_OK);

  compressed.resize(stream.total_out);
  return compressed;
}

std::string tempZipPath() {
  return "/tmp/glyph-zip-test-" + std::to_string(static_cast<long long>(std::time(nullptr))) +
         ".zip";
}

bool writeDeflatedZip(const std::string& path, const std::string& name, const std::string& text) {
  const std::vector<uint8_t> compressed = deflateRaw(text);
  const uint32_t crc = crc32(0, reinterpret_cast<const Bytef*>(text.data()), text.size());
  std::vector<uint8_t> zip;

  const uint32_t local_offset = static_cast<uint32_t>(zip.size());
  appendLe32(zip, 0x04034b50);
  appendLe16(zip, 20);
  appendLe16(zip, 0);
  appendLe16(zip, 8);
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe32(zip, crc);
  appendLe32(zip, static_cast<uint32_t>(compressed.size()));
  appendLe32(zip, static_cast<uint32_t>(text.size()));
  appendLe16(zip, static_cast<uint16_t>(name.size()));
  appendLe16(zip, 0);
  appendBytes(zip, name);
  zip.insert(zip.end(), compressed.begin(), compressed.end());

  const uint32_t central_offset = static_cast<uint32_t>(zip.size());
  appendLe32(zip, 0x02014b50);
  appendLe16(zip, 20);
  appendLe16(zip, 20);
  appendLe16(zip, 0);
  appendLe16(zip, 8);
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe32(zip, crc);
  appendLe32(zip, static_cast<uint32_t>(compressed.size()));
  appendLe32(zip, static_cast<uint32_t>(text.size()));
  appendLe16(zip, static_cast<uint16_t>(name.size()));
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe32(zip, 0);
  appendLe32(zip, local_offset);
  appendBytes(zip, name);
  const uint32_t central_size = static_cast<uint32_t>(zip.size()) - central_offset;

  appendLe32(zip, 0x06054b50);
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe16(zip, 1);
  appendLe16(zip, 1);
  appendLe32(zip, central_size);
  appendLe32(zip, central_offset);
  appendLe16(zip, 0);

  std::ofstream file(path, std::ios::binary);
  if (!file) {
    return false;
  }
  file.write(reinterpret_cast<const char*>(zip.data()), static_cast<std::streamsize>(zip.size()));
  return static_cast<bool>(file);
}

} // namespace

TEST_CASE("zip archive reads deflated entries") {
  const std::string path = tempZipPath();
  const std::string text = "Hello compressed EPUB content.\nSecond line.";
  REQUIRE(writeDeflatedZip(path, "OPS/chapter.xhtml", text));

  glyph::ZipArchive archive;
  REQUIRE(archive.open(path));
  const glyph::ZipReadResult result = archive.readFile("OPS/chapter.xhtml");
  REQUIRE(result.ok);
  REQUIRE(glyph::bytesToString(result.bytes) == text);

  std::remove(path.c_str());
}
