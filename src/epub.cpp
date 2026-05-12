#include "epub.h"

#include <algorithm>
#include <cctype>
#include <map>

namespace glyph {

namespace {

struct XmlTag {
  std::string name;
  std::string local_name;
  std::map<std::string, std::string> attrs;
  bool closing = false;
  bool self_closing = false;
  size_t start = 0;
  size_t end = 0;
  size_t content_start = 0;
};

std::string localName(const std::string& name) {
  const size_t colon = name.rfind(':');
  if (colon == std::string::npos) {
    return name;
  }
  return name.substr(colon + 1);
}

std::string toLowerAscii(std::string value) {
  for (char& ch : value) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  return value;
}

bool isNameChar(char ch) {
  return std::isalnum(static_cast<unsigned char>(ch)) != 0 || ch == '_' || ch == '-' || ch == ':' ||
         ch == '.';
}

void skipSpaces(const std::string& text, size_t& pos, size_t limit) {
  while (pos < limit && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
    ++pos;
  }
}

std::string decodeEntities(const std::string& text) {
  std::string out;
  out.reserve(text.size());
  for (size_t i = 0; i < text.size(); ++i) {
    if (text[i] != '&') {
      out.push_back(text[i]);
      continue;
    }

    const size_t semi = text.find(';', i + 1);
    if (semi == std::string::npos || semi - i > 12) {
      out.push_back(text[i]);
      continue;
    }

    const std::string entity = text.substr(i + 1, semi - i - 1);
    if (entity == "amp") {
      out.push_back('&');
    } else if (entity == "lt") {
      out.push_back('<');
    } else if (entity == "gt") {
      out.push_back('>');
    } else if (entity == "quot") {
      out.push_back('"');
    } else if (entity == "apos") {
      out.push_back('\'');
    } else if (entity == "nbsp") {
      out.push_back(' ');
    } else {
      out.append(text, i, semi - i + 1);
    }
    i = semi;
  }
  return out;
}

std::map<std::string, std::string> parseAttributes(const std::string& text, size_t pos,
                                                   size_t limit) {
  std::map<std::string, std::string> attrs;
  while (pos < limit) {
    skipSpaces(text, pos, limit);
    if (pos >= limit || text[pos] == '/') {
      break;
    }

    const size_t key_start = pos;
    while (pos < limit && isNameChar(text[pos])) {
      ++pos;
    }
    if (key_start == pos) {
      ++pos;
      continue;
    }

    std::string key = text.substr(key_start, pos - key_start);
    skipSpaces(text, pos, limit);
    if (pos >= limit || text[pos] != '=') {
      attrs[localName(key)] = "";
      continue;
    }
    ++pos;
    skipSpaces(text, pos, limit);
    if (pos >= limit) {
      attrs[localName(key)] = "";
      break;
    }

    std::string value;
    if (text[pos] == '"' || text[pos] == '\'') {
      const char quote = text[pos++];
      const size_t value_start = pos;
      while (pos < limit && text[pos] != quote) {
        ++pos;
      }
      value = text.substr(value_start, pos - value_start);
      if (pos < limit) {
        ++pos;
      }
    } else {
      const size_t value_start = pos;
      while (pos < limit && std::isspace(static_cast<unsigned char>(text[pos])) == 0 &&
             text[pos] != '/') {
        ++pos;
      }
      value = text.substr(value_start, pos - value_start);
    }
    attrs[localName(key)] = decodeEntities(value);
  }
  return attrs;
}

bool nextTag(const std::string& xml, size_t& pos, XmlTag& tag) {
  while (true) {
    const size_t start = xml.find('<', pos);
    if (start == std::string::npos) {
      return false;
    }
    if (start + 1 >= xml.size()) {
      return false;
    }
    if (xml[start + 1] == '!' || xml[start + 1] == '?') {
      const size_t end = xml.find('>', start + 2);
      if (end == std::string::npos) {
        return false;
      }
      tag = {};
      tag.start = start;
      tag.end = end;
      tag.content_start = end + 1;
      tag.self_closing = true;
      pos = end + 1;
      return true;
    }

    size_t cursor = start + 1;
    tag = {};
    tag.start = start;
    if (xml[cursor] == '/') {
      tag.closing = true;
      ++cursor;
    }
    skipSpaces(xml, cursor, xml.size());

    const size_t name_start = cursor;
    while (cursor < xml.size() && isNameChar(xml[cursor])) {
      ++cursor;
    }
    if (name_start == cursor) {
      pos = start + 1;
      continue;
    }

    bool quoted = false;
    char quote = '\0';
    size_t end = cursor;
    for (; end < xml.size(); ++end) {
      if (quoted) {
        if (xml[end] == quote) {
          quoted = false;
        }
      } else if (xml[end] == '"' || xml[end] == '\'') {
        quoted = true;
        quote = xml[end];
      } else if (xml[end] == '>') {
        break;
      }
    }
    if (end >= xml.size()) {
      return false;
    }

    tag.name = xml.substr(name_start, cursor - name_start);
    tag.local_name = localName(tag.name);
    tag.end = end;
    tag.content_start = end + 1;

    size_t before_end = end;
    while (before_end > cursor &&
           std::isspace(static_cast<unsigned char>(xml[before_end - 1])) != 0) {
      --before_end;
    }
    tag.self_closing = before_end > cursor && xml[before_end - 1] == '/';
    const size_t attr_limit = tag.self_closing ? before_end - 1 : before_end;
    if (!tag.closing) {
      tag.attrs = parseAttributes(xml, cursor, attr_limit);
    }

    pos = end + 1;
    return true;
  }
}

const std::string* attr(const XmlTag& tag, const std::string& key) {
  const auto found = tag.attrs.find(key);
  if (found == tag.attrs.end()) {
    return nullptr;
  }
  return &found->second;
}

std::string stripFragment(std::string href) {
  const size_t hash = href.find('#');
  if (hash != std::string::npos) {
    href.erase(hash);
  }
  const size_t query = href.find('?');
  if (query != std::string::npos) {
    href.erase(query);
  }
  return href;
}

std::string dirnameOf(const std::string& path) {
  const size_t slash = path.rfind('/');
  if (slash == std::string::npos) {
    return "";
  }
  return path.substr(0, slash + 1);
}

std::string resolveZipPath(const std::string& base_path, const std::string& href) {
  std::string path = stripFragment(href);
  if (path.empty() || path.find(':') != std::string::npos || (!path.empty() && path[0] == '/')) {
    return "";
  }

  path = dirnameOf(base_path) + path;
  std::vector<std::string> parts;
  size_t start = 0;
  while (start <= path.size()) {
    const size_t slash = path.find('/', start);
    const std::string part =
        slash == std::string::npos ? path.substr(start) : path.substr(start, slash - start);
    if (part.empty() || part == ".") {
      // skip
    } else if (part == "..") {
      if (parts.empty()) {
        return "";
      }
      parts.pop_back();
    } else {
      parts.push_back(part);
    }
    if (slash == std::string::npos) {
      break;
    }
    start = slash + 1;
  }

  std::string resolved;
  for (const std::string& part : parts) {
    if (!resolved.empty()) {
      resolved.push_back('/');
    }
    resolved += part;
  }
  return resolved;
}

std::string firstTextForTag(const std::string& xml, const std::string& local_name) {
  size_t pos = 0;
  XmlTag tag;
  while (nextTag(xml, pos, tag)) {
    if (tag.closing || tag.self_closing || tag.local_name != local_name) {
      continue;
    }

    const size_t close = xml.find("</", tag.content_start);
    if (close == std::string::npos || close < tag.content_start) {
      return "";
    }
    return extractXhtmlText(xml.substr(tag.content_start, close - tag.content_start));
  }
  return "";
}

bool parseContainer(const std::string& xml, std::string& package_path) {
  size_t pos = 0;
  XmlTag tag;
  while (nextTag(xml, pos, tag)) {
    if (tag.closing || tag.local_name != "rootfile") {
      continue;
    }
    const std::string* full_path = attr(tag, "full-path");
    if (full_path != nullptr && !full_path->empty()) {
      package_path = *full_path;
      return true;
    }
  }
  return false;
}

void parsePackage(const std::string& xml, const std::string& package_path, EpubBook& book) {
  book.package_path = package_path;
  book.metadata.title = firstTextForTag(xml, "title");
  book.metadata.language = firstTextForTag(xml, "language");
  book.metadata.identifier = firstTextForTag(xml, "identifier");

  size_t pos = 0;
  XmlTag tag;
  while (nextTag(xml, pos, tag)) {
    if (tag.closing) {
      continue;
    }
    if (tag.local_name == "creator") {
      const size_t close = xml.find("</", tag.content_start);
      if (close != std::string::npos && close >= tag.content_start) {
        const std::string name =
            extractXhtmlText(xml.substr(tag.content_start, close - tag.content_start));
        if (!name.empty()) {
          book.metadata.authors.push_back(name);
        }
      }
    } else if (tag.local_name == "item") {
      EpubManifestItem item;
      if (const std::string* value = attr(tag, "id")) {
        item.id = *value;
      }
      if (const std::string* value = attr(tag, "href")) {
        item.href = resolveZipPath(package_path, *value);
      }
      if (const std::string* value = attr(tag, "media-type")) {
        item.media_type = *value;
      }
      if (const std::string* value = attr(tag, "properties")) {
        item.properties = *value;
      }
      if (!item.id.empty() && !item.href.empty()) {
        book.manifest.push_back(item);
      }
    }
  }

  pos = 0;
  while (nextTag(xml, pos, tag)) {
    if (tag.closing || tag.local_name != "itemref") {
      continue;
    }
    const std::string* idref = attr(tag, "idref");
    if (idref == nullptr || idref->empty()) {
      continue;
    }

    EpubSpineItem spine;
    spine.idref = *idref;
    if (const std::string* linear = attr(tag, "linear")) {
      spine.linear = toLowerAscii(*linear) != "no";
    }

    const auto manifest_item =
        std::find_if(book.manifest.begin(), book.manifest.end(),
                     [&spine](const EpubManifestItem& item) { return item.id == spine.idref; });
    if (manifest_item == book.manifest.end()) {
      book.warnings.push_back("spine item missing manifest entry: " + spine.idref);
      continue;
    }
    spine.href = manifest_item->href;
    spine.media_type = manifest_item->media_type;
    book.spine.push_back(spine);
  }
}

bool isBlockTag(const std::string& local_name) {
  return local_name == "p" || local_name == "div" || local_name == "section" ||
         local_name == "article" || local_name == "blockquote" || local_name == "li" ||
         local_name == "h1" || local_name == "h2" || local_name == "h3" || local_name == "h4" ||
         local_name == "h5" || local_name == "h6" || local_name == "tr";
}

void trimTrailingSpaces(std::string& out) {
  while (!out.empty() && out.back() == ' ') {
    out.pop_back();
  }
}

void appendNewline(std::string& out) {
  trimTrailingSpaces(out);
  if (!out.empty() && out.back() != '\n') {
    out.push_back('\n');
  }
}

void appendSpace(std::string& out) {
  if (!out.empty() && out.back() != ' ' && out.back() != '\n') {
    out.push_back(' ');
  }
}

std::string trimText(std::string text) {
  while (!text.empty() && std::isspace(static_cast<unsigned char>(text.back())) != 0) {
    text.pop_back();
  }
  size_t start = 0;
  while (start < text.size() && std::isspace(static_cast<unsigned char>(text[start])) != 0) {
    ++start;
  }
  if (start > 0) {
    text.erase(0, start);
  }
  return text;
}

} // namespace

bool EpubDocument::open(const std::string& path) {
  book_ = {};
  error_.clear();
  book_.file_path = path;

  if (!archive_.open(path)) {
    error_ = archive_.error();
    return false;
  }

  const ZipReadResult container = archive_.readFile("META-INF/container.xml");
  if (!container.ok) {
    error_ = "EPUB missing META-INF/container.xml: " + container.error;
    return false;
  }

  std::string package_path;
  if (!parseContainer(bytesToString(container.bytes), package_path)) {
    error_ = "EPUB container.xml does not name an OPF package";
    return false;
  }

  const ZipReadResult package = archive_.readFile(package_path);
  if (!package.ok) {
    error_ = "EPUB package could not be read: " + package.error;
    return false;
  }

  parsePackage(bytesToString(package.bytes), package_path, book_);
  if (book_.spine.empty()) {
    error_ = "EPUB package has no readable spine items";
    return false;
  }
  if (book_.metadata.title.empty()) {
    book_.metadata.title = path;
  }

  return true;
}

const std::string& EpubDocument::error() const {
  return error_;
}

const EpubBook& EpubDocument::book() const {
  return book_;
}

EpubTextResult EpubDocument::readSpineText(size_t spine_index) const {
  EpubTextResult result;
  if (spine_index >= book_.spine.size()) {
    result.error = "spine index is out of range";
    return result;
  }

  const EpubSpineItem& spine = book_.spine[spine_index];
  if (spine.media_type != "application/xhtml+xml" && spine.media_type != "text/html" &&
      spine.media_type != "application/xml") {
    result.error = "spine item is not XHTML: " + spine.href;
    return result;
  }

  const ZipReadResult chapter = archive_.readFile(spine.href);
  if (!chapter.ok) {
    result.error = chapter.error;
    return result;
  }

  result.text = extractXhtmlText(bytesToString(chapter.bytes));
  result.ok = !result.text.empty();
  if (!result.ok) {
    result.error = "spine item did not contain readable text: " + spine.href;
  }
  return result;
}

std::string extractXhtmlText(const std::string& xhtml) {
  std::string out;
  out.reserve(xhtml.size());

  int skip_depth = 0;
  XmlTag tag;
  size_t cursor = 0;
  size_t pos = 0;
  while (nextTag(xhtml, pos, tag)) {
    if (tag.start > cursor && skip_depth == 0) {
      const std::string text = decodeEntities(xhtml.substr(cursor, tag.start - cursor));
      for (char ch : text) {
        if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
          appendSpace(out);
        } else {
          out.push_back(ch);
        }
      }
    }

    const std::string local = toLowerAscii(tag.local_name);
    const bool skip_tag = local == "script" || local == "style" || local == "head";
    if (tag.closing) {
      if (skip_tag && skip_depth > 0) {
        --skip_depth;
      }
      if (skip_depth == 0 && isBlockTag(local)) {
        appendNewline(out);
      }
    } else {
      if (skip_depth == 0 && isBlockTag(local)) {
        appendNewline(out);
        if (local == "li") {
          out += "- ";
        }
      }
      if (skip_depth == 0 && local == "br") {
        appendNewline(out);
      }
      if (skip_tag && !tag.self_closing) {
        ++skip_depth;
      }
      if (tag.self_closing && skip_depth == 0 && isBlockTag(local)) {
        appendNewline(out);
      }
    }
    cursor = tag.end + 1;
  }

  if (cursor < xhtml.size() && skip_depth == 0) {
    const std::string text = decodeEntities(xhtml.substr(cursor));
    for (char ch : text) {
      if (std::isspace(static_cast<unsigned char>(ch)) != 0) {
        appendSpace(out);
      } else {
        out.push_back(ch);
      }
    }
  }

  return trimText(out);
}

} // namespace glyph
