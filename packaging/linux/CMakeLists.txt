# Injects the required variables into the Desktop and MetaInfo files
configure_file(isledecomp.desktop.in "${APP_ID}.desktop" @ONLY)
configure_file(isledecomp.metainfo.xml.in "${APP_ID}.metainfo.xml" @ONLY)

install(FILES "../icons/isle.svg" RENAME "${APP_ID}.svg" DESTINATION "share/icons/hicolor/scalable/apps")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.desktop" DESTINATION "share/applications")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/${APP_ID}.metainfo.xml" DESTINATION "share/metainfo")
