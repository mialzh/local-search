#include <utility>
#include <stdexcept>
#include <limits>

#include "inverted_index.hpp"

namespace local_search {

void InvertedIndex::AddDocument(
    DocumentId id,
    const std::vector<std::string>& words) {
        if (id == 0) {
    throw std::runtime_error("Document id must be positive");
}
if (words.empty()) {
    throw std::runtime_error("Document must contain at least one word");
}

    if (document_lengths_.find(id) != document_lengths_.end()) {
        throw std::runtime_error("Document already indexed");
    }

    std::unordered_map<std::string, std::size_t> frequencies;
    for (const auto& word : words) {
        ++frequencies[word];
    }

    struct Change {
        const std::string* word;
        std::vector<Posting>* postings;
        bool created;
        bool appended;
    };

    std::vector<Change> changes;
    changes.reserve(frequencies.size());

    auto document_it =
        document_lengths_.emplace(id, words.size()).first;

    try {
        for (const auto& [word, frequency] : frequencies) {
            auto [it, created] = postings_.try_emplace(word);

            changes.push_back(
                Change{&word, &it->second, created, false});

            it->second.push_back(Posting{id, frequency});
            changes.back().appended = true;
        }
    } catch (...) {
        for (auto it = changes.rbegin(); it != changes.rend(); ++it) {
            if (it->created) {
                auto posting_it = postings_.find(*it->word);
                postings_.erase(posting_it);
            } else if (it->appended) {
                it->postings->pop_back();
            }
        }

        document_lengths_.erase(document_it);
        throw;
    }

    total_word_count_ += words.size();
}

const std::vector<Posting>* InvertedIndex::Find(std::string_view word) const {
    auto it = postings_.find(std::string(word));
    if (it == postings_.end()) {
        return nullptr;
    }
    return &it->second;
}

std::size_t InvertedIndex::GetDocumentLength(DocumentId id) const {
    auto it = document_lengths_.find(id);
    if (it == document_lengths_.end()) {
        throw std::runtime_error("Invalid id");
    }
    return it->second;
}

std::size_t InvertedIndex::DocumentCount() const {
    return document_lengths_.size();
}

double InvertedIndex::AverageDocumentLength() const {
    if (DocumentCount() == 0) {
        return 0;
    }
    return static_cast<double>(total_word_count_) / static_cast<double>(DocumentCount()); 
}

void InvertedIndex::Swap(InvertedIndex& other) noexcept {
    postings_.swap(other.postings_);
    std::swap(total_word_count_, other.total_word_count_);
    document_lengths_.swap(other.document_lengths_);
}

} //namespace local_search