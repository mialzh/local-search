#include "vector_index.hpp"

#include <unordered_map>
#include <algorithm>
#include <stdexcept>

namespace local_search {
    std::vector<VectorHit> VectorIndex::FindNearest(
        const Embedding& query, std::size_t limit) const {

        std::unordered_map<DocumentId, VectorHit> closest_documents; 

        for (const auto& chunk : chunks_) {
            float dist = EmbeddingDist(chunk.embedding, query);
            DocumentId id = chunk.document_id;
            auto it = closest_documents.find(id);
            if (it == closest_documents.end() ||
                dist < it->second.dist ||
                (dist == it->second.dist && chunk.begin_byte < it->second.begin_byte)) {
                closest_documents.insert_or_assign(
                    id, VectorHit{id, chunk.begin_byte, chunk.end_byte, dist});
            }
        }

        std::vector<VectorHit> result;
        result.reserve(closest_documents.size());
        
        for (auto& [id, vector_hit] : closest_documents) {
            result.push_back(vector_hit);
        }

        std::sort(result.begin(), result.end(),
          [](const VectorHit& a, const VectorHit& b) {
              if (a.dist != b.dist) {
                  return a.dist < b.dist;
              }
              return a.document_id < b.document_id;
          });
        
        if (limit < result.size()) {
            result.resize(limit);
        }

        return result;
    }

void VectorIndex::AddChunks(std::span<const IndexedChunk> chunks) {
    if (chunks.empty()) {
        return;
    }

    const DocumentId id = chunks.front().document_id;
    for (const auto& chunk : chunks) {
        if (chunk.document_id != id) {
            throw std::invalid_argument("Chunks must belong to one document");
        }
    }

    const auto [it, inserted] = indexed_document_ids_.insert(id);
    if (!inserted) {
        throw std::runtime_error("Document already indexed");
    }

    try {
        chunks_.insert(chunks_.end(), chunks.begin(), chunks.end());
    } catch (...) {
        indexed_document_ids_.erase(it);
        throw;
    }
}

void VectorIndex::Add(IndexedChunk chunk) {
    AddChunks(std::span<const IndexedChunk>{&chunk, 1});
}
} //namespace local_search  