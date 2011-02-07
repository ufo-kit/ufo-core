cmake_minimum_required(VERSION 2.6)

# --- Build static Apple OpenCL FFT library
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/ufo-common/cmake")
set(CMAKE_CXX_FLAGS "-fPIC")

find_package(OpenCL)

if(OPENCL_FOUND)
    include_directories(${OPENCL_INCLUDE_DIRS})

    add_library(oclfft STATIC 
        fft_execute.cpp
        fft_setup.cpp
        fft_kernelstring.cpp)

    target_link_libraries(oclfft ${OPENCL_LIBRARIES})

endif(OPENCL_FOUND)
