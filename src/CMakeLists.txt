find_package(ICU REQUIRED COMPONENTS uc)

add_library(search_core STATIC
    search/text_processor.cpp
    search/inverted_index.cpp
    search/bm25.cpp
    storage/document_store.cpp
)
target_include_directories(search_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(search_core PUBLIC ICU::uc)
target_link_libraries(search_core PRIVATE nlohmann_json::nlohmann_json)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(search_core PRIVATE -Wall -Wextra -Wpedantic)
elseif(MSVC)
    target_compile_options(search_core PRIVATE /W4 /utf-8)
endif()
