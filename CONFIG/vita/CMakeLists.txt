cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

project(isle-config LANGUAGES CXX C VERSION 0.1)

include("${VITASDK}/share/vita.cmake" REQUIRED)

include(FetchContent)
FetchContent_Declare(
  ScePaf_External
  URL https://github.com/olebeck/ScePaf/releases/download/continuous/ScePaf-1.0.0.zip
  UPDATE_DISCONNECTED TRUE
)
FetchContent_MakeAvailable(ScePaf_External)

add_executable(isle-config
  src/app.cpp
  src/main.cpp
)

set_target_properties(isle-config PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

target_compile_options(isle-config PRIVATE
  -fno-rtti -fno-exceptions -Wl,-q -Wall -fno-builtin -fshort-wchar -Wno-unused-function -Wno-sign-compare
)

target_link_options(isle-config PRIVATE
  -nostartfiles -nostdlib
)

target_link_libraries(isle-config PRIVATE
  SceAppMgr_stub
  SceLibKernel_stub
  SceSysmodule_stub

  ScePafToplevel_stub
  ScePafResource_stub
  ScePafWidget_stub
  ScePafCommon_stub
  ScePafStdc_stub
)

block()
  vita_create_self(isle-config.self isle-config
    CONFIG exports.yml
    UNSAFE
    STRIPPED
    REL_OPTIMIZE
  )
endblock()

include(${scepaf_external_SOURCE_DIR}/rco.cmake)
make_rco(cxml/config_plugin.xml config_plugin.rco)
