cmake_minimum_required(VERSION 2.6)

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

add_executable(${SUITE_BIN} ${TEST_SRCS})

target_link_libraries(${SUITE_BIN} ufo ${UFOCORE_DEPS})

add_test(${SUITE_BIN} ${SUITE_BIN})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl"
               "${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl"
               @ONLY IMMEDIATE)
