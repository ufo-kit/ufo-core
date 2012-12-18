cmake_minimum_required(VERSION 2.6)
project(ufo C)

set(TARNAME "libufo")
set(UFO_VERSION_MAJOR "0")
set(UFO_VERSION_MINOR "3")
set(UFO_VERSION_PATCH "0")
set(UFO_GIR_VERSION "${UFO_VERSION_MAJOR}.${UFO_VERSION_MINOR}")

# increase UFO_SO_VERSION on each version that breaks ABI compatibility
set(UFO_SO_VERSION "3")

set(UFO_DESCRIPTION "UFO high-speed image processing core library")
set(UFO_DESCRIPTION_SUMMARY "UFO high-speed image processing core library")

set(PACKAGE_VERSION_MAJOR ${UFO_VERSION_MAJOR})
set(PACKAGE_VERSION_MINOR ${UFO_VERSION_MINOR})
set(PACKAGE_VERSION_PATCH ${UFO_VERSION_PATCH})
set(PACKAGE_VERSION "${PACKAGE_VERSION_MAJOR}.${PACKAGE_VERSION_MINOR}.${PACKAGE_VERSION_PATCH}")
set(PACKAGE_NAME ${TARNAME})
set(PACKAGE_TARNAME ${TARNAME})
set(PACKAGE_STRING "${PACKAGE_NAME} ${PACKAGE_VERSION}")
set(PACKAGE_BUGREPORT "http://ufo.kit.edu/ufo/newticket")

enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")


# --- Options -----------------------------------------------------------------
option(WITH_PROFILING "Enable profiling" OFF)
if (WITH_PROFILING)
    add_definitions("-pg")
    set(CMAKE_C_FLAGS "-pg")
endif ()


# --- Find packages and libraries ---------------------------------------------

# These packages are required in all sub-directories because Glib and GObject is
# part of the API/ABI.

find_package(OpenCL REQUIRED)
find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 glib-2.0>=2.22 REQUIRED)
pkg_check_modules(GOBJECT2 gobject-2.0>=2.22 REQUIRED)
pkg_check_modules(GMODULE2 gmodule-2.0>=2.22 REQUIRED)
pkg_check_modules(GTHREAD2 gthread-2.0>=2.22 REQUIRED)
pkg_check_modules(JSON_GLIB json-glib-1.0 REQUIRED)
pkg_check_modules(ZMQ libzmq>=3.2 REQUIRED)

set(UFOCORE_DEPS
    ${OPENCL_LIBRARIES}
    ${GLIB2_LIBRARIES}
    ${GOBJECT2_LIBRARIES}
    ${GMODULE2_LIBRARIES}
    ${GTHREAD2_LIBRARIES}
    ${JSON_GLIB_LIBRARIES}
    ${ZMQ_LIBRARIES})


include_directories(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${CMAKE_CURRENT_BINARY_DIR}/src
    ${GLIB2_INCLUDE_DIRS}
    ${OPENCL_INCLUDE_DIRS}
    ${JSON_GLIB_INCLUDE_DIRS}
    ${ZMQ_INCLUDE_DIRS})

link_directories(
    ${ZMQ_LIBRARY_DIRS}
    ${JSON_GLIB_LIBRARY_DIRS})

add_definitions("-std=c99 -pedantic -Wall -Wextra -fPIC")

if (CMAKE_COMPILER_IS_GNUCC)
    add_definitions("-Wmissing-prototypes -Wmissing-declarations -Wshadow
    -Wpointer-arith -Wcast-align -Wwrite-strings -Wredundant-decls -Wcast-qual
    -Wnested-externs -Winline -Wno-long-long -Wconversion -Wstrict-prototypes")

    add_definitions("-Wno-unused-parameter -Wno-missing-field-initializers")
endif()

add_definitions(-DG_LOG_DOMAIN=\"Ufo\")

add_subdirectory(src)
#add_subdirectory(docs)
#add_subdirectory(python)
add_subdirectory(tests)
add_subdirectory(tools)


# --- Package generation ------------------------------------------------------
set(CPACK_PACKAGE_DESCRIPTION ${UFO_DESCRIPTION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY ${UFO_DESCRIPTION_SUMMARY})
set(CPACK_PACKAGE_NAME ${TARNAME})

set(CPACK_PACKAGE_CONTACT "matthias.vogelgesang@kit.edu")
set(CPACK_PACKAGE_VENDOR "Karlsruhe Institute of Technology/IPE")
set(CPACK_PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PACKAGE_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PACKAGE_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME "${CPACK_PACKAGE_NAME}-${PACKAGE_VERSION}-${CMAKE_SYSTEM_PROCESSOR}")
set(VERSION ${PACKAGE_VERSION})

set(CPACK_GENERATOR "DEB;RPM;")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES "tags" ".bzr" ".swp" "~1~" ".git" ".xml")
set(CPACK_SOURCE_PACKAGE_FILE_NAME "${PACKAGE_NAME}-${PACKAGE_VERSION}" CACHE INTERNAL "tarball basename")

# --- Distro specific
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6 (>= 2.3.6), libgcc1 (>= 1:4.1)")

include(CPack)
