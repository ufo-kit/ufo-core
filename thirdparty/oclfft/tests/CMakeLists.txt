cmake_minimum_required(VERSION 2.6)

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/ufo-common/cmake")
find_package(Check)
find_package(OpenCL)

if(CHECK_FOUND AND OPENCL_FOUND)
    include_directories(${PROJECT_SOURCE_DIR}/thirdparty/oclfft/src
        ${CHECK_INCLUDE_DIRS}
        ${OPENCL_INCLUDE_DIRS})

    add_definitions("-Wall -std=c99")
    add_executable(oclfft-test oclfft-tests.c)

    target_link_libraries(oclfft-test oclfft
        ${CHECK_LIBRARIES}
        ${OPENCL_LIBRARIES})

    add_test(oclfft oclfft-test)
endif()
