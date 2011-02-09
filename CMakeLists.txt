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

include(CPack)
