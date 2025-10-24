file(GLOB SDL2_HEADERS "${SDL2_INCLUDE_DIR}/*.h")
foreach(header IN LISTS SDL2_HEADERS)
  get_filename_component(fname ${header} NAME)
  configure_file(${header} ${CMAKE_CURRENT_BINARY_DIR}/SDL3/${fname} COPYONLY)
endforeach()

add_library(SDL3_shim STATIC
        main.cpp
)

target_link_libraries(SDL3_shim PUBLIC SDL2::SDL2)
target_include_directories(SDL3_shim PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_BINARY_DIR}"
)

add_library(SDL3::SDL3 ALIAS SDL3_shim)
add_library(SDL3::Headers ALIAS SDL3_shim)
