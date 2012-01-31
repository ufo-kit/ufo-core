cmake_minimum_required(VERSION 2.8)

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
