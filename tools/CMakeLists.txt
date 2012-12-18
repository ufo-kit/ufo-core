cmake_minimum_required(VERSION 2.8)

add_executable(runjson runjson.c)
add_executable(ufod ufo-server.c)

target_link_libraries(runjson ufo ${UFOCORE_DEPS})
target_link_libraries(ufod ufo ${UFOCORE_DEPS})

install(TARGETS ufod runjson
        RUNTIME DESTINATION bin)

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/clprof
        DESTINATION bin)
