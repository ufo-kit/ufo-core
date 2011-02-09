cmake_minimum_required(VERSION 2.6)
project(ufo)

enable_testing()

add_subdirectory(ufo-core)
add_subdirectory(ufo-tools)
add_subdirectory(ufo-bindings)
add_subdirectory(thirdparty)

# --- Generate packages -------------------------------------------------------
set(CPACK_GENERATOR "DEB")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Matthias Vogelgesang <matthias.vogelgesang@ipe.fzk.de>")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION "Core libraries and tools of the UFO project")
set(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY "Core libraries and tools of the UFO project")
set(CPACK_DEBIAN_PACKAGE_VERSION "0.0.1")

# The following re-naming is necessary in order to create Debian-like .deb
# files. This might change with a further patch to CPack.
set(CPACK_PACKAGE_FILE_NAME
    "${CMAKE_PROJECT_NAME}_${CPACK_DEBIAN_PACKAGE_VERSION}_${CPACK_DEBIAN_PACKAGE_ARCHITECTURE}")
include(CPack)
