#include "search/inverted_index.hpp"

#include <catch2/catch_test_macros.hpp>

#include <cstddef>
#include <cstdlib>
#include <new>
#include <string>
#include <vector>

// Keep the replacement allocation functions in a separate test executable.
// Failure injection is enabled only around the index operation, never around
// Catch2 assertions, registration, or reporting.
namespace allocation_failure {
struct State {
    bool enabled = false;
    std::size_t remaining = 0;
    bool failed = false;
    std::size_t allocations_after_failure = 0;
};

thread_local State state;

class Scope {
public:
    explicit Scope(std::size_t successful_allocations) noexcept {
        state = State{true, successful_allocations, false, 0};
    }

    ~Scope() noexcept {
        state.enabled = false;
    }

    Scope(const Scope&) = delete;
    Scope& operator=(const Scope&) = delete;
};
} // namespace allocation_failure

void* operator new(std::size_t size) {
    auto& state = allocation_failure::state;
    if (state.enabled) {
        if (state.remaining == 0) {
            if (state.failed) {
                ++state.allocations_after_failure;
            }
            state.failed = true;
            throw std::bad_alloc();
        }
        --state.remaining;
    }

    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void* operator new[](std::size_t size) {
    return ::operator new(size);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

using local_search::DocumentId;
using local_search::InvertedIndex;

namespace {
void RequireSameContents(
    const InvertedIndex& actual,
    const InvertedIndex& expected,
    const std::vector<DocumentId>& document_ids,
    const std::vector<std::string>& words) {
    REQUIRE(actual.DocumentCount() == expected.DocumentCount());
    REQUIRE(actual.AverageDocumentLength() == expected.AverageDocumentLength());

    for (const auto id : document_ids) {
        REQUIRE(actual.GetDocumentLength(id) == expected.GetDocumentLength(id));
    }

    for (const auto& word : words) {
        CAPTURE(word);
        const auto* actual_postings = actual.Find(word);
        const auto* expected_postings = expected.Find(word);
        if (expected_postings == nullptr) {
            REQUIRE(actual_postings == nullptr);
            continue;
        }

        REQUIRE(actual_postings != nullptr);
        REQUIRE(actual_postings->size() == expected_postings->size());
        for (std::size_t i = 0; i < expected_postings->size(); ++i) {
            REQUIRE((*actual_postings)[i].document_id == (*expected_postings)[i].document_id);
            REQUIRE((*actual_postings)[i].frequency == (*expected_postings)[i].frequency);
        }
    }
}
} // namespace

TEST_CASE("InvertedIndex rolls back every allocation failure during AddDocument", "[inverted_index][allocation]") {
    const std::string first(60, 'a');
    const std::string second(60, 'b');
    InvertedIndex baseline;
    baseline.AddDocument(1, {first, first, second});
    baseline.AddDocument(2, {first, "existing"});

    // Mix existing words with many new long words to exercise vector growth,
    // new map nodes, string allocation, and unordered_map rehashing.
    std::vector<std::string> words{first, first, second, "existing"};
    for (std::size_t i = 0; i < 64; ++i) {
        words.push_back("new_" + std::to_string(i) + std::string(40, 'x'));
    }

    InvertedIndex expected = baseline;
    expected.AddDocument(3, words);

    bool reached_success = false;
    std::size_t failures = 0;
    constexpr std::size_t kMaxAllocations = 2000;
    for (std::size_t limit = 0; limit < kMaxAllocations; ++limit) {
        InvertedIndex index = baseline;
        bool threw = false;
        {
            allocation_failure::Scope failure(limit);
            try {
                index.AddDocument(3, words);
            } catch (const std::bad_alloc&) {
                threw = true;
            }
        }

        CAPTURE(limit);
        REQUIRE(allocation_failure::state.allocations_after_failure == 0);
        if (threw) {
            ++failures;
            RequireSameContents(index, baseline, {1, 2}, words);
            REQUIRE_THROWS(index.GetDocumentLength(3));

            // The failed ID must be reusable; rollback must leave the index
            // capable of completing the same addition on a later attempt.
            index.AddDocument(3, words);
            RequireSameContents(index, expected, {1, 2, 3}, words);
        } else {
            RequireSameContents(index, expected, {1, 2, 3}, words);
            reached_success = true;
            break;
        }
    }

    REQUIRE(failures > 0);
    REQUIRE(reached_success);
}

TEST_CASE("InvertedIndex Swap performs no allocations", "[inverted_index][swap][allocation]") {
    const std::string cat(60, 'c');
    const std::string dog(60, 'd');
    InvertedIndex left;
    left.AddDocument(1, {cat, cat});
    InvertedIndex right;
    right.AddDocument(2, {dog});
    right.AddDocument(3, {dog, dog});
    const InvertedIndex old_left = left;
    const InvertedIndex old_right = right;

    {
        allocation_failure::Scope failure(0);
        left.Swap(right);
    }

    REQUIRE_FALSE(allocation_failure::state.failed);
    RequireSameContents(left, old_right, {2, 3}, {cat, dog});
    RequireSameContents(right, old_left, {1}, {cat, dog});
}
