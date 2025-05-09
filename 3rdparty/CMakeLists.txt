set(CMAKE_C_CLANG_TIDY)

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

set_property(TARGET miniaudio PROPERTY ARCHIVE_OUTPUT_NAME "miniaudio$<$<CONFIG:Debug>:d>")
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
set_property(TARGET libsmacker PROPERTY ARCHIVE_OUTPUT_NAME "libsmacker$<$<CONFIG:Debug>:d>")
target_include_directories(libsmacker PUBLIC ${libsmacker_SOURCE_DIR})
