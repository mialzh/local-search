#include "search_engine.hpp"
#include <algorithm>

namespace local_search {

SearchEngine::SearchEngine(DocumentStore &store)
    : store_(store), next_document_id_(1) {}

void SearchEngine::Restore() {
    //todo
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

std::vector<SearchResult> SearchEngine::Search(std::string query,
                                               std::size_t limit) const {
  std::vector<std::string> normalized_query = TextProcessor::Process(query);
  std::unordered_map<DocumentId, double> document_to_score;
  const std::size_t document_count = index_.DocumentCount();
const double average_document_length = index_.AverageDocumentLength();
  for (auto &word : normalized_query) {
    const std::vector<Posting> *word_postings = index_.Find(word);
    if (word_postings == nullptr) {
      continue;
    }
    for (const auto &posting : *word_postings) {
      const std::size_t term_frequency = posting.frequency;
      const std::size_t document_frequency = word_postings->size();
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
    search_result.snippet = document.text.substr(0, 200);
    result.push_back(search_result);
  }

  std::sort(result.begin(), result.end(),
            [](const auto &a, const auto &b) { return a.score > b.score; });

  if (result.size() > limit) {
    result.resize(limit);
  }
  return result;
}
} // namespace local_search