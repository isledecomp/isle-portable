cmake_minimum_required(VERSION 3.21)

include(FetchContent)

set(SDL3_PATCH_FILE "${CMAKE_CURRENT_LIST_DIR}/sdl3_vita_shaders_fix.patch")

FetchContent_Declare(
    SDL3
    GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
    GIT_TAG "main"
    EXCLUDE_FROM_ALL
)

FetchContent_GetProperties(SDL3)

if(NOT SDL3_POPULATED)
  FetchContent_Populate(SDL3)
  execute_process(
    COMMAND git apply --verbose "${SDL3_PATCH_FILE}"
    WORKING_DIRECTORY "${sdl3_SOURCE_DIR}"
    RESULT_VARIABLE APPLY_RESULT
  )
  if(NOT APPLY_RESULT EQUAL 0)
    message(FATAL_ERROR "Failed to apply patch to SDL3")
  endif()
endif()

block()
  set(VIDEO_VITA_PVR ON)
  get_target_property(PVR_INCLUDES GLESv2 INTERFACE_INCLUDE_DIRECTORIES)
  list(APPEND CMAKE_REQUIRED_INCLUDES ${PVR_INCLUDES})
  add_subdirectory(${sdl3_SOURCE_DIR} ${sdl3_BINARY_DIR})
endblock()
