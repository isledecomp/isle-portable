cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

project(isle-config LANGUAGES CXX C VERSION 0.1)

add_subdirectory(ScePaf)

enable_language(CXX ASM)

add_executable(${PROJECT_NAME}
    src/app.cpp
    src/main.cpp
)

target_compile_options(${PROJECT_NAME} PRIVATE
    -fno-rtti -fno-exceptions -Wl,-q -Wall -fno-builtin -fshort-wchar -Wno-unused-function -Wno-sign-compare
)

# todo _start symbol warning
target_link_options(${PROJECT_NAME} PRIVATE
    -nostartfiles -nostdlib
)

target_link_libraries(${PROJECT_NAME} PRIVATE
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
    vita_create_self(${PROJECT_NAME}.self ${PROJECT_NAME}
        CONFIG exports.yml
        UNSAFE
        STRIPPED
        REL_OPTIMIZE
    )
endblock()
