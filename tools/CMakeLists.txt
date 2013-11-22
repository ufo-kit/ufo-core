cmake_minimum_required(VERSION 2.6)
#{{{ Binaries
add_executable(ufo-runjson runjson.c)
add_executable(ufod ufod.c)

target_link_libraries(ufo-runjson ufo ${UFOCORE_DEPS})
target_link_libraries(ufod ufo ${UFOCORE_DEPS})

install(TARGETS ufod ufo-runjson
        RUNTIME DESTINATION ${UFO_BINDIR})
#}}}
#{{{ ufo-mkfilter
set(UFO_FILTER_TEMPLATE_DIR "${UFO_DATADIR}/ufo/templates")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufo-mkfilter.py
               ${CMAKE_CURRENT_BINARY_DIR}/ufo-mkfilter
               @ONLY IMMEDIATE)

file(GLOB templates "templates/*.in")

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ufo-mkfilter
        DESTINATION ${UFO_BINDIR})

install(FILES ${templates}
        DESTINATION ${UFO_FILTER_TEMPLATE_DIR})
#}}}
