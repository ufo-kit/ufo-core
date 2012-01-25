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

# --- If Python and NumPy libraries are available build the UfoBuffer wrapper
find_package(PythonLibs)
find_package(Numpy)
pkg_check_modules(PYGOBJECT pygobject-2.0)

if (PYTHONLIBS_FOUND AND PYTHON_NUMPY_FOUND AND PYGOBJECT_FOUND)
    include_directories(
        ${PYTHON_INCLUDE_DIRS}
        ${PYTHON_NUMPY_INCLUDE_DIR}
        ${PYGOBJECT_INCLUDE_DIRS})
    add_library(ufonp SHARED ufonp.c)
    set_target_properties(ufonp PROPERTIES PREFIX "")
    target_link_libraries(ufonp 
        ufo
        ${PYTHON_LIBRARIES}
        ${PYGOBJECT_LIBRARIES})
endif()
