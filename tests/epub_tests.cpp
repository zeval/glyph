#include "epub.h"

#include <catch2/catch_test_macros.hpp>
#include <zlib.h>

#include <cstdio>
#include <ctime>
#include <fstream>
#include <string>
#include <vector>

namespace {

struct ZipFixtureEntry {
  std::string name;
  std::string data;
  uint32_t local_offset = 0;
  uint32_t crc = 0;
};

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

std::string tempEpubPath() {
  return "/tmp/glyph-epub-test-" + std::to_string(static_cast<long long>(std::time(nullptr))) +
         ".epub";
}

bool writeStoredZip(const std::string& path, std::vector<ZipFixtureEntry> entries) {
  std::vector<uint8_t> zip;

  for (ZipFixtureEntry& entry : entries) {
    entry.local_offset = static_cast<uint32_t>(zip.size());
    entry.crc = crc32(0, reinterpret_cast<const Bytef*>(entry.data.data()), entry.data.size());

    appendLe32(zip, 0x04034b50);
    appendLe16(zip, 20);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe32(zip, entry.crc);
    appendLe32(zip, static_cast<uint32_t>(entry.data.size()));
    appendLe32(zip, static_cast<uint32_t>(entry.data.size()));
    appendLe16(zip, static_cast<uint16_t>(entry.name.size()));
    appendLe16(zip, 0);
    appendBytes(zip, entry.name);
    appendBytes(zip, entry.data);
  }

  const uint32_t central_offset = static_cast<uint32_t>(zip.size());
  for (const ZipFixtureEntry& entry : entries) {
    appendLe32(zip, 0x02014b50);
    appendLe16(zip, 20);
    appendLe16(zip, 20);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe32(zip, entry.crc);
    appendLe32(zip, static_cast<uint32_t>(entry.data.size()));
    appendLe32(zip, static_cast<uint32_t>(entry.data.size()));
    appendLe16(zip, static_cast<uint16_t>(entry.name.size()));
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe16(zip, 0);
    appendLe32(zip, 0);
    appendLe32(zip, entry.local_offset);
    appendBytes(zip, entry.name);
  }
  const uint32_t central_size = static_cast<uint32_t>(zip.size()) - central_offset;

  appendLe32(zip, 0x06054b50);
  appendLe16(zip, 0);
  appendLe16(zip, 0);
  appendLe16(zip, static_cast<uint16_t>(entries.size()));
  appendLe16(zip, static_cast<uint16_t>(entries.size()));
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

bool writeTinyEpub(const std::string& path) {
  return writeStoredZip(
      path,
      {
          {"mimetype", "application/epub+zip"},
          {"META-INF/container.xml",
           "<?xml version=\"1.0\"?>"
           "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
           "<rootfiles><rootfile full-path=\"OPS/package.opf\" "
           "media-type=\"application/oebps-package+xml\"/></rootfiles></container>"},
          {"OPS/package.opf",
           "<?xml version=\"1.0\"?>"
           "<package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
           "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
           "<dc:title>Tiny Book</dc:title>"
           "<dc:creator>Ada Reader</dc:creator>"
           "<dc:language>en</dc:language>"
           "<dc:identifier>tiny-id</dc:identifier>"
           "</metadata>"
           "<manifest>"
           "<item id=\"chap1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "</manifest>"
           "<spine><itemref idref=\"chap1\"/></spine>"
           "</package>"},
          {"OPS/chapter1.xhtml",
           "<?xml version=\"1.0\"?>"
           "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Hidden</title></head>"
           "<body><h1>Chapter One</h1><p>Hello <em>reader</em>.</p>"
           "<p>Second paragraph &amp; more text.</p></body></html>"},
      });
}

} // namespace

TEST_CASE("xhtml extraction keeps readable text") {
  const std::string text =
      glyph::extractXhtmlText("<body><h1>Title</h1><p>Hello <em>reader</em>.</p>"
                              "<ul><li>One</li><li>Two</li></ul></body>");

  REQUIRE(text.find("Title") != std::string::npos);
  REQUIRE(text.find("Hello reader.") != std::string::npos);
  REQUIRE(text.find("- One") != std::string::npos);
  REQUIRE(text.find("- Two") != std::string::npos);
}

TEST_CASE("xhtml extraction ignores declarations and head metadata") {
  const std::string text = glyph::extractXhtmlText(
      "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
      "<!DOCTYPE html>"
      "<html xmlns=\"http://www.w3.org/1999/xhtml\">"
      "<head><title>Glyph Tiny Fixture</title></head>"
      "<body><h1>Start</h1>"
      "<p>This tiny EPUB is generated deterministically for glyph tests.</p>"
      "<p>It has one spine item, one nav entry, and one NCX entry.</p>"
      "</body></html>");

  CHECK(text == "Start\n"
                "This tiny EPUB is generated deterministically for glyph tests.\n"
                "It has one spine item, one nav entry, and one NCX entry.");
}

TEST_CASE("epub document loads metadata, spine, and chapter text") {
  const std::string path = tempEpubPath();
  REQUIRE(writeTinyEpub(path));

  glyph::EpubDocument document;
  REQUIRE(document.open(path));
  REQUIRE(document.book().metadata.title == "Tiny Book");
  REQUIRE(document.book().metadata.authors.size() == 1);
  REQUIRE(document.book().metadata.authors[0] == "Ada Reader");
  REQUIRE(document.book().metadata.language == "en");
  REQUIRE(document.book().spine.size() == 1);
  REQUIRE(document.book().spine[0].href == "OPS/chapter1.xhtml");

  const glyph::EpubTextResult chapter = document.readSpineText(0);
  REQUIRE(chapter.ok);
  REQUIRE(chapter.text.find("Chapter One") != std::string::npos);
  REQUIRE(chapter.text.find("Hello reader.") != std::string::npos);
  REQUIRE(chapter.text.find("Second paragraph & more text.") != std::string::npos);

  std::remove(path.c_str());
}
