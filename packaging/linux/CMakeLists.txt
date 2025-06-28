if(DEFINED ENV{FLATPAK_DEST})
    set(METADATA_ROOT "$ENV{FLATPAK_DEST}/share")
else()
    set(METADATA_ROOT "/usr/share")
endif()

# Injects the required variables into the Desktop and MetaInfo files
configure_file(isledecomp.desktop.in "${APP_ID}.desktop" @ONLY)
configure_file(isledecomp.metainfo.xml.in "${APP_ID}.metainfo.xml" @ONLY)

install(FILES "../icons/isle.svg" RENAME "${APP_ID}.svg" DESTINATION "${METADATA_ROOT}/icons/hicolor/scalable/apps/")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.desktop" DESTINATION "${METADATA_ROOT}/applications/")
install(FILES "${CMAKE_CURRENT_SOURCE_DIR}/${APP_ID}.metainfo.xml" DESTINATION "${METADATA_ROOT}/metainfo/")