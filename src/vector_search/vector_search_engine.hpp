#include <vector>
#include <string>

#include "embedder.hpp"
#include "embedding.hpp"
#include "storage/document_store.hpp"
#include "search/document.hpp"
#include "vector_index.hpp"
#include "search/search_result.hpp"
#include "vector_search_result.hpp"

namespace local_search {

class VectorSearchEngine {
public:
    VectorSearchEngine(DocumentStore& store, Embedder& embedder);

    void IndexDocument(DocumentId id, std::string_view text);
    void Restore();

    std::vector<VectorSearchResult> Search(
        std::string_view query, std::size_t limit) const;

private:
    DocumentStore& store_;
    Embedder& embedder_;
    VectorIndex index_;

    struct ChunkRange {
        std::size_t begin_byte;
        std::size_t end_byte;
    };

    static std::vector<std::string_view> SplitIntoChunks(
        std::string_view text, std::vector<ChunkRange>& ranges);
};

} //namespace local_search