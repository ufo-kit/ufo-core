cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS
    ufo-aux.c
    ufo-base-scheduler.c
    ufo-channel.c
    ufo-graph.c
    ufo-buffer.c
    ufo-filter.c
    ufo-filter-source.c
    ufo-filter-sink.c
    ufo-filter-reduce.c
    ufo-plugin-manager.c
    ufo-relation.c
    ufo-resource-manager.c
    )

set(ufocore_HDRS
    ufo-aux.h
    ufo-base-scheduler.h
    ufo-channel.h
    ufo-graph.h
    ufo-buffer.h
    ufo-filter.h
    ufo-filter-source.h
    ufo-filter-sink.h
    ufo-filter-reduce.h
    ufo-plugin-manager.h
    ufo-relation.h
    ufo-resource-manager.h
    )

# --- Find packages and libraries ---------------------------------------------
find_program(INTROSPECTION_SCANNER "g-ir-scanner")
find_program(INTROSPECTION_COMPILER "g-ir-compiler")
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)

pkg_check_modules(GMODULE2 gmodule-2.0>=2.22 REQUIRED)
pkg_check_modules(GTHREAD2 gthread-2.0>=2.22 REQUIRED)
pkg_check_modules(JSON_GLIB json-glib-1.0 REQUIRED)

set(ufocore_LIBS
    ${OPENCL_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD2_LIBRARIES}
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

# --- Target ------------------------------------------------------------------
get_property(LIB64 GLOBAL PROPERTY FIND_LIBRARY_USE_LIB64_PATHS)

set(LIB_INSTALL_DIR "lib${LIB_SUFFIX}")

include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${JSON_GLIB_INCLUDE_DIRS}
    )

if(CMAKE_BUILD_TYPE MATCHES "Release")
    add_definitions(-DG_DISABLE_ASSERT)
endif()

add_library(ufo SHARED ${ufocore_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c)

set_target_properties(ufo PROPERTIES
    VERSION ${PACKAGE_VERSION}
    SOVERSION ${UFO_SO_VERSION})

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
set(UFO_PKG_INCLUDEDIR "${UFO_PKG_PREFIX}/include")
set(UFO_PKG_GIRDIR "${UFO_PKG_PREFIX}/share/gir-1.0")
set(UFO_PKG_TYPELIBDIR "${UFO_PKG_PREFIX}/${LIB_INSTALL_DIR}/girepository-1.0")

# FIXME: inside the ufo.pc.in we should set the lib names that we found out, not
# hard coded values
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ufo.pc.in"
    "${CMAKE_CURRENT_BINARY_DIR}/ufo.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ufo.pc DESTINATION lib/pkgconfig)


# --- Introspection files -----------------------------------------------------
if (INTROSPECTION_SCANNER AND INTROSPECTION_COMPILER)
    option(WITH_GIR "Build introspection files" ON)

    if (WITH_GIR)
        set(GIR_PREFIX "Ufo-${UFO_GIR_VERSION}")
        set(GIR_XML "${GIR_PREFIX}.gir")
        set(GIR_TYPELIB "${GIR_PREFIX}.typelib")
        set(_gir_input)

        foreach(_src ${ufocore_SRCS} ${ufocore_HDRS})
            list(APPEND _gir_input "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endforeach()

        add_custom_command(OUTPUT ${GIR_XML}
            COMMAND ${INTROSPECTION_SCANNER}
                    --namespace=Ufo
                    --nsversion=${UFO_GIR_VERSION}
                    --library=ufo
                    --no-libtool
                    --include=GObject-2.0
                    --include=GModule-2.0
                    -I${OPENCL_INCLUDE_DIRS}
                    --output ${GIR_XML}
                    --warn-all
                    ${_gir_input}
            DEPENDS ${ufocore_SRCS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_command(OUTPUT ${GIR_TYPELIB}
            COMMAND ${INTROSPECTION_COMPILER}
                    -o ${GIR_TYPELIB}
                    ${GIR_XML}
            DEPENDS ${GIR_XML}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_target(gir ALL DEPENDS ${GIR_XML} ${GIR_TYPELIB})
        add_dependencies(gir ufo)

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_XML}
            DESTINATION share/gir-1.0)

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_TYPELIB}
            DESTINATION ${LIB_INSTALL_DIR}/girepository-1.0)
    endif()
endif()


# --- Generate config.h -------------------------------------------------------
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)


# --- Build API reference -----------------------------------------------------
pkg_check_modules(GTK_DOC gtk-doc)
if(GTK_DOC_FOUND)
    option(WITH_GTK_DOC "Build API reference" ON)
    if (WITH_GTK_DOC)
        set(docs_dir "${CMAKE_CURRENT_BINARY_DIR}/../docs")
        set(docs_out "${docs_dir}/reference")
        set(_xml_doc_input)

        # Create xml entries for Ufo-docs.xml.in
        foreach(_src ${ufocore_SRCS})
            string(REPLACE ".c" ".xml" _xml_doc ${_src})
            list(APPEND _xml_doc_input "<xi:include href=\"xml/${_xml_doc}\"/>")
        endforeach()

        string(REPLACE ";" "\n" _xml_doc_input ${_xml_doc_input})
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../docs/Ufo-docs.xml.in" "${docs_out}/Ufo-docs.xml")
        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../docs/Ufo-sections.txt.in" "${docs_out}/Ufo-sections.txt")

        set(reference_files
            "${docs_out}/index.html"
            "${docs_out}/annotation-glossary.html"
            "${docs_out}/api-index-full.html"
            "${docs_out}/ch01.html"
            "${docs_out}/Ufo-UfoBuffer.html"
            "${docs_out}/Ufo-UfoChannel.html"
            "${docs_out}/Ufo-UfoFilter.html"
            "${docs_out}/Ufo-UfoGraph.html"
            "${docs_out}/Ufo-UfoPluginManager.html"
            "${docs_out}/Ufo-UfoResourceManager.html"
            "${docs_out}/style.css"
            "${docs_out}/Ufo.devhelp2"
            "${docs_out}/home.png"
            "${docs_out}/left.png"
            "${docs_out}/right.png"
            "${docs_out}/up.png")

        find_program(GTK_DOC_SCAN gtkdoc-scan REQUIRED)
        find_program(GTK_DOC_MKDB gtkdoc-mkdb REQUIRED)
        find_program(GTK_DOC_MKHTML gtkdoc-mkhtml REQUIRED)

        add_custom_command(OUTPUT ${docs_out}/Ufo-decl.txt
            COMMAND ${GTK_DOC_SCAN}
                    --module=Ufo
                    --source-dir=${CMAKE_CURRENT_SOURCE_DIR}/../src
                    DEPENDS ${ufocore_SRCS}
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_out}/sgml.stamp
            COMMAND ${GTK_DOC_MKDB}
                    --module=Ufo
                    --source-dir=${CMAKE_CURRENT_SOURCE_DIR}
                    --sgml-mode
                    --output-format=xml
                    DEPENDS ${docs_out}/Ufo-decl.txt ${docs_out}/Ufo-sections.txt ${ufocore_SRCS}
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_dir}/html.stamp
            COMMAND ${GTK_DOC_MKHTML}
                    Ufo
                    ${docs_out}/Ufo-docs.xml
            DEPENDS ${docs_out}/sgml.stamp
            WORKING_DIRECTORY ${docs_out})

        add_custom_target(reference ALL DEPENDS ${docs_dir}/html.stamp)

        install(FILES ${reference_files} DESTINATION share/gtk-doc/html/Ufo)
    endif()
endif(GTK_DOC_FOUND)

