#include "vector_search_engine.hpp"
#include "search/snippet.hpp"

#include <stdexcept>
#include <utility>

namespace local_search {
    

VectorSearchEngine::VectorSearchEngine(DocumentStore& store, Embedder& embedder) 
    : store_(store), embedder_(embedder) {}

std::vector<std::string_view> VectorSearchEngine::SplitIntoChunks(
    std::string_view text, std::vector<ChunkRange>& ranges) {
    constexpr std::size_t kMaxCodePoints = 800;
    constexpr std::size_t kMinPreferredCodePoints = 600;

    std::vector<std::string_view> passages;
    std::vector<ChunkRange> new_ranges;
    std::size_t begin = 0;

    while (begin < text.size()) {
        std::size_t end = begin;
        std::size_t paragraph_end = begin;
        std::size_t sentence_end = begin;
        std::size_t word_end = begin;

        for (std::size_t count = 1;
             count <= kMaxCodePoints && end < text.size(); ++count) {
            const std::size_t char_begin = end;
            const char ch = text[end++];

            // Пропускаем байты-продолжения текущего символа UTF-8.
            while (end < text.size() &&
                   (static_cast<unsigned char>(text[end]) & 0xC0) == 0x80) {
                ++end;
            }

            if (count < kMinPreferredCodePoints ||
                (ch != ' ' && ch != '\t' && ch != '\n')) {
                continue;
            }

            word_end = end;
            const auto prefix = text.substr(begin, end - begin);
            if (prefix.ends_with("\n\n") || prefix.ends_with("\r\n\r\n")) {
                paragraph_end = end;
            }

            const char previous = text[char_begin - 1];
            if (previous == '.' || previous == '!' || previous == '?') {
                sentence_end = end;
            }
        }

        if (end < text.size()) {
            if (paragraph_end > begin) {
                end = paragraph_end;
            } else if (sentence_end > begin) {
                end = sentence_end;
            } else if (word_end > begin) {
                end = word_end;
            }
        }

        passages.push_back(text.substr(begin, end - begin));
        new_ranges.push_back({begin, end});
        begin = end;
    }

    ranges.swap(new_ranges);
    return passages;
}


void VectorSearchEngine::IndexDocument(DocumentId id, std::string_view text) {
    std::vector<ChunkRange> ranges;
    const auto passages = SplitIntoChunks(text, ranges);
    const auto embeddings = embedder_.EmbedPassages(passages);
    std::vector<IndexedChunk> chunks;

    if (embeddings.size() != ranges.size()) {
        throw std::runtime_error("EmbedPassages returned an unexpected number of embeddings");
    }
    chunks.reserve(ranges.size());

    for (std::size_t i = 0; i < ranges.size(); ++i) {
        auto& [begin, end] = ranges[i];
        chunks.emplace_back(id, begin, end, embeddings[i]);
    }
    index_.AddChunks(chunks);
}

void VectorSearchEngine::Restore() {
    VectorSearchEngine restored(store_, embedder_);

    for (const auto& document : store_.LoadAll()) {
        restored.IndexDocument(document.id, document.text);
    }

    index_ = std::move(restored.index_);
}

std::vector<VectorSearchResult> VectorSearchEngine::Search(
    std::string_view query, std::size_t limit) const {
    if (limit == 0) {
        return {};
    }

    const auto query_embedding = embedder_.EmbedQuery(query);
    const auto hits = index_.FindNearest(query_embedding, limit);

    std::vector<VectorSearchResult> results;
    results.reserve(hits.size());

    for (const auto& hit : hits) {
        auto document = store_.Get(hit.document_id);

        if (hit.begin_byte > hit.end_byte ||
            hit.end_byte > document.text.size()) {
            throw std::logic_error("Indexed chunk is outside the document");
        }

        const auto chunk = std::string_view{document.text}.substr(
            hit.begin_byte, hit.end_byte - hit.begin_byte);

        results.push_back(VectorSearchResult{
            .document_id = hit.document_id,
            .title = std::move(document.title),
            .distance = hit.dist,
            .snippet = MakeSnippet(chunk, {}),
        });
    }

    return results;
}

} //namespace local_search