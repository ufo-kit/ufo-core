cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS 
    ufo-channel.c
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    ufo-plugin-manager.c
    ufo-resource-manager.c
    )

set(ufocore_HDRS
    ufo-channel.h
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    ufo-plugin-manager.h
    ufo-resource-manager.h
    )

# --- Find packages and libraries ---------------------------------------------
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)

pkg_check_modules(JSON_GLIB json-glib-1.0 REQUIRED)

set(ufocore_LIBS
    ${OPENCL_LIBRARIES}
    ${GLIB2_LIBRARIES} 
    ${GOBJECT2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    )

# --- Add enum generation targets ---------------------------------------------
add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
  COMMAND ${GLIB2_MKENUMS}
  ARGS
    --template ufo-enums.h.template
    ${ufocore_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${ufocore_HDRS}
          ${CMAKE_CURRENT_SOURCE_DIR}/ufo-enums.h.template
)

add_custom_command(
  OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c
  COMMAND ${GLIB2_MKENUMS}
  ARGS
    --template ufo-enums.c.template
    ${ufocore_HDRS} > ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  DEPENDS ${ufocore_HDRS} ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
          ${CMAKE_CURRENT_SOURCE_DIR}/ufo-enums.c.template
)

set(ufocore_SRCS ${ufocore_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c)

# --- Target ------------------------------------------------------------------
get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${JSON_GLIB_INCLUDE_DIRS}
    )

add_library(ufo SHARED ${ufocore_SRCS})

set_target_properties(ufo PROPERTIES
    VERSION ${UFO_ABI_VERSION}
    SOVERSION ${UFO_ABI_MAJOR_VERSION})

target_link_libraries(ufo ${ufocore_LIBS})

install(TARGETS ufo 
    ARCHIVE DESTINATION ${LIB_INSTALL_DIR}
    LIBRARY DESTINATION ${LIB_INSTALL_DIR})

install(FILES 
    ${ufocore_HDRS} 
    ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
    DESTINATION include/ufo)


# --- pkg-config --------------------------------------------------------------
set(UFO_PKG_PREFIX ${CMAKE_INSTALL_PREFIX})
set(UFO_PKG_EXEC_PREFIX "${UFO_PKG_PREFIX}/bin")
set(UFO_PKG_LIBDIR "${CMAKE_INSTALL_PREFIX}/${LIB_INSTALL_DIR}")
set(UFO_PKG_INCLUDEDIR "${UFO_PKG_PREFIX}/include/ufo")
set(UFO_PKG_GIRDIR "${UFO_PKG_PREFIX}/share/gir-1.0")
set(UFO_PKG_TYPELIBDIR "${UFO_PKG_PREFIX}/${LIB_INSTALL_DIR}/girepository-1.0")

# FIXME: inside the ufo.pc.in we should set the lib names that we found out, not
# hard coded values
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ufo.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ufo.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ufo.pc DESTINATION lib/pkgconfig)


# --- Introspection files -----------------------------------------------------
if (INTROSPECTION_FOUND)
    set(GIR_PREFIX "Ufo-${UFO_GIR_VERSION}")
    set(GIR_XML "${GIR_PREFIX}.gir")
    set(GIR_TYPELIB "${GIR_PREFIX}.typelib")

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/bindings/ufo-gir.sh.in"
        "${CMAKE_CURRENT_BINARY_DIR}/ufo-gir.sh")

    add_custom_command(OUTPUT ${GIR_XML} ${GIR_TYPELIB}
        COMMAND sh ${CMAKE_CURRENT_BINARY_DIR}/ufo-gir.sh
        DEPENDS ${ufocore_SRCS}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    add_custom_target(gir ALL DEPENDS ${GIR_XML} ${GIR_TYPELIB})
    add_dependencies(gir ufo)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_XML}
        DESTINATION share/gir-1.0)

    install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_TYPELIB}
        DESTINATION ${LIB_INSTALL_DIR}/girepository-1.0)
endif()


# --- Generate config.h -------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

add_subdirectory(bindings)


# --- Build API reference -----------------------------------------------------
pkg_check_modules(GTK_DOC gtk-doc)
if(GTK_DOC_FOUND)
    option(WITH_GTK_DOC "Build API reference" ON)
    if (WITH_GTK_DOC)
        set(doc_in "${CMAKE_CURRENT_SOURCE_DIR}/../docs")
        set(doc_out "${CMAKE_CURRENT_BINARY_DIR}/../docs/reference")
        set(doc_html "${CMAKE_CURRENT_BINARY_DIR}/../docs/reference/html")

        configure_file("${doc_in}/upd-ref.sh.in" "${doc_out}/upd-ref.sh")
        configure_file("${doc_in}/Ufo-docs.xml.in" "${doc_out}/Ufo-docs.xml")

        set(reference_files 
            "${doc_html}/index.html"
            "${doc_html}/annotation-glossary.html"
            "${doc_html}/api-index-full.html"
            "${doc_html}/ch01.html"
            "${doc_html}/Ufo-UfoBuffer.html"
            "${doc_html}/Ufo-UfoChannel.html"
            "${doc_html}/Ufo-UfoFilter.html"
            "${doc_html}/Ufo-UfoGraph.html"
            "${doc_html}/Ufo-UfoPluginManager.html"
            "${doc_html}/Ufo-UfoResourceManager.html"
            "${doc_html}/style.css"
            "${doc_html}/Ufo.devhelp2"
            "${doc_html}/home.png"
            "${doc_html}/left.png"
            "${doc_html}/right.png"
            "${doc_html}/up.png")

        add_custom_command(OUTPUT ${reference_files}
            COMMAND sh "${doc_out}/upd-ref.sh"
            DEPENDS ${ufocore_SRCS}
            WORKING_DIRECTORY ${doc_out})
        add_custom_target(reference ALL DEPENDS ${reference_files})
        add_dependencies(reference ufo)

        install(FILES ${reference_files} DESTINATION share/gtk-doc/html/Ufo)
    endif()
endif(GTK_DOC_FOUND)

