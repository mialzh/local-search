find_package(ICU REQUIRED COMPONENTS uc)

add_library(search_core STATIC
    search/text_processor.cpp
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

if(BUILD_SERVER)
    add_executable(local_search
        main.cpp
        http/health_handler.cpp
        components/search_component.cpp
        http/api_error.cpp
        http/search_handler.cpp
        http/upload_handler.cpp
    )

    target_link_libraries(local_search PRIVATE
        search_core
        userver::core
    )

    set_target_properties(local_search PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
    )
endif()
