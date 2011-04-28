cmake_minimum_required(VERSION 2.6)
project(ufo)

set(UFO_VERSION_MAJOR "0")
set(UFO_VERSION_MINOR "1")
set(UFO_VERSION_PATCH "0")
set(UFO_VERSION_STRING_LONG "${UFO_VERSION_MAJOR}.${UFO_VERSION_MINOR}.${UFO_VERSION_PATCH}")
set(UFO_VERSION_STRING_SHORT "${UFO_VERSION_MAJOR}.${UFO_VERSION_MINOR}")

enable_testing()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/common/cmake")

include_directories("${CMAKE_CURRENT_SOURCE_DIR}/core/src")

add_subdirectory(core)

