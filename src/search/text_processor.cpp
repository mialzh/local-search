#include "text_processor.hpp"
#include "errors.hpp"

namespace local_search {

bool IsCombiningMark(UChar32 ch) {
  const auto type = u_charType(ch);

  return type == U_NON_SPACING_MARK || type == U_COMBINING_SPACING_MARK ||
         type == U_ENCLOSING_MARK;
}

icu::UnicodeString TextProcessor::Normalize(std::string &text) {
  if (text.find('\0') != std::string::npos) {
    throw ValidationError("ReadError", "zero char inside");
  }
  if (text.size() >
      static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
    throw ValidationError("INVALID_TEXT", "Text is too large");
  }

  const int32_t length = static_cast<int32_t>(text.size());

  for (int32_t i = 0; i < length;) {
    UChar32 ch;
    U8_NEXT(text.data(), i, length, ch);

    if (ch < 0) {
      throw ValidationError("INVALID_UTF8", "Invalid UTF-8 sequence");
    }
  }
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
 bool inside_token = false;
  for (int32_t i = 0; i < text.length(); i = text.moveIndex32(i, 1)) {
    UChar32 ch = text.char32At(i);

    bool keep = u_isalnum(ch) ||
                (inside_token && IsCombiningMark(ch));
    if (!keep) {
      int32_t next = text.moveIndex32(i, 1);
      text.replace(i, next - i, u' ');
    }
    inside_token = keep;
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
