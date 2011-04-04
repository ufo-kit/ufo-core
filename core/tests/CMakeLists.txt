cmake_minimum_required(VERSION 2.6)

find_package(Check)

if(CHECK_FOUND)
    include_directories(${CHECK_INCLUDE_DIRS})

    add_executable(ufotest ufo-tests.c)
    target_link_libraries(ufotest ufocore ${CHECK_LIBRARIES})

    add_test(ufocore ufotest)

endif()
