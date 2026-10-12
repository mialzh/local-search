#include "search_engine.hpp"
#include <algorithm>
#include <unordered_set>

namespace local_search {

SearchEngine::SearchEngine(DocumentStore &store)
    : store_(store), next_document_id_(1) {}

void SearchEngine::Restore() {
  InvertedIndex new_index;
  DocumentId max_id = 0;
  for (const Document &document : store_.LoadAll()) {
    std::vector<std::string> normalized_text =
        TextProcessor::Process(document.text);
    new_index.AddDocument(document.id, normalized_text);
    max_id = std::max(max_id, document.id);
  }
  if (max_id == std::numeric_limits<DocumentId>::max()) {
    throw std::overflow_error("Cannot issue the next document ID");
  }
  index_.Swap(new_index);
  next_document_id_ = max_id + 1;
}

AddedDocument SearchEngine::AddDocument(std::string title, std::string text) {
  std::vector<std::string> normalized_text = TextProcessor::Process(text);
  AddedDocument result;
  result.document_id = next_document_id_;
  result.title = title;
  result.word_count = normalized_text.size();

  Document document(next_document_id_, title, text);

  store_.Save(document);

  try {
    index_.AddDocument(next_document_id_, normalized_text);
  } catch (...) {
    store_.Remove(next_document_id_);
    throw;
  }

  ++next_document_id_;
  return result;
}

std::string MakeSnippet(std::string_view text) {
  std::size_t position = 0;
  std::size_t count = 0;

  while (position < text.size() && count < 200) {
    ++position;
    while (position < text.size() &&
           (static_cast<unsigned char>(text[position]) & 0xC0) == 0x80) {
      ++position;
    }

    ++count;
  }

  return std::string(text.substr(0, position));
}

std::vector<SearchResult> SearchEngine::Search(std::string query,
                                               std::size_t limit) const {

  if (limit == 0 || limit > 100) {
    throw std::invalid_argument("Limit must be between 1 and 100");
  }

  auto normalized_query = TextProcessor::Process(query);

  if (normalized_query.empty()) {
    throw std::invalid_argument("Query contains no words");
  }
  std::unordered_map<DocumentId, double> document_to_score;
  const std::size_t document_count = index_.DocumentCount();
  const double average_document_length = index_.AverageDocumentLength();
  const std::unordered_set<std::string> query_words(normalized_query.begin(),
                                                    normalized_query.end());
  for (auto &word : query_words) {
    const std::vector<Posting> *word_postings = index_.Find(word);
    if (word_postings == nullptr) {
      continue;
    }
    const std::size_t document_frequency = word_postings->size();
    for (const auto &posting : *word_postings) {
      const std::size_t term_frequency = posting.frequency;
      const std::size_t document_length =
          index_.GetDocumentLength(posting.document_id);
      const double bm25 =
          Bm25Contribution(term_frequency, document_frequency, document_count,
                           document_length, average_document_length);
      document_to_score[posting.document_id] += bm25;
    }
  }
  std::vector<SearchResult> result;
  result.reserve(document_to_score.size());
  for (const auto &[document_id, score] : document_to_score) {
    SearchResult search_result;
    Document document = store_.Get(document_id);
    search_result.document_id = document_id;
    search_result.score = score;
    search_result.title = document.title;
    search_result.snippet = MakeSnippet(document.text);
    result.push_back(search_result);
  }

  std::sort(result.begin(), result.end(), [](const auto &a, const auto &b) {
    if (a.score != b.score) {
      return a.score > b.score;
    }
    return a.document_id < b.document_id;
  });

  if (result.size() > limit) {
    result.resize(limit);
  }
  return result;
}
} // namespace local_search