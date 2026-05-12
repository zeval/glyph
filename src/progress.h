#ifndef GLYPH_PROGRESS_H
#define GLYPH_PROGRESS_H

#include <string>
#include <vector>

namespace glyph {

struct BookProgress {
  std::string file_path;
  int reader_scroll = 0;
  int total_lines = 0;
  int lines_per_page = 0;
};

std::string defaultProgressPath();
std::vector<BookProgress> loadProgressFile(const std::string& path);
bool saveBookProgress(const std::string& path, const BookProgress& progress);
BookProgress findBookProgress(const std::vector<BookProgress>& entries,
                              const std::string& file_path);

int progressCurrentPage(const BookProgress& progress);
int progressTotalPages(const BookProgress& progress);
int progressPercent(const BookProgress& progress);
std::string progressSummary(const BookProgress& progress);

} // namespace glyph

#endif // GLYPH_PROGRESS_H
