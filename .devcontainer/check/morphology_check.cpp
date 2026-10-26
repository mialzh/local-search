#include <api.h>
#include <rus.h>
#include <eng.h>
#include <moonycode/codes.h>

#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>

void Check(libmorphGetAPI load, std::string_view word, std::string_view expected) {
    void* raw = nullptr;
    if (load(LIBMORPH_API_4_MAGIC ":1251", &raw) != 0 || raw == nullptr) {
        throw std::runtime_error("Analyzer initialization failed");
    }
    const auto release = [](IMlmaMb* p) { p->Detach(); };
    std::unique_ptr<IMlmaMb, decltype(release)> api(static_cast<IMlmaMb*>(raw), release);

    // Dependency check only: both dictionaries use their native single-byte
    // encoding here. The application's Lemmatizer will expose a UTF-8 interface.
    const auto input = codepages::mbcstombcs(codepages::codepage_1251,
        codepages::codepage_utf8, word.data(), word.size());
    std::array<SLemmInfoA, 64> analyses{};
    std::array<char, 4096> lemmas{};
    const int count = api->Lemmatize(input.data(), input.size(), analyses.data(),
        analyses.size(), lemmas.data(), lemmas.size(), nullptr, 0, sfIgnoreCapitals);
    if (count <= 0) {
        throw std::runtime_error("Lemmatize failed for " + std::string(word));
    }
    for (int i = 0; i < count; ++i) {
        const auto lemma = codepages::mbcstombcs(codepages::codepage_utf8,
            codepages::codepage_1251, analyses[i].plemma);
        if (lemma == expected) {
            std::cout << word << " -> " << lemma << '\n';
            return;
        }
    }
    throw std::runtime_error("Expected lemma missing for " + std::string(word));
}

int main() {
    try {
        Check(mlmaruGetAPI, "документами", "документ");
        Check(mlmaenGetAPI, "documents", "document");
        Check(mlmaenGetAPI, "mice", "mouse");
        std::cout << "Morphology dependencies OK\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
