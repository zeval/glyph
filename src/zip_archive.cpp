#include "zip_archive.h"

#include <zlib.h>

#include <algorithm>
#include <cstdio>
#include <fstream>

namespace glyph {

namespace {

constexpr uint32_t kEndOfCentralDirectorySignature = 0x06054b50;
constexpr uint32_t kCentralDirectorySignature = 0x02014b50;
constexpr uint32_t kLocalFileHeaderSignature = 0x04034b50;
constexpr uint16_t kStoredMethod = 0;
constexpr uint16_t kDeflatedMethod = 8;

uint16_t readLe16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) | (static_cast<uint16_t>(data[1]) << 8);
}

uint32_t readLe32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

bool hasRange(size_t size, size_t offset, size_t length) {
  return offset <= size && length <= size - offset;
}

} // namespace

bool ZipArchive::open(const std::string& path) {
  data_.clear();
  entries_.clear();
  error_.clear();

  std::ifstream file(path, std::ios::binary);
  if (!file) {
    error_ = "could not open ZIP file: " + path;
    return false;
  }

  file.seekg(0, std::ios::end);
  const std::streamoff size = file.tellg();
  if (size < 0) {
    error_ = "could not determine ZIP file size";
    return false;
  }
  file.seekg(0, std::ios::beg);

  data_.resize(static_cast<size_t>(size));
  if (!data_.empty()) {
    file.read(reinterpret_cast<char*>(data_.data()), static_cast<std::streamsize>(data_.size()));
  }
  if (!file && !data_.empty()) {
    error_ = "could not read ZIP file";
    return false;
  }

  return parseCentralDirectory();
}

const std::string& ZipArchive::error() const {
  return error_;
}

const std::vector<ZipEntry>& ZipArchive::entries() const {
  return entries_;
}

bool ZipArchive::hasFile(const std::string& name) const {
  return findEntry(name) != nullptr;
}

ZipReadResult ZipArchive::readFile(const std::string& name) const {
  ZipReadResult result;
  const ZipEntry* entry = findEntry(name);
  if (entry == nullptr) {
    result.error = "ZIP entry not found: " + name;
    return result;
  }

  const size_t local = entry->local_header_offset;
  if (!hasRange(data_.size(), local, 30) ||
      readLe32(data_.data() + local) != kLocalFileHeaderSignature) {
    result.error = "invalid ZIP local header for: " + name;
    return result;
  }

  const uint16_t filename_len = readLe16(data_.data() + local + 26);
  const uint16_t extra_len = readLe16(data_.data() + local + 28);
  const size_t payload_offset = local + 30u + filename_len + extra_len;
  if (!hasRange(data_.size(), payload_offset, entry->compressed_size)) {
    result.error = "ZIP entry payload is out of bounds: " + name;
    return result;
  }

  const uint8_t* payload = data_.data() + payload_offset;
  if (entry->method == kStoredMethod) {
    result.bytes.assign(payload, payload + entry->compressed_size);
    result.ok = true;
    return result;
  }

  if (entry->method != kDeflatedMethod) {
    result.error = "unsupported ZIP compression method for: " + name;
    return result;
  }

  result.bytes.resize(entry->uncompressed_size);
  z_stream stream = {};
  stream.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(payload));
  stream.avail_in = entry->compressed_size;
  stream.next_out = reinterpret_cast<Bytef*>(result.bytes.data());
  stream.avail_out = entry->uncompressed_size;

  int z_result = inflateInit2(&stream, -MAX_WBITS);
  if (z_result != Z_OK) {
    result.error = "could not initialize deflate stream";
    result.bytes.clear();
    return result;
  }

  z_result = inflate(&stream, Z_FINISH);
  inflateEnd(&stream);
  if (z_result != Z_STREAM_END || stream.total_out != entry->uncompressed_size) {
    result.error = "could not inflate ZIP entry: " + name;
    result.bytes.clear();
    return result;
  }

  result.ok = true;
  return result;
}

const ZipEntry* ZipArchive::findEntry(const std::string& name) const {
  const auto found = std::find_if(entries_.begin(), entries_.end(),
                                  [&name](const ZipEntry& entry) { return entry.name == name; });
  if (found == entries_.end()) {
    return nullptr;
  }
  return &(*found);
}

bool ZipArchive::parseCentralDirectory() {
  if (data_.size() < 22) {
    error_ = "file is too small to be a ZIP archive";
    return false;
  }

  const size_t min_pos = data_.size() > 22u + 65535u ? data_.size() - (22u + 65535u) : 0u;
  size_t eocd_pos = data_.size();
  for (size_t pos = data_.size() - 22u;; --pos) {
    if (readLe32(data_.data() + pos) == kEndOfCentralDirectorySignature) {
      eocd_pos = pos;
      break;
    }
    if (pos == min_pos) {
      break;
    }
  }

  if (eocd_pos == data_.size()) {
    error_ = "ZIP end of central directory not found";
    return false;
  }

  const uint16_t disk_number = readLe16(data_.data() + eocd_pos + 4);
  const uint16_t central_disk = readLe16(data_.data() + eocd_pos + 6);
  if (disk_number != 0 || central_disk != 0) {
    error_ = "multi-disk ZIP archives are not supported";
    return false;
  }

  const uint16_t entry_count = readLe16(data_.data() + eocd_pos + 10);
  const uint32_t central_size = readLe32(data_.data() + eocd_pos + 12);
  const uint32_t central_offset = readLe32(data_.data() + eocd_pos + 16);
  if (!hasRange(data_.size(), central_offset, central_size)) {
    error_ = "ZIP central directory is out of bounds";
    return false;
  }

  size_t pos = central_offset;
  entries_.reserve(entry_count);
  for (uint16_t i = 0; i < entry_count; ++i) {
    if (!hasRange(data_.size(), pos, 46) ||
        readLe32(data_.data() + pos) != kCentralDirectorySignature) {
      error_ = "invalid ZIP central directory entry";
      return false;
    }

    ZipEntry entry;
    entry.method = readLe16(data_.data() + pos + 10);
    entry.compressed_size = readLe32(data_.data() + pos + 20);
    entry.uncompressed_size = readLe32(data_.data() + pos + 24);
    const uint16_t filename_len = readLe16(data_.data() + pos + 28);
    const uint16_t extra_len = readLe16(data_.data() + pos + 30);
    const uint16_t comment_len = readLe16(data_.data() + pos + 32);
    entry.local_header_offset = readLe32(data_.data() + pos + 42);

    const size_t name_offset = pos + 46u;
    if (!hasRange(data_.size(), name_offset, filename_len)) {
      error_ = "ZIP central directory filename is out of bounds";
      return false;
    }
    entry.name.assign(reinterpret_cast<const char*>(data_.data() + name_offset), filename_len);
    entries_.push_back(entry);

    const size_t next = name_offset + filename_len + extra_len + comment_len;
    if (next < pos || next > data_.size()) {
      error_ = "ZIP central directory entry is out of bounds";
      return false;
    }
    pos = next;
  }

  return true;
}

std::string bytesToString(const std::vector<uint8_t>& bytes) {
  if (bytes.empty()) {
    return "";
  }
  return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

} // namespace glyph
