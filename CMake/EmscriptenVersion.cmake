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
file(WRITE "${OUTPUT_DIR}/sourceMappingURL" "/symbols/${H}/isle.wasm.map")
