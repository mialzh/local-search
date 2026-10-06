#pragma once

#include "errors.hpp"

#include <string>
#include <vector>
#include <cstdint>
#include <unicode/locid.h>
#include <unicode/normalizer2.h>
#include <unicode/stringpiece.h>
#include <unicode/unistr.h>


class TextProcessor {
public:
    icu::UnicodeString Normalize(std::string& text);

    std::vector<std::string> Split(icu::UnicodeString& text);

    std::vector<std::string> Process(std::string_view text) ;
};