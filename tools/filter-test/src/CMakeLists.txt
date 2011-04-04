cmake_minimum_required(VERSION 2.8)

find_package(PkgConfig REQUIRED)

pkg_check_modules(GLIB2 glib-2.0)

include_directories(${PROJECT_SOURCE_DIR}/ufo-core/src ${GLIB2_INCLUDE_DIRS})

add_definitions("-std=c99 -Wall -Werror")

add_executable(filter-test main.c)
target_link_libraries(filter-test ufocore)
