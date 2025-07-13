set(APP_ID "org.legoisland.Isle")
set(APP_NAME "Isle Portable")
set(APP_SUMMARY "Portable version of the LEGO Island Decompilation Project")
set(APP_SPDX "LGPL-3.0-or-later")

string(TIMESTAMP BUILD_DATE UTC)

file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/icons)
file(COPY_FILE icons/isle.svg ${CMAKE_BINARY_DIR}/icons/${APP_ID}.svg)

# The following will need to be refined if we wish to post actual releases to a repo such as Flathub
if(DEFINED ENV{GITHUB_ACTIONS} AND ENV{GITHUB_ACTIONS} EQUAL TRUE)
    # Use the sequential run# of the current pipeline when running in GH Actions
    set(SEMANTIC_VERSION "${PROJECT_VERSION}~build$ENV{GITHUB_RUN_NUMBER}")
else()
    # Don't worry about the build number for local builds
    set(SEMANTIC_VERSION "${PROJECT_VERSION}")
endif()

if(LINUX)
    add_subdirectory(linux)
endif()

if(WINDOWS_STORE)
    add_subdirectory(UWP)
endif()

if(APPLE AND NOT IOS)
    add_subdirectory(macos)
endif()

if(IOS)
    add_subdirectory(ios)
endif()
