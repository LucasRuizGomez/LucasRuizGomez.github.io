file(GLOB HC_TEST_SOURCES CONFIGURE_DEPENDS *.cpp)
add_executable(hc_tests ${HC_TEST_SOURCES})
target_link_libraries(hc_tests PRIVATE hc_game hc_doctest)
target_compile_definitions(hc_tests PRIVATE HC_CONFIG_DIR="${CMAKE_SOURCE_DIR}/config/")
if(EMSCRIPTEN)
  # Runs under Node: give it direct access to the real file system so it can read config/.
  target_link_options(hc_tests PRIVATE -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=1048576 -sEXIT_RUNTIME=1)
endif()
add_test(NAME hc_tests COMMAND hc_tests)
