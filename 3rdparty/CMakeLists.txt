add_library(miniaudio STATIC
    miniaudio/extras/miniaudio_split/miniaudio.c
)
target_include_directories(miniaudio PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/miniaudio/extras/miniaudio_split")
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

add_library(libsmacker STATIC
    libsmacker/smacker.c
)
set_property(TARGET libsmacker PROPERTY ARCHIVE_OUTPUT_NAME "libsmacker$<$<CONFIG:Debug>:d>")
target_include_directories(libsmacker PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/libsmacker")
