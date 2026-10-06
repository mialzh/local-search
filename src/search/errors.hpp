#pragma once

#include <stdexcept>

namespace local_search {
class InvalidQueryError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

class InvalidDOcumentError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace local_search