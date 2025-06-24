# Injects the required variables into the Desktop and MetaInfo files
configure_file(isle_decomp.desktop.in "${APP_ID}.desktop" @ONLY)
configure_file(isle_decomp.metainfo.xml.in "${APP_ID}.metainfo.xml" @ONLY)

# Injects the required variables into the Flatpak manifest
if (CONFIGURE_FLATPAK)
    configure_file(flatpak/manifest.json.in "${APP_ID}.json" ESCAPE_QUOTES @ONLY)
endif()