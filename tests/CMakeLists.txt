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
    test-suite.c
    test-buffer.c
    test-channel.c
    test-filter.c
    test-relation.c
    )

set(SUITE_BIN "test-suite")

option(WITH_TESTS "Run tests on each build" ON)

if (WITH_TESTS)
    include_directories({CMAKE_CURRENT_SOURCE_DIR})
    add_executable(${SUITE_BIN} ${TEST_SRCS})
    target_link_libraries(${SUITE_BIN} ufo ${GLIB2_LIBRARIES})
    add_test(${SUITE_BIN} ${SUITE_BIN})

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl"
        "${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl" @ONLY IMMEDIATE)
endif()

add_executable(runjson test.c)
target_link_libraries(runjson ufo ${GLIB2_LIBRARIES})
install(TARGETS runjson
    RUNTIME DESTINATION bin)
