cmake_minimum_required(VERSION 2.6)

#{{{ Binaries
set(WITH_SYSTEMDSYSTEMUNITDIR "${UFO_PREFIX}/lib/systemd/system" CACHE STRING "systemd unit directory")

add_executable(ufo-launch ufo-launch.c)
add_executable(ufo-runjson ufo-runjson.c)
add_executable(ufod ufod.c)

target_link_libraries(ufo-launch ufo ${UFOCORE_DEPS})
target_link_libraries(ufo-runjson ufo ${UFOCORE_DEPS})
target_link_libraries(ufod ufo ${UFOCORE_DEPS})

install(TARGETS ufod ufo-launch ufo-runjson
        RUNTIME DESTINATION ${UFO_BINDIR})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufod.service.in
               ${CMAKE_CURRENT_BINARY_DIR}/ufod.service)

install(FILES ${CMAKE_CURRENT_BINARY_DIR}/ufod.service
        DESTINATION ${WITH_SYSTEMDSYSTEMUNITDIR})
#}}}
#{{{ ufo-mkfilter
set(UFO_FILTER_TEMPLATE_DIR "${UFO_DATADIR}/ufo/templates")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufo-mkfilter.in
               ${CMAKE_CURRENT_BINARY_DIR}/ufo-mkfilter
               @ONLY IMMEDIATE)

file(GLOB templates "templates/*.in")

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ufo-mkfilter
        DESTINATION ${UFO_BINDIR})

install(FILES ${templates}
        DESTINATION ${UFO_FILTER_TEMPLATE_DIR})
#}}}
