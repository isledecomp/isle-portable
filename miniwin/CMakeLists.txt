add_library(miniwin-headers INTERFACE)
target_include_directories(miniwin-headers INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)

add_library(miniwin STATIC EXCLUDE_FROM_ALL
  # Core
  src/windows/windows.cpp

  # DDraw
  src/ddraw/ddpalette.cpp
  src/ddraw/ddraw.cpp
  src/ddraw/ddsurface.cpp
  src/ddraw/framebuffer.cpp

  # D3DRM
  src/d3drm/d3drm.cpp
  src/d3drm/d3drmdevice.cpp
  src/d3drm/d3drmframe.cpp
  src/d3drm/d3drmlight.cpp
  src/d3drm/d3drmmesh.cpp
  src/d3drm/d3drmtexture.cpp
  src/d3drm/d3drmviewport.cpp
  src/d3drm/d3drmrenderer.cpp
  src/internal/meshutils.cpp
)

target_compile_definitions(miniwin PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
)

list(APPEND GRAPHICS_BACKENDS USE_SOFTWARE_RENDER)
if(NOT USE_SDL2)
  list(APPEND GRAPHICS_BACKENDS USE_SDL_GPU)
endif()

if(NOT WINDOWS_STORE)
  find_package(OpenGL)
    if(OpenGL_FOUND)
      message(STATUS "Found OpenGL: enabling OpenGL 1.x renderer")
      target_sources(miniwin PRIVATE
      src/d3drm/backends/opengl1/actual.cpp
      src/d3drm/backends/opengl1/renderer.cpp
    )
    list(APPEND GRAPHICS_BACKENDS USE_OPENGL1)
    target_link_libraries(miniwin PRIVATE OpenGL::GL)
  else()
    message(STATUS "🧩 OpenGL 1.x support not enabled — needs OpenGL")
  endif()

  find_library(OPENGL_ES2_LIBRARY NAMES GLESv2)
  if(EMSCRIPTEN OR OPENGL_ES2_LIBRARY)
    message(STATUS "Found OpenGL: enabling OpenGL ES 2.x renderer")
    target_sources(miniwin PRIVATE src/d3drm/backends/opengles2/renderer.cpp)
    list(APPEND GRAPHICS_BACKENDS USE_OPENGLES2)
    if(OPENGL_ES2_LIBRARY)
      target_link_libraries(miniwin PRIVATE ${OPENGL_ES2_LIBRARY})
    endif()
  else()
    message(STATUS "🧩 OpenGL ES 2.x support not enabled")
  endif()

  find_library(OPENGL_ES3_LIBRARY NAMES GLESv3 GLESv2)
  if(EMSCRIPTEN OR OPENGL_ES3_LIBRARY)
    message(STATUS "Found OpenGL: enabling OpenGL ES 3.x renderer")
    target_sources(miniwin PRIVATE src/d3drm/backends/opengles3/renderer.cpp)
    list(APPEND GRAPHICS_BACKENDS USE_OPENGLES3)
    if(OPENGL_ES3_LIBRARY)
      target_link_libraries(miniwin PRIVATE ${OPENGL_ES3_LIBRARY})
    endif()
  else()
    message(STATUS "🧩 OpenGL ES 3.x support not enabled")
  endif()
endif()


if(NINTENDO_3DS)
  if(ISLE_DEBUG)
    find_library(CITRO3D_LIBRARY NAMES citro3dd)
  else()
    find_library(CITRO3D_LIBRARY NAMES citro3d)
  endif()
  if(CITRO3D_LIBRARY)
    message(STATUS "Found citro3d: enabling Citro3D renderer")
    target_sources(miniwin PRIVATE src/d3drm/backends/citro3d/renderer.cpp)
    ctr_add_shader_library(vshader src/d3drm/backends/citro3d/vshader.v.pica)
    dkp_add_embedded_binary_library(3ds_shaders vshader)
    target_link_libraries(miniwin PRIVATE ${CITRO3D_LIBRARY} 3ds_shaders)
    list(APPEND GRAPHICS_BACKENDS USE_CITRO3D)
  else()
    message(STATUS "🧩 Citro3D support not enabled")
  endif()
endif()

if(WIN32 AND NOT WINDOWS_STORE)
  target_sources(miniwin PRIVATE
    src/d3drm/backends/directx9/actual.cpp
    src/d3drm/backends/directx9/renderer.cpp
  )
  target_link_libraries(miniwin PRIVATE d3d9)
  list(APPEND GRAPHICS_BACKENDS USE_DIRECTX9)
endif()

if(WINDOWS_STORE)
  add_compile_definitions(WINDOWS_STORE)
endif()

if(USE_SDL_GPU IN_LIST GRAPHICS_BACKENDS)
  target_sources(miniwin PRIVATE
    src/d3drm/backends/sdl3gpu/renderer.cpp
    src/d3drm/backends/sdl3gpu/shaders/generated/ShaderIndex.cpp
  )
endif()

if(USE_SOFTWARE_RENDER IN_LIST GRAPHICS_BACKENDS)
  target_sources(miniwin PRIVATE
    src/d3drm/backends/software/renderer.cpp
  )
endif()

target_compile_definitions(miniwin PUBLIC MINIWIN)

target_include_directories(miniwin PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/internal
)

if(USE_SDL_GPU IN_LIST GRAPHICS_BACKENDS)
target_include_directories(miniwin
  PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/generated
)
endif()
target_link_libraries(miniwin PUBLIC miniwin-headers)

target_link_libraries(miniwin PRIVATE SDL3::SDL3)

target_compile_definitions(miniwin PUBLIC ${GRAPHICS_BACKENDS})


# Shader stuff
if(USE_SDL_GPU IN_LIST GRAPHICS_BACKENDS)
set(shader_src_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/src")
set(shader_gen_dir "${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/generated")
set(py_gencshadersource "${CMAKE_CURRENT_SOURCE_DIR}/src/d3drm/backends/sdl3gpu/shaders/gencshadersource.py")

set(miniwin_shaders
  "${shader_src_dir}/PositionColor.vert.hlsl"
  "${shader_src_dir}/SolidColor.frag.hlsl"
)
set(miniwin_shaders_DEPENDS
  "${shader_src_dir}/Common.hlsl"
)

if(ISLE_COMPILE_SHADERS)
  set(compiled_bindir "${CMAKE_CURRENT_BINARY_DIR}/shaders/compiled")
  set(generated_dirs "${compiled_bindir}/dxil" "${compiled_bindir}/msl" "${compiled_bindir}/spirv")

  add_custom_command(OUTPUT ${generated_dirs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory ${generated_dirs}
  )
  add_custom_target(create-generated-shader-directories DEPENDS ${generated_dirs})
endif()

target_sources(miniwin PRIVATE ${py_gencshadersource})

set(shader_variables )
set(shader_headers )
set(shader_stages )
foreach(shader_src IN LISTS miniwin_shaders)
  get_filename_component(filename_wle "${shader_src}" NAME_WLE)
  get_filename_component(shader_name "${filename_wle}" NAME_WLE)
  get_filename_component(src_format_ext "${shader_src}" LAST_EXT)
  get_filename_component(src_stage_ext "${filename_wle}" LAST_EXT)
  string(MAKE_C_IDENTIFIER "${filename_wle}" shader_variable)
  set(shader_json "${shader_src}.json")
  if(src_format_ext STREQUAL ".hlsl")
    set(src_format "HLSL")
  else()
    message(FATAL_ERROR "Unknown source format (${src_format_ext})")
  endif()
  if(src_stage_ext STREQUAL ".vert")
    set(shader_stage "vertex")
  elseif(src_stage_ext STREQUAL ".frag")
    set(shader_stage "fragment")
  elseif(src_stage_ext STREQUAL ".comp")
    set(shader_stage "compute")
    message(FATAL_ERROR "Compute shaders are not (yet) supported")
  else()
    message(FATAL_ERROR "Unknown stage (${src_stage_ext})")
  endif()
  set(dxil "${compiled_bindir}/dxil/${filename_wle}.dxil")
  set(msl "${compiled_bindir}/msl/${filename_wle}.msl")
  set(spirv "${compiled_bindir}/spirv/${filename_wle}.spv")
  set(shader_filename "${filename_wle}.h")
  set(shader_header "${shader_gen_dir}/${shader_filename}")
  if(ISLE_COMPILE_SHADERS)
    add_custom_command(OUTPUT "${dxil}"
      COMMAND "${SDL_SHADERCROSS_BIN}" "${shader_src}" -o "${dxil}"
        -s "${src_format}" -d "DXIL" -t "${shader_stage}"
      DEPENDS create-generated-shader-directories "${shader_src}" ${miniwin_shaders_DEPENDS})
    add_custom_command(OUTPUT "${msl}"
      COMMAND "${SDL_SHADERCROSS_BIN}" "${shader_src}" -o "${msl}"
        -s "${src_format}" -d "MSL" -t "${shader_stage}"
      DEPENDS create-generated-shader-directories "${shader_src}" ${miniwin_shaders_DEPENDS})
    add_custom_command(OUTPUT "${spirv}"
      COMMAND "${SDL_SHADERCROSS_BIN}" "${shader_src}" -o "${spirv}"
        -s "${src_format}" -d "SPIRV" -t "${shader_stage}"
      DEPENDS create-generated-shader-directories "${shader_src}" ${miniwin_shaders_DEPENDS})
    add_custom_command(OUTPUT "${shader_header}"
      COMMAND Python3::Interpreter "${py_gencshadersource}" "header" "--output" "${shader_header}"
        "--stage" "${shader_stage}" "--variable" "${shader_variable}"
        "--dxil" "${dxil}" "--msl" "${msl}" "--spirv" "${spirv}"
      DEPENDS "${py_gencshadersource}" "${dxil}" "${msl}" "${spirv}")
  endif()
  target_sources(miniwin PRIVATE "${shader_header}")
  list(APPEND shader_names "${shader_name}")
  list(APPEND shader_variables "${shader_variable}")
  list(APPEND shader_headers "${shader_header}")
  list(APPEND shader_stages "${shader_stage}")
  list(APPEND shader_jsons "${shader_json}")
endforeach()

set(index_cpp "${shader_gen_dir}/ShaderIndex.cpp")
set(index_h "${shader_gen_dir}/ShaderIndex.h")
endif()
if(ISLE_COMPILE_SHADERS)
  add_custom_command(OUTPUT "${index_h}" "${index_cpp}"
    COMMAND Python3::Interpreter "${py_gencshadersource}" "index"
      "--output" "${index_cpp}"
      "--header" "${index_h}"
      "--shader-names" ${shader_names}
      "--shader-variables" ${shader_variables}
      "--shader-headers" ${shader_headers}
      "--shader-stages" ${shader_stages}
      "--shader-jsons" ${shader_jsons}
    DEPENDS "${py_gencshadersource}" ${shader_headers} ${shader_jsons})
endif()
target_sources(miniwin PRIVATE "${index}" "${index_cpp}")
