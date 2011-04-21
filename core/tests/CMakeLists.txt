cmake_minimum_required(VERSION 2.6)

file(COPY test.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(ufotest ufo-tests.c)
target_link_libraries(ufotest ufocore ${GLIB2_LIBRARIES})

add_test(ufocore ufotest)
