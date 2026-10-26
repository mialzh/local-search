#include "text_processor.hpp"
#include "errors.hpp"
#include "lemmatizer.hpp"
#include <limits>
#include <utility>

namespace local_search {


bool IsCombiningMark(UChar32 ch) {
  const auto type = u_charType(ch);

  return type == U_NON_SPACING_MARK || type == U_COMBINING_SPACING_MARK ||
         type == U_ENCLOSING_MARK;
}

icu::UnicodeString TextProcessor::Normalize(std::string_view text){
  if (text.find('\0') != std::string_view::npos) {
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
  auto unicode = icu::UnicodeString::fromUTF8(
    icu::StringPiece(text.data(), length)
  );

  

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

size_t TextProcessor::Split(icu::UnicodeString &text) {
  size_t num_tokens = 0;
  bool inside_token = false;
  for (int32_t i = 0; i < text.length(); i = text.moveIndex32(i, 1)) {
    UChar32 ch = text.char32At(i);

    bool keep = u_isalnum(ch) || (inside_token && IsCombiningMark(ch));
    if (!keep) {
      int32_t next = text.moveIndex32(i, 1);
      text.replace(i, next - i, u' ');
    }
    inside_token = keep;
  }
  std::string pre_split;
  text.toUTF8String(pre_split);
  std::size_t start = 0;

  for (std::size_t i = 0; i <= pre_split.size(); ++i) {
    if (i == pre_split.size() || pre_split[i] == ' ') {
      if (i > start) {
        ++num_tokens;
      }
      start = i + 1;
    }
  }

  return num_tokens;
}

std::vector<Token> TextProcessor::SplitToTokens(std::string_view text) {
  if (text.size() >
      static_cast<std::size_t>(std::numeric_limits<int32_t>::max())) {
    throw ValidationError("INVALID_TEXT", "Text is too large");
  }

  std::vector<Token> tokens;

  auto normalized = Normalize(text);
  tokens.reserve(Split(normalized));
  const int32_t length = static_cast<int32_t>(text.size());

  int32_t offset = 0;
  int32_t start = 0;
  bool inside_word = false;

  auto add_token = [&](int32_t end) {
    std::string original_word(text.substr(start, end - start));
    auto normalized = Normalize(original_word);

    std::string word;
    normalized.toUTF8String(word);

    tokens.push_back(Token{
        std::move(word),
        Occurrence{
            tokens.size(),
            static_cast<std::size_t>(start),
            static_cast<std::size_t>(end)
        }
    });
  };

  while (offset < length) {
    const int32_t begin = offset;

    UChar32 cp;
    U8_NEXT(text.data(), offset, length, cp);

    if (cp < 0) {
      throw ValidationError("INVALID_UTF8", "Invalid UTF-8 sequence");
    }
    if (cp == 0) {
      throw ValidationError("ReadError", "zero char inside");
    }

    const bool suitable =
        u_isalnum(cp) || (inside_word && IsCombiningMark(cp));

    if (suitable) {
      if (!inside_word) {
        start = begin;
        inside_word = true;
      }
    } else if (inside_word) {
      add_token(begin);
      inside_word = false;
    }
  }

  // Последнее слово может заканчиваться вместе со строкой.
  if (inside_word) {
    add_token(length);
  }

  return tokens;
}

std::vector<Token> TextProcessor::Process(std::string_view text) {
    auto tokens = SplitToTokens(text);

    static const Lemmatizer lemmatizer;

    for (auto& token : tokens) {
        token.lemmas = lemmatizer.Lemmatize(token.word);
    }

    return tokens;
}
} // namespace local_search
