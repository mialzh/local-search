#include "bm25.hpp"

#include <cmath>
#include <stdexcept>

namespace local_search {
double Bm25Contribution(std::size_t tf, std::size_t df,
                        std::size_t document_count, std::size_t document_length,
                        double average_document_length, double k1, double b) {
  if (tf == 0) {
    return 0.0;
  }
  if (document_count == 0) {
    throw std::invalid_argument("invalid document_count");
  }
  if (df == 0 || df > document_count) {
    throw std::invalid_argument("invalid df");
  }
  if (average_document_length <= 0 || !std::isfinite(average_document_length)) {
    throw std::invalid_argument("invalid average_document_length");
  }
  if (k1 <= 0 || !std::isfinite(k1)) {
    throw std::invalid_argument("invalid k1");
  }
  if (!std::isfinite(b) || b < 0.0 || b > 1.0) {
    throw std::invalid_argument("invalid b");
  }
  if (tf > document_length) {
    throw std::invalid_argument("invalid tf");
  }
  double document_imortance_fraction = (document_count - df + 0.5) / (df + 0.5);
  double document_importance = std::log1p(document_imortance_fraction);
  double word_imprtance_numenator = tf * (k1 + 1);
  double word_importance_denumenator =
      tf + k1 * (1 - b + b * (document_length / average_document_length));
  double word_importance =
      word_imprtance_numenator / word_importance_denumenator;
  return word_importance * document_importance;
}
} // namespace local_search