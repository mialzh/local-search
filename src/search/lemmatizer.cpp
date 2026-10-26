#include "lemmatizer.hpp"

#include <api.h>              // интерфейс и общие константы
#include <rus.h>              // создание русского анализатора
#include <eng.h>              // создание английского анализатора
#include <moonycode/codes.h>   // преобразование кодировок

#include <algorithm>          // sort, unique
#include <stdexcept>          // исключения


namespace local_search {

    namespace {

std::shared_ptr<IMlmaMb> LoadAnalyzer(libmorphGetAPI get_api) {
    void* raw = nullptr;

    const int error = get_api(
        LIBMORPH_API_4_MAGIC ":1251",
        &raw
    );

    if (error != 0 || raw == nullptr) {
        throw std::runtime_error(
            "Cannot initialize morphological analyzer"
        );
    }

    return std::shared_ptr<IMlmaMb>(
        static_cast<IMlmaMb*>(raw),
        [](IMlmaMb* analyzer) {
            analyzer->Detach();
        }
    );
}

}  // namespace

Lemmatizer::Lemmatizer()
    : russian_(LoadAnalyzer(mlmaruGetAPI)),
      english_(LoadAnalyzer(mlmaenGetAPI)) {}


std::vector<std::string> Lemmatizer::Lemmatize(
    std::string_view word
) const {
    if (word.find('\0') != std::string_view::npos) {
        throw std::invalid_argument("Token contains a zero byte");
    }

    if (word.empty()) {
        return {std::string(word)};
    }

    // Переводим UTF-8 в кодировку анализаторов.
    std::vector<char> encoded(word.size() + 1);

    const auto encoded_size = codepages::mbcstombcs(
        codepages::codepage_1251,
        encoded.data(),
        encoded.size(),
        codepages::codepage_utf8,
        word.data(),
        word.size()
    );

    // Например, китайское слово нельзя представить в Windows-1251.
    if (encoded_size == static_cast<std::size_t>(-1)) {
        return {std::string(word)};
    }

    std::vector<std::string> result;
    std::vector<SLemmInfoA> analyses(8);
    std::vector<char> lemma_buffer(256);

    for (IMlmaMb* analyzer : {russian_.get(), english_.get()}) {
        while (true) {
            const int count = analyzer->Lemmatize(
                encoded.data(), encoded_size,
                analyses.data(), analyses.size(),
                lemma_buffer.data(), lemma_buffer.size(),
                nullptr, 0,
                sfIgnoreCapitals
            );

            // Анализатор не может обработать слово, например слишком длинное.
            if (count == WORDBUFF_FAILED) {
                break;
            }

            // При нехватке места увеличиваем буфер и повторяем вызов.
            if (count == LIDSBUFF_FAILED) {
                analyses.resize(analyses.size() * 2);
                continue;
            }

            if (count == LEMMBUFF_FAILED) {
                lemma_buffer.resize(lemma_buffer.size() * 2);
                continue;
            }

            if (count < 0) {
                throw std::runtime_error(
                    "Morphological analysis failed: " +
                    std::to_string(count)
                );
            }

            for (int i = 0; i < count; ++i) {
                const char* lemma = analyses[i].plemma;

                if (lemma == nullptr || *lemma == '\0') {
                    continue;
                }

                // Словарь может вернуть имя собственное с большой буквы.
                auto lowercase = codepages::strtolower(
                    codepages::codepage_1251, lemma
                );

                result.push_back(codepages::mbcstombcs(
                    codepages::codepage_utf8,
                    codepages::codepage_1251,
                    lowercase.data(),
                    lowercase.size()
                ));
            }

            break;
        }
    }

    std::sort(result.begin(), result.end());
    result.erase(
        std::unique(result.begin(), result.end()),
        result.end()
    );

    if (result.empty()) {
        result.emplace_back(word);
    }

    return result;
}

}  // namespace local_search