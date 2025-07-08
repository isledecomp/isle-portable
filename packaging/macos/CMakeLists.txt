set(_icon_file AppIcon)
set(MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_ID})
set(MACOSX_BUNDLE_COPYRIGHT ${APP_SPDX})
set(MACOSX_ISLE_BUNDLE_NAME isle)
set(MACOSX_ISLE_BUNDLE_DISPLAY_NAME ${APP_NAME})
set(MACOSX_CONFIG_BUNDLE_NAME isle-config)
set(MACOSX_CONFIG_BUNDLE_DISPLAY_NAME "Configure LEGO Island")
set(MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")

# TODO: darwin < 9
set(MACOSX_BUNDLE_REQUIRED_PLATFORM Carbon)

if(ISLE_BUILD_APP)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/isle/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/isle/Info.plist"
    @ONLY
  )
  set(RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/isle/${_icon_file}.icns")
  target_sources(isle PRIVATE ${RESOURCE_FILES})
  set_target_properties(isle PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "${_icon_file}.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/isle/Info.plist"
    RESOURCE ${RESOURCE_FILES})
  install(TARGETS isle DESTINATION ./)
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(${CMAKE_BINARY_DIR}/${MACOSX_ISLE_BUNDLE_NAME}.app \"\" \"\")
    "
    COMPONENT Runtime)
endif()
if(ISLE_BUILD_CONFIG)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config/Info.plist"
    @ONLY
  )
  set(RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/${_icon_file}.icns")
  target_sources(isle-config PRIVATE ${RESOURCE_FILES})
  set_target_properties(isle-config PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "${_icon_file}.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/config/Info.plist"
    RESOURCE ${RESOURCE_FILES})
  install(TARGETS isle-config DESTINATION ./)
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(${CMAKE_BINARY_DIR}/${MACOSX_CONFIG_BUNDLE_NAME}.app \"\" \"\")
    "
    COMPONENT Runtime)
  qt_generate_deploy_app_script(
    TARGET isle-config
    OUTPUT_SCRIPT deploy_script
    NO_COMPILER_RUNTIME
    NO_TRANSLATIONS
  )
  install(SCRIPT "${deploy_script}")
  install(CODE "
    execute_process(COMMAND /usr/bin/install_name_tool
      -add_rpath \"@executable_path/../Frameworks\"
      \"\$\{CMAKE_INSTALL_PREFIX\}/isle-config.app/Contents/MacOS/isle-config\")
  ")
  install(CODE "
    execute_process(COMMAND /usr/bin/codesign
      --force --deep --sign - --timestamp
      \"\$\{CMAKE_INSTALL_PREFIX\}/isle-config.app/Contents/MacOS/isle-config\")
  ")
endif()
