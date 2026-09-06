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
deepforge_set_warnings(deepforge_core)

# --- content ----------------------------------------------------------------
deepforge_layer(content)
target_link_libraries(deepforge_content PUBLIC deepforge_core PRIVATE deepforge_json_headers)
deepforge_set_warnings(deepforge_content)

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
    set_target_properties(deepforge_game PROPERTIES OUTPUT_NAME "deepforge")
    install(TARGETS deepforge_game RUNTIME DESTINATION bin)
    if(TARGET SDL3-shared)
        install(IMPORTED_RUNTIME_ARTIFACTS SDL3::SDL3 RUNTIME DESTINATION bin LIBRARY DESTINATION bin)
    endif()
endif()
