find_package(Catch2 3 CONFIG REQUIRED)

add_executable(search_tests
    text_processor_test.cpp
    inverted_index_test.cpp
    bm25_test.cpp
)
target_link_libraries(search_tests PRIVATE
    search_core
    Catch2::Catch2WithMain
)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(search_tests PRIVATE
        -finput-charset=UTF-8 -fexec-charset=UTF-8
    )
elseif(MSVC)
    target_compile_options(search_tests PRIVATE /utf-8)
endif()

include(Catch)
catch_discover_tests(search_tests DISCOVERY_MODE PRE_TEST)

add_executable(inverted_index_allocation_tests
    inverted_index_allocation_test.cpp
)

target_link_libraries(inverted_index_allocation_tests PRIVATE
    search_core
    Catch2::Catch2WithMain
)

catch_discover_tests(
    inverted_index_allocation_tests
    DISCOVERY_MODE PRE_TEST
)
