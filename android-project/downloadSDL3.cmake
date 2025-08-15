cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

include(FetchContent)

set(FETCHCONTENT_BASE_DIR "build/_deps")

FetchContent_Populate(
  SDL3
  GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
  GIT_TAG "main"
  SOURCE_DIR "build/_deps/sdl3-src"
  BINARY_DIR "build/_deps/sdl3-build"
)
