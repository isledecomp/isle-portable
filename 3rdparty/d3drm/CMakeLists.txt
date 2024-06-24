project(wine_d3drm LANGUAGES C)

add_library(d3drm-wine SHARED EXCLUDE_FROM_ALL
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

    d3drm.def
)
target_include_directories(d3drm-wine SYSTEM INTERFACE "${CMAKE_CURRENT_LIST_DIR}/include")
target_link_libraries(d3drm-wine PRIVATE d3dxof ddraw)
set_property(TARGET d3drm-wine PROPERTY PREFIX "")
set_property(TARGET d3drm-wine PROPERTY OUTPUT_NAME "d3drm")
target_compile_definitions(d3drm-wine PRIVATE "__WINESRC__")
target_compile_definitions(d3drm-wine PRIVATE "WINE_NO_TRACE_MSGS")
target_compile_definitions(d3drm-wine PRIVATE "WINE_NO_DEBUG_MSGS")
target_compile_definitions(d3drm-wine PRIVATE "DECLSPEC_EXPORT=")
