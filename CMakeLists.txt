cmake_minimum_required(VERSION 3.20)
project(local_search LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(BUILD_SERVER "Build the HTTP server" OFF)

find_package(nlohmann_json CONFIG REQUIRED)

find_package(LocalSearchMorphology CONFIG REQUIRED)

if(BUILD_SERVER)
    find_package(userver COMPONENTS core REQUIRED)
endif()

include(CTest)
add_subdirectory(src)

if(BUILD_TESTING)
    add_subdirectory(tests/unit)
endif()