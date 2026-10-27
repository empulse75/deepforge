file(GLOB_RECURSE DEEPFORGE_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
# Settings.cpp — the code Settings.test.cpp exercises — is SDL glue and is
# only compiled when the client is built, so the cases are excluded on
# client-off presets: without load_settings() they would not even link. The
# pure seams (FocusModel, InputCore, Theme machinery) link and run on every
# preset — that is what they were extracted for.
if(NOT DEEPFORGE_BUILD_CLIENT)
    list(FILTER DEEPFORGE_TEST_SOURCES EXCLUDE REGEX "/Settings\\.test\\.cpp$")
endif()
add_executable(deepforge_tests ${DEEPFORGE_TEST_SOURCES})
target_link_libraries(deepforge_tests PRIVATE
    deepforge_core deepforge_content deepforge_world deepforge_domain deepforge_app deepforge_store
    deepforge_platform deepforge_presentation
    Catch2::Catch2WithMain)
target_include_directories(deepforge_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
deepforge_set_warnings(deepforge_tests)
target_compile_definitions(deepforge_tests PRIVATE
    DEEPFORGE_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    DEEPFORGE_SOURCE_DIR="${CMAKE_SOURCE_DIR}/src"
    DEEPFORGE_TEST_TMP_DIR="${CMAKE_BINARY_DIR}/test-tmp")
# Catch2's default POST_BUILD discovery runs the freshly linked test binary at
# build time to enumerate test cases. That is impossible when cross-compiling:
# the Windows .exe cannot execute on the Linux host, so the build fails after a
# fully successful link. PRE_TEST defers enumeration to ctest time on the target
# machine. Native builds keep the original POST_BUILD behaviour unchanged.
if(CMAKE_CROSSCOMPILING)
    catch_discover_tests(deepforge_tests DISCOVERY_MODE PRE_TEST)
else()
    catch_discover_tests(deepforge_tests)
endif()

add_test(NAME content_validation
         COMMAND deepforge_content_validator --data "${CMAKE_SOURCE_DIR}/data")

# Visual regression against committed baselines (SPEC slice 1). Skips itself
# when the client is not built or no offscreen display is available, so it is
# safe under every preset.
add_test(NAME visual_regression
         COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/visual_regression.py"
                 "${CMAKE_BINARY_DIR}/src/deepforge"
                 "${CMAKE_SOURCE_DIR}/data"
                 "${CMAKE_SOURCE_DIR}/tests/visual/baselines")

# Regeneration must be a no-op (FUNC-01). gen_content.py documents "edit intent, re-run,
# review the diff", so running it into a scratch copy of the data must reproduce every file
# it emits byte for byte; otherwise the documented workflow silently loses content.
add_test(NAME content_generator_no_op
         COMMAND python3 "${CMAKE_SOURCE_DIR}/tools/content_gen/check_regeneration_noop.py"
                 "${CMAKE_SOURCE_DIR}")
