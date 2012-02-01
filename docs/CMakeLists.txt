cmake_minimum_required(VERSION 2.8)

find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)
mark_as_advanced(SPHINX)

# --- End-user manual ---------------------------------------------------------
if(SPHINX)
    option(WITH_MANUAL "Build user manual" ON)

    if (WITH_MANUAL)
        set(sphinx_output_dir ${CMAKE_CURRENT_BINARY_DIR}/manual)

        set(sphinx_source
            contents.rst
            api.rst
            bugs.rst
            build.rst
            copyright.rst
            faq.rst
            filters.rst
            glossary.rst
            install.rst
            install-linux.rst
            install-mac.rst
            json.rst
            using/index.rst
            whatsnew/0.1.rst
            )

        set(sphinx_static
            _static/ufo-logo.png
            _templates/indexcontent.html
            _templates/indexsidebar.html
            )

        configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/conf.py.in
            ${sphinx_output_dir}/conf.py)
        
        foreach(file ${sphinx_static})
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/source/${file}
                ${CMAKE_CURRENT_BINARY_DIR}/manual/${file} COPYONLY)
        endforeach()

        set(sphinx_output "")
        set(sphinx_input "")

        foreach(file ${sphinx_source})
            string(REPLACE "rst" "html" html_file ${file})
            set(sphinx_output ${sphinx_output} ${sphinx_output_dir}/html/${html_file})
            set(sphinx_input ${sphinx_input} ${CMAKE_CURRENT_SOURCE_DIR}/source/${file})
        endforeach()

        add_custom_command(OUTPUT ${sphinx_output}
            COMMAND ${SPHINX} -b html -c ${sphinx_output_dir} ${CMAKE_CURRENT_SOURCE_DIR}/source html
            DEPENDS ${sphinx_input}
            WORKING_DIRECTORY ${sphinx_output_dir})
        add_custom_target(manual ALL DEPENDS ${sphinx_output})
        add_dependencies(manual ufo)
    endif()
endif()
