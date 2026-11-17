cmake_minimum_required(VERSION 3.24)
project(HollowCrypt VERSION 2.0.0 DESCRIPTION "Top-down open-world action RPG" LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(HC_BUILD_TESTS "Build the unit tests" ON)
option(HC_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)
include(Dependencies)

# hc_game: rules and simulation. No SDL, no rendering: this is what the tests link against.
file(GLOB HC_GAME_SOURCES CONFIGURE_DEPENDS src/core/*.cpp src/game/*.cpp)
add_library(hc_game STATIC ${HC_GAME_SOURCES})
target_include_directories(hc_game PUBLIC src)
target_link_libraries(hc_game PUBLIC nlohmann_json::nlohmann_json)
hc_set_warnings(hc_game)

# hollowcrypt: platform layer (SDL2), rendering, interface and the app shell.
file(GLOB HC_APP_SOURCES CONFIGURE_DEPENDS src/engine/*.cpp src/view/*.cpp src/app/*.cpp)
add_executable(hollowcrypt ${HC_APP_SOURCES})
target_link_libraries(hollowcrypt PRIVATE hc_game hc_stb)
target_compile_definitions(hollowcrypt PRIVATE HC_VERSION="${PROJECT_VERSION}")
hc_set_warnings(hollowcrypt)

if(EMSCRIPTEN)
  include(StageAssets)
  hc_stage_assets(hollowcrypt "${CMAKE_BINARY_DIR}/stage")
  set(HC_PORTS -sUSE_SDL=2 -sUSE_SDL_MIXER=2 -sSDL2_MIXER_FORMATS=ogg,mp3)
  target_compile_options(hollowcrypt PRIVATE ${HC_PORTS})
  target_link_options(hollowcrypt PRIVATE ${HC_PORTS}
    -sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=1048576
    -sEXPORTED_RUNTIME_METHODS=FS -sFORCE_FILESYSTEM=1
    "--preload-file=${CMAKE_BINARY_DIR}/stage@/"
    "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
    $<$<CONFIG:Debug>:-sASSERTIONS=1>
    $<$<NOT:$<CONFIG:Debug>>:-O3>)
  set_target_properties(hollowcrypt PROPERTIES OUTPUT_NAME "index" SUFFIX ".html"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/dist")
  set_property(TARGET hollowcrypt APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html")
else()
  target_link_libraries(hollowcrypt PRIVATE SDL2::SDL2main SDL2::SDL2-static SDL2_mixer::SDL2_mixer-static)
  # Native builds read assets/ and config/ straight from the source tree.
  target_compile_definitions(hollowcrypt PRIVATE HC_DATA_ROOT="${CMAKE_CURRENT_SOURCE_DIR}/")
endif()

if(HC_BUILD_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()
