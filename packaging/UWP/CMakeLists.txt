file(GLOB_RECURSE GENERATED_ASSETS
    "assets/*"
)
set(ASSET_FILES ${GENERATED_ASSETS})
set_source_files_properties(${ASSET_FILES} PROPERTIES
    VS_DEPLOYMENT_CONTENT 1
    VS_DEPLOYMENT_LOCATION "assets"
)

set(MANIFEST_FILE Package.appxmanifest)
set_source_files_properties(${MANIFEST_FILE} PROPERTIES
    VS_DEPLOYMENT_CONTENT 1
)

set(SIGNING_KEY_FILE IslePortable.pfx)
set_source_files_properties(${SIGNING_KEY_FILE} PROPERTIES
    VS_DEPLOYMENT_CONTENT 1
)

target_sources(isle PRIVATE ${ASSET_FILES} ${MANIFEST_FILE} ${SIGNING_KEY_FILE})
