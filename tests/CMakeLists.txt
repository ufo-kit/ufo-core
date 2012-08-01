#
# We create two targets that call the unit test binaries in core/tests in
# different ways:
#    - test
#    - gtest
# The first target uses CMake's own testing facilities and gives a simple
# overview if a certain test module passed or not. 'gtest' on the other hand
# runs the `gtester` tool which outputs failing test cases and creates a summary
# HTML file called results.html using the `gtester-report` tool.
#
cmake_minimum_required(VERSION 2.8)

# configure unit tests
set(TEST_SRCS
    test-buffer.c
    test-filter.c
    test-relation.c
    )

option(WITH_TESTS "Run tests on each build" ON)
if (WITH_TESTS)
    set(TEST_DEPENDENCIES)
    set(TEST_BINARIES)

    foreach(_src ${TEST_SRCS})
        string(REPLACE ".c" "" _binary ${_src})
        list(APPEND TEST_BINARIES ${_binary})

        add_executable(${_binary} ${_src})
        target_link_libraries(${_binary} ufo ${GLIB2_LIBRARIES})
        add_test(${_binary} ${_binary})

        string(REPLACE "test" "ufo" _dependency ${_src})
        list(APPEND TEST_DEPENDENCIES "${CMAKE_CURRENT_SOURCE_DIR}/${_src}")
        list(APPEND TEST_DEPENDENCIES "${CMAKE_CURRENT_SOURCE_DIR}/../src/${_dependency}")
    endforeach()

    add_executable(runjson test.c)
    target_link_libraries(runjson ufo ${GLIB2_LIBRARIES})
    install(TARGETS runjson
        RUNTIME DESTINATION bin)
endif()
