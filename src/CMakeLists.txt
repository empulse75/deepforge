# Layer targets. Dependency direction is enforced by what each target links:
#   core <- content <- world <- domain <- app <- presentation/platform <- game
# See docs/ARCHITECTURE.md.

function(deepforge_layer name)
    file(GLOB_RECURSE _sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${name}/*.cpp")
    file(GLOB_RECURSE _headers CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/${name}/*.hpp")
    if(_sources)
        add_library(deepforge_${name} STATIC ${_sources} ${_headers})
    else()
        add_library(deepforge_${name} INTERFACE)
    endif()
endfunction()

# --- core -------------------------------------------------------------------
deepforge_layer(core)
target_include_directories(deepforge_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_BINARY_DIR}/generated")
# core/JsonFile.hpp wraps the JSON parser used at every load boundary.
target_link_libraries(deepforge_core PUBLIC deepforge_json_headers)
deepforge_set_warnings(deepforge_core)

# --- content ----------------------------------------------------------------
deepforge_layer(content)
target_link_libraries(deepforge_content PUBLIC deepforge_core PRIVATE deepforge_json_headers)
deepforge_set_warnings(deepforge_content)

# --- store (monetisation ingest; sits above the simulation closure, ADR 10) --
# core <- content <- world <- domain never reference the store; this layer
# parses it and is read only by presentation, tools and tests.
deepforge_layer(store)
target_link_libraries(deepforge_store PUBLIC deepforge_core PRIVATE deepforge_json_headers)
deepforge_set_warnings(deepforge_store)

# --- world ------------------------------------------------------------------
deepforge_layer(world)
target_link_libraries(deepforge_world PUBLIC deepforge_core deepforge_content)
deepforge_set_warnings(deepforge_world)

# --- domain -----------------------------------------------------------------
deepforge_layer(domain)
target_link_libraries(deepforge_domain PUBLIC deepforge_core deepforge_content deepforge_world)
deepforge_set_warnings(deepforge_domain)

# --- app (orchestration, save coordination; still headless) -----------------
deepforge_layer(app)
target_link_libraries(deepforge_app PUBLIC deepforge_domain PRIVATE deepforge_json_headers)
deepforge_set_warnings(deepforge_app)

if(DEEPFORGE_BUILD_CLIENT)
    # --- platform (SDL) -----------------------------------------------------
    deepforge_layer(platform)
    target_link_libraries(deepforge_platform PUBLIC deepforge_core PRIVATE SDL3::SDL3 deepforge_json_headers)
    deepforge_set_warnings(deepforge_platform)

    # --- presentation -------------------------------------------------------
    deepforge_layer(presentation)
    target_link_libraries(deepforge_presentation PUBLIC deepforge_app deepforge_platform PRIVATE SDL3::SDL3)
    deepforge_set_warnings(deepforge_presentation)

    # --- executable ---------------------------------------------------------
    add_executable(deepforge_game main.cpp)
    target_link_libraries(deepforge_game PRIVATE deepforge_presentation deepforge_app SDL3::SDL3)
    deepforge_set_warnings(deepforge_game)
    set_target_properties(deepforge_game PROPERTIES
        OUTPUT_NAME "deepforge"
        # Packaged builds find SDL next to (Windows) or one directory up from
        # (Unix) the executable, so the release archive is self-contained.
        INSTALL_RPATH "$ORIGIN;$ORIGIN/../lib"
        BUILD_WITH_INSTALL_RPATH FALSE)
    install(TARGETS deepforge_game RUNTIME DESTINATION bin)

    # SDL3::SDL3 is an alias in both the installed package and the fetched
    # subproject, and install() rejects aliases, so resolve it first.
    set(_sdl_real SDL3::SDL3)
    get_target_property(_sdl_alias SDL3::SDL3 ALIASED_TARGET)
    if(_sdl_alias)
        set(_sdl_real ${_sdl_alias})
    endif()
    get_target_property(_sdl_imported ${_sdl_real} IMPORTED)
    if(_sdl_imported)
        install(IMPORTED_RUNTIME_ARTIFACTS ${_sdl_real}
                RUNTIME DESTINATION bin
                LIBRARY DESTINATION lib)
    else()
        # Built as a subproject: install the produced files, plus the SONAME
        # symlink on Unix so the runtime loader finds it.
        install(FILES $<TARGET_FILE:${_sdl_real}>
                DESTINATION $<IF:$<BOOL:${WIN32}>,bin,lib>)
        if(NOT WIN32)
            install(FILES $<TARGET_SONAME_FILE:${_sdl_real}> DESTINATION lib OPTIONAL)
        endif()
    endif()
    install(FILES "${SDL3_SOURCE_DIR}/LICENSE.txt"
            DESTINATION share/deepforge/licenses
            RENAME SDL3-LICENSE.txt
            OPTIONAL)
endif()
