#pragma once

#include <stdexcept>
#include <string>
#include <utility>

namespace local_search {
class ValidationError : public std::runtime_error {
public:
  ValidationError(std::string code, const std::string &message)
      : std::runtime_error(message), code_(std::move(code)) {}

  const std::string &Code() const noexcept { return code_; }

private:
  std::string code_;
};

class StorageError : public std::runtime_error {
public:
  using std::runtime_error::runtime_error;
};

} // namespace local_search