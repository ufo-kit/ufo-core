cmake_minimum_required(VERSION 2.8)

set(DEST_DIR ${CMAKE_CURRENT_BINARY_DIR}/../src)

if (INTROSPECTION_FOUND)
    set(GIR_PREFIX "Ufo-${UFO_GIR_VERSION}")

    # FIXME: this might not be the best way, to get only the ethos include dir
    list(GET ETHOS_INCLUDE_DIRS 0 ETHOS_INC_DIR)

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufo-gir.sh.in
        ${DEST_DIR}/ufo-gir.sh)

    add_custom_target(gir
        sh ${DEST_DIR}/ufo-gir.sh
        WORKING_DIRECTORY ${DEST_DIR})

    install(FILES ${DEST_DIR}/${GIR_PREFIX}.gir
        DESTINATION ${INTROSPECTION_GIRDIR})

    install(FILES ${DEST_DIR}/${GIR_PREFIX}.typelib
        DESTINATION ${INTROSPECTION_TYPELIBDIR})
endif()
