cmake_minimum_required(VERSION 3.25...4.0 FATAL_ERROR)

project(isle-config LANGUAGES CXX C VERSION 0.1)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets)
qt_standard_project_setup()
qt_add_executable(isle-config WIN32
  ../../LEGO1/mxdirectx/mxdirectxinfo.cpp
  ../../LEGO1/mxdirectx/legodxinfo.cpp
  config.cpp
  AboutDlg.cpp
  MainDlg.cpp
  detectdx5.cpp
  res/config.rc
  res/config.qrc
)
target_link_libraries(isle-config PRIVATE Qt6::Core Qt6::Widgets)
set_property(TARGET isle-config PROPERTY AUTOMOC ON)
set_property(TARGET isle-config PROPERTY AUTORCC ON)
set_property(TARGET isle-config PROPERTY AUTOUIC ON)
set_property(TARGET isle-config PROPERTY AUTOUIC_SEARCH_PATHS "${CMAKE_CURRENT_SOURCE_DIR}/res")
target_compile_definitions(isle-config PRIVATE _AFXDLL MXDIRECTX_FOR_CONFIG)
target_include_directories(isle-config PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../../LEGO1")
target_include_directories(isle-config PUBLIC "$<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/util>")
if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 14)
  target_link_libraries(isle-config PRIVATE DirectX5::DirectX5)
endif()
target_compile_definitions(isle-config PRIVATE DIRECT3D_VERSION=0x500)
target_link_libraries(isle-config PRIVATE SDL3::SDL3 Isle::iniparser)
if (NOT ISLE_MINIWIN)
  target_link_libraries(isle-config PRIVATE ddraw dxguid)
endif()

if(WIN32)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt)
  if(WINDEPLOYQT_EXECUTABLE)
    install(CODE "message(STATUS \"Running windeployqt with minimal dependencies\")
      execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\"
        \"$<TARGET_FILE:isle-config>\"
        --dir ${CMAKE_CURRENT_BINARY_DIR}/QTLibs
        --no-compiler-runtime
        --no-opengl-sw
        --no-system-d3d-compiler
        --no-translations
        --no-quick-import
      )"
    )
    install(DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/QTLibs/" DESTINATION "${CMAKE_INSTALL_BINDIR}")
  else()
    message(STATUS "windeployqt not found: Qt binaries will not be installed")
  endif()
endif()
install(TARGETS isle-config
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
  BUNDLE DESTINATION "."
)

set(isle_targets ${isle_targets};isle-config PARENT_SCOPE)
