cmake_minimum_required(VERSION 2.8)

find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)
mark_as_advanced(SPHINX)

# --- End-user manual ---------------------------------------------------------
if(SPHINX)
    option(WITH_MANUAL "Build user manual" ON)

    if (WITH_MANUAL)
        set(input_dir ${CMAKE_CURRENT_SOURCE_DIR}/manual)
        set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/manual)

        file(GLOB_RECURSE sphinx_source ${input_dir}/*.rst)
        list(APPEND sphinx_source "${output_dir}/conf.py")

        set(sphinx_static
            _static/ufo-logo.png)

        configure_file(${input_dir}/conf.py.in ${output_dir}/conf.py)
        
        foreach(file ${sphinx_static})
            configure_file(${input_dir}/${file} ${output_dir}/${file} COPYONLY)
        endforeach()

        add_custom_command(OUTPUT ${output_dir}/html/index.html
            COMMAND ${SPHINX} -b html -c ${output_dir} ${input_dir} html
            DEPENDS ${sphinx_source}
            WORKING_DIRECTORY ${output_dir}
            COMMENT "Build Sphinx HTML")

        add_custom_target(manual ALL DEPENDS ${output_dir}/html/index.html)

        add_dependencies(manual ufo)
    endif()
endif()

