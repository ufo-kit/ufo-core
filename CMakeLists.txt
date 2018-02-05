cmake_minimum_required(VERSION 2.8)

if (POLICY CMP0053)
cmake_policy(SET CMP0053 OLD)
endif ()

if (POLICY CMP0054)
cmake_policy(SET CMP0054 OLD)
endif ()

project(ufo C)

set(TARNAME "libufo")

set(UFO_NAME                "libufo")
set(UFO_DESCRIPTION         "UFO high-speed image processing core library")
set(UFO_DESCRIPTION_SUMMARY "UFO high-speed image processing core library")

#{{{ Library version
set(UFO_VERSION_MAJOR "0")
set(UFO_VERSION_MINOR "15")
set(UFO_VERSION_PATCH "2")
set(UFO_VERSION "${UFO_VERSION_MAJOR}.${UFO_VERSION_MINOR}.${UFO_VERSION_PATCH}")
set(UFO_GIR_VERSION "${UFO_VERSION_MAJOR}.0")
#}}}
#{{{ CMake
enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")

include(GNUInstallDirs)
include(PkgConfigVars)

set(CMAKE_INSTALL_INCLUDEDIR    "${CMAKE_INSTALL_INCLUDEDIR}/ufo-${UFO_VERSION_MAJOR}")
set(CMAKE_INSTALL_KERNELDIR     "${CMAKE_INSTALL_DATAROOTDIR}/ufo")
set(CMAKE_INSTALL_GIRDIR        "${CMAKE_INSTALL_DATAROOTDIR}/gir-1.0")
set(CMAKE_INSTALL_PLUGINDIR     "${CMAKE_INSTALL_LIBDIR}/ufo")
set(CMAKE_INSTALL_TYPELIBDIR    "${CMAKE_INSTALL_LIBDIR}/girepository-1.0")
set(CMAKE_INSTALL_PKGCONFIGDIR  "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

if (APPLE)
    set(CMAKE_MACOSX_RPATH      "ON")
    set(CMAKE_INSTALL_RPATH     "${CMAKE_INSTALL_PREFIX}/lib")
endif()

#}}}
#{{{ Options
option(WITH_TESTS "Build test suite" ON)
option(WITH_DEPRECATED_OPENCL_1_1_API "Build with deprecated OpenCL 1.1 API" ON)

if (WITH_DEPRECATED_OPENCL_1_1_API)
    add_definitions ("-DCL_USE_DEPRECATED_OPENCL_1_1_APIS")
endif ()
#}}}
#{{{ Dependencies
set(PKG_GLIB2_MIN_REQUIRED "2.38")
set(PKG_JSON_GLIB_MIN_REQUIRED "0.10.0")

find_package(OpenCL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 glib-2.0>=${PKG_GLIB2_MIN_REQUIRED} REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=${PKG_GLIB2_MIN_REQUIRED} REQUIRED)
pkg_check_modules(GMODULE2 gmodule-2.0>=${PKG_GLIB2_MIN_REQUIRED} REQUIRED)
pkg_check_modules(GTHREAD2 gthread-2.0>=${PKG_GLIB2_MIN_REQUIRED} REQUIRED)
pkg_check_modules(GIO2 gio-2.0>=${PKG_GLIB2_MIN_REQUIRED} REQUIRED)
pkg_check_modules(JSON_GLIB json-glib-1.0>=${PKG_JSON_GLIB_MIN_REQUIRED} REQUIRED)
pkg_check_modules(PYTHON python)

set(UFOCORE_DEPS
    m
    ${OpenCL_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD2_LIBRARIES}
    ${GIO2_LIBRARIES}
    ${JSON_GLIB_LIBRARIES})

if (PYTHON_FOUND)
    option(WITH_PYTHON "Enable Python multithreading support" ON)

    if (WITH_PYTHON)
        include_directories(${PYTHON_INCLUDE_DIRS})
        list(APPEND UFOCORE_DEPS ${PYTHON_LIBRARIES})
    endif ()
endif()

#{{{ Link dirs of dependencies
link_directories(
    ${GLIB2_LIBRARY_DIRS}
    ${JSON_GLIB_LIBRARY_DIRS})
#}}}
#{{{ Add include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}
    ${GLIB2_INCLUDE_DIRS}
    ${GIO2_INCLUDE_DIRS}
    ${OpenCL_INCLUDE_DIRS}
    ${JSON_GLIB_INCLUDE_DIRS})
#}}}
#}}}
#{{{ Definitions
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=gnu99 -Wall -Wextra -fPIC -Wno-unused-parameter -Wno-deprecated-declarations")

add_definitions(-DG_LOG_DOMAIN=\"Ufo\"
                -D_FILE_OFFSET_BITS=64)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/config.h)
#}}}
#{{{ Subdirectories
add_subdirectory(ufo)
add_subdirectory(docs)
add_subdirectory(bin)

if (WITH_TESTS)
    add_subdirectory(tests)
endif()
#}}}
