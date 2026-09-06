file(GLOB_RECURSE DEEPFORGE_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
add_executable(deepforge_tests ${DEEPFORGE_TEST_SOURCES})
target_link_libraries(deepforge_tests PRIVATE
    deepforge_core deepforge_content deepforge_world deepforge_domain deepforge_app
    Catch2::Catch2WithMain)
target_include_directories(deepforge_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
deepforge_set_warnings(deepforge_tests)
target_compile_definitions(deepforge_tests PRIVATE
    DEEPFORGE_DATA_DIR="${CMAKE_SOURCE_DIR}/data"
    DEEPFORGE_TEST_TMP_DIR="${CMAKE_BINARY_DIR}/test-tmp")
catch_discover_tests(deepforge_tests)

add_test(NAME content_validation
         COMMAND deepforge_content_validator --data "${CMAKE_SOURCE_DIR}/data")
