#include "text_processor.hpp"

namespace local_search {

icu::UnicodeString TextProcessor::Normalize(std::string &text) {
  UErrorCode status = U_ZERO_ERROR;

  const auto *nfc = icu::Normalizer2::getNFCInstance(status);
  if (U_FAILURE(status)) {
    throw std::runtime_error("Cannot initialize NFC normalizer");
  }
  auto unicode = icu::UnicodeString::fromUTF8(text);

  auto normalized = nfc->normalize(unicode, status);
  if (U_FAILURE(status)) {
    throw std::runtime_error("NFC normalization failed");
  }
  normalized.toLower(icu::Locale::getRoot());
  auto result = nfc->normalize(normalized, status);
  if (U_FAILURE(status)) {
    throw std::runtime_error("NFC normalization failed");
  }
  return result;
}

std::vector<std::string> TextProcessor::Split(icu::UnicodeString &text) {
  for (int32_t i = 0; i < text.length(); i = text.moveIndex32(i, 1)) {
    UChar32 ch = text.char32At(i);

    bool letter_or_digit = u_isalnum(ch);
    if (!letter_or_digit) {
      int32_t next = text.moveIndex32(i, 1);
      text.replace(i, next - i, u' ');
    }
  }
  std::string pre_split;
  text.toUTF8String(pre_split);
  std::vector<std::string> tokens;
  std::size_t start = 0;

  for (std::size_t i = 0; i <= pre_split.size(); ++i) {
    if (i == pre_split.size() || pre_split[i] == ' ') {
      if (i > start) {
        tokens.emplace_back(pre_split, start, i - start);
      }
      start = i + 1;
    }
  }

  return tokens;
}

std::vector<std::string> TextProcessor::Process(std::string_view text) {
    std::string str(text);
    icu::UnicodeString normalized = Normalize(str);
    return Split(normalized);
}
} // namespace local_search
