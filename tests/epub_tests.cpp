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
  static int counter = 0;
  return "/tmp/glyph-epub-test-" + std::to_string(static_cast<long long>(std::time(nullptr))) +
         "-" + std::to_string(counter++) + ".epub";
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

bool writeEpubWithEmptyTitlePage(const std::string& path) {
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
           "<dc:title>Book With Title Page</dc:title>"
           "<dc:creator>Ada Reader</dc:creator>"
           "<dc:language>en</dc:language>"
           "<dc:identifier>titlepage-id</dc:identifier>"
           "</metadata>"
           "<manifest>"
           "<item id=\"title\" href=\"titlepage.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "<item id=\"chap1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "</manifest>"
           "<spine><itemref idref=\"title\"/><itemref idref=\"chap1\"/></spine>"
           "</package>"},
          {"OPS/titlepage.xhtml",
           "<?xml version=\"1.0\"?>"
           "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Title</title></head>"
           "<body><img src=\"cover.jpg\" alt=\"cover\"/></body></html>"},
          {"OPS/chapter1.xhtml",
           "<?xml version=\"1.0\"?>"
           "<html xmlns=\"http://www.w3.org/1999/xhtml\"><head><title>Chapter</title></head>"
           "<body><h1>Chapter One</h1><p>Readable chapter text.</p></body></html>"},
      });
}

bool writeEpubWithMultipleReadableSpineItems(const std::string& path) {
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
           "<dc:title>Multi Chapter Book</dc:title>"
           "<dc:creator>Ada Reader</dc:creator>"
           "<dc:language>en</dc:language>"
           "<dc:identifier>multi-id</dc:identifier>"
           "</metadata>"
           "<manifest>"
           "<item id=\"chap1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "<item id=\"chap2\" href=\"chapter2.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "</manifest>"
           "<spine><itemref idref=\"chap1\"/><itemref idref=\"chap2\"/></spine>"
           "</package>"},
          {"OPS/chapter1.xhtml", "<?xml version=\"1.0\"?>"
                                 "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
                                 "<h1>Chapter One</h1><p>First chapter text.</p></body></html>"},
          {"OPS/chapter2.xhtml", "<?xml version=\"1.0\"?>"
                                 "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
                                 "<h1>Chapter Two</h1><p>Second chapter text.</p></body></html>"},
      });
}

bool writeEpubWithCoverImage(const std::string& path) {
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
           "<dc:title>Covered Book</dc:title>"
           "<dc:creator>Ada Reader</dc:creator>"
           "<dc:language>en</dc:language>"
           "<dc:identifier>cover-id</dc:identifier>"
           "<meta name=\"cover\" content=\"cover-image\"/>"
           "</metadata>"
           "<manifest>"
           "<item id=\"cover-image\" href=\"images/cover.jpg\" media-type=\"image/jpeg\"/>"
           "<item id=\"chap1\" href=\"chapter1.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "</manifest>"
           "<spine><itemref idref=\"chap1\"/></spine>"
           "</package>"},
          {"OPS/images/cover.jpg", "fake-jpeg"},
          {"OPS/chapter1.xhtml", "<?xml version=\"1.0\"?>"
                                 "<html xmlns=\"http://www.w3.org/1999/xhtml\"><body>"
                                 "<h1>Chapter One</h1><p>Readable chapter text.</p></body></html>"},
      });
}

bool writeEpubWithoutContainer(const std::string& path) {
  return writeStoredZip(
      path,
      {
          {"mimetype", "application/epub+zip"},
          {"OPS/package.opf", "<?xml version=\"1.0\"?>"
                              "<package version=\"2.0\" xmlns=\"http://www.idpf.org/2007/opf\">"
                              "<metadata xmlns:dc=\"http://purl.org/dc/elements/1.1/\">"
                              "<dc:title>No Container</dc:title>"
                              "</metadata><manifest/><spine/></package>"},
      });
}

bool writeEpubWithBrokenContainer(const std::string& path) {
  return writeStoredZip(
      path,
      {
          {"mimetype", "application/epub+zip"},
          {"META-INF/container.xml",
           "<?xml version=\"1.0\"?>"
           "<container version=\"1.0\" xmlns=\"urn:oasis:names:tc:opendocument:xmlns:container\">"
           "<rootfiles><rootfile media-type=\"application/oebps-package+xml\"/></rootfiles>"
           "</container>"},
      });
}

bool writeEpubWithInvalidManifestHref(const std::string& path) {
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
           "<dc:title>Bad Href</dc:title>"
           "</metadata>"
           "<manifest>"
           "<item id=\"remote\" href=\"http://example.invalid/chapter.xhtml\" "
           "media-type=\"application/xhtml+xml\"/>"
           "<item id=\"escape\" href=\"../escape.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "</manifest>"
           "<spine><itemref idref=\"remote\"/><itemref idref=\"escape\"/></spine>"
           "</package>"},
      });
}

bool writeEpubWithMissingSpineResource(const std::string& path) {
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
           "<dc:title>Missing Chapter</dc:title>"
           "</metadata>"
           "<manifest>"
           "<item id=\"chap1\" href=\"missing.xhtml\" media-type=\"application/xhtml+xml\"/>"
           "</manifest>"
           "<spine><itemref idref=\"chap1\"/></spine>"
           "</package>"},
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

TEST_CASE("xhtml extraction handles malformed markup without crashing") {
  const std::string text = glyph::extractXhtmlText(
      "<body><h1>Broken chapter<p>Text before <em>unterminated emphasis</body>");

  CHECK(text.find("Broken chapter") != std::string::npos);
  CHECK(text.find("Text before") != std::string::npos);
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

TEST_CASE("epub document skips empty title page spine items") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithEmptyTitlePage(path));

  glyph::EpubDocument document;
  REQUIRE(document.open(path));
  REQUIRE(document.book().spine.size() == 2);

  const glyph::EpubTextResult title_page = document.readSpineText(0);
  REQUIRE_FALSE(title_page.ok);

  const glyph::EpubTextResult chapter = document.readFirstReadableSpineText();
  REQUIRE(chapter.ok);
  CHECK(chapter.spine_index == 1);
  CHECK(chapter.text.find("Chapter One") != std::string::npos);
  CHECK(chapter.text.find("Readable chapter text.") != std::string::npos);

  std::remove(path.c_str());
}

TEST_CASE("epub document combines readable spine items") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithMultipleReadableSpineItems(path));

  glyph::EpubDocument document;
  REQUIRE(document.open(path));

  const glyph::EpubTextResult book_text = document.readAllReadableSpineText();
  REQUIRE(book_text.ok);
  CHECK(book_text.spine_index == 0);
  CHECK(book_text.text.find("Chapter One") != std::string::npos);
  CHECK(book_text.text.find("First chapter text.") != std::string::npos);
  CHECK(book_text.text.find("Chapter Two") != std::string::npos);
  CHECK(book_text.text.find("Second chapter text.") != std::string::npos);

  std::remove(path.c_str());
}

TEST_CASE("epub document resolves cover image resources") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithCoverImage(path));

  glyph::EpubDocument document;
  REQUIRE(document.open(path));
  REQUIRE(document.book().cover_image_path == "OPS/images/cover.jpg");

  const glyph::ZipReadResult cover = document.readResource(document.book().cover_image_path);
  REQUIRE(cover.ok);
  CHECK(glyph::bytesToString(cover.bytes) == "fake-jpeg");

  std::remove(path.c_str());
}

TEST_CASE("epub document rejects missing container file") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithoutContainer(path));

  glyph::EpubDocument document;
  REQUIRE_FALSE(document.open(path));
  CHECK(document.error().find("container.xml") != std::string::npos);

  std::remove(path.c_str());
}

TEST_CASE("epub document rejects container without package path") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithBrokenContainer(path));

  glyph::EpubDocument document;
  REQUIRE_FALSE(document.open(path));
  CHECK(document.error().find("OPF package") != std::string::npos);

  std::remove(path.c_str());
}

TEST_CASE("epub document rejects unsafe or remote manifest hrefs") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithInvalidManifestHref(path));

  glyph::EpubDocument document;
  REQUIRE_FALSE(document.open(path));
  CHECK(document.error().find("spine") != std::string::npos);

  std::remove(path.c_str());
}

TEST_CASE("epub document reports missing spine resources cleanly") {
  const std::string path = tempEpubPath();
  REQUIRE(writeEpubWithMissingSpineResource(path));

  glyph::EpubDocument document;
  REQUIRE(document.open(path));
  REQUIRE(document.book().spine.size() == 1);

  const glyph::EpubTextResult result = document.readSpineText(0);
  REQUIRE_FALSE(result.ok);
  CHECK(result.text.empty());
  CHECK(result.error.find("not found") != std::string::npos);

  std::remove(path.c_str());
}
