#pragma once

#include "errors.hpp"

#include <cstddef>
#include <string_view>
#include <string>
#include <vector>
#include <cstdint>
#include <unicode/locid.h>
#include <unicode/normalizer2.h>
#include <unicode/stringpiece.h>
#include <unicode/unistr.h>
#include <unicode/uchar.h>
#include <unicode/utf8.h>

namespace local_search {

struct Occurrence {
  std::size_t position;
  std::size_t begin_byte;
  std::size_t end_byte;
};

struct Token {
  std::string word;
  Occurrence occurrence;
};

class TextProcessor {
public:
   static icu::UnicodeString Normalize(std::string_view text);

    static size_t Split(icu::UnicodeString &text);

    static std::vector<Token> SplitToTokens(std::string_view text);

    static std::vector<Token> Process(std::string_view text) ;
};

} //local_search