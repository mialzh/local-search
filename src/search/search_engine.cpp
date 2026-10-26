#include "search_engine.hpp"
#include "snippet.hpp"

#include <algorithm>
#include <unordered_set>
#include <limits>
#include <stdexcept>
#include <utility>
#include <unordered_map>
#include <set>

namespace local_search {

SearchEngine::SearchEngine(DocumentStore &store)
    : store_(store), next_document_id_(1) {}

void SearchEngine::Restore() {
  InvertedIndex new_index;
  DocumentId max_id = 0;
  for (const Document &document : store_.LoadAll()) {
    std::vector<Token> normalized_text =
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
  std::vector<Token> normalized_text = TextProcessor::Process(text);
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

Document SearchEngine::GetDocument(DocumentId document_id) const {
    return store_.Get(document_id);
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
    struct Candidate {
    double score = 0;
    std::vector<Occurrence> matches;
  };

  std::unordered_map<DocumentId, Candidate> candidates;
  const std::size_t document_count = index_.DocumentCount();
  const double average_document_length = index_.AverageDocumentLength();

  std::set<std::vector<std::string>> query_groups;

  for (const auto& token : normalized_query) {
      auto lemmas = token.lemmas;

      if (lemmas.empty()) {
          lemmas.push_back(token.word);
      }

      // Одинаковые группы должны совпадать независимо от порядка лемм.
      std::sort(lemmas.begin(), lemmas.end());
      lemmas.erase(
          std::unique(lemmas.begin(), lemmas.end()),
          lemmas.end());

      query_groups.insert(std::move(lemmas));
  }

  for (const auto& group : query_groups) {
      // Лучший вклад этой группы для каждого документа.
      std::unordered_map<DocumentId, double> best_scores;

      for (const auto& lemma : group) {
          const auto* postings = index_.Find(lemma);
          if (postings == nullptr) {
              continue;
          }

          const std::size_t document_frequency = postings->size();

          for (const auto& posting : *postings) {
              const double score = Bm25Contribution(
                  posting.occurrences.size(),
                  document_frequency,
                  document_count,
                  index_.GetDocumentLength(posting.document_id),
                  average_document_length);

              auto& best = best_scores[posting.document_id];
              best = std::max(best, score);

              // Собираем совпадения всех подходящих лемм.
              auto& matches = candidates[posting.document_id].matches;
              matches.insert(
                  matches.end(),
                  posting.occurrences.begin(),
                  posting.occurrences.end());
          }
      }

      // Между разными группами вклады складываются.
      for (const auto& [document_id, score] : best_scores) {
          candidates[document_id].score += score;
      }
  }
  // Пока сохраняем только ID и рейтинг.
  std::vector<SearchResult> result;
  result.reserve(candidates.size());

  for (const auto& [document_id, candidate] : candidates) {
    SearchResult item;
    item.document_id = document_id;
    item.score = candidate.score;
    result.push_back(std::move(item));
  }

  std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
    if (a.score != b.score) {
      return a.score > b.score;
    }
    return a.document_id < b.document_id;
  });

  if (result.size() > limit) {
    result.resize(limit);
  }

  // Загружаем документы только для итоговой выдачи.
  for (auto& item : result) {
    Document document = store_.Get(item.document_id);
    item.title = std::move(document.title);

    item.snippet = MakeSnippet(
        document.text,
        std::move(candidates.at(item.document_id).matches));
  }

  return result;
}
} // namespace local_search