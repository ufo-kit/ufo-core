cmake_minimum_required(VERSION 2.6)

# --- Find packages and libraries ---------------------------------------------
find_program(PYTHON "python")

if (PYTHON)
    option(WITH_PYTHON_TOOLS "Build Python tools module" ON)

    if (WITH_PYTHON_TOOLS)
        set(SETUP_PY_IN "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in")
        set(SETUP_PY    "${CMAKE_CURRENT_BINARY_DIR}/setup.py")
        set(DEPS        "${CMAKE_CURRENT_SOURCE_DIR}/ufotools/__init__.py")
        set(OUTPUT      "${CMAKE_CURRENT_BINARY_DIR}/build")

        configure_file(${SETUP_PY_IN} ${SETUP_PY})

        add_custom_command(OUTPUT ${OUTPUT}
                           COMMAND ${PYTHON}
                           ARGS setup.py build
                           DEPENDS ${DEPS}
                           COMMENT "Build ufotools")

        add_custom_target(ufotools ALL DEPENDS ${OUTPUT})

        install(CODE "execute_process(COMMAND ${PYTHON} ${SETUP_PY} install)")
    endif()
endif()
