cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

enable_language(ASM)

find_program(PSP2CGC NAMES ${CMAKE_CURRENT_SOURCE_DIR}/psp2cgc.exe psp2cgc.exe psp2cgc)
find_program(PSP2SHADERPERF NAMES ${CMAKE_CURRENT_SOURCE_DIR}/psp2shaderperf.exe psp2shaderperf.exe psp2shaderperf)

list(APPEND CGC_COMMON_FLAGS "-Wperf" "-cache" "-cachedir" "${CMAKE_CURRENT_BINARY_DIR}/cache" "-W4" "-Wsuppress=5206,5203")

make_directory("${CMAKE_CURRENT_BINARY_DIR}/cache")

# compile .cg to .gxp if psp2cgc is found
macro(COMPILE_SHADER INPUT_CG OUTPUT_GXP PROFILE)
  set(EXTRA_FLAGS "${ARGN}")

  set(INPUT_CG_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${INPUT_CG}")
  set(OUTPUT_GXP_PATH "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_GXP}")
  set(TRACKED_GXP_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${OUTPUT_GXP}")

  if(PSP2CGC)
      if(EXISTS ${TRACKED_GXP_PATH})
        file(COPY ${TRACKED_GXP_PATH} DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
      endif()
      add_custom_command(
        OUTPUT "${OUTPUT_GXP_PATH}"
        COMMAND "${PSP2CGC}" ${CGC_COMMON_FLAGS} -profile "${PROFILE}" ${EXTRA_FLAGS} "${INPUT_CG_PATH}" -o "${OUTPUT_GXP_PATH}"
        COMMAND ${CMAKE_COMMAND} -E copy "${OUTPUT_GXP_PATH}" "${TRACKED_GXP_PATH}"
        DEPENDS "${INPUT_CG_PATH}"
        COMMENT "Compiling ${INPUT_CG} -> ${OUTPUT_GXP} ${PROFILE}"
      )
  else()
      if(NOT EXISTS "${TRACKED_GXP_PATH}")
        message(FATAL_ERROR "missing shader ${TRACKED_GXP_PATH} ${INPUT_CG}, but psp2cgc.exe not found")
      endif()
      add_custom_command(
        OUTPUT "${OUTPUT_GXP_PATH}"
        COMMAND /usr/bin/env bash
        ARGS "-c" 
          "if [ \"${INPUT_CG_PATH}\" -nt \"${TRACKED_GXP_PATH}\" ]; then 
              echo 'warning: ${INPUT_CG} changed but dont have psp2cgc, cant recompile' >&2; 
          fi"
        COMMAND ${CMAKE_COMMAND} -E copy "${TRACKED_GXP_PATH}" "${OUTPUT_GXP_PATH}"
        DEPENDS "${INPUT_CG_PATH}" "${TRACKED_GXP_PATH}"
        VERBATIM
        COMMENT "Copy ${TRACKED_GXP_PATH} -> ${OUTPUT_GXP} ${PROFILE}"
      )
  endif()
  set(TARGET_NAME "compile_${OUTPUT_GXP}")
  string(REPLACE "." "_" TARGET_NAME "${TARGET_NAME}")
  add_custom_target("${TARGET_NAME}" DEPENDS "${OUTPUT_GXP_PATH}")
  add_dependencies(gxm_shaders_compile "${TARGET_NAME}")
endmacro()


# analyze gxp to create a .perf.txt
macro(GENERATE_PERF GXP_FILE)
  set(GXP_PATH "${CMAKE_CURRENT_BINARY_DIR}/${GXP_FILE}")
  string(REPLACE ".gxp" ".perf.txt" PERF_FILE_NAME "${GXP_FILE}")
  set(PERF_FILE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${PERF_FILE_NAME}")

  add_custom_command(
    OUTPUT "${PERF_FILE_PATH}"
    COMMAND "${PSP2SHADERPERF}" -stats -symbols -disasm "${GXP_PATH}" > "${PERF_FILE_PATH}"
    DEPENDS "${GXP_PATH}"
    COMMENT "Generating performance analysis for ${GXP_FILE}"
  )
  set(TARGET_NAME "perf_${PERF_FILE_NAME}")
  string(REPLACE "." "_" TARGET_NAME "${TARGET_NAME}")
  add_custom_target("${TARGET_NAME}" DEPENDS "${PERF_FILE_PATH}")
  add_dependencies(gxm_perfs "${TARGET_NAME}" )
endmacro()



add_library(gxm_shaders STATIC gxm_shaders.s)

target_include_directories(gxm_shaders PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

add_custom_target(gxm_shaders_compile)
add_dependencies(gxm_shaders gxm_shaders_compile)

COMPILE_SHADER(plane.vert.cg plane.vert.gxp sce_vp_psp2)
COMPILE_SHADER(color.frag.cg color.frag.gxp sce_fp_psp2)
COMPILE_SHADER(image.frag.cg image.frag.gxp sce_fp_psp2)
COMPILE_SHADER(main.vert.cg main.vert.gxp sce_vp_psp2)
COMPILE_SHADER(main.frag.cg main.color.frag.gxp sce_fp_psp2)
COMPILE_SHADER(main.frag.cg main.texture.frag.gxp sce_fp_psp2 -DTEXTURED=1)


# .perf.txt
if(PSP2SHADERPERF)
  add_custom_target(gxm_perfs)
  add_dependencies(gxm_shaders gxm_perfs)
  GENERATE_PERF(plane.vert.gxp)
  GENERATE_PERF(color.frag.gxp)
  GENERATE_PERF(image.frag.gxp)
  GENERATE_PERF(main.vert.gxp)
  GENERATE_PERF(main.color.frag.gxp)
  GENERATE_PERF(main.texture.frag.gxp)
else()
  message(STATUS "psp2shaderperf not found")
endif()


