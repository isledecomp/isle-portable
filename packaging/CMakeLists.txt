set(APP_ID "org.legoisland.Isle")
set(APP_NAME "Isle Portable")
set(APP_SUMMARY "Portable version of the LEGO Island Decompilation Project")
set(APP_SPDX "LGPL-3.0-or-later")

string(TIMESTAMP BUILD_DATE UTC)

# The following will need to be refined if we wish to post actual releases to a repo such as Flathub
if(DEFINED ENV{GITHUB_RUN_ATTEMPT})
    # Use the sequential run# of the current pipeline when running in GH Actions
    set(SEMANTIC_VERSION "${PROJECT_VERSION}~build$ENV{GITHUB_RUN_ATTEMPT}")
else()
    # Don't worry about the build number for local builds
    set(SEMANTIC_VERSION "${PROJECT_VERSION}")
endif()

# Following block sourced from https://jonathanhamberg.com/post/cmake-embedding-git-hash/
# Get the latest abbreviated commit hash of the working branch
# execute_process(
#     COMMAND git log -1 --format=%h
#     WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#     OUTPUT_VARIABLE GIT_HASH
#     OUTPUT_STRIP_TRAILING_WHITESPACE
# )

if(LINUX)
    add_subdirectory(linux)
endif()