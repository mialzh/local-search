#include "snippet.hpp"

#include <algorithm>
#include <optional>

namespace local_search {
namespace {

std::string FormatSnippet(std::string_view text,
                          std::size_t begin,
                          std::size_t end) {
  std::string result;
  if (begin > 0) {
    result += "…";
  }
  result.append(text.substr(begin, end - begin));
  if (end < text.size()) {
    result += "…";
  }
  return result;
}

bool IsContinuationByte(char ch) {
  return (static_cast<unsigned char>(ch) & 0xC0) == 0x80;
}

// Advance a byte position by at most limit code points.
// Return the number of code points actually consumed.
std::size_t MoveRight(std::string_view text,
                      std::size_t& position,
                      std::size_t limit) {
  std::size_t moved = 0;
  while (position < text.size() && moved < limit) {
    ++position;
    while (position < text.size() && IsContinuationByte(text[position])) {
      ++position;
    }
    ++moved;
  }
  return moved;
}

std::size_t MoveLeft(std::string_view text,
                     std::size_t& position,
                     std::size_t limit) {
  std::size_t moved = 0;
  while (position > 0 && moved < limit) {
    --position;
    while (position > 0 && IsContinuationByte(text[position])) {
      --position;
    }
    ++moved;
  }
  return moved;
}

// Inspect at most limit code points, even if the byte range is very large.
std::optional<std::size_t> WindowLength(
    std::string_view text,
    std::size_t begin,
    std::size_t end,
    std::size_t limit) {
  std::size_t position = begin;
  const std::size_t length = MoveRight(text.substr(0, end), position, limit);
  if (position != end) {
    return std::nullopt;
  }
  return length;
}

} // namespace

std::string MakeSnippet(
    std::string_view text,
    std::vector<Occurrence> matches,
    std::size_t max_code_points) {
  if (text.empty() || max_code_points == 0) {
    return {};
  }

  if (matches.empty()) {
    std::size_t end = 0;
    MoveRight(text, end, max_code_points);
    return FormatSnippet(text, 0, end);
  }

  std::sort(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
    if (a.begin_byte != b.begin_byte) {
      return a.begin_byte < b.begin_byte;
    }
    return a.end_byte < b.end_byte;
  });

  // A repeated input occurrence must not increase the match count.
  matches.erase(
      std::unique(matches.begin(), matches.end(), [](const auto& a, const auto& b) {
        return a.begin_byte == b.begin_byte && a.end_byte == b.end_byte;
      }),
      matches.end());

  std::size_t left = 0;
  std::size_t best_left = 0;
  std::size_t best_right = 0;
  std::size_t best_count = 0;
  std::size_t best_length = 0;

  for (std::size_t right = 0; right < matches.size(); ++right) {
    std::optional<std::size_t> length;

    while (left <= right) {
      length = WindowLength(text,
                            matches[left].begin_byte,
                            matches[right].end_byte,
                            max_code_points);
      if (length.has_value()) {
        break;
      }
      ++left;
    }

    if (!length.has_value()) {
      continue;
    }

    const std::size_t count = right - left + 1;
    if (count > best_count ||
        (count == best_count && *length < best_length)) {
      best_count = count;
      best_length = *length;
      best_left = left;
      best_right = right;
    }
  }

  // Every matching word is longer than the limit: show the first one's start.
  if (best_count == 0) {
    const std::size_t begin = matches.front().begin_byte;
    std::size_t end = begin;
    MoveRight(text, end, max_code_points);
    return FormatSnippet(text, begin, end);
  }

  std::size_t begin = matches[best_left].begin_byte;
  std::size_t end = matches[best_right].end_byte;
  const std::size_t spare = max_code_points - best_length;

  const std::size_t added_left = MoveLeft(text, begin, spare / 2);
  const std::size_t added_right = MoveRight(text, end, spare - added_left);

  // Near the document's end, move unused context to the left.
  MoveLeft(text, begin, spare - added_left - added_right);

  return FormatSnippet(text, begin, end);
}

} // namespace local_search
