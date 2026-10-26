cmake_minimum_required(VERSION 3.20)
project(local_search_morphology_install LANGUAGES C CXX)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 4.0)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()
if(NOT EXISTS "${LIBMORPH_SOURCE_DIR}/api.h" OR
   NOT EXISTS "${MOONYCODE_SOURCE_DIR}/codes.h")
    message(FATAL_ERROR "Set LIBMORPH_SOURCE_DIR and MOONYCODE_SOURCE_DIR")
endif()

set(DEVELOPER_MODE OFF)
set(BUILD_TESTS OFF)
set(MORPHRUS_SOURCE_DIR "${LIBMORPH_SOURCE_DIR}/lang-rus")
set(MORPHENG_SOURCE_DIR "${LIBMORPH_SOURCE_DIR}/lang-eng")
set(MoonyCode_LIB moonycode-objset)
get_filename_component(moonycode_parent "${MOONYCODE_SOURCE_DIR}" DIRECTORY)

include_directories(SYSTEM
    "${LIBMORPH_SOURCE_DIR}"
    "${LIBMORPH_SOURCE_DIR}/contrib"
    "${moonycode_parent}"
)

# Build the dictionary modules directly; upstream's top-level project also
# enables Python bindings and probabilistic analyzers that this project omits.
add_subdirectory("${MOONYCODE_SOURCE_DIR}" moonycode-build EXCLUDE_FROM_ALL)
add_subdirectory("${LIBMORPH_SOURCE_DIR}/contrib/mtc" mtc-build EXCLUDE_FROM_ALL)
add_executable(libmorph-tfc
    "${LIBMORPH_SOURCE_DIR}/utils/tfc/ftable.cpp"
    "${LIBMORPH_SOURCE_DIR}/utils/tfc/gramap.cpp"
    "${LIBMORPH_SOURCE_DIR}/utils/tfc/tfc.cpp"
)
target_link_libraries(libmorph-tfc PRIVATE moonycode-objset)
add_subdirectory("${MORPHRUS_SOURCE_DIR}/libdict" russian-build EXCLUDE_FROM_ALL)
add_subdirectory("${MORPHENG_SOURCE_DIR}/libdict" english-build EXCLUDE_FROM_ALL)
set_target_properties(morphrus-objset morpheng-objset PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
# Avoid exporting upstream's unused compatibility definition into the server.
target_compile_definitions(morphrus-objset PRIVATE
    __stack_chk_fail=local_search_libmorph_unused_stack_chk_fail)

install(FILES
    "$<TARGET_FILE:morphrus-static>"
    "$<TARGET_FILE:morpheng-static>"
    "$<TARGET_FILE:moonycode-static>"
    DESTINATION lib COMPONENT LocalSearchMorphology)
install(FILES "${LIBMORPH_SOURCE_DIR}/api.h" "${LIBMORPH_SOURCE_DIR}/api.hpp"
    "${LIBMORPH_SOURCE_DIR}/rus.h" "${LIBMORPH_SOURCE_DIR}/eng.h"
    DESTINATION include/libmorph COMPONENT LocalSearchMorphology)
install(FILES "${MOONYCODE_SOURCE_DIR}/codes.h" "${MOONYCODE_SOURCE_DIR}/chartype.h"
    DESTINATION include/moonycode COMPONENT LocalSearchMorphology)
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/LocalSearchMorphologyConfig.cmake"
    DESTINATION lib/cmake/LocalSearchMorphology COMPONENT LocalSearchMorphology)
install(FILES "${LIBMORPH_SOURCE_DIR}/LICENSE"
    DESTINATION share/licenses/local-search/libmorph COMPONENT LocalSearchMorphology)
install(FILES "${MOONYCODE_SOURCE_DIR}/LICENSE"
    DESTINATION share/licenses/local-search/moonycode COMPONENT LocalSearchMorphology)
install(FILES "${LIBMORPH_SOURCE_DIR}/contrib/mtc/LICENSE"
    DESTINATION share/licenses/local-search/mtc COMPONENT LocalSearchMorphology)
