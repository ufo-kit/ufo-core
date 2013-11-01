cmake_minimum_required(VERSION 2.6)

add_executable(runjson runjson.c)
add_executable(ufod ufod.c)

target_link_libraries(runjson ufo ${UFOCORE_DEPS})
target_link_libraries(ufod ufo ${UFOCORE_DEPS})

install(TARGETS ufod runjson
        RUNTIME DESTINATION ${UFO_BINDIR})
