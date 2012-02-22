cmake_minimum_required(VERSION 2.8)

find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)
mark_as_advanced(SPHINX)

# --- End-user manual ---------------------------------------------------------
if(SPHINX)
    option(WITH_MANUAL "Build user manual" ON)

    if (WITH_MANUAL)
        set(sphinx_output_dir ${CMAKE_CURRENT_BINARY_DIR}/manual)

        file(GLOB_RECURSE sphinx_source ${CMAKE_CURRENT_SOURCE_DIR}/source/*.rst)
        list(APPEND sphinx_source "${sphinx_output_dir}/conf.py")

        set(sphinx_static
            _static/ufo-logo.png
            _templates/indexcontent.html
            _templates/indexsidebar.html
            _templates/layout.html
            )

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/conf.py.in
            ${sphinx_output_dir}/conf.py)
        
        foreach(file ${sphinx_static})
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/${file}
                ${CMAKE_CURRENT_BINARY_DIR}/manual/${file} COPYONLY)
        endforeach()

        message("${sphinx_source}")

        add_custom_command(OUTPUT ${sphinx_output_dir}/html/index.html
            COMMAND ${SPHINX} -b html -c ${sphinx_output_dir} ${CMAKE_CURRENT_SOURCE_DIR}/source html
            DEPENDS ${sphinx_source}
            WORKING_DIRECTORY ${sphinx_output_dir}
            COMMENT "Build Sphinx HTML")

        add_custom_target(manual 
            ALL DEPENDS ${sphinx_output_dir}/html/index.html)

        add_dependencies(manual ufo)
    endif()
endif()

