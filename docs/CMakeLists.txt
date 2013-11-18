cmake_minimum_required(VERSION 2.6)

find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)
mark_as_advanced(SPHINX)

# --- End-user manual ---------------------------------------------------------
if(SPHINX)
    option(WITH_MANUAL "Build user manual" ON)

    if (WITH_MANUAL)
        set(input_dir ${CMAKE_CURRENT_SOURCE_DIR}/manual)
        set(output_dir ${CMAKE_CURRENT_BINARY_DIR}/manual/html)

        file(GLOB_RECURSE sphinx_source ${input_dir}/*.rst)
        
        foreach(file ${sphinx_static})
            configure_file(${input_dir}/${file} ${output_dir}/${file} COPYONLY)
        endforeach()

        add_custom_command(OUTPUT ${output_dir}/index.html
            COMMAND ${SPHINX} -b html ${input_dir} ${output_dir}
            DEPENDS ${sphinx_source}
            COMMENT "Build Sphinx HTML")

        add_custom_target(manual ALL DEPENDS ${output_dir}/index.html)

        add_dependencies(manual ufo)
    endif()
endif()

