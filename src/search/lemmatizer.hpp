#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct IMlmaMb;

namespace local_search {

class Lemmatizer {
public:
    Lemmatizer();

    std::vector<std::string> Lemmatize(
        std::string_view word
    ) const;

private:
    std::shared_ptr<IMlmaMb> russian_;
    std::shared_ptr<IMlmaMb> english_;
};

} // namespace local_search