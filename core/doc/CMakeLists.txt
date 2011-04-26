cmake_minimum_required(VERSION 2.8)

include(ExternalProject)

find_package(Doxygen)
find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)

# --- Create Doxygen source documentation by typing `make doc`
if(DOXYGEN_FOUND)
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    # add 'ALL after 'doc' to create documentation, whenever you type make
    add_custom_target(doxygen
        ${DOXYGEN_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen" VERBATIM
        )
endif(DOXYGEN_FOUND)

if(SPHINX)
    set(SPHINX_RESOURCES
        source/contents.rst
        source/api.rst
        source/bugs.rst
        source/build.rst
        source/copyright.rst
        source/faq.rst
        source/glossary.rst
        source/install.rst
        source/json.rst
        source/using/index.rst
        source/whatsnew/0.1.rst
        source/_static/ufo-logo.png
        source/_templates/indexcontent.html
        source/_templates/indexsidebar.html
        )

    # Check if ExternalProject supports Git repositories
    if (${CMAKE_VERSION} VERSION_GREATER "2.8.1")
        # To use Doxygen's output, we use the Doxygen-to-Sphinx bridge called
        # Breathe that we download via Git.
        ExternalProject_Add(breathe
            GIT_REPOSITORY "https://github.com/michaeljones/breathe.git"
            GIT_TAG "origin/master"
            UPDATE_COMMAND ""
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
            INSTALL_COMMAND ""
            )

        # Let's configure the Sphinx configuration file with the paths to the
        # Doxygen output and the Breathe home directory
        set(UFO_SPHINX_EXTENSIONS "'breathe'")
        set(UFO_BREATHE_PATH
            "'${CMAKE_CURRENT_BINARY_DIR}/breathe-prefix/src/breathe'")
        set(UFO_DOXYGEN_XML_OUTPUT "'${CMAKE_CURRENT_BINARY_DIR}/xml'")
    else()
        set(UFO_SPHINX_EXTENSIONS "''")
        set(UFO_BREATHE_PATH "''")
        set(UFO_DOXYGEN_XML_OUTPUT "''")
    endif()

    foreach(file ${SPHINX_RESOURCES})
        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${file}
            ${CMAKE_CURRENT_BINARY_DIR}/${file} COPYONLY)
    endforeach()

    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/conf.py.in
        ${CMAKE_CURRENT_BINARY_DIR}/source/conf.py)

    add_custom_target(docs
        ${SPHINX} -b html ${CMAKE_CURRENT_BINARY_DIR}/source docs-${UFO_VERSION_STRING_SHORT} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating user documentation with Sphinx" VERBATIM
        )
endif()
