include(FetchContent)
set(FETCHCONTENT_QUIET ON)

# Header-only libraries are downloaded once into build/_deps (shared by every preset),
# pinned to a version and checked against a SHA-256 so a build can't silently pick up something else.
set(HC_DEPS_DIR "${CMAKE_SOURCE_DIR}/build/_deps" CACHE PATH "Where downloaded third-party headers are kept")

function(hc_fetch_header url sha256 dest)
  set(path "${HC_DEPS_DIR}/${dest}")
  if(EXISTS "${path}")
    file(SHA256 "${path}" have)
    if(have STREQUAL "${sha256}")
      return()
    endif()
  endif()
  message(STATUS "Downloading ${dest}")
  file(DOWNLOAD "${url}" "${path}" EXPECTED_HASH SHA256=${sha256} TLS_VERIFY ON STATUS status)
  list(GET status 0 code)
  if(NOT code EQUAL 0)
    file(REMOVE "${path}")
    message(FATAL_ERROR "Could not download ${url}: ${status}")
  endif()
endfunction()

# JSON (config files, saves)
hc_fetch_header(https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
  9bea4c8066ef4a1c206b2be5a36302f8926f7fdc6087af5d20b417d0cf103ea6 json/nlohmann/json.hpp)
add_library(hc_json INTERFACE)
target_include_directories(hc_json SYSTEM INTERFACE "${HC_DEPS_DIR}/json")
add_library(nlohmann_json::nlohmann_json ALIAS hc_json)

# stb single-header libraries (PNG decoding, TrueType rasteriser, PNG writing for screenshots)
set(HC_STB_COMMIT 9600522de53973336fa033a289858a4354cbb04c)
hc_fetch_header(https://raw.githubusercontent.com/nothings/stb/${HC_STB_COMMIT}/stb_image.h
  deb4f18c9ca21b751a1323019031c94c0bf5ca1d2929ef5913cc56f00aab6e91 stb/stb_image.h)
hc_fetch_header(https://raw.githubusercontent.com/nothings/stb/${HC_STB_COMMIT}/stb_image_write.h
  a8e7f77f0ccfdc9bac09a71268b131931ff81aafca82c393aeb6aaba10d9e7ca stb/stb_image_write.h)
hc_fetch_header(https://raw.githubusercontent.com/nothings/stb/${HC_STB_COMMIT}/stb_truetype.h
  428cfe93520f2aed5e413a220b0db5bba5b17e2ebe5a64e4a8efa9eeeb67ec5f stb/stb_truetype.h)
add_library(hc_stb INTERFACE)
target_include_directories(hc_stb SYSTEM INTERFACE "${HC_DEPS_DIR}/stb")

if(HC_BUILD_TESTS)
  hc_fetch_header(https://raw.githubusercontent.com/doctest/doctest/v2.4.11/doctest/doctest.h
    44faa038e9c3f9728efbda143748d01124ea0a27f4bf78f35a15d8fab2e039fb doctest/doctest.h)
  add_library(hc_doctest INTERFACE)
  target_include_directories(hc_doctest SYSTEM INTERFACE "${HC_DEPS_DIR}/doctest")
endif()

# Emscripten ships SDL2 and SDL2_mixer as ports; native builds compile them from source.
if(NOT EMSCRIPTEN)
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(SDL2
    URL https://github.com/libsdl-org/SDL/releases/download/release-2.30.9/SDL2-2.30.9.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
  FetchContent_MakeAvailable(SDL2)

  set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_INSTALL OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_SAMPLES OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_VENDORED OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_OPUS OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_FLAC OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_MOD OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_MIDI OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_WAVPACK OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_GME OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_MP3_MPG123 OFF CACHE BOOL "" FORCE)
  set(SDL2MIXER_MP3_MINIMP3 ON CACHE BOOL "" FORCE)
  set(SDL2MIXER_VORBIS "STB" CACHE STRING "" FORCE)
  FetchContent_Declare(SDL2_mixer
    URL https://github.com/libsdl-org/SDL_mixer/releases/download/release-2.8.0/SDL2_mixer-2.8.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP ON)
  FetchContent_MakeAvailable(SDL2_mixer)
endif()
