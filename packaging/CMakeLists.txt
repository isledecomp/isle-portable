set(APP_ID "org.legoisland.Isle")
set(APP_NAME "Isle Portable")
set(APP_SUMMARY "Portable version of the LEGO Island Decompilation Project")
set(APP_SPDX "LGPL-3.0-or-later")
set(QT_VERSION 6.8)

string(TIMESTAMP BUILD_DATE UTC)

# Following block sourced from https://jonathanhamberg.com/post/cmake-embedding-git-hash/
# Get the latest abbreviated commit hash of the working branch
execute_process(
    COMMAND git log -1 --format=%h
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

# TODO: Generate minfied SVG icon and PNG variants

add_subdirectory(linux EXCLUDE_FROM_ALL)