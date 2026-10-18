#pragma once

#include "text_processor.hpp"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace local_search {

// text is valid UTF-8; matches refer to half-open byte ranges in this text.
// A copy of matches is sorted internally. Pass std::move(matches) to reuse it.
// max_code_points limits source text; truncation markers are added separately.
std::string MakeSnippet(
    std::string_view text,
    std::vector<Occurrence> matches,
    std::size_t max_code_points = 200);

} // namespace local_search
