cmake_minimum_required(VERSION 2.8)

add_executable(runjson runjson.c)

target_link_libraries(runjson
    ufo
    ${GLIB2_LIBRARIES})

install(TARGETS runjson
    RUNTIME DESTINATION bin)
