cmake_minimum_required(VERSION 2.6)

find_package(Doxygen)
find_program(SPHINX sphinx-build PATHS /usr/bin)

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
    add_custom_target(docs
        ${SPHINX} -b html ${CMAKE_CURRENT_SOURCE_DIR}/source docs-${UFO_VERSION_STRING_SHORT} 
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating user documentation with Sphinx" VERBATIM
        )
endif()
