include_guard(GLOBAL)
get_filename_component(_lsm_prefix "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)

foreach(_lsm_pair "Russian;morphrus" "English;morpheng" "Encoding;moonycode")
    list(GET _lsm_pair 0 _lsm_target)
    list(GET _lsm_pair 1 _lsm_library)
    add_library(LocalSearchMorphology::${_lsm_target} STATIC IMPORTED GLOBAL)
    set_target_properties(LocalSearchMorphology::${_lsm_target} PROPERTIES
        IMPORTED_LOCATION "${_lsm_prefix}/lib/lib${_lsm_library}.a"
        INTERFACE_INCLUDE_DIRECTORIES "${_lsm_prefix}/include;${_lsm_prefix}/include/libmorph")
endforeach()
add_library(LocalSearchMorphology::Morphology INTERFACE IMPORTED GLOBAL)
set_target_properties(LocalSearchMorphology::Morphology PROPERTIES
    INTERFACE_LINK_LIBRARIES "LocalSearchMorphology::Russian;LocalSearchMorphology::English;LocalSearchMorphology::Encoding")
unset(_lsm_prefix)
unset(_lsm_pair)
unset(_lsm_target)
unset(_lsm_library)
