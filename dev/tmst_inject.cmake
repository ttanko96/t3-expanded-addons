# TEMPORARY developer shim, until the Expanded's own add-on hook exists
# (docs/API_GAPS.md G11): adds this add-on to the game's build without
# touching any of the Expanded's files.
#
#   cmake -S <expanded> -B <build> -DCMAKE_PROJECT_INCLUDE=<this file> ...
#
# CMAKE_PROJECT_INCLUDE runs after every project() call (the fetched
# dependencies' too): only the top-level project counts. The add-on itself
# is set up by a deferred call, once the Expanded's whole CMakeLists has run
# and psx-runtime exists. Everything it adds comes from ../addon.cmake.

if(NOT CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR OR DEFINED TMST_INJECTED
   OR NOT EXISTS "${CMAKE_SOURCE_DIR}/game.toml")
    return()
endif()
set(TMST_INJECTED 1)
get_filename_component(TMST_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)

function(tmst_setup)
    include("${TMST_DIR}/addon.cmake")
    if(NOT TARGET psx-runtime)
        message(WARNING "TMST: no psx-runtime target in this Expanded build; the add-on is left out.")
        return()
    endif()

    set(_sources "")
    foreach(_s IN LISTS TMST_SOURCES)
        list(APPEND _sources "${TMST_DIR}/${_s}")
    endforeach()
    set(_includes "")
    foreach(_i IN LISTS TMST_INCLUDE_DIRS)
        list(APPEND _includes "${TMST_DIR}/${_i}")
    endforeach()
    target_sources(psx-runtime PRIVATE ${_sources})
    # Only our sources see our include folders and definitions.
    execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" describe --tags --always
        OUTPUT_VARIABLE _rev OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET)
    if(NOT _rev)
        set(_rev "unknown")
    endif()
    include("${TMST_DIR}/src/compat/tmst_features.cmake")
    tmst_detect_features(_features)
    set_source_files_properties(${_sources} PROPERTIES
        INCLUDE_DIRECTORIES "${_includes}"
        COMPILE_DEFINITIONS "TMST_VERSION=\"${TMST_VERSION}\";TMST_UPSTREAM_REV=\"${_rev}\";TMST_TESTED_WITH=\"${TMST_TESTED_WITH}\";${_features}")

    foreach(_w IN LISTS TMST_WRAPS)
        string(REPLACE "|" ";" _parts "${_w}")
        list(GET _parts 0 _sym)
        list(GET _parts 1 _file)
        if(COMMAND tekken3_wrap_symbol)
            # The Expanded's own helper: the same --wrap, emulated on macOS.
            tekken3_wrap_symbol(${_sym} WRAPPER "${TMST_DIR}/${_file}")
        else()
            target_link_options(psx-runtime PRIVATE "-Wl,--wrap=${_sym}")
        endif()
    endforeach()
    if(APPLE AND TMST_WRAPS)
        message(WARNING "TMST: on macOS the Expanded applies its wraps before the add-on's (docs/COMPAT.md).")
    endif()

    # After the Expanded's own POST_BUILD steps, which recreate mods/.
    foreach(_p IN LISTS TMST_PACKAGES)
        add_custom_command(TARGET psx-runtime POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -rf "$<TARGET_FILE_DIR:psx-runtime>/mods/packages/${_p}"
            COMMAND ${CMAKE_COMMAND} -E copy_directory "${TMST_DIR}/mods/${_p}"
                "$<TARGET_FILE_DIR:psx-runtime>/mods/packages/${_p}"
            COMMENT "TMST: staging ${_p}"
            VERBATIM)
    endforeach()
    message(STATUS "TMST ${TMST_VERSION}: ${CMAKE_SOURCE_DIR} at ${_rev}; features: ${_features}")
endfunction()

cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL tmst_setup)
