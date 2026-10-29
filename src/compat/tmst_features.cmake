# Build-time feature detection: which parts of the Expanded's plugin API this
# release has. Each found item becomes a definition TMST_HAS_<NAME>=1 for the
# add-on's sources only; src/compat/ turns a missing one into a mod that
# switches itself off instead of a build error. Names, not version numbers.

function(tmst_detect_features out)
    set(_header "${CMAKE_SOURCE_DIR}/psxrecomp/runtime/include/mod_plugins.h")
    set(_defs "")
    if(EXISTS "${_header}")
        file(READ "${_header}" _api)
        foreach(_pair
                "ACTIVATION_PLUGIN|psx_mod_register_activation_plugin"
                "VBLANK_PLUGIN|psx_mod_register_vblank_plugin"
                "GUEST_MEMORY|psx_mod_alloc_guest_memory"
                "HIGH_VRAM_UPLOAD|psx_mod_gpu_high_vram_upload"
                "VRAM_WRITE_OBSERVER|psx_mod_add_vram_write_observer")
            string(REPLACE "|" ";" _p "${_pair}")
            list(GET _p 0 _name)
            list(GET _p 1 _sym)
            string(FIND "${_api}" "${_sym}(" _at)
            if(NOT _at EQUAL -1)
                list(APPEND _defs "TMST_HAS_${_name}=1")
            endif()
        endforeach()
    endif()
    if(EXISTS "${CMAKE_SOURCE_DIR}/src/tekken3_game_options.h")
        file(READ "${CMAKE_SOURCE_DIR}/src/tekken3_game_options.h" _opts)
        string(FIND "${_opts}" "tekken3_game_options_add(" _at)
        if(NOT _at EQUAL -1)
            list(APPEND _defs "TMST_HAS_GAME_OPTIONS=1")
        endif()
    endif()
    set(${out} "${_defs}" PARENT_SCOPE)
endfunction()
