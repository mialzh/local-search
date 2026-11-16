#include <array>
#include <cmath>

namespace local_search {

constexpr std::size_t kVectorSize = 384;
using Embedding = std::array<float, kVectorSize>;

float EmbeddingDist(const Embedding& first, const Embedding& second)  {
    float result = 0.0f;
    for (std::size_t i = 0; i < kVectorSize; ++i) {
        result += (first[i] - second[i]) * (first[i] - second[i]);
    }
    result = std::sqrt(result);
    return result;
}

} //namespace local_search