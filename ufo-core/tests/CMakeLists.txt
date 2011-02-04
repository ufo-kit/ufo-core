cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/ufo-common/cmake")
find_package(Check)

if(CHECK_FOUND)
    include_directories(${PROJECT_SOURCE_DIR}/ufo-core/src ${CHECK_INCLUDE_DIRS})

    add_executable(ufotest main.c)
    target_link_libraries(ufotest ufocore ${CHECK_LIBRARIES})

    add_test(ufocore ufotest)

endif()
