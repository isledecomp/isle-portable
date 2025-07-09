set(MACOSX_BUNDLE_GUI_IDENTIFIER ${APP_ID})
set(MACOSX_BUNDLE_COPYRIGHT ${APP_SPDX})
set(ISLE_TARGET_NAME isle)
set(MACOSX_ISLE_BUNDLE_NAME ${APP_NAME}) # Do note that it can be up to 15 characters long
set(MACOSX_ISLE_BUNDLE_DISPLAY_NAME ${APP_NAME})
set(MACOSX_BUNDLE_INFO_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION})
set(MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION})
set(MACOSX_BUNDLE_LONG_VERSION_STRING "Version ${PROJECT_VERSION}")
set(MACOSX_BUNDLE_REQUIRED_PLATFORM IPhoneOS)

if(ISLE_BUILD_APP)
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/isle/Info.plist.in"
    "${CMAKE_CURRENT_BINARY_DIR}/isle/Info.plist"
    @ONLY
  )
  set(RESOURCE_FILES "${CMAKE_CURRENT_SOURCE_DIR}/isle/LaunchScreen.storyboard" "${CMAKE_CURRENT_SOURCE_DIR}/isle/Assets.xcassets")
  target_sources(${ISLE_TARGET_NAME} PRIVATE ${RESOURCE_FILES})
  set_source_files_properties(${RESOURCE_FILES}
    TARGET_DIRECTORY isle
    PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set_target_properties(${ISLE_TARGET_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/isle/Info.plist"
    XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2")
  install(TARGETS ${ISLE_TARGET_NAME} DESTINATION ./)
  install(CODE "
    file(COPY
      \"\$<TARGET_FILE:SDL3::SDL3>\"
      \"\$<TARGET_FILE:lego1>\"
      DESTINATION \"\$\{CMAKE_INSTALL_PREFIX\}/${ISLE_TARGET_NAME}.app/Frameworks\")
    execute_process(COMMAND /usr/bin/install_name_tool
      -add_rpath @executable_path/Frameworks
      \"\$\{CMAKE_INSTALL_PREFIX\}/${ISLE_TARGET_NAME}.app/${ISLE_TARGET_NAME}\"
    )
    file(MAKE_DIRECTORY
      \"\$\{CMAKE_INSTALL_PREFIX\}/Payload\")
    file(RENAME
      \"\$\{CMAKE_INSTALL_PREFIX\}/${ISLE_TARGET_NAME}.app\"
      \"\$\{CMAKE_INSTALL_PREFIX\}/Payload/${ISLE_TARGET_NAME}.app\")
  ")
endif()

install(CODE "
  if(IS_DIRECTORY \"\$\{CMAKE_INSTALL_PREFIX\}/bin\" OR IS_DIRECTORY \"\$\{CMAKE_INSTALL_PREFIX\}/lib\" OR EXISTS \"\$\{CMAKE_INSTALL_PREFIX\}/AppIcon.icns\")
    execute_process(COMMAND /bin/rm
      -rf \"\$\{CMAKE_INSTALL_PREFIX\}/bin\" \"\$\{CMAKE_INSTALL_PREFIX\}/lib\" \"\$\{CMAKE_INSTALL_PREFIX\}/AppIcon.icns\"
    )
  endif()
")
