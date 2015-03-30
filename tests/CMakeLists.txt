cmake_minimum_required(VERSION 2.6)

set(TEST_SRCS
    test-suite.c
    test-buffer.c
    test-graph.c
    test-node.c
    test-profiler.c
    test-remote-node.c
    )

set(SUITE_BIN "test-suite")

if (WITH_ZMQ)
    list(APPEND TEST_SRCS test-zmq-messenger.c)
endif ()

if (WITH_MPI)
    list(APPEND TEST_SRCS test-mpi-remote-node.c)
endif ()

if (WITH_KIRO)
    list(APPEND TEST_SRCS test-kiro-messenger.c)
endif ()

add_executable(${SUITE_BIN} ${TEST_SRCS})

target_link_libraries(${SUITE_BIN} ufo ${UFOCORE_DEPS})

add_test(${SUITE_BIN} ${SUITE_BIN})

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/gtester.xsl"
               "${CMAKE_CURRENT_BINARY_DIR}/gtester.xsl"
               @ONLY IMMEDIATE)
