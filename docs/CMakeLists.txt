cmake_minimum_required(VERSION 2.8)


pkg_check_modules(GTK_DOC gtk-doc)
find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)
mark_as_advanced(SPHINX)

# --- Create Doxygen source documentation by typing `make doc`
if(GTK_DOC_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/upd-ref.sh.in
        ${CMAKE_CURRENT_BINARY_DIR}/upd-ref.sh)

    add_custom_target(reference 
        sh ${CMAKE_CURRENT_BINARY_DIR}/upd-ref.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${ufocore_SRCS})
endif(GTK_DOC_FOUND)

if(SPHINX)
    set(SPHINX_RESOURCES
        source/contents.rst
        source/api.rst
        source/bugs.rst
        source/build.rst
        source/copyright.rst
        source/faq.rst
        source/filters.rst
        source/glossary.rst
        source/install.rst
        source/install-linux.rst
        source/install-mac.rst
        source/json.rst
        source/using/index.rst
        source/whatsnew/0.1.rst
        source/_static/ufo-logo.png
        source/_templates/indexcontent.html
        source/_templates/indexsidebar.html
        )

    foreach(file ${SPHINX_RESOURCES})
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${file}
            ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
    endforeach()

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/conf.py.in
        ${CMAKE_CURRENT_BINARY_DIR}/source/conf.py)

    add_custom_target(manual
        ${SPHINX} -b html ${CMAKE_CURRENT_BINARY_DIR}/source docs
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating user documentation with Sphinx" VERBATIM
        )
endif()
