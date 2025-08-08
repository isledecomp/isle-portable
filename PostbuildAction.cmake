function(add_xbox_build_steps TARGET_NAME XBE_TITLE XBOX_ISO_DIR)
    # Ensure ISO output directory exists
    file(MAKE_DIRECTORY ${XBOX_ISO_DIR})

    # Convert EXE to XBE
    add_custom_target(${TARGET_NAME}_cxbe_convert ALL
        COMMENT "CXBE Conversion: [EXE -> XBE]"
        VERBATIM
        COMMAND "${CMAKE_COMMAND}" -E env
                ${NXDK_DIR}/tools/cxbe/cxbe
                -OUT:${CMAKE_CURRENT_BINARY_DIR}/default.xbe
                -TITLE:${XBE_TITLE}
                ${CMAKE_CURRENT_BINARY_DIR}/${XBE_TITLE}.exe > NUL 2>&1
    )
    add_dependencies(${TARGET_NAME}_cxbe_convert ${TARGET_NAME})

    # Convert XBE to XISO
    add_custom_target(${TARGET_NAME}_xbe_iso ALL
        COMMENT "XISO Conversion: [XBE -> XISO]"
        COMMAND ${CMAKE_COMMAND} -E copy
                "${CMAKE_CURRENT_BINARY_DIR}/default.xbe"
                "${XBOX_ISO_DIR}/default.xbe"
        COMMAND "${CMAKE_COMMAND}" -E env
                ${NXDK_DIR}/tools/extract-xiso/build/extract-xiso
                -c ${XBOX_ISO_DIR}
                ${CMAKE_CURRENT_BINARY_DIR}/${XBE_TITLE}.iso
        WORKING_DIRECTORY ${XBOX_ISO_DIR}
        VERBATIM
    )
    add_dependencies(${TARGET_NAME}_xbe_iso ${TARGET_NAME}_cxbe_convert)

    # Silence output
    set_target_properties(${TARGET_NAME}_cxbe_convert PROPERTIES OUTPUT_QUIET ON)
    set_target_properties(${TARGET_NAME}_xbe_iso PROPERTIES OUTPUT_QUIET ON)
endfunction()
