cmake_minimum_required(VERSION 2.8)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufo-gir.sh.in
    ${CMAKE_CURRENT_BINARY_DIR}/ufo-gir.sh)

add_custom_target(gir 
    sh ${CMAKE_CURRENT_BINARY_DIR}/ufo-gir.sh >& /dev/null)

set(GIR_NAME "Ufo-${UFO_VERSION_STRING_SHORT}.gir")

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${GIR_NAME}
    DESTINATION share/gir-1.0)

