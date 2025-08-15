cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

project(isle LANGUAGES CXX C VERSION 0.1)

if (IOS)
  set(CMAKE_XCODE_ATTRIBUTE_CODE_SIGNING_ALLOWED NO)
  set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0")
  add_compile_definitions(IOS)
endif()

if (WINDOWS_STORE)
  add_compile_definitions(WINDOWS_STORE)
endif()

if (EMSCRIPTEN)
  add_compile_options(-pthread)
  add_link_options(-sUSE_WEBGL2=1 -sMIN_WEBGL_VERSION=2 -sALLOW_MEMORY_GROWTH=1 -sMAXIMUM_MEMORY=2gb -sUSE_PTHREADS=1 -sPROXY_TO_PTHREAD=1 -sOFFSCREENCANVAS_SUPPORT=1 -sPTHREAD_POOL_SIZE_STRICT=0 -sFORCE_FILESYSTEM=1 -sWASMFS=1 -sEXIT_RUNTIME=1)
  set(SDL_PTHREADS ON CACHE BOOL "Enable SDL pthreads" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

include(CheckCXXSourceCompiles)
include(CMakeDependentOption)
include(CMakePushCheckState)
include(CMake/detectcpu.cmake)

DetectTargetCPUArchitectures(ISLE_CPUS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (NOT MINGW)
  set(NOT_MINGW ON)
else()
  set(NOT_MINGW OFF)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

find_program(SDL_SHADERCROSS_BIN NAMES "shadercross")
find_package(Python3 3.12 COMPONENTS Interpreter)

option(ISLE_BUILD_APP "Build isle application" ON)
option(ISLE_BUILD_ASSETS "Build assets from the /assets directory" OFF)
option(ISLE_ASAN "Enable Address Sanitizer" OFF)
option(ISLE_UBSAN "Enable Undefined Behavior Sanitizer" OFF)
option(ISLE_WERROR "Treat warnings as errors" OFF)
option(ISLE_DEBUG "Enable imgui debug" ON)
cmake_dependent_option(ISLE_USE_DX5 "Build with internal DirectX 5 SDK" "${NOT_MINGW}" "WIN32;CMAKE_SIZEOF_VOID_P EQUAL 4" OFF)
cmake_dependent_option(ISLE_MINIWIN "Use miniwin" ON "NOT ISLE_USE_DX5" OFF)
cmake_dependent_option(ISLE_EXTENSIONS "Use extensions" ON "NOT ISLE_USE_DX5;NOT WINDOWS_STORE" OFF)
cmake_dependent_option(ISLE_BUILD_CONFIG "Build CONFIG.EXE application" ON "MSVC OR ISLE_MINIWIN;NOT NINTENDO_3DS;NOT WINDOWS_STORE" OFF)
cmake_dependent_option(ISLE_COMPILE_SHADERS "Compile shaders" ON "SDL_SHADERCROSS_BIN;TARGET Python3::Interpreter" OFF)
option(CMAKE_POSITION_INDEPENDENT_CODE "Build with -fPIC" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy")
option(DOWNLOAD_DEPENDENCIES "Download dependencies" ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}" CACHE PATH "Directory where to put executables and dll")
set(ISLE_EMSCRIPTEN_HOST "" CACHE STRING "Host URL for Emscripten streaming (e.g., https://test.com)")
cmake_dependent_option(BUILD_SHARED_LIBS "Build lego1 as a shared library" ON "NOT EMSCRIPTEN" OFF)

message(STATUS "Isle app:               ${ISLE_BUILD_APP}")
message(STATUS "Config app:             ${ISLE_BUILD_CONFIG}")
message(STATUS "Internal DirectX5 SDK:  ${ISLE_USE_DX5}")
message(STATUS "Internal miniwin:       ${ISLE_MINIWIN}")
message(STATUS "Isle extensions:        ${ISLE_EXTENSIONS}")
message(STATUS "Isle debugging:         ${ISLE_DEBUG}")
message(STATUS "Compile shaders:        ${ISLE_COMPILE_SHADERS}")

add_library(Isle::iniparser INTERFACE IMPORTED)

if (DOWNLOAD_DEPENDENCIES)
  # FetchContent downloads and configures dependencies
  message(STATUS "Fetching SDL3 and iniparser. This might take a while...")
  include(FetchContent)

  if(ANDROID)
    # Built by Gradle
    find_package(SDL3 REQUIRED CONFIG COMPONENTS Shared)
  else()
    if (WINDOWS_STORE)
      FetchContent_Declare(
        SDL3
        GIT_REPOSITORY "https://github.com/Helloyunho/SDL3-uwp.git"
        GIT_TAG "main"
        EXCLUDE_FROM_ALL
      )
    else()
      FetchContent_Declare(
        SDL3
        GIT_REPOSITORY "https://github.com/libsdl-org/SDL.git"
        GIT_TAG "main"
        EXCLUDE_FROM_ALL
      )
    endif()
    FetchContent_MakeAvailable(SDL3)
  endif()

  FetchContent_Declare(
    iniparser
    GIT_REPOSITORY "https://gitlab.com/iniparser/iniparser.git"
    GIT_TAG "main"
    EXCLUDE_FROM_ALL
  )
  block()
    set(BUILD_DOCS off)
    set(BUILD_SHARED_LIBS off)
    FetchContent_MakeAvailable(iniparser)
    target_link_libraries(Isle::iniparser INTERFACE iniparser-static)
  endblock()
else()
  # find_package looks for already-installed system packages.
  # Configure with `-DCMAKE_PREFIX_PATH="/path/to/package1;/path/to/package2"`
  # to add search paths.
  find_package(SDL3 CONFIG REQUIRED)

  find_package(iniparser REQUIRED CONFIG COMPONENTS shared)
  target_link_libraries(Isle::iniparser INTERFACE iniparser-shared)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if (ENABLE_CLANG_TIDY)
  find_program(CLANG_TIDY_BIN NAMES "clang-tidy" ENV "LLVM_ROOT" REQUIRED)
  set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_BIN}")
  set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_BIN}")
endif()

if (ISLE_ASAN)
  add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
  add_link_options(-fsanitize=address)
endif()

if (ISLE_UBSAN)
  add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined)
  add_link_options(-fsanitize=undefined)
endif()

add_subdirectory(miniwin EXCLUDE_FROM_ALL)

set(isle_targets)

function(add_cxx_warning WARNING)
  if (ISLE_WERROR)
    set(compiler_option "-Werror=${WARNING}")
  else()
    set(compiler_option "-W${WARNING}")
  endif()
  string(MAKE_C_IDENTIFIER "COMPILER_SUPPORTS${compiler_option}" varname)

  cmake_push_check_state(RESET)
  set(CMAKE_REQUIRED_FLAGS "${compiler_option} ")
  if (MSVC)
    string(APPEND CMAKE_REQUIRED_FLAGS "/WX")
  else()
    string(APPEND CMAKE_REQUIRED_FLAGS "-Werror")
  endif()
  check_cxx_source_compiles("int main() { return 0; }" ${varname})
  cmake_pop_check_state()

  if (${varname})
    add_compile_options(${compiler_option})
  endif()
endfunction()

add_subdirectory(3rdparty EXCLUDE_FROM_ALL SYSTEM)

add_cxx_warning(parentheses)

add_library(DirectX5::DirectX5 INTERFACE IMPORTED)
target_include_directories(DirectX5::DirectX5 INTERFACE "${CMAKE_SOURCE_DIR}/3rdparty/dx5/inc")
target_link_directories(DirectX5::DirectX5 INTERFACE "${CMAKE_SOURCE_DIR}/3rdparty/dx5/lib")

add_library(Vec::Vec INTERFACE IMPORTED)
target_include_directories(Vec::Vec INTERFACE "${CMAKE_SOURCE_DIR}/3rdparty/vec")

add_library(lego1
  LEGO1/main.cpp
)
target_precompile_headers(lego1 PRIVATE "LEGO1/lego1_pch.h")
set_property(TARGET lego1 PROPERTY DEFINE_SYMBOL "LEGO1_DLL")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/util>")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/extensions/include>")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/LEGO1>")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/LEGO1/omni/include>")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/LEGO1/lego/sources>")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/LEGO1/lego/legoomni/include>")
target_include_directories(lego1 PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/LEGO1/lego/legoomni/include/actions>")
target_link_libraries(lego1 PRIVATE SDL3::SDL3)
target_link_libraries(lego1 PUBLIC SDL3::Headers)
target_link_libraries(lego1 PRIVATE $<$<BOOL:${ISLE_USE_DX5}>:DirectX5::DirectX5>)
# Allow unconditional include of miniwin/miniwind3d.h
target_link_libraries(lego1 PRIVATE miniwin-headers)
if(WIN32)
  set_property(TARGET lego1 PROPERTY PREFIX "")
endif()

target_compile_definitions(lego1 PRIVATE $<$<BOOL:${ISLE_USE_DX5}>:DIRECTX5_SDK>)
list(APPEND isle_targets lego1)

# tglrl sources
target_sources(lego1 PRIVATE
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
target_include_directories(lego1 PUBLIC "${CMAKE_SOURCE_DIR}/LEGO1")

# realtime sources
target_sources(lego1 PRIVATE
  LEGO1/realtime/orientableroi.cpp
  LEGO1/realtime/realtime.cpp
  LEGO1/realtime/realtimeview.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1")
target_link_libraries(lego1 PRIVATE Vec::Vec)

# viewmanager sources
target_sources(lego1 PRIVATE
  LEGO1/viewmanager/viewlod.cpp
  LEGO1/viewmanager/viewlodlist.cpp
  LEGO1/viewmanager/viewmanager.cpp
  LEGO1/viewmanager/viewroi.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1")
target_link_libraries(lego1 PRIVATE Vec::Vec)

# mxdirectx sources
target_sources(lego1 PRIVATE
  LEGO1/mxdirectx/mxdirect3d.cpp
  LEGO1/mxdirectx/mxdirectdraw.cpp
  LEGO1/mxdirectx/mxdirectxinfo.cpp
  LEGO1/mxdirectx/legodxinfo.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1")
if (WIN32)
  target_link_libraries(lego1 PRIVATE ddraw)
endif()

# roi sources
target_sources(lego1 PRIVATE
  LEGO1/lego/sources/roi/legolod.cpp
  LEGO1/lego/sources/roi/legoroi.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")
target_link_libraries(lego1 PRIVATE Vec::Vec)

# geom sources
target_sources(lego1 PRIVATE
  LEGO1/lego/sources/geom/legoedge.cpp
  LEGO1/lego/sources/geom/legoorientededge.cpp
  LEGO1/lego/sources/geom/legoweedge.cpp
  LEGO1/lego/sources/geom/legowegedge.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")

# shape sources
target_sources(lego1 PRIVATE
  LEGO1/lego/sources/shape/legocolor.cpp
  LEGO1/lego/sources/shape/legobox.cpp
  LEGO1/lego/sources/shape/legomesh.cpp
  LEGO1/lego/sources/shape/legosphere.cpp
  LEGO1/lego/sources/shape/legovertex.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")

# anim sources
target_sources(lego1 PRIVATE
  LEGO1/lego/sources/anim/legoanim.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")

# misc sources
target_sources(lego1 PRIVATE
  LEGO1/lego/sources/misc/legocontainer.cpp
  LEGO1/lego/sources/misc/legoimage.cpp
  LEGO1/lego/sources/misc/legostorage.cpp
  LEGO1/lego/sources/misc/legotexture.cpp
  LEGO1/lego/sources/misc/legotree.cpp
  LEGO1/lego/sources/misc/legounknown.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")

# 3dmanager sources
target_sources(lego1 PRIVATE
  LEGO1/lego/sources/3dmanager/lego3dmanager.cpp
  LEGO1/lego/sources/3dmanager/lego3dview.cpp
  LEGO1/lego/sources/3dmanager/legoview1.cpp
  LEGO1/lego/sources/3dmanager/tglsurface.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1")
target_link_libraries(lego1 PRIVATE Vec::Vec)

# omni sources
target_sources(lego1 PRIVATE
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
  LEGO1/omni/src/common/mxmediapresenter.cpp
  LEGO1/omni/src/common/mxmisc.cpp
  LEGO1/omni/src/common/mxobjectfactory.cpp
  LEGO1/omni/src/common/mxpresentationmanager.cpp
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
  LEGO1/omni/src/main/mxmain.cpp
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
  LEGO1/omni/src/video/mxsmk.cpp
  LEGO1/omni/src/video/mxsmkpresenter.cpp
  LEGO1/omni/src/video/mxstillpresenter.cpp
  LEGO1/omni/src/video/mxvideomanager.cpp
  LEGO1/omni/src/video/mxvideoparam.cpp
  LEGO1/omni/src/video/mxvideoparamflags.cpp
  LEGO1/omni/src/video/mxvideopresenter.cpp
)
target_include_directories(lego1 PRIVATE "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1")
if (WIN32)
  target_link_libraries(lego1 INTERFACE winmm)
endif()
target_link_libraries(lego1 PRIVATE libsmacker miniaudio)
target_include_directories(lego1 PUBLIC $<BUILD_INTERFACE:$<TARGET_PROPERTY:miniaudio,INTERFACE_INCLUDE_DIRECTORIES>>)

# lego1_impl sources
target_sources(lego1 PRIVATE
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
  LEGO1/lego/legoomni/src/common/legobuildingmanager.cpp
  LEGO1/lego/legoomni/src/common/legocharactermanager.cpp
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
  LEGO1/lego/legoomni/src/entity/legolocations.cpp
  LEGO1/lego/legoomni/src/entity/legonavcontroller.cpp
  LEGO1/lego/legoomni/src/entity/legopovcontroller.cpp
  LEGO1/lego/legoomni/src/entity/legoworld.cpp
  LEGO1/lego/legoomni/src/entity/legoworldpresenter.cpp
  LEGO1/lego/legoomni/src/input/legoinputmanager.cpp
  LEGO1/lego/legoomni/src/main/legomain.cpp
  LEGO1/lego/legoomni/src/main/scripts.cpp
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
target_link_libraries(lego1 PRIVATE Vec::Vec)
if (NOT ISLE_MINIWIN)
  target_link_libraries(lego1 PRIVATE d3drm dxguid)
  target_compile_definitions(lego1 PRIVATE DIRECTINPUT_VERSION=0x0500)
endif()

if (ISLE_EXTENSIONS)
  target_link_libraries(lego1 PRIVATE libweaver)
  target_compile_definitions(lego1 PUBLIC EXTENSIONS)
  target_sources(lego1 PRIVATE
    extensions/src/extensions.cpp
    extensions/src/siloader.cpp
    extensions/src/textureloader.cpp
  )
endif()

if (ISLE_BUILD_APP)

  if (ANDROID)
    function(add_executable TARGET PLATFORM)
      add_library(${TARGET} SHARED ${ARGN})
    endfunction()
  endif()

  add_executable(isle WIN32
    ISLE/res/isle.rc
    ISLE/isleapp.cpp
    ISLE/islefiles.cpp
    ${CMAKE_SOURCE_DIR}/ISLE/res/arrow_bmp.h
    ${CMAKE_SOURCE_DIR}/ISLE/res/busy_bmp.h
    ${CMAKE_SOURCE_DIR}/ISLE/res/no_bmp.h
  )
  list(APPEND isle_targets isle)
  if (WIN32)
    add_custom_command(TARGET isle POST_BUILD
      COMMAND "${CMAKE_COMMAND}" -E copy $<TARGET_RUNTIME_DLLS:isle> "$<TARGET_FILE_DIR:isle>"
      COMMAND_EXPAND_LISTS
    )
  endif()

  target_compile_definitions(isle PRIVATE ISLE_APP)

  # Use internal DirectX 5 if required
  target_link_libraries(isle PRIVATE $<$<BOOL:${ISLE_USE_DX5}>:DirectX5::DirectX5>)

  # Link SDL and iniparser
  target_link_libraries(isle PRIVATE SDL3::SDL3 Isle::iniparser)

  # Allow unconditional include of miniwin/miniwindevice.h
  target_link_libraries(isle PRIVATE miniwin-headers)

  # Vector math
  target_link_libraries(isle PRIVATE Vec::Vec)

  # Link DSOUND and WINMM
  if (WIN32)
    target_link_libraries(isle PRIVATE winmm)
  endif()
  # Link LEGO1
  target_link_libraries(isle PRIVATE lego1)
  if(ISLE_DEBUG)
    target_sources(isle PRIVATE
      ISLE/isledebug.cpp
    )
    target_compile_definitions(isle PRIVATE ISLE_DEBUG)
    target_link_libraries(isle PRIVATE imgui)
    find_path(valgrind_INCLUDE_PATH NAMES valgrind/callgrind.h)
    if(valgrind_INCLUDE_PATH)
      # Run isle under valgrind to create a profile. Use e.g. kcachegrind to view the profile.
      # > valgrind --tool=callgrind --dump-instr=yes --simulate-cache=yes --collect-jumps=yes \
      # >         --collect-atstart=no --instr-atstart=no ./isle --debug
      target_compile_definitions(isle PRIVATE ISLE_VALGRIND)
      target_include_directories(isle PRIVATE ${valgrind_INCLUDE_PATH})
    endif()
  endif()
  if(EMSCRIPTEN)
    target_sources(isle PRIVATE
      ISLE/emscripten/config.cpp
      ISLE/emscripten/events.cpp
      ISLE/emscripten/filesystem.cpp
      ISLE/emscripten/haptic.cpp
      ISLE/emscripten/messagebox.cpp
      ISLE/emscripten/window.cpp
    )
    target_compile_definitions(isle PRIVATE "ISLE_EMSCRIPTEN_HOST=\"${ISLE_EMSCRIPTEN_HOST}\"")
    set_property(TARGET isle PROPERTY SUFFIX ".html")
  endif()
  if(NINTENDO_3DS)
    target_sources(isle PRIVATE
      ISLE/3ds/apthooks.cpp
      ISLE/3ds/config.cpp
    )
  endif()
  if(WINDOWS_STORE)
    target_sources(isle PRIVATE
      ISLE/xbox_one_series/config.cpp
    )
  endif()
  if (IOS)
    target_sources(isle PRIVATE
      ISLE/ios/config.cpp
    )
  endif()
  if (ANDROID)
    target_sources(isle PRIVATE
            ISLE/android/config.cpp
    )
  endif()
  if(Python3_FOUND)
    if(NOT DEFINED PYTHON_PIL_AVAILABLE)
      execute_process(
        COMMAND ${Python3_EXECUTABLE} -c "import PIL; print('pil')"
        RESULT_VARIABLE PIL_RESULT
        OUTPUT_VARIABLE PIL_OUTPUT
        ERROR_QUIET
        OUTPUT_STRIP_TRAILING_WHITESPACE
      )
      if(PIL_RESULT EQUAL 0 AND PIL_OUTPUT STREQUAL "pil")
        set(PIL_AVAILABLE 1)
      else()
        message(STATUS "Python PIL not found, using pre-generated headers.")
        set(PIL_AVAILABLE 0)
      endif()
      set(PYTHON_PIL_AVAILABLE ${PIL_AVAILABLE} CACHE BOOL "Is Python3 Pillow available?")
    endif()
    if(PYTHON_PIL_AVAILABLE)
      add_custom_command(
        OUTPUT
          ${CMAKE_SOURCE_DIR}/ISLE/res/arrow_bmp.h
          ${CMAKE_SOURCE_DIR}/ISLE/res/busy_bmp.h
          ${CMAKE_SOURCE_DIR}/ISLE/res/no_bmp.h
        COMMAND ${Python3_EXECUTABLE} tools/curpng2h.py ISLE/res/arrow.png ISLE/res/busy.png ISLE/res/no.png
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        DEPENDS
          ${CMAKE_SOURCE_DIR}/tools/curpng2h.py
          ${CMAKE_SOURCE_DIR}/ISLE/res/arrow.png
          ${CMAKE_SOURCE_DIR}/ISLE/res/busy.png
          ${CMAKE_SOURCE_DIR}/ISLE/res/no.png
      )
    endif()
  endif()
endif()

if (ISLE_BUILD_CONFIG)
  find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
  qt_standard_project_setup()
  qt_add_executable(isle-config WIN32
    LEGO1/mxdirectx/mxdirectxinfo.cpp
    LEGO1/mxdirectx/legodxinfo.cpp
    CONFIG/config.cpp
    CONFIG/AboutDlg.cpp
    CONFIG/MainDlg.cpp
    CONFIG/detectdx5.cpp
    CONFIG/res/config.rc
    CONFIG/res/config.qrc
  )
  target_link_libraries(isle-config PRIVATE Qt6::Core Qt6::Widgets)
  set_property(TARGET isle-config PROPERTY AUTOMOC ON)
  set_property(TARGET isle-config PROPERTY AUTORCC ON)
  set_property(TARGET isle-config PROPERTY AUTOUIC ON)
  set_property(TARGET isle-config PROPERTY AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/CONFIG/res")
  list(APPEND isle_targets isle-config)
  target_compile_definitions(isle-config PRIVATE _AFXDLL MXDIRECTX_FOR_CONFIG)
  target_include_directories(isle-config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/LEGO1")
  target_include_directories(isle-config PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/util>")
  if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
    target_link_libraries(isle-config PRIVATE DirectX5::DirectX5)
  endif()
  target_compile_definitions(isle-config PRIVATE DIRECT3D_VERSION=0x500)
  target_link_libraries(isle-config PRIVATE SDL3::SDL3 Isle::iniparser)
  if (NOT ISLE_MINIWIN)
    target_link_libraries(isle-config PRIVATE ddraw dxguid)
  endif()
endif()

if(ISLE_BUILD_ASSETS)
  message(STATUS "Asset building is enabled")
  set(GENERATED_ASSETS_DIR "${CMAKE_BINARY_DIR}/assets")
  set(GENERATED_ASSETS_DEPFILE "${GENERATED_ASSETS_DIR}/assets.d")

  add_executable(asset_generator EXCLUDE_FROM_ALL
    assets/main.cpp
  )
  target_link_libraries(asset_generator PRIVATE libweaver)
  target_include_directories(asset_generator PRIVATE "${CMAKE_SOURCE_DIR}/util" "${CMAKE_SOURCE_DIR}/LEGO1/omni/include" "${CMAKE_SOURCE_DIR}/LEGO1" "${CMAKE_SOURCE_DIR}/LEGO1/lego/sources")

  add_custom_command(
    OUTPUT  ${GENERATED_ASSETS_DIR}/.stamp
    DEPFILE ${GENERATED_ASSETS_DEPFILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${GENERATED_ASSETS_DIR}
    COMMAND $<TARGET_FILE:asset_generator> ${GENERATED_ASSETS_DIR} ${GENERATED_ASSETS_DEPFILE}
    COMMAND ${CMAKE_COMMAND} -E touch ${GENERATED_ASSETS_DIR}/.stamp
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/assets
    DEPENDS asset_generator
    COMMENT "Generating assets into ${GENERATED_ASSETS_DIR}/"
    VERBATIM
  )

  add_custom_target(build_assets ALL
    DEPENDS ${GENERATED_ASSETS_DIR}/.stamp
  )

  install(DIRECTORY ${GENERATED_ASSETS_DIR}/
    DESTINATION assets
  )
endif()

if (ISLE_MINIWIN)
  set_property(TARGET ${isle_targets} APPEND PROPERTY LINK_LIBRARIES "miniwin")
endif()

if (MSVC)
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "15")
    set_property(TARGET ${isle_targets} APPEND PROPERTY COMPILE_DEFINITIONS "_CRT_SECURE_NO_WARNINGS")
    if (TARGET isle)
      target_compile_definitions(isle PRIVATE "_CRT_SECURE_NO_WARNINGS")
    endif()
    if (TARGET isle-config)
      target_compile_definitions(isle-config PRIVATE "_CRT_SECURE_NO_WARNINGS")
    endif()
    if (TARGET iniparser-static)
      target_compile_definitions(iniparser-static PRIVATE "_CRT_SECURE_NO_WARNINGS")
    endif()
    if (TARGET libsmacker)
      target_compile_definitions(libsmacker PRIVATE "_CRT_SECURE_NO_WARNINGS")
    endif()
  endif()
  # Visual Studio 2017 version 15.7 needs "/Zc:__cplusplus" for __cplusplus
  if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "19.14.26428")
    set_property(TARGET ${isle_targets} APPEND PROPERTY COMPILE_OPTIONS "-Zc:__cplusplus")
    if (TARGET isle)
      target_compile_options(isle PRIVATE "-Zc:__cplusplus")
    endif()
    if (TARGET isle-config)
      target_compile_options(isle-config PRIVATE "-Zc:__cplusplus")
    endif()
    if (TARGET asset_generator)
      target_compile_options(asset_generator PRIVATE "-Zc:__cplusplus")
    endif()
  endif()
endif()

if (MSVC)
  target_link_options(isle PRIVATE "/SAFESEH:NO")
  target_link_options(lego1 PRIVATE "/SAFESEH:NO")
endif()

find_program(CLANGFORMAT_BIN NAMES clang-format)
if (EXISTS "${CLANGFORMAT_BIN}")
  execute_process(COMMAND "${CLANGFORMAT_BIN}" --version
    OUTPUT_VARIABLE "CLANGFORMAT_VERSION_OUTPUT"
    RESULT_VARIABLE "CLANGFORMAT_RESULT"
  )
  if (CLANGFORMAT_RESULT EQUAL 0 AND CLANGFORMAT_VERSION_OUTPUT MATCHES "version ([0-9\\.]+)")
    set(CLANGFORMAT_VERSION "${CMAKE_MATCH_1}")
    set(CLANGFORMAT_VERSION_REQUIRED "17.0")
    message(DEBUG "Found clang-format version ${CLANGFORMAT_VERSION} (needs ${CLANGFORMAT_VERSION_REQUIRED}")
    if (CLANGFORMAT_VERSION VERSION_GREATER_EQUAL "${CLANGFORMAT_VERSION_REQUIRED}")
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

set(install_extra_targets)
if(DOWNLOAD_DEPENDENCIES)
  get_property(sdl3_type TARGET SDL3::SDL3 PROPERTY TYPE)
  if(sdl3_type STREQUAL "SHARED_LIBRARY" AND NOT ANDROID)
    list(APPEND install_extra_targets "SDL3-shared")
  endif()
endif()

if(MSVC)
  set(CMAKE_INSTALL_BINDIR "." CACHE PATH "Binary install directory")
  set(CMAKE_INSTALL_LIBDIR "." CACHE PATH "Binary install directory")
else()
  include(GNUInstallDirs)
endif()

string(REPLACE ";" "-" ISLE_CPUS_STRING "${ISLE_CPUS}")
string(TOLOWER "${ISLE_CPUS_STRING}" ISLE_CPUS_STRING)
if (WINDOWS_STORE)
  set(ISLE_PACKAGE_NAME "Xbox_One_Series_XS-${ISLE_CPUS_STRING}" CACHE STRING "Platform name of the package")
else()
  set(ISLE_PACKAGE_NAME "${CMAKE_SYSTEM_NAME}-${ISLE_CPUS_STRING}" CACHE STRING "Platform name of the package")
endif()
if(BUILD_SHARED_LIBS)
  list(APPEND install_extra_targets lego1)
endif()
if (NOT (NINTENDO_3DS OR WINDOWS_STORE))
  install(TARGETS isle ${install_extra_targets}
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
    BUNDLE DESTINATION "."
  )
endif()
if (ISLE_BUILD_CONFIG)
  if(WIN32)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
    if(WINDEPLOYQT_EXECUTABLE)
      install(CODE "message(STATUS \"Running windeployqt with minimal dependencies\")
        execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
          \"$<TARGET_FILE:isle-config>\"
          --dir QTLibs
          --no-compiler-runtime
          --no-opengl-sw
          --no-system-d3d-compiler
          --no-translations
          --no-quick-import
        )"
      )
      install(DIRECTORY "Build/QTLibs/" DESTINATION "${CMAKE_INSTALL_BINDIR}")
    else()
      message(STATUS "windeployqt not found: Qt binaries will not be installed")
    endif()
  endif()
  install(TARGETS isle-config
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
    BUNDLE DESTINATION "."
  )
endif()
if(EMSCRIPTEN)
  install(FILES "$<TARGET_FILE_DIR:isle>/isle.js" "$<TARGET_FILE_DIR:isle>/isle.wasm"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
  )
endif()

add_subdirectory(packaging)

set(CPACK_PACKAGE_DIRECTORY "dist")
set(CPACK_PACKAGE_FILE_NAME "isle-${PROJECT_VERSION}-${ISLE_PACKAGE_NAME}")
if(NINTENDO_3DS)
  find_program(BANNERTOOL bannertool)
  find_program(MAKEROM makerom)

  ctr_generate_smdh(isle.smdh
    NAME "LEGO Island"
    TITLE "LEGO Island"
    DESCRIPTION "LEGO Island for the Nintendo 3DS"
    AUTHOR "isledecomp/isle-portable"
    VERSION "${PROJECT_VERSION}"
    ICON "${CMAKE_SOURCE_DIR}/packaging/3ds/icon.png"
  )

  ctr_create_3dsx(isle SMDH isle.smdh)
  if(BANNERTOOL AND MAKEROM)
    add_custom_command(
      OUTPUT "isle.bnr"
      COMMAND "${BANNERTOOL}" makebanner
        -i "${CMAKE_SOURCE_DIR}/packaging/3ds/banner.png"
        -a "${CMAKE_SOURCE_DIR}/packaging/3ds/banner.wav"
        -o "isle.bnr"
      DEPENDS "${CMAKE_SOURCE_DIR}/packaging/3ds/banner.png" "${CMAKE_SOURCE_DIR}/packaging/3ds/banner.wav"
      VERBATIM
    )

    add_custom_command(
      OUTPUT "isle.cia"
      COMMAND "${MAKEROM}"
        -f cia
        -exefslogo
        -o "isle.cia"
        -rsf "${CMAKE_SOURCE_DIR}/packaging/3ds/template.rsf"
        -major "${CMAKE_PROJECT_VERSION_MAJOR}"
        -minor "${CMAKE_PROJECT_VERSION_MINOR}"
        -micro 0
        -icon "isle.smdh"
        -banner "isle.bnr"
        -elf "isle.elf"
      DEPENDS "${CMAKE_SOURCE_DIR}/packaging/3ds/template.rsf" "isle.smdh" "isle.bnr"
      COMMENT "Building CIA executable target isle.cia"
      VERBATIM
    )

    add_custom_target("isle_cia" ALL DEPENDS "isle.cia" isle)
    install(FILES "$<TARGET_FILE_DIR:isle>/isle.cia" DESTINATION "${CMAKE_INSTALL_BINDIR}")
  endif()
  install(FILES "$<TARGET_FILE_DIR:isle>/isle.3dsx" DESTINATION "${CMAKE_INSTALL_BINDIR}")
endif()
if(WINDOWS_STORE)
  install(
    DIRECTORY
      "${CMAKE_BINARY_DIR}/AppPackages/isle/"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    FILES_MATCHING PATTERN "*/Dependencies/x64/Microsoft.VCLibs.x64*.14.00.appx"
      PATTERN "*/*.msix"
      PATTERN "*/*.msixbundle")
endif()
if(MSVC OR IOS)
  set(CPACK_GENERATOR ZIP)
  if(IOS)
    set(CPACK_ARCHIVE_FILE_EXTENSION ".ipa")
    set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
  endif()
elseif(APPLE AND NOT IOS)
  set(CPACK_GENERATOR DragNDrop)
else()
  set(CPACK_GENERATOR TGZ)
endif()

include(CPack)
