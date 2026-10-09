#pragma once

#include <cstdint>

namespace local_search {
double Bm25Contribution(std::size_t tf, std::size_t df,
                        std::size_t document_count, std::size_t document_length,
                        double average_document_length, double k1 = 1.2,
                        double b = 0.75);
} // namespace local_search