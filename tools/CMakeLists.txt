cmake_minimum_required(VERSION 2.8)

#add_executable(runjson runjson.c)
add_executable(ufod ufo-server.c)

#target_link_libraries(runjson ufo ${GLIB2_LIBRARIES})
target_link_libraries(ufod ufo ${GLIB2_LIBRARIES})

install(TARGETS ufod #runjson
    RUNTIME DESTINATION bin)

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/clprof
    DESTINATION bin)
