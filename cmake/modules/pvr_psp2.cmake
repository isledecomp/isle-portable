cmake_minimum_required(VERSION 3.21)

include(FetchContent)

# headers
FetchContent_Declare(
  PVR_PSP2
  URL https://github.com/GrapheneCt/PVR_PSP2/archive/refs/tags/v3.9.tar.gz
  SOURCE_SUBDIR .
  DOWNLOAD_EXTRACT_TIMESTAMP FALSE
)
FetchContent_MakeAvailable(PVR_PSP2)
set(PVR_PSP2_SOURCE_DIR ${pvr_psp2_SOURCE_DIR})
set(PVR_PSP2_BINARY_DIR ${pvr_psp2_BINARY_DIR})

# stubs
file(DOWNLOAD
  https://github.com/GrapheneCt/PVR_PSP2/releases/download/v3.9/vitasdk_stubs.zip
  ${PVR_PSP2_BINARY_DIR}/vitasdk_stubs.zip
  EXPECTED_HASH SHA256=7ee2498b58cb97871fcb0e3e134ce1045acf2c22ce4873b1844a391b5da4fe47
)

# suprxs
file(DOWNLOAD
  https://github.com/GrapheneCt/PVR_PSP2/releases/download/v3.9/PSVita_Release.zip
  ${PVR_PSP2_BINARY_DIR}/PSVita_Release.zip
  EXPECTED_HASH SHA256=ed69be89f21c4894e8009a8c3567c89b1778c8db0beb3c2f4ea134adab4c494f
)

# extract
file(MAKE_DIRECTORY ${PVR_PSP2_BINARY_DIR}/extracted)

execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${PVR_PSP2_BINARY_DIR}/vitasdk_stubs.zip
                WORKING_DIRECTORY ${PVR_PSP2_BINARY_DIR}/extracted)

execute_process(COMMAND ${CMAKE_COMMAND} -E tar xf ${PVR_PSP2_BINARY_DIR}/PSVita_Release.zip
                WORKING_DIRECTORY ${PVR_PSP2_BINARY_DIR}/extracted)

# create library
add_library(GLESv2 INTERFACE)
target_include_directories(GLESv2 INTERFACE
  ${PVR_PSP2_SOURCE_DIR}/include
)
target_link_directories(GLESv2 INTERFACE
  ${PVR_PSP2_BINARY_DIR}/extracted/libGLESv2_stub_vitasdk.a/
  ${PVR_PSP2_BINARY_DIR}/extracted/libgpu_es4_ext_stub_vitasdk.a/
  ${PVR_PSP2_BINARY_DIR}/extracted/libIMGEGL_stub_vitasdk.a/
)
target_link_libraries(GLESv2 INTERFACE
  libGLESv2_stub_weak
  libgpu_es4_ext_stub_weak
  libIMGEGL_stub_weak
)
set_target_properties(GLESv2 PROPERTIES
  MODULES "${PVR_PSP2_BINARY_DIR}/extracted/libGLESv2.suprx;${PVR_PSP2_BINARY_DIR}/extracted/libIMGEGL.suprx;${PVR_PSP2_BINARY_DIR}/extracted/libgpu_es4_ext.suprx;${PVR_PSP2_BINARY_DIR}/extracted/libpvrPSP2_WSEGL.suprx"
)
