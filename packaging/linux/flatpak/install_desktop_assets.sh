#!/usr/bin/env bash
# Install Desktop assets and Icons in a Flatpak build environment

APP_ID=$1

# Set up main constants
ICON_INSTALL_DIR="${FLATPAK_DEST}/share/icons/hicolor"
ICON_SIZES=(64 128) # TODO: Figure out why only the first element is being checked

# Rename and install SVG icon
mv icons/isle.svg "icons/${APP_ID}.svg"
install -Dm0644 "icons/${APP_ID}.svg" -t "${ICON_INSTALL_DIR}/scalable/apps/"

# Rename and install optional PNG icons
for size in $ICON_SIZES; do
    icon="icons/${APP_ID}_${size}.png"
    if [ ! -f "${icon}" ]; then
        # Skip if icon doesn't exist
        echo "\"${icon}\" not present. Skipping..."
        continue
    fi
    mv "${icon}" "icons/${APP_ID}.png"

    icon="icons/${APP_ID}.png"
    size_sq="${size}x${size}"
    target_dir="${ICON_INSTALL_DIR}/${size_sq}/apps/"

    mkdir -p "${target_dir}"
    install -Dm0644 "${icon}" -t "${target_dir}"
    echo "Installed ${size_sq} icon"
done

# Install Desktop file and AppStream data
install -Dm0644 "${APP_ID}.desktop" -t "${FLATPAK_DEST}/share/applications/"
install -Dm0644 "${APP_ID}.metainfo.xml" -t "${FLATPAK_DEST}/share/metainfo/"
