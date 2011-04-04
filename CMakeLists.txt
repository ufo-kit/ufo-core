cmake_minimum_required(VERSION 2.6)
project(ufo)

enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/core/src")

add_subdirectory(core)
add_subdirectory(tools)
add_subdirectory(bindings)
add_subdirectory(thirdparty)


# --- Generate packages -------------------------------------------------------
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Matthias Vogelgesang <matthias.vogelgesang@kit.edu>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Core libraries and tools of the UFO project")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY "Core libraries and tools of the UFO project")
set(CPACK_DEBIAN_PACKAGE_VERSION "0.0.1")
# FIXME: this should not be hardcoded
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

# The following re-naming is necessary in order to create Debian-like .deb
# files. This might change with a further patch to CPack.
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")

include(CPack)
