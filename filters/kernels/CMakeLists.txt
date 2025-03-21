cmake_minimum_required(VERSION 2.6)

# make burst backprojection kernels
set(COMMON_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/common.in)
set(DEFINITIONS ${CMAKE_CURRENT_SOURCE_DIR}/templates/definitions.in)
set(Z_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/z_template.in)
set(LAMINO_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/lamino_template.in)
set(CENTER_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/center_template.in)
set(ROLL_TEMPLATE ${CMAKE_CURRENT_SOURCE_DIR}/templates/roll_template.in)
set(GENERAL_BP_DEFINITIONS templates/general_bp_definitions.in)
set(GENERAL_BP_HEADER_SCALAR templates/general_bp_header_scalar.in)
set(GENERAL_BP_HEADER_VECTOR templates/general_bp_header_vector.in)
set(GENERAL_BP_BODY templates/general_bp_body.in)
set(GENERATOR ${CMAKE_CURRENT_SOURCE_DIR}/tools/make_burst_kernels.py)
set(Z_KERNEL z_kernel.cl)
set(LAMINO_KERNEL lamino_kernel.cl)
set(CENTER_KERNEL center_kernel.cl)
set(ROLL_KERNEL roll_kernel.cl)

find_package(PythonInterp REQUIRED)

add_custom_command(
    OUTPUT ${Z_KERNEL} ${CENTER_KERNEL} ${LAMINO_KERNEL} ${ROLL_KERNEL}
    COMMAND ${PYTHON_EXECUTABLE} ${GENERATOR} ${Z_TEMPLATE} 1 2 4 8 16 > ${Z_KERNEL}
    COMMAND ${PYTHON_EXECUTABLE} ${GENERATOR} ${CENTER_TEMPLATE} 1 2 4 8 16 > ${CENTER_KERNEL}
    COMMAND ${PYTHON_EXECUTABLE} ${GENERATOR} ${LAMINO_TEMPLATE} 1 2 4 8 16 > ${LAMINO_KERNEL}
    COMMAND ${PYTHON_EXECUTABLE} ${GENERATOR} ${ROLL_TEMPLATE} 1 2 4 8 16 > ${ROLL_KERNEL}
    DEPENDS ${GENERATOR} ${COMMON_TEMPLATE} ${DEFINITIONS} ${Z_TEMPLATE} ${CENTER_TEMPLATE} ${LAMINO_TEMPLATE} ${ROLL_TEMPLATE}
    COMMENT "Generating burst backprojection kernels"
)

add_custom_target(burst ALL DEPENDS ${Z_KERNEL} ${CENTER_KERNEL} ${LAMINO_KERNEL} ${ROLL_KERNEL})

install(
    FILES ${CMAKE_CURRENT_BINARY_DIR}/${Z_KERNEL}
          ${CMAKE_CURRENT_BINARY_DIR}/${CENTER_KERNEL}
          ${CMAKE_CURRENT_BINARY_DIR}/${LAMINO_KERNEL}
          ${CMAKE_CURRENT_BINARY_DIR}/${ROLL_KERNEL}
          ${CMAKE_CURRENT_SOURCE_DIR}/${GENERAL_BP_DEFINITIONS}
          ${CMAKE_CURRENT_SOURCE_DIR}/${GENERAL_BP_HEADER_SCALAR}
          ${CMAKE_CURRENT_SOURCE_DIR}/${GENERAL_BP_HEADER_VECTOR}
          ${CMAKE_CURRENT_SOURCE_DIR}/${GENERAL_BP_BODY}
    DESTINATION ${UFO_KERNELDIR}
)

# copy kernels
file(GLOB ufofilter_KERNELS "*.cl")

foreach(_kernel ${ufofilter_KERNELS})
    install(FILES ${_kernel} DESTINATION ${UFO_KERNELDIR})
endforeach()
