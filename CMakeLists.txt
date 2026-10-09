cmake_minimum_required(VERSION 3.20)
project(local_search LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(nlohmann_json CONFIG REQUIRED)

include(CTest)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests/unit)
endif()