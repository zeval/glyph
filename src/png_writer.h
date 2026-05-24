#ifndef GLYPH_PNG_WRITER_H
#define GLYPH_PNG_WRITER_H

#include <string>
#include <vector>

namespace glyph {

bool writeRgbPng(const std::string& path, int width, int height,
                 const std::vector<unsigned char>& pixels);

} // namespace glyph

#endif // GLYPH_PNG_WRITER_H
