set(CMAKE_C_CLANG_TIDY)
set(CMAKE_CXX_CLANG_TIDY)

if(DOWNLOAD_DEPENDENCIES)
    include(FetchContent)
    FetchContent_Declare(
        miniaudio
        URL https://github.com/mackron/miniaudio/archive/refs/tags/0.11.22.tar.gz
        URL_MD5 4944268151ad037f148b089237566d05
    )
    FetchContent_MakeAvailable(miniaudio)
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
        URL https://github.com/isledecomp/SIEdit/archive/ae447259389f3bf8273c7e7a4844743faf7cbdb8.tar.gz
        URL_MD5 dee68424fde8db6d5cef3b9034a8151f
    )
else()
    set(libweaver_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/libweaver")
endif()

add_library(libweaver STATIC
    ${libweaver_SOURCE_DIR}/lib/core.cpp
    ${libweaver_SOURCE_DIR}/lib/file.cpp
    ${libweaver_SOURCE_DIR}/lib/interleaf.cpp
    ${libweaver_SOURCE_DIR}/lib/object.cpp
    ${libweaver_SOURCE_DIR}/lib/sitypes.cpp
)
add_library(libweaver::libweaver ALIAS libweaver)
target_include_directories(libweaver PUBLIC ${libweaver_SOURCE_DIR}/lib)
