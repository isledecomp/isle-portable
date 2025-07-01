if(WIN32)
    set(__iniparser_shared_names "libiniparser.dll.a" "iniparser.lib")
    set(__iniparser_static_names "libiniparser.a" "libiniparser.lib")
else()
    if(APPLE)
        set(__iniparser_shared_names "libiniparser.dylib")
    else()
        set(__iniparser_shared_names "libiniparser.so")
    endif()
    set(__iniparser_static_names "libiniparser.a")
endif()

find_path(iniparser_INCLUDE_PATH
    NAMES "iniparser.h"
)

set(__iniparser_required_vars iniparser_INCLUDE_PATH)

set(__iniparser_shared_required FALSE)
set(__iniparser_static_required FALSE)

foreach(__comp ${iniparser_FIND_COMPONENTS})
    if(__comp STREQUAL "shared")
        set(__iniparser_shared_required TRUE)
        find_library(iniparser_shared_LIBRARY
            NAMES ${__iniparser_shared_names}
        )
        set(iniparser_shared_FOUND "${iniparser_shared_LIBRARY}")
        if(iniparser_FIND_REQUIRED_shared)
            list(APPEND __iniparser_required_vars iniparser_shared_LIBRARY)
        endif()
    endif()
    if(__comp STREQUAL "static")
        set(__iniparser_static_required TRUE)
        find_library(iniparser_static_LIBRARY
            NAMES ${__iniparser_static_names}
        )
        set(iniparser_static_FOUND "${iniparser_static_LIBRARY}")
        if(iniparser_FIND_REQUIRED_static)
            list(APPEND __iniparser_required_vars iniparser_static_LIBRARY)
        endif()
    endif()
endforeach()

if(NOT __iniparser_shared_required AND NOT __iniparser_static_required)
    list(APPEND __iniparser_required_vars iniparser_shared_LIBRARY)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(iniparser
    FOUND_VAR iniparser_FOUND
    REQUIRED_VARS ${__iniparser_required_vars}
    HANDLE_COMPONENTS
)

if(iniparser_FOUND)
    if(NOT TARGET iniparser-shared AND iniparser_shared_LIBRARY)
        if(WIN32)
            # We don't know the name and location of the dll, so use an imported target
            add_library(iniparser-shared UNKNOWN IMPORTED)
            set_property(TARGET iniparser-shared PROPERTY IMPORTED_IMPLIB "${iniparser_shared_LIBRARY}")
        else()
            add_library(iniparser-shared SHARED IMPORTED)
            set_property(TARGET iniparser-shared PROPERTY IMPORTED_LOCATION "${iniparser_shared_LIBRARY}")
        endif()
        set_property(TARGET iniparser-shared PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${iniparser_INCLUDE_PATH}")
    endif()
    if(NOT TARGET iniparser-static AND iniparser_static_LIBRARY)
        add_library(iniparser-static STATIC IMPORTED)
        set_property(TARGET iniparser-static PROPERTY IMPORTED_LOCATION "${iniparser_static_LIBRARY}")
        set_property(TARGET iniparser-static PROPERTY INTERFACE_INCLUDE_DIRECTORIES "${iniparser_INCLUDE_PATH}")
    endif()
endif()