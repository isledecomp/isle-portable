cmake_policy(SET CMP0007 NEW)

execute_process(
  COMMAND git rev-parse --short HEAD
  WORKING_DIRECTORY "${SOURCE_DIR}"
  OUTPUT_VARIABLE H OUTPUT_STRIP_TRAILING_WHITESPACE ERROR_QUIET
)
if(NOT H)
  set(H "unknown")
endif()

# version.js — only rewrite if hash changed
set(JS_FILE "${OUTPUT_DIR}/version.js")
set(JS_CONTENT "Module[\"wasmVersion\"]=\"${H}\"")
if(EXISTS "${JS_FILE}")
  file(READ "${JS_FILE}" OLD)
  if("${OLD}" STREQUAL "${JS_CONTENT}")
    # sourceMappingURL uses same hash, skip both
    return()
  endif()
endif()
file(WRITE "${JS_FILE}" "${JS_CONTENT}")
set(SOURCE_MAP_URL "/symbols/${H}/isle.wasm.map")
string(LENGTH "${SOURCE_MAP_URL}" SOURCE_MAP_URL_LENGTH)
string(ASCII ${SOURCE_MAP_URL_LENGTH} SOURCE_MAP_URL_PREFIX)
file(WRITE "${OUTPUT_DIR}/sourceMappingURL" "${SOURCE_MAP_URL_PREFIX}${SOURCE_MAP_URL}")
