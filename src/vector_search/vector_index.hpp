#pragma once

#include <vector>
#include <span>
#include <unordered_set>

#include "search/document.hpp"
#include "embedding.hpp"

namespace local_search {

struct IndexedChunk {
    DocumentId document_id;
    std::size_t begin_byte;
    std::size_t end_byte;
    Embedding embedding;
};

struct VectorHit {
    DocumentId document_id;
    std::size_t begin_byte;
    std::size_t end_byte;
    float dist;
};

class VectorIndex {
public:
    void Add(IndexedChunk chunk);
    void AddChunks(std::span<const IndexedChunk> chunks);
    std::vector<VectorHit> FindNearest(
        const Embedding& query, std::size_t limit) const;
private:
    std::vector<IndexedChunk> chunks_;
    std::unordered_set<DocumentId> indexed_document_ids_;
};

} //namespace local_search