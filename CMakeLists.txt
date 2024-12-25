cmake_minimum_required(VERSION 3.25 FATAL_ERROR)

# MSVC runtime library flags are selected by an abstraction
cmake_policy(SET CMP0091 NEW)

# To set BUILD_* variables for iniparser below. Is there another way?
set(CMAKE_POLICY_DEFAULT_CMP0077 NEW)

project(isle CXX C)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH "Directory where to put executables and dll")

# By configuring CMake with -DDOWNLOAD_DEPENDENCIES=ON/OFF,
# users can choose between downloading dependencies or using system libraries
option(DOWNLOAD_DEPENDENCIES "Download dependencies" TRUE)

if(DOWNLOAD_DEPENDENCIES)
    # FetchContent downloads and configures dependencies
    include(FetchContent)
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "main"
        EXCLUDE_FROM_ALL
    )
    FetchContent_MakeAvailable(SDL3)

    FetchContent_Declare(
        iniparser
        GIT_REPOSITORY "https://github.com/ndevilla/iniparser.git"
        GIT_TAG "main"
        EXCLUDE_FROM_ALL
    )
    set(BUILD_DOCS off)
    set(BUILD_SHARED_LIBS off)
    FetchContent_MakeAvailable(iniparser)
else()
    # find_package looks for already-installed system packages.
    # Configure with `-DCMAKE_PREFIX_PATH="/path/to/package1;/path/to/package2"`
    # to add search paths.
    find_package(SDL3 CONFIG REQUIRED)

    find_package(iniparser CONFIG COMPONENTS static)
    if(NOT TARGET iniparser-static)
        find_package(iniparser REQUIRED MODULE COMPONENTS static)
    endif()
endif()

include(CheckCXXSourceCompiles)
include(CMakeDependentOption)
include(CMakePushCheckState)

set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)
option(ENABLE_CLANG_TIDY "Enable clang-tidy")
if (ENABLE_CLANG_TIDY)
    find_program(CLANG_TIDY_BIN NAMES "clang-tidy")
    set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_BIN}")
    set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_BIN}")
endif()

function(add_cxx_warning WARNING)
    if(ISLE_WERROR)
        set(compiler_option "-Werror=${WARNING}")
    else()
        set(compiler_option "-W${WARNING}")
    endif()
    string(MAKE_C_IDENTIFIER "COMPILER_SUPPORTS${compiler_option}" varname)

    cmake_push_check_state(RESET)
    set(CMAKE_REQUIRED_FLAGS "${compiler_option} ")
    if(MSVC)
        string(APPEND CMAKE_REQUIRED_FLAGS "/WX")
    else()
        string(APPEND CMAKE_REQUIRED_FLAGS "-Werror")
    endif()
    check_cxx_source_compiles("int main() { return 0; }" ${varname})
    cmake_pop_check_state()

    if(${varname})
        add_compile_options(${compiler_option})
    endif()
endfunction()

if(NOT MINGW)
    set(NOT_MINGW TRUE)
else()
    set(NOT_MINGW FALSE)
endif()

add_subdirectory(3rdparty EXCLUDE_FROM_ALL SYSTEM)

option(ISLE_WERROR "Treat warnings as errors" OFF)
option(ISLE_BUILD_APP "Build ISLE.EXE application" ON)
cmake_dependent_option(ISLE_BUILD_CONFIG "Build CONFIG.EXE application" ON "NOT MINGW" OFF)
option(ISLE_USE_DX5 "Build with internal DirectX 5 SDK" ${NOT_MINGW})
cmake_dependent_option(ISLE_D3DRM_FROM_WINE "Use d3drm from wine" "${MINGW}" "NOT ISLE_USE_DX5" OFF)

message(STATUS "Using internal DirectX5 SDK: ${ISLE_USE_DX5}")
message(STATUS "Using d3drm from wine:       ${ISLE_D3DRM_FROM_WINE}")

add_cxx_warning(parentheses)

add_library(DirectX5::DirectX5 INTERFACE IMPORTED)
target_include_directories(DirectX5::DirectX5 INTERFACE "${CMAKE_SOURCE_DIR}/3rdparty/dx5/inc")
target_link_directories(DirectX5::DirectX5 INTERFACE "${CMAKE_SOURCE_DIR}/3rdparty/dx5/lib")

add_subdirectory(3rdparty/d3drm EXCLUDE_FROM_ALL)

add_library(Vec::Vec INTERFACE IMPORTED)
target_include_directories(Vec::Vec INTERFACE "${CMAKE_SOURCE_DIR}/3rdparty/vec")

set(lego1_targets)
set(lego1_interface_targets)
macro(add_lego1_static_library NAME)
  list(APPEND lego1_targets ${NAME})
  list(APPEND lego1_interface_targets ${NAME}-interface)
  add_library(${NAME}-interface INTERFACE)
  add_library(${NAME}-objects STATIC ${ARGN})
  target_link_libraries(${NAME}-objects PRIVATE ${NAME}-interface)

  target_include_directories(${NAME}-objects PRIVATE $<$<BOOL:${ISLE_D3DRM_FROM_WINE}>:$<TARGET_PROPERTY:d3drm-wine,INTERFACE_INCLUDE_DIRECTORIES>>)
  target_link_libraries(${NAME}-interface INTERFACE $<$<BOOL:${ISLE_USE_DX5}>:DirectX5::DirectX5> SDL3::SDL3)
  target_compile_definitions(${NAME}-objects PRIVATE $<$<BOOL:${ISLE_USE_DX5}>:DIRECTX5_SDK>)
  target_compile_definitions(${NAME}-objects PRIVATE $<$<BOOL:${ISLE_D3DRM_FROM_WINE}>:D3DRM_WINE>)
endmacro()

add_lego1_static_library(tglrl
  LEGO1/tgl/d3drm/camera.cpp
  LEGO1/tgl/d3drm/device.cpp
  LEGO1/tgl/d3drm/group.cpp
  LEGO1/tgl/d3drm/light.cpp
  LEGO1/tgl/d3drm/mesh.cpp
  LEGO1/tgl/d3drm/meshbuilder.cpp
  LEGO1/tgl/d3drm/renderer.cpp
  LEGO1/tgl/d3drm/texture.cpp
  LEGO1/tgl/d3drm/view.cpp
)
target_include_directories(tglrl-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/util")
if(ISLE_D3DRM_FROM_WINE)
    target_link_libraries(tglrl-interface INTERFACE d3drm-wine)
else()
    target_link_libraries(tglrl-interface INTERFACE d3drm)
endif()

add_lego1_static_library(realtime
  LEGO1/realtime/orientableroi.cpp
  LEGO1/realtime/realtime.cpp
  LEGO1/realtime/realtimeview.cpp
)
target_include_directories(realtime-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(realtime-interface INTERFACE Vec::Vec)

add_lego1_static_library(viewmanager
  LEGO1/viewmanager/viewlod.cpp
  LEGO1/viewmanager/viewlodlist.cpp
  LEGO1/viewmanager/viewmanager.cpp
  LEGO1/viewmanager/viewroi.cpp
)
target_include_directories(viewmanager-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(viewmanager-interface INTERFACE Vec::Vec)

add_lego1_static_library(mxdirectx
  LEGO1/mxdirectx/mxdirect3d.cpp
  LEGO1/mxdirectx/mxdirectdraw.cpp
  LEGO1/mxdirectx/mxdirectxinfo.cpp
  LEGO1/mxdirectx/legodxinfo.cpp
)
target_include_directories(mxdirectx-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(mxdirectx-interface INTERFACE ddraw)

add_lego1_static_library(roi
  LEGO1/lego/sources/roi/legolod.cpp
  LEGO1/lego/sources/roi/legoroi.cpp
)
target_include_directories(roi-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(roi-interface INTERFACE viewmanager-interface Vec::Vec)

add_lego1_static_library(geom
  LEGO1/lego/sources/geom/legoedge.cpp
  LEGO1/lego/sources/geom/legounkown100db7f4.cpp
  LEGO1/lego/sources/geom/legoweedge.cpp
  LEGO1/lego/sources/geom/legowegedge.cpp
)
target_include_directories(geom-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(geom-interface INTERFACE)

add_lego1_static_library(shape
  LEGO1/lego/sources/shape/legocolor.cpp
  LEGO1/lego/sources/shape/legobox.cpp
  LEGO1/lego/sources/shape/legomesh.cpp
  LEGO1/lego/sources/shape/legosphere.cpp
  LEGO1/lego/sources/shape/legovertex.cpp
)
target_include_directories(shape-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(shape-interface INTERFACE)

add_lego1_static_library(anim
  LEGO1/lego/sources/anim/legoanim.cpp
)
target_include_directories(anim-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(anim-interface INTERFACE)

add_lego1_static_library(misc
  LEGO1/lego/sources/misc/legocontainer.cpp
  LEGO1/lego/sources/misc/legoimage.cpp
  LEGO1/lego/sources/misc/legostorage.cpp
  LEGO1/lego/sources/misc/legotexture.cpp
  LEGO1/lego/sources/misc/legotree.cpp
  LEGO1/lego/sources/misc/legounknown.cpp
)
target_include_directories(misc-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(misc-interface INTERFACE)

add_lego1_static_library(3dmanager
  LEGO1/lego/sources/3dmanager/lego3dmanager.cpp
  LEGO1/lego/sources/3dmanager/lego3dview.cpp
  LEGO1/lego/sources/3dmanager/legoview1.cpp
  LEGO1/lego/sources/3dmanager/tglsurface.cpp
)
target_include_directories(3dmanager-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(3dmanager-interface INTERFACE Vec::Vec)

add_lego1_static_library(omni
  LEGO1/omni/src/action/mxdsaction.cpp
  LEGO1/omni/src/action/mxdsanim.cpp
  LEGO1/omni/src/action/mxdsevent.cpp
  LEGO1/omni/src/action/mxdsmediaaction.cpp
  LEGO1/omni/src/action/mxdsmultiaction.cpp
  LEGO1/omni/src/action/mxdsobjectaction.cpp
  LEGO1/omni/src/action/mxdsobject.cpp
  LEGO1/omni/src/action/mxdsparallelaction.cpp
  LEGO1/omni/src/action/mxdsselectaction.cpp
  LEGO1/omni/src/action/mxdsserialaction.cpp
  LEGO1/omni/src/action/mxdssound.cpp
  LEGO1/omni/src/action/mxdsstill.cpp
  LEGO1/omni/src/action/mxdsstreamingaction.cpp
  LEGO1/omni/src/audio/mxaudiomanager.cpp
  LEGO1/omni/src/audio/mxaudiopresenter.cpp
  LEGO1/omni/src/audio/mxsoundmanager.cpp
  LEGO1/omni/src/audio/mxsoundpresenter.cpp
  LEGO1/omni/src/audio/mxwavepresenter.cpp
  LEGO1/omni/src/common/mxatom.cpp
  LEGO1/omni/src/common/mxcompositepresenter.cpp
  LEGO1/omni/src/common/mxcore.cpp
  LEGO1/omni/src/common/mxdebug.cpp
  LEGO1/omni/src/common/mxmediamanager.cpp
  LEGO1/omni/src/common/mxmediapresenter.cpp
  LEGO1/omni/src/common/mxmisc.cpp
  LEGO1/omni/src/common/mxobjectfactory.cpp
  LEGO1/omni/src/common/mxpresenter.cpp
  LEGO1/omni/src/common/mxstring.cpp
  LEGO1/omni/src/common/mxticklemanager.cpp
  LEGO1/omni/src/common/mxtimer.cpp
  LEGO1/omni/src/common/mxutilities.cpp
  LEGO1/omni/src/common/mxvariable.cpp
  LEGO1/omni/src/common/mxvariabletable.cpp
  LEGO1/omni/src/entity/mxentity.cpp
  LEGO1/omni/src/event/mxeventmanager.cpp
  LEGO1/omni/src/event/mxeventpresenter.cpp
  LEGO1/omni/src/main/mxomni.cpp
  LEGO1/omni/src/main/mxomnicreateflags.cpp
  LEGO1/omni/src/main/mxomnicreateparam.cpp
  LEGO1/omni/src/notify/mxactionnotificationparam.cpp
  LEGO1/omni/src/notify/mxnotificationmanager.cpp
  LEGO1/omni/src/notify/mxnotificationparam.cpp
  LEGO1/omni/src/stream/mxdiskstreamcontroller.cpp
  LEGO1/omni/src/stream/mxdiskstreamprovider.cpp
  LEGO1/omni/src/stream/mxdsbuffer.cpp
  LEGO1/omni/src/stream/mxdschunk.cpp
  LEGO1/omni/src/stream/mxdsfile.cpp
  LEGO1/omni/src/stream/mxdssubscriber.cpp
  LEGO1/omni/src/stream/mxio.cpp
  LEGO1/omni/src/stream/mxramstreamcontroller.cpp
  LEGO1/omni/src/stream/mxramstreamprovider.cpp
  LEGO1/omni/src/stream/mxstreamchunk.cpp
  LEGO1/omni/src/stream/mxstreamcontroller.cpp
  LEGO1/omni/src/stream/mxstreamer.cpp
  LEGO1/omni/src/system/mxautolock.cpp
  LEGO1/omni/src/system/mxcriticalsection.cpp
  LEGO1/omni/src/system/mxscheduler.cpp
  LEGO1/omni/src/system/mxsemaphore.cpp
  LEGO1/omni/src/system/mxthread.cpp
  LEGO1/omni/src/system/mxticklethread.cpp
  LEGO1/omni/src/video/flic.cpp
  LEGO1/omni/src/video/mxbitmap.cpp
  LEGO1/omni/src/video/mxdisplaysurface.cpp
  LEGO1/omni/src/video/mxflcpresenter.cpp
  LEGO1/omni/src/video/mxloopingflcpresenter.cpp
  LEGO1/omni/src/video/mxloopingsmkpresenter.cpp
  LEGO1/omni/src/video/mxpalette.cpp
  LEGO1/omni/src/video/mxregion.cpp
  LEGO1/omni/src/video/mxregioncursor.cpp
  LEGO1/omni/src/video/mxsmack.cpp
  LEGO1/omni/src/video/mxsmkpresenter.cpp
  LEGO1/omni/src/video/mxstillpresenter.cpp
  LEGO1/omni/src/video/mxvideomanager.cpp
  LEGO1/omni/src/video/mxvideoparam.cpp
  LEGO1/omni/src/video/mxvideoparamflags.cpp
  LEGO1/omni/src/video/mxvideopresenter.cpp
)
target_include_directories(omni-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/util")
target_link_libraries(omni-interface INTERFACE winmm libsmacker miniaudio)

add_lego1_static_library(lego1_impl
  LEGO1/define.cpp
  LEGO1/lego/legoomni/src/actors/act2actor.cpp
  LEGO1/lego/legoomni/src/actors/act2genactor.cpp
  LEGO1/lego/legoomni/src/actors/act3actors.cpp
  LEGO1/lego/legoomni/src/actors/act3ammo.cpp
  LEGO1/lego/legoomni/src/actors/ambulance.cpp
  LEGO1/lego/legoomni/src/actors/bike.cpp
  LEGO1/lego/legoomni/src/actors/buildingentity.cpp
  LEGO1/lego/legoomni/src/actors/buildings.cpp
  LEGO1/lego/legoomni/src/actors/bumpbouy.cpp
  LEGO1/lego/legoomni/src/actors/doors.cpp
  LEGO1/lego/legoomni/src/actors/dunebuggy.cpp
  LEGO1/lego/legoomni/src/actors/helicopter.cpp
  LEGO1/lego/legoomni/src/actors/isleactor.cpp
  LEGO1/lego/legoomni/src/actors/islepathactor.cpp
  LEGO1/lego/legoomni/src/actors/jetski.cpp
  LEGO1/lego/legoomni/src/actors/jukeboxentity.cpp
  LEGO1/lego/legoomni/src/actors/motorcycle.cpp
  LEGO1/lego/legoomni/src/actors/pizza.cpp
  LEGO1/lego/legoomni/src/actors/pizzeria.cpp
  LEGO1/lego/legoomni/src/actors/racecar.cpp
  LEGO1/lego/legoomni/src/actors/radio.cpp
  LEGO1/lego/legoomni/src/actors/skateboard.cpp
  LEGO1/lego/legoomni/src/actors/towtrack.cpp
  LEGO1/lego/legoomni/src/audio/lego3dsound.cpp
  LEGO1/lego/legoomni/src/audio/lego3dwavepresenter.cpp
  LEGO1/lego/legoomni/src/audio/legocachsound.cpp
  LEGO1/lego/legoomni/src/audio/legocachesoundmanager.cpp
  LEGO1/lego/legoomni/src/audio/legoloadcachesoundpresenter.cpp
  LEGO1/lego/legoomni/src/audio/legosoundmanager.cpp
  LEGO1/lego/legoomni/src/audio/mxbackgroundaudiomanager.cpp
  LEGO1/lego/legoomni/src/build/legocarbuild.cpp
  LEGO1/lego/legoomni/src/build/legocarbuildpresenter.cpp
  LEGO1/lego/legoomni/src/common/legoactioncontrolpresenter.cpp
  LEGO1/lego/legoomni/src/common/legoactors.cpp
  LEGO1/lego/legoomni/src/common/legoanimationmanager.cpp
  LEGO1/lego/legoomni/src/common/legoanimmmpresenter.cpp
  LEGO1/lego/legoomni/src/common/legobackgroundcolor.cpp
  LEGO1/lego/legoomni/src/common/legobuildingmanager.cpp
  LEGO1/lego/legoomni/src/common/legocharactermanager.cpp
  LEGO1/lego/legoomni/src/common/legofullscreenmovie.cpp
  LEGO1/lego/legoomni/src/common/legogamestate.cpp
  LEGO1/lego/legoomni/src/common/legoobjectfactory.cpp
  LEGO1/lego/legoomni/src/common/legophoneme.cpp
  LEGO1/lego/legoomni/src/common/legoplantmanager.cpp
  LEGO1/lego/legoomni/src/common/legoplants.cpp
  LEGO1/lego/legoomni/src/common/legostate.cpp
  LEGO1/lego/legoomni/src/common/legotextureinfo.cpp
  LEGO1/lego/legoomni/src/common/legoutils.cpp
  LEGO1/lego/legoomni/src/common/legovariables.cpp
  LEGO1/lego/legoomni/src/common/misc.cpp
  LEGO1/lego/legoomni/src/common/mxcompositemediapresenter.cpp
  LEGO1/lego/legoomni/src/common/mxcontrolpresenter.cpp
  LEGO1/lego/legoomni/src/common/mxtransitionmanager.cpp
  LEGO1/lego/legoomni/src/control/legocontrolmanager.cpp
  LEGO1/lego/legoomni/src/control/legometerpresenter.cpp
  LEGO1/lego/legoomni/src/entity/act2brick.cpp
  LEGO1/lego/legoomni/src/entity/act2policestation.cpp
  LEGO1/lego/legoomni/src/entity/legoactor.cpp
  LEGO1/lego/legoomni/src/entity/legoactorpresenter.cpp
  LEGO1/lego/legoomni/src/entity/legocameracontroller.cpp
  LEGO1/lego/legoomni/src/entity/legoentity.cpp
  LEGO1/lego/legoomni/src/entity/legoentitypresenter.cpp
  LEGO1/lego/legoomni/src/entity/legojetskiraceactor.cpp
  LEGO1/lego/legoomni/src/entity/legolocations.cpp
  LEGO1/lego/legoomni/src/entity/legonavcontroller.cpp
  LEGO1/lego/legoomni/src/entity/legopovcontroller.cpp
  LEGO1/lego/legoomni/src/entity/legoworld.cpp
  LEGO1/lego/legoomni/src/entity/legoworldpresenter.cpp
  LEGO1/lego/legoomni/src/input/legoinputmanager.cpp
  LEGO1/lego/legoomni/src/main/legomain.cpp
  LEGO1/lego/legoomni/src/main/scripts.cpp
  LEGO1/lego/legoomni/src/notify/legoeventnotificationparam.cpp
  LEGO1/lego/legoomni/src/paths/legoanimactor.cpp
  LEGO1/lego/legoomni/src/paths/legoextraactor.cpp
  LEGO1/lego/legoomni/src/paths/legopathactor.cpp
  LEGO1/lego/legoomni/src/paths/legopathboundary.cpp
  LEGO1/lego/legoomni/src/paths/legopathcontroller.cpp
  LEGO1/lego/legoomni/src/paths/legopathpresenter.cpp
  LEGO1/lego/legoomni/src/paths/legopathstruct.cpp
  LEGO1/lego/legoomni/src/race/carrace.cpp
  LEGO1/lego/legoomni/src/race/jetskirace.cpp
  LEGO1/lego/legoomni/src/race/legorace.cpp
  LEGO1/lego/legoomni/src/race/legoraceactor.cpp
  LEGO1/lego/legoomni/src/race/legoracemap.cpp
  LEGO1/lego/legoomni/src/race/legoracers.cpp
  LEGO1/lego/legoomni/src/race/legoracespecial.cpp
  LEGO1/lego/legoomni/src/race/raceskel.cpp
  LEGO1/lego/legoomni/src/video/legoanimpresenter.cpp
  LEGO1/lego/legoomni/src/video/legoflctexturepresenter.cpp
  LEGO1/lego/legoomni/src/video/legohideanimpresenter.cpp
  LEGO1/lego/legoomni/src/video/legolocomotionanimpresenter.cpp
  LEGO1/lego/legoomni/src/video/legoloopinganimpresenter.cpp
  LEGO1/lego/legoomni/src/video/legomodelpresenter.cpp
  LEGO1/lego/legoomni/src/video/legopalettepresenter.cpp
  LEGO1/lego/legoomni/src/video/legopartpresenter.cpp
  LEGO1/lego/legoomni/src/video/legophonemepresenter.cpp
  LEGO1/lego/legoomni/src/video/legotexturepresenter.cpp
  LEGO1/lego/legoomni/src/video/legovideomanager.cpp
  LEGO1/lego/legoomni/src/worlds/act3.cpp
  LEGO1/lego/legoomni/src/worlds/elevatorbottom.cpp
  LEGO1/lego/legoomni/src/worlds/gasstation.cpp
  LEGO1/lego/legoomni/src/worlds/historybook.cpp
  LEGO1/lego/legoomni/src/worlds/hospital.cpp
  LEGO1/lego/legoomni/src/worlds/infocenter.cpp
  LEGO1/lego/legoomni/src/worlds/infocenterdoor.cpp
  LEGO1/lego/legoomni/src/worlds/isle.cpp
  LEGO1/lego/legoomni/src/worlds/jukebox.cpp
  LEGO1/lego/legoomni/src/worlds/legoact2.cpp
  LEGO1/lego/legoomni/src/worlds/police.cpp
  LEGO1/lego/legoomni/src/worlds/registrationbook.cpp
  LEGO1/lego/legoomni/src/worlds/score.cpp
  LEGO1/modeldb/modeldb.cpp
)
target_include_directories(lego1_impl-interface INTERFACE "${CMAKE_SOURCE_DIR}/util")
target_include_directories(lego1_impl-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1")
target_include_directories(lego1_impl-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include")
target_include_directories(lego1_impl-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")
target_include_directories(lego1_impl-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/lego/legoomni/include")
target_include_directories(lego1_impl-interface INTERFACE "${CMAKE_SOURCE_DIR}/LEGO1/lego/legoomni/include/actions")
target_link_libraries(lego1_impl-interface INTERFACE omni-interface Vec::Vec)

if(MINGW)
  target_compile_definitions(lego1_impl-interface INTERFACE DIRECTINPUT_VERSION=0x0500)
endif()

set(lego1_objects)
set(lego1_link_libraries dxguid d3drm_guid)
foreach(lego1_library IN LISTS lego1_targets)
  message("lego1_library:${lego1_library}")
  target_compile_definitions(${lego1_library}-objects PRIVATE LEGO1_DLL)
  list(APPEND lego1_objects $<TARGET_OBJECTS:${lego1_library}-objects>)
  list(APPEND lego1_link_libraries ${lego1_library}-interface)
endforeach()
add_library(lego1 SHARED
  LEGO1/main.cpp
  ${lego1_objects}
)
target_link_libraries(lego1 PUBLIC ${lego1_link_libraries})

# Make sure filenames are ALL CAPS
set_property(TARGET lego1 PROPERTY OUTPUT_NAME LEGO1)
set_property(TARGET lego1 PROPERTY PREFIX "")
set_property(TARGET lego1 PROPERTY SUFFIX ".DLL")

if (ISLE_BUILD_APP)
  add_executable(isle WIN32
    ISLE/res/isle.rc
    ISLE/isleapp.cpp
  )
  add_custom_command(TARGET isle POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_RUNTIME_DLLS:isle> "$<TARGET_FILE_DIR:isle>"
    COMMAND_EXPAND_LISTS
  )

  target_compile_definitions(isle PRIVATE ISLE_APP)

  # Use internal DirectX 5 if required
  target_link_libraries(isle PRIVATE $<$<BOOL:${ISLE_USE_DX5}>:DirectX5::DirectX5>)

  # Link SDL and iniparser
  target_link_libraries(isle PRIVATE SDL3::SDL3 iniparser-static)

  # Link DSOUND, WINMM, and LEGO1
  target_link_libraries(isle PRIVATE winmm lego1)

  target_include_directories(isle PRIVATE $<$<BOOL:${ISLE_D3DRM_FROM_WINE}>:$<TARGET_PROPERTY:d3drm-wine,INTERFACE_INCLUDE_DIRECTORIES>>)

  # Make sure filenames are ALL CAPS
  set_property(TARGET isle PROPERTY OUTPUT_NAME ISLE)
  set_property(TARGET isle PROPERTY SUFFIX ".EXE")
endif()

if (ISLE_BUILD_CONFIG)
  add_executable(config WIN32
    LEGO1/mxdirectx/mxdirectxinfo.cpp
    LEGO1/mxdirectx/legodxinfo.cpp
    CONFIG/config.cpp
    CONFIG/ConfigCommandLineInfo.cpp
    CONFIG/AboutDlg.cpp
    CONFIG/MainDlg.cpp
    CONFIG/detectdx5.cpp
    CONFIG/StdAfx.cpp
    CONFIG/res/config.rc
  )
  target_compile_definitions(config PRIVATE _AFXDLL MXDIRECTX_FOR_CONFIG)
  target_include_directories(config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/util" "${CMAKE_CURRENT_SOURCE_DIR}/LEGO1")
  if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    target_link_libraries(config PRIVATE DirectX5::DirectX5)
  endif()
  target_compile_definitions(config PRIVATE DIRECT3D_VERSION=0x500)
  target_link_libraries(config PRIVATE SDL3::SDL3)
  target_link_libraries(config PRIVATE ddraw dxguid)
  set_property(TARGET config PROPERTY OUTPUT_NAME "CONFIG")
  set_property(TARGET config PROPERTY SUFFIX ".EXE")
  set_property(TARGET config PROPERTY MSVC_RUNTIME_LIBRARY MultiThreaded$<$<CONFIG:Debug>:Debug>DLL)
endif()

if (MSVC)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")
    set_property(TARGET ${lego1_interface_targets} APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
    if (TARGET isle)
      target_compile_definitions(isle PRIVATE "_CRT_SECURE_NO_WARNINGS")
    endif()
    if (TARGET config)
      target_compile_definitions(config PRIVATE "_CRT_SECURE_NO_WARNINGS")
    endif()
  endif()
  # Visual Studio 2017 version 15.7 needs "/Zc:__cplusplus" for __cplusplus
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "19.14.26428")
    foreach(tgt IN LISTS lego1_interface_targets)
      target_compile_options(${tgt} INTERFACE "-Zc:__cplusplus")
    endforeach()
    if (TARGET isle)
      target_compile_options(isle PRIVATE "-Zc:__cplusplus")
    endif()
    if (TARGET config)
      target_compile_options(config PRIVATE "-Zc:__cplusplus")
    endif()
  endif()
endif()

if(MSVC)
  target_link_options(isle PRIVATE "/SAFESEH:NO")
  target_link_options(lego1 PRIVATE "/SAFESEH:NO")
endif()

find_program(CLANGFORMAT_BIN NAMES clang-format)
if(EXISTS "${CLANGFORMAT_BIN}")
  execute_process(COMMAND "${CLANGFORMAT_BIN}" --version
    OUTPUT_VARIABLE "CLANGFORMAT_VERSION_OUTPUT"
    RESULT_VARIABLE "CLANGFORMAT_RESULT"
  )
  if(CLANGFORMAT_RESULT EQUAL 0 AND CLANGFORMAT_VERSION_OUTPUT MATCHES "version ([0-9\\.]+)")
    set(CLANGFORMAT_VERSION "${CMAKE_MATCH_1}")
    set(CLANGFORMAT_VERSION_REQUIRED "17.0")
    message(DEBUG "Found clang-format version ${CLANGFORMAT_VERSION} (needs ${CLANGFORMAT_VERSION_REQUIRED}")
    if(CLANGFORMAT_VERSION VERSION_GREATER_EQUAL "${CLANGFORMAT_VERSION_REQUIRED}")
      file(GLOB_RECURSE isle_sources
        "${PROJECT_SOURCE_DIR}/ISLE/*.cpp"
        "${PROJECT_SOURCE_DIR}/ISLE/*.h"
        "${PROJECT_SOURCE_DIR}/LEGO1/*.cpp"
        "${PROJECT_SOURCE_DIR}/LEGO1/*.h"
      )
      string(REPLACE ";" "\n" isle_sources_lines "${isle_sources}")
      file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/isle_sources.txt" "${isle_sources_lines}\n")
      add_custom_target(clang-format ${CLANGFORMAT_BIN} -i "--files=${CMAKE_CURRENT_BINARY_DIR}/isle_sources.txt")
    endif()
  endif()
endif()
