cmake_minimum_required(VERSION 2.6)

# --- Set sources -------------------------------------------------------------
set(ufocore_SRCS ufo.c)

# --- Find packages and libraries ---------------------------------------------
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/ufo-common/cmake/")

find_package(OpenCL)
find_package(FFTW3)

# --- Generate config.h -------------------------------------------------------
if (OPENCL_FOUND)
    set(HAVE_OPENCL TRUE)
    include_directories(${OPENCL_INCLUDE_DIRS})
endif(OPENCL_FOUND)

if (FFTW3_FOUND)
    set(HAVE_FFTW TRUE)
endif(FFTW3_FOUND)

find_path(OCLFFT_INCLUDE_DIR clFFT.h ${PROJECT_SOURCE_DIR}/thirdparty/oclfft)
if (OCLFFT_INCLUDE_DIR)
    set(HAVE_OCLFFT TRUE)
    include_directories(${OCLFFT_INCLUDE_DIR})
endif(OCLFFT_INCLUDE_DIR)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/config.h.in ${CMAKE_CURRENT_BINARY_DIR}/config.h)

# --- Target ------------------------------------------------------------------
include_directories(${CMAKE_CURRENT_BINARY_DIR})
add_library(ufocore ${ufocore_SRCS})
target_link_libraries(ufocore oclfft)
