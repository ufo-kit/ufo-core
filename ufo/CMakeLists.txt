cmake_minimum_required(VERSION 2.6)

#{{{ sources
set(ufocore_SRCS
    compat.c
    ufo-priv.c
    ufo-base-scheduler.c
    ufo-copy-task.c
    ufo-buffer.c
    ufo-copyable-iface.c
    ufo-cpu-node.c
    ufo-daemon.c
    ufo-dummy-task.c
    ufo-fixed-scheduler.c
    ufo-gpu-node.c
    ufo-graph.c
    ufo-group.c
    ufo-group-scheduler.c
    ufo-input-task.c
    ufo-local-scheduler.c
    ufo-messenger-iface.c
    ufo-method-iface.c
    ufo-misc.c
    ufo-node.c
    ufo-output-task.c
    ufo-plugin-manager.c
    ufo-profiler.c
    ufo-processor.c
    ufo-remote-node.c
    ufo-remote-task.c
    ufo-resources.c
    ufo-scheduler.c
    ufo-task-iface.c
    ufo-task-graph.c
    ufo-task-node.c
    ufo-transform-iface.c
    ufo-two-way-queue.c
    ufo-basic-ops.c
    )
#}}}
#{{{ headers
set(ufocore_HDRS
    ufo-base-scheduler.h
    ufo-copy-task.h
    ufo-buffer.h
    ufo-copyable-iface.h
    ufo-cpu-node.h
    ufo-daemon.h
    ufo-dummy-task.h
    ufo-fixed-scheduler.h
    ufo-gpu-node.h
    ufo-graph.h
    ufo-group.h
    ufo-group-scheduler.h
    ufo-input-task.h
    ufo-local-scheduler.h
    ufo-messenger-iface.h
    ufo-method-iface.h
    ufo-misc.h
    ufo-node.h
    ufo-output-task.h
    ufo-plugin-manager.h
    ufo-profiler.h
    ufo-processor.h
    ufo-remote-node.h
    ufo-remote-task.h
    ufo-resources.h
    ufo-scheduler.h
    ufo-task-iface.h
    ufo-task-graph.h
    ufo-task-node.h
    ufo-transform-iface.h
    ufo-two-way-queue.h
    ufo-basic-ops.h
    )
#}}}
#{{{ libufo

add_definitions(-DUFO_COMPILATION)

if(CMAKE_BUILD_TYPE MATCHES "Release")
    add_definitions(-DG_DISABLE_ASSERT)
endif()

if (WITH_ZMQ)
    list(APPEND ufocore_SRCS ufo-zmq-messenger.c)
    list(APPEND ufocore_HDRS ufo-zmq-messenger.h)
endif ()

add_library(ufo SHARED ${ufocore_SRCS} ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.c)

set_target_properties(ufo PROPERTIES
    VERSION ${PACKAGE_VERSION}
    SOVERSION ${UFO_VERSION_MAJOR})

target_link_libraries(ufo ${UFOCORE_DEPS})

#{{{ install target
install(TARGETS ufo
        ARCHIVE DESTINATION ${UFO_LIBDIR}
        LIBRARY DESTINATION ${UFO_LIBDIR})

install(FILES ${ufocore_HDRS}
              ${CMAKE_CURRENT_SOURCE_DIR}/ufo.h
              ${CMAKE_CURRENT_BINARY_DIR}/ufo-enums.h
        DESTINATION ${UFO_INCLUDEDIR}/ufo)

install(FILES ufo-basic-ops.cl
        DESTINATION ${UFO_KERNELDIR})
#}}}
#}}}
#{{{ glib-mkenums
find_program(GLIB2_MKENUMS glib-mkenums REQUIRED)

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
#}}}
#{{{ GObject introspection
pkg_check_modules(GOBJECT_INTROSPECTION gobject-introspection-1.0)

if (GOBJECT_INTROSPECTION_FOUND)
    option(WITH_GIR "Build introspection files" ON)

    if (WITH_GIR)
        pkg_check_variable(gobject-introspection-1.0 g_ir_scanner)
        pkg_check_variable(gobject-introspection-1.0 g_ir_compiler)

        set(GIR_PREFIX "Ufo-${UFO_GIR_VERSION}")
        set(GIR_XML "${GIR_PREFIX}.gir")
        set(GIR_TYPELIB "${GIR_PREFIX}.typelib")
        set(_gir_input)

        foreach(_src ${ufocore_SRCS} ${ufocore_HDRS})
            list(APPEND _gir_input "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        endforeach()

        add_custom_command(OUTPUT ${GIR_XML}
            COMMAND ${GOBJECT_INTROSPECTION_1.0_G_IR_SCANNER}
                    --namespace=Ufo
                    --nsversion=${UFO_GIR_VERSION}
                    --library=ufo
                    --no-libtool
                    --include=GObject-2.0
                    --include=GModule-2.0
                    --include=GLib-2.0
                    --include=Json-1.0
                    -I${OPENCL_INCLUDE_DIRS}
                    -I${JSON_GLIB_INCLUDE_DIRS}
                    -I${CMAKE_CURRENT_SOURCE_DIR}/..
                    -DUFO_COMPILATION
                    --output ${GIR_XML}
                    --warn-all
                    --quiet
                    ${_gir_input}
            DEPENDS ${ufocore_SRCS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_command(OUTPUT ${GIR_TYPELIB}
            COMMAND ${GOBJECT_INTROSPECTION_1.0_G_IR_COMPILER}
                    -o ${GIR_TYPELIB}
                    ${GIR_XML}
            DEPENDS ${GIR_XML}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

        add_custom_target(gir ALL DEPENDS ${GIR_XML} ${GIR_TYPELIB})
        add_dependencies(gir ufo)

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_XML}
                DESTINATION ${UFO_GIRDIR})

        install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_TYPELIB}
                DESTINATION ${UFO_TYPELIBDIR})
    endif()
endif()
#}}}
#{{{ gtk-doc
pkg_check_modules(GTK_DOC gtk-doc)

if(GTK_DOC_FOUND)
    option(WITH_GTK_DOC "Build API reference" ON)
    if (WITH_GTK_DOC)
        find_program(GTK_DOC_SCAN gtkdoc-scan REQUIRED)
        find_program(GTK_DOC_SCANGOBJ gtkdoc-scangobj REQUIRED)
        find_program(GTK_DOC_MKDB gtkdoc-mkdb REQUIRED)
        find_program(GTK_DOC_MKHTML gtkdoc-mkhtml REQUIRED)

        set(docs_dir "${CMAKE_CURRENT_BINARY_DIR}/../docs")
        set(docs_out "${docs_dir}/reference")
        set(docs_xml_in "${CMAKE_CURRENT_SOURCE_DIR}/../docs/Ufo-docs.xml")
        set(docs_xml_out "${docs_out}/Ufo-docs.xml")

        set(GTK_DOC_CFLAGS)
        set(GTK_DOC_LDFLAGS)

        get_directory_property(_current_include_dirs INCLUDE_DIRECTORIES)

        foreach(_incl ${_current_include_dirs})
            set(GTK_DOC_CFLAGS "-I${_incl} ${GTK_DOC_CFLAGS}")
        endforeach()

        foreach(_lib ${UFOCORE_DEPS})
            if (NOT ${_lib} MATCHES "^[/]")
                set(GTK_DOC_LDFLAGS "-l${_lib} ${GTK_DOC_LDFLAGS}")
            endif()
        endforeach()

        get_directory_property(_current_link_dirs LINK_DIRECTORIES)

        foreach(_linkdir ${_current_link_dirs})
            set(GTK_DOC_LDFLAGS "-L${_linkdir} ${GTK_DOC_LDFLAGS}")
        endforeach()

        configure_file("${CMAKE_CURRENT_SOURCE_DIR}/../docs/scangobj.sh.in" "${docs_out}/scangobj.sh")

        add_custom_target(docbook)
        add_custom_command(TARGET docbook PRE_BUILD
                           COMMAND ${CMAKE_COMMAND} -E
                                   copy ${docs_xml_in} ${docs_xml_out}
                                   DEPENDS ${docs_xml_out})

        add_custom_command(OUTPUT ${docs_out}/Ufo-decl.txt
            COMMAND ${GTK_DOC_SCAN}
                    --module=Ufo
                    --source-dir=${CMAKE_CURRENT_SOURCE_DIR}
                    --ignore-headers=ufo-mpi-messenger.h
            DEPENDS ufo
            COMMENT "Running gtkdoc-scan"
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_out}/Ufo.args
            COMMAND sh scangobj.sh
            DEPENDS ufo ${docs_out}/Ufo-decl.txt
            COMMENT "Running gtkdoc-scangobj"
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_out}/sgml.stamp
            COMMAND ${GTK_DOC_MKDB}
                    --module=Ufo
                    --source-dir=${CMAKE_CURRENT_SOURCE_DIR}
                    --sgml-mode
                    --output-format=xml
            DEPENDS ufo ${docs_out}/Ufo.args
            COMMENT "Running gtkdoc-mkdb"
            WORKING_DIRECTORY ${docs_out})

        add_custom_command(OUTPUT ${docs_dir}/html.stamp
            COMMAND ${GTK_DOC_MKHTML}
                    --path="${CMAKE_CURRENT_BINARY_DIR}/../docs/reference"
                    Ufo
                    ${docs_xml_out}
            DEPENDS ufo ${docs_out}/sgml.stamp docbook
            COMMENT "Running gtkdoc-mkhtml"
            WORKING_DIRECTORY ${docs_out})

        add_custom_target(reference ALL DEPENDS ${docs_dir}/html.stamp)

        install(DIRECTORY ${docs_out}/
                DESTINATION ${UFO_DATADIR}/gtk-doc/html/Ufo
                FILES_MATCHING REGEX ".*\\.(html|png|devhelp2|css)$")
    endif()
endif(GTK_DOC_FOUND)
#}}}
#{{{ pkg-config
# FIXME: inside the ufo.pc.in we should set the lib names that we found out, not
# hard coded values
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/ufo.pc.in"
               "${CMAKE_CURRENT_BINARY_DIR}/ufo.pc" @ONLY IMMEDIATE)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ufo.pc
        DESTINATION ${UFO_PKGCONFIGDIR})
#}}}
