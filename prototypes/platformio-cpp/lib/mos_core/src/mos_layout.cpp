#include "mos_layout.h"

namespace mos {
std::vector<std::string> wrapText(const char* text, int cols) {
  std::vector<std::string> lines;
  if (!text || cols < 1) return lines;
  std::string cur;
  const char* p = text;
  while (*p) {
    while (*p == ' ') ++p;
    if (!*p) break;
    const char* start = p;
    while (*p && *p != ' ') ++p;
    std::string word(start, p - start);
    while (static_cast<int>(word.size()) > cols) {  // split overlong words
      if (!cur.empty()) { lines.push_back(cur); cur.clear(); }
      lines.push_back(word.substr(0, cols));
      word.erase(0, cols);
    }
    if (cur.empty()) cur = word;
    else if (static_cast<int>(cur.size() + 1 + word.size()) <= cols) cur += " " + word;
    else { lines.push_back(cur); cur = word; }
  }
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}
}  // namespace mos
