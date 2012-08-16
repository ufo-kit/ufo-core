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

option(WITH_TESTS "Build test suite" ON)

if (WITH_TESTS)
    include_directories({CMAKE_CURRENT_SOURCE_DIR})
    add_executable(${SUITE_BIN} ${TEST_SRCS})
    target_link_libraries(${SUITE_BIN} ufo ${GLIB2_LIBRARIES})
    add_test(${SUITE_BIN} ${SUITE_BIN})

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl"
        "${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl" @ONLY IMMEDIATE)
endif()
