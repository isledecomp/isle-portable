# config app
include(FetchContent)
FetchContent_Declare(
  ScePaf_External
  URL https://github.com/olebeck/ScePaf/releases/download/continuous/ScePaf-1.0.0.zip
)
FetchContent_MakeAvailable(ScePaf_External)

add_executable(isle-config
  ${CMAKE_CURRENT_LIST_DIR}/src/app.cpp
  ${CMAKE_CURRENT_LIST_DIR}/src/main.cpp
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
  set(VITA_MAKE_FSELF_FLAGS "${VITA_MAKE_FSELF_FLAGS} -a 0x2F00000000000101")
  vita_create_self(isle-config.self isle-config
    CONFIG CONFIG_vita/exports.yml
    UNSAFE
    STRIPPED
    REL_OPTIMIZE
  )
endblock()

include(${scepaf_external_SOURCE_DIR}/rco.cmake)
make_rco(${CMAKE_CURRENT_LIST_DIR}/cxml/config_plugin.xml config_plugin.rco)
