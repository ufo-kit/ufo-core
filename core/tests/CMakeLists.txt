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
set(TEST_BINARIES
    buffer
    sequence
    split
    )

foreach(binary ${TEST_BINARIES})
    add_executable(${binary} ${binary}.c)
    target_link_libraries(${binary} ufocore ${GLIB2_LIBRARIES})
    add_test(${binary} ${binary})
endforeach()

add_custom_target(gtest
    gtester ${TEST_BINARIES} -o results.xml && gtester-report results.xml > results.html
)

# non-unit test
file(COPY test.json DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
add_executable(foobar test.c)
target_link_libraries(foobar ufocore ${GLIB2_LIBRARIES})

