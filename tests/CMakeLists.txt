cmake_minimum_required(VERSION 2.8)

# configure unit tests
set(TEST_SRCS
    test-suite.c
    test-buffer.c
    test-config.c
    test-graph.c
    test-profiler.c
    test-remote-node.c
    test-zmq-messenger.c
    )

set(SUITE_BIN "test-suite")

option(WITH_TESTS "Build test suite" ON)

if (WITH_TESTS)
    include_directories(
        {CMAKE_CURRENT_SOURCE_DIR})

    add_executable(${SUITE_BIN} ${TEST_SRCS})

    target_link_libraries(${SUITE_BIN}
        ufo
        ${UFOCORE_DEPS})

    add_test(${SUITE_BIN} ${SUITE_BIN})

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl"
        "${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl" @ONLY IMMEDIATE)
endif()
