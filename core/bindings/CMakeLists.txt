cmake_minimum_required(VERSION 2.8)

if (INTROSPECTION_FOUND)
    set(GIR_PREFIX "Ufo-${UFO_VERSION_STRING_SHORT}")

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufo-gir.sh.in
        ${CMAKE_CURRENT_BINARY_DIR}/ufo-gir.sh)

    add_custom_target(gir 
        sh ${CMAKE_CURRENT_BINARY_DIR}/ufo-gir.sh >& /dev/null)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_PREFIX}.gir
        DESTINATION ${INTROSPECTION_GIRDIR})

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_PREFIX}.typelib
        DESTINATION ${INTROSPECTION_TYPELIBDIR})
endif()
