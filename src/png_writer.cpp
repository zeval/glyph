#include "png_writer.h"

#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <vector>

namespace glyph {

namespace {

void appendBe32(std::vector<unsigned char>& out, uint32_t value) {
  out.push_back(static_cast<unsigned char>((value >> 24) & 0xFF));
  out.push_back(static_cast<unsigned char>((value >> 16) & 0xFF));
  out.push_back(static_cast<unsigned char>((value >> 8) & 0xFF));
  out.push_back(static_cast<unsigned char>(value & 0xFF));
}

void appendChunk(std::vector<unsigned char>& out, const char* kind,
                 const std::vector<unsigned char>& data) {
  appendBe32(out, static_cast<uint32_t>(data.size()));
  const size_t kind_offset = out.size();
  out.push_back(static_cast<unsigned char>(kind[0]));
  out.push_back(static_cast<unsigned char>(kind[1]));
  out.push_back(static_cast<unsigned char>(kind[2]));
  out.push_back(static_cast<unsigned char>(kind[3]));
  out.insert(out.end(), data.begin(), data.end());

  uint32_t crc = crc32(0, nullptr, 0);
  crc = crc32(crc, out.data() + kind_offset, static_cast<uInt>(4 + data.size()));
  appendBe32(out, crc);
}

} // namespace

bool writeRgbPng(const std::string& path, int width, int height,
                 const std::vector<unsigned char>& pixels) {
  if (path.empty() || width <= 0 || height <= 0) {
    return false;
  }

  const size_t stride = static_cast<size_t>(width) * 3;
  if (pixels.size() != stride * static_cast<size_t>(height)) {
    return false;
  }

  std::vector<unsigned char> raw;
  raw.reserve((stride + 1) * static_cast<size_t>(height));
  for (int y = 0; y < height; ++y) {
    raw.push_back(0);
    const size_t row_offset = static_cast<size_t>(y) * stride;
    raw.insert(raw.end(), pixels.begin() + static_cast<std::ptrdiff_t>(row_offset),
               pixels.begin() + static_cast<std::ptrdiff_t>(row_offset + stride));
  }

  uLongf compressed_size = compressBound(static_cast<uLong>(raw.size()));
  std::vector<unsigned char> compressed(compressed_size);
  if (compress2(compressed.data(), &compressed_size, raw.data(), static_cast<uLong>(raw.size()),
                Z_BEST_COMPRESSION) != Z_OK) {
    return false;
  }
  compressed.resize(compressed_size);

  std::vector<unsigned char> png = {0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
  std::vector<unsigned char> ihdr;
  appendBe32(ihdr, static_cast<uint32_t>(width));
  appendBe32(ihdr, static_cast<uint32_t>(height));
  ihdr.push_back(8);
  ihdr.push_back(2);
  ihdr.push_back(0);
  ihdr.push_back(0);
  ihdr.push_back(0);
  appendChunk(png, "IHDR", ihdr);
  appendChunk(png, "IDAT", compressed);
  appendChunk(png, "IEND", {});

  std::FILE* file = std::fopen(path.c_str(), "wb");
  if (file == nullptr) {
    return false;
  }
  const bool ok = std::fwrite(png.data(), 1, png.size(), file) == png.size();
  return std::fclose(file) == 0 && ok;
}

} // namespace glyph
