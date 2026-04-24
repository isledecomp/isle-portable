set(CMAKE_C_CLANG_TIDY)
set(CMAKE_CXX_CLANG_TIDY)

if(DOWNLOAD_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Declare(
        miniaudio
        URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.25.tar.gz
        URL_HASH MD5=6fae6da8f30afb3ddcba26fcaa64f540
    )
    block()
        set(BUILD_SHARED_LIBS OFF)
        FetchContent_MakeAvailable(miniaudio)
    endblock()
else()
    add_library(miniaudio STATIC
        miniaudio/extras/miniaudio_split/miniaudio.c
    )
    target_include_directories(miniaudio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/extras/miniaudio_split")
endif()

# Disable most features since we don't need them.
target_compile_definitions(miniaudio PUBLIC
    MA_ENABLE_ONLY_SPECIFIC_BACKENDS
    MA_NO_DECODING
    MA_NO_ENCODING
    MA_NO_WAV
    MA_NO_FLAC
    MA_NO_MP3
    MA_NO_DEVICE_IO
    MA_NO_RESOURCE_MANAGER
    MA_NO_GENERATION
    MA_NO_THREADING
    MA_NO_RUNTIME_LINKING
)

if(DOWNLOAD_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Declare(
        libsmacker
        URL https://github.com/foxtacles/libsmacker/archive/b3d4e97e0c95d5259d858495a5addd2d93bce5f4.tar.gz
        URL_MD5 7f822319c489ec1a8e41c9f1e2629195
    )
    FetchContent_MakeAvailable(libsmacker)
else()
    set(libsmacker_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libsmacker")
endif()

add_library(libsmacker STATIC
    ${libsmacker_SOURCE_DIR}/smacker.c
)
target_include_directories(libsmacker PUBLIC ${libsmacker_SOURCE_DIR})

if(DOWNLOAD_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Populate(
        libweaver
        URL https://github.com/isledecomp/SIEdit/archive/17c7736a6ff31413f1e74ab4e989011b545b6926.tar.gz
        URL_MD5 04edbc974df8884f283d920ded10f1f6
    )
    add_library(libweaver STATIC
        ${libweaver_SOURCE_DIR}/lib/core.cpp
        ${libweaver_SOURCE_DIR}/lib/file.cpp
        ${libweaver_SOURCE_DIR}/lib/interleaf.cpp
        ${libweaver_SOURCE_DIR}/lib/object.cpp
        ${libweaver_SOURCE_DIR}/lib/sitypes.cpp
    )
    add_library(libweaver::libweaver ALIAS libweaver)
    target_compile_definitions(libweaver PRIVATE $<$<BOOL:${WIN32}>:NOMINMAX>)
    target_include_directories(libweaver PUBLIC ${libweaver_SOURCE_DIR}/include/libweaver)
    set_property(TARGET libweaver PROPERTY CXX_STANDARD 98)
    set_property(TARGET libweaver PROPERTY CXX_STANDARD_REQUIRED ON)
endif()

if(ISLE_USE_LWS)
    if(DOWNLOAD_DEPENDENCIES)
        include(FetchContent)

        # Fetch mbedTLS for TLS/WSS support in libwebsockets
        FetchContent_Declare(
            mbedtls
            URL https://github.com/Mbed-TLS/mbedtls/releases/download/mbedtls-3.6.5/mbedtls-3.6.5.tar.bz2
            URL_MD5 bc79602daf85f1cf35a686b53056de58
            # Patch cmake_minimum_required to avoid deprecation error with -Werror=dev
            PATCH_COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_SOURCE_DIR}/patch_mbedtls_cmake.cmake
        )
        block()
            set(ENABLE_TESTING OFF CACHE BOOL "" FORCE)
            set(ENABLE_PROGRAMS OFF CACHE BOOL "" FORCE)
            set(MBEDTLS_FATAL_WARNINGS OFF CACHE BOOL "" FORCE)
            set(BUILD_SHARED_LIBS OFF)
            FetchContent_MakeAvailable(mbedtls)

            # Point lws at the mbedTLS targets so it skips its own find logic
            # Must be inside block() to access mbedtls_SOURCE_DIR/mbedtls_BINARY_DIR
            set(LWS_MBEDTLS_INCLUDE_DIRS
                "${mbedtls_SOURCE_DIR}/include;${mbedtls_BINARY_DIR}/include"
                CACHE STRING "" FORCE
            )
            set(LWS_MBEDTLS_LIBRARIES
                "mbedtls;mbedx509;mbedcrypto"
                CACHE STRING "" FORCE
            )
        endblock()

        FetchContent_Declare(
            libwebsockets
            GIT_REPOSITORY "https://github.com/warmcat/libwebsockets.git"
            GIT_TAG "v4.5-stable"
            EXCLUDE_FROM_ALL
        )
        block()
            set(LWS_WITH_SSL ON CACHE BOOL "" FORCE)
            set(LWS_WITH_MBEDTLS ON CACHE BOOL "" FORCE)
            set(LWS_WITH_EXPORT_LWSTARGETS OFF CACHE BOOL "" FORCE)
            # mbedTLS isn't built yet at configure time, so lws's check_function_exists
            # calls fail. Pre-set the results for APIs that exist in mbedTLS 3.x.
            set(LWS_HAVE_mbedtls_md_setup 1 CACHE INTERNAL "" FORCE)
            set(LWS_HAVE_mbedtls_rsa_complete 1 CACHE INTERNAL "" FORCE)
            set(LWS_WITH_SHARED OFF CACHE BOOL "" FORCE)
            set(LWS_WITH_STATIC ON CACHE BOOL "" FORCE)
            set(LWS_WITHOUT_TESTAPPS ON CACHE BOOL "" FORCE)
            set(LWS_WITH_MINIMAL_EXAMPLES OFF CACHE BOOL "" FORCE)
            # Disable treating warnings as errors in libwebsockets (GCC/Clang)
            set(DISABLE_WERROR ON CACHE BOOL "" FORCE)
            FetchContent_MakeAvailable(libwebsockets)
        endblock()
        # Disable MSVC /WX (warnings as errors) that lws sets unconditionally
        if(TARGET websockets AND MSVC)
            target_compile_options(websockets PRIVATE /WX-)
        endif()
    else()
        find_package(Libwebsockets REQUIRED)
    endif()
endif()

