project(wine_d3drm LANGUAGES C)

set(CMAKE_C_CLANG_TIDY)

if(MSVC)
    # FIXME: this is wrong.
    if(CMAKE_SIZEOF_VOID_P EQUAL 4)
        set(def "${CMAKE_CURRENT_SOURCE_DIR}/d3dxof32.def")
    else()
        set(def "${CMAKE_CURRENT_SOURCE_DIR}/d3dxof64.def")
    endif()
    set(imp "${CMAKE_CURRENT_BINARY_DIR}/d3dxof.lib")
    set(exp "${CMAKE_CURRENT_BINARY_DIR}/d3dxof.exp")
    add_custom_command(OUTPUT "${imp}" "${exp}"
        COMMAND "${CMAKE_AR}" "${CMAKE_STATIC_LINKER_FLAGS}" "/NOLOGO" "/DEF:${def}" "/OUT:${imp}"
        DEPENDS "${def}"
    )
    add_custom_target(d3dxof-implib DEPENDS "${imp}")
    target_sources(d3dxof-implib PRIVATE "${def}")
    add_library(d3dxof INTERFACE)
    target_link_libraries(d3dxof INTERFACE "${imp}")
    add_dependencies(d3dxof "d3dxof-implib")

    add_library(d3drm_guid STATIC d3drm_guid.c)
    target_include_directories(d3drm_guid PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
else()
    set(d3drm_guid)
    add_library(d3drm_guid INTERFACE)
endif()

option(WINE_D3DRM_DYNAMIC_D3DXOF "Dynamic d3dxof" ON)

add_library(d3drm-wine SHARED
    d3drm.c
    d3drm_main.c
    d3drm_private.h
    d3drm.spec
    device.c
    face.c
    frame.c
    light.c
    material.c
    math.c
    meshbuilder.c
    texture.c
    version.rc
    viewport.c
)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    target_sources(d3drm-wine PRIVATE d3drm32.def)
else()
    target_sources(d3drm-wine PRIVATE d3drm64.def)
endif()
if(WINE_D3DRM_DYNAMIC_D3DXOF)
    target_sources(d3drm-wine PRIVATE dyn_d3dxof.c dyn_d3dxof.h)
    target_compile_definitions(d3drm-wine PRIVATE DYNAMIC_D3DXOF)
endif()

target_include_directories(d3drm-wine SYSTEM PUBLIC "${CMAKE_CURRENT_LIST_DIR}/include")
if(NOT WINE_D3DRM_DYNAMIC_D3DXOF)
    target_link_libraries(d3drm-wine PRIVATE d3dxof)
endif()
target_link_libraries(d3drm-wine PRIVATE ddraw)
set_property(TARGET d3drm-wine PROPERTY PREFIX "")
set_property(TARGET d3drm-wine PROPERTY OUTPUT_NAME "d3drm")
target_compile_definitions(d3drm-wine PRIVATE "__WINESRC__")
target_compile_definitions(d3drm-wine PRIVATE "WINE_NO_TRACE_MSGS")
target_compile_definitions(d3drm-wine PRIVATE "WINE_NO_DEBUG_MSGS")
target_compile_definitions(d3drm-wine PRIVATE "DECLSPEC_EXPORT=")
