cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(SDL3_PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/sdl3_vita_shaders_fix.patch")

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG "main"
    EXCLUDE_FROM_ALL
    PATCH_COMMAND git apply "${SDL3_PATCH_FILE}"
)

block()
  set(VIDEO_VITA_PVR ON)
  get_target_property(PVR_INCLUDES GLESv2 INTERFACE_INCLUDE_DIRECTORIES)
  list(APPEND CMAKE_REQUIRED_INCLUDES ${PVR_INCLUDES})
  FetchContent_MakeAvailable(SDL3)
  target_include_directories(SDL3-static PRIVATE ${PVR_INCLUDES})
endblock()
