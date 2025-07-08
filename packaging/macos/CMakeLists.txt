set(_icon_file AppIcon)
set(MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_ID})
set(MACOSX_BUNDLE_COPYRIGHT ${APP_SPDX})
set(ISLE_TARGET_NAME isle)
set(MACOSX_ISLE_BUNDLE_NAME ${APP_NAME}) # Do note that it can be up to 15 characters long
set(MACOSX_ISLE_BUNDLE_DISPLAY_NAME ${APP_NAME})
set(CONFIG_TARGET_NAME isle-config)
set(MACOSX_CONFIG_BUNDLE_NAME "Config Isle") # Do note that it can be up to 15 characters long
set(MACOSX_CONFIG_BUNDLE_DISPLAY_NAME "Configure ${APP_NAME}")
set(MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")

# TODO: darwin < 9
set(MACOSX_BUNDLE_REQUIRED_PLATFORM Carbon)

set(CPACK_DMG_VOLUME_NAME "Isle Portable")

if(ISLE_BUILD_APP)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/isle/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/isle/Info.plist"
    @ONLY
  )
  set(RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/isle/${_icon_file}.icns")
  target_sources(${ISLE_TARGET_NAME} PRIVATE ${RESOURCE_FILES})
  set_target_properties(${ISLE_TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "${_icon_file}.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/isle/Info.plist"
    RESOURCE ${RESOURCE_FILES})
  install(TARGETS ${ISLE_TARGET_NAME} DESTINATION ./)
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(${CMAKE_BINARY_DIR}/${ISLE_TARGET_NAME}.app \"\" \"\")
    "
    COMPONENT Runtime)
  install(CODE "
    execute_process(COMMAND /usr/bin/codesign
      --force --deep --sign - --timestamp
      \"\$\{CMAKE_INSTALL_PREFIX\}/${ISLE_TARGET_NAME}.app/Contents/MacOS/${ISLE_TARGET_NAME}\")
  ")
  install(CODE "
    file(RENAME
      \"\$\{CMAKE_INSTALL_PREFIX\}/${ISLE_TARGET_NAME}.app\"
      \"\$\{CMAKE_INSTALL_PREFIX\}/${MACOSX_ISLE_BUNDLE_DISPLAY_NAME}.app\")
  ")
endif()
if(ISLE_BUILD_CONFIG)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/config/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/config/Info.plist"
    @ONLY
  )
  set(RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/config/${_icon_file}.icns")
  target_sources(${CONFIG_TARGET_NAME} PRIVATE ${RESOURCE_FILES})
  set_target_properties(${CONFIG_TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_ICON_FILE "${_icon_file}.icns"
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/config/Info.plist"
    RESOURCE ${RESOURCE_FILES})
  install(TARGETS ${CONFIG_TARGET_NAME} DESTINATION ./)
  install(CODE "
    include(BundleUtilities)
    fixup_bundle(${CMAKE_BINARY_DIR}/${CONFIG_TARGET_NAME}.app \"\" \"\")
    "
    COMPONENT Runtime)
  qt_generate_deploy_app_script(
    TARGET ${CONFIG_TARGET_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_COMPILER_RUNTIME
    NO_TRANSLATIONS
  )
  install(SCRIPT "${deploy_script}")
  install(CODE "
    execute_process(COMMAND /usr/bin/install_name_tool
      -add_rpath \"@executable_path/../Frameworks\"
      \"\$\{CMAKE_INSTALL_PREFIX\}/${CONFIG_TARGET_NAME}.app/Contents/MacOS/${CONFIG_TARGET_NAME}\")
  ")
  install(CODE "
    execute_process(COMMAND /usr/bin/codesign
      --force --deep --sign - --timestamp
      \"\$\{CMAKE_INSTALL_PREFIX\}/${CONFIG_TARGET_NAME}.app/Contents/MacOS/${CONFIG_TARGET_NAME}\")
  ")
  install(CODE "
    file(RENAME
      \"\$\{CMAKE_INSTALL_PREFIX\}/${CONFIG_TARGET_NAME}.app\"
      \"\$\{CMAKE_INSTALL_PREFIX\}/${MACOSX_CONFIG_BUNDLE_DISPLAY_NAME}.app\")
  ")
endif()

install(CODE "
  if(IS_DIRECTORY \"\$\{CMAKE_INSTALL_PREFIX\}/bin\" OR IS_DIRECTORY \"\$\{CMAKE_INSTALL_PREFIX\}/lib\" OR EXISTS \"\$\{CMAKE_INSTALL_PREFIX\}/AppIcon.icns\")
    execute_process(COMMAND /bin/rm
      -rf \"\$\{CMAKE_INSTALL_PREFIX\}/bin\" \"\$\{CMAKE_INSTALL_PREFIX\}/lib\" \"\$\{CMAKE_INSTALL_PREFIX\}/AppIcon.icns\"
    )
  endif()
")
