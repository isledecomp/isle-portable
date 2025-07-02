cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

project(ScePaf)

FetchContent_Declare(
    ScePaf_RE
    GIT_REPOSITORY "https://github.com/GrapheneCt/ScePaf-RE"
    GIT_TAG "b76d6ae00516b107d37e7e8a1a16211e3b729a07"
    UPDATE_DISCONNECTED TRUE
    EXCLUDE_FROM_ALL
    PATCH_COMMAND patch -p2 < ${CMAKE_CURRENT_LIST_DIR}/ScePaf.patch
)

FetchContent_MakeAvailable(ScePaf_RE)

set(SCEPAF_LIBRARIES
    ScePafCommon
    ScePafThread
    ScePafAutoTestTty
    ScePafMisc
    ScePafLowlayer
    ScePafWidget
    ScePafStdc
    ScePafGraphics
    ScePafResource
    ScePafToplevel
    SceAppSettings
    SceCommonGuiDialog
    ScePafWebMapView
    SceWebUIPlugin
)

file(MAKE_DIRECTORY ${scepaf_re_BINARY_DIR}/include)
add_custom_command(
    OUTPUT ${scepaf_re_BINARY_DIR}/include/paf.h
    COMMAND cp -r ${scepaf_re_SOURCE_DIR}/include/ ${scepaf_re_BINARY_DIR}
    COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/include/map ${scepaf_re_BINARY_DIR}/include/paf/std/map
    COMMAND cp ${CMAKE_CURRENT_LIST_DIR}/include/math.h ${scepaf_re_BINARY_DIR}/include/paf/math/math.h
)

foreach(library ${SCEPAF_LIBRARIES})
    set(emd ${scepaf_re_SOURCE_DIR}/lib/${library}.emd)
    set(yml ${scepaf_re_BINARY_DIR}/${library}.yml)
    set(libdir ${scepaf_re_BINARY_DIR}/lib/${library})
    set(libbuilddir ${scepaf_re_BINARY_DIR}/libbuild/${library})
    set(stub ${libdir}/lib${library}_stub.a)
    file(MAKE_DIRECTORY ${libbuilddir})
    add_custom_command(
        OUTPUT "${yml}"
        DEPENDS "${emd}"
        COMMAND python3 ${CMAKE_CURRENT_LIST_DIR}/emd2yml.py "${yml}" "${emd}"
        COMMENT emd2yml ${library}
    )
    add_custom_command(
        OUTPUT "${stub}"
        DEPENDS "${yml}"
        COMMAND vita-libs-gen-2 --yml="${yml}" --output="${libbuilddir}" --cmake=true
        COMMAND cmake "${libbuilddir}" -B "${libbuilddir}/cmake"
        COMMAND cmake --build "${libbuilddir}/cmake"
        COMMAND cp "${libbuilddir}/cmake/lib${library}_stub.a" "${stub}"
        COMMENT vita-libs-gen ${library}
    )
    list(APPEND SCEPAF_STUBS ${stub})
endforeach()

add_custom_target(
    ScePaf_stubs
    DEPENDS ${SCEPAF_STUBS}
)

add_custom_target(
    paf_includes
    DEPENDS ${scepaf_re_BINARY_DIR}/include/paf.h
)

foreach(library ${SCEPAF_LIBRARIES})
    set(stub "${scepaf_re_BINARY_DIR}/lib/${library}/lib${library}_stub.a")
    add_library(${library}_stub STATIC IMPORTED GLOBAL)
    target_include_directories(${library}_stub INTERFACE
        ${scepaf_re_BINARY_DIR}/include
    )
    target_link_directories(${library}_stub INTERFACE
        "${scepaf_re_BINARY_DIR}/lib/${library}"
    )
    set_target_properties(${library}_stub PROPERTIES
        IMPORTED_LOCATION ${stub}
    )
    target_compile_options(${library}_stub INTERFACE -include ${CMAKE_CURRENT_LIST_DIR}/include/declspec.h)
    add_dependencies(${library}_stub paf_includes)
endforeach()
