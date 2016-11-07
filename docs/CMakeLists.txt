cmake_minimum_required(VERSION 2.6)

find_program(SPHINX sphinx-build PATHS /usr/local/bin /usr/bin)
find_program(A2X NAMES a2x a2x.py)
mark_as_advanced(SPHINX)
mark_as_advanced(A2X)

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

# --- Manpages # --------------------------------------------------------------
if (A2X)
    option(WITH_MANPAGES "Build manpages" ON)

    if (WITH_MANPAGES)
        set(MAN_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/man)
        set(MAN_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/man/man1)

        file(MAKE_DIRECTORY ${MAN_BUILD_DIR})

        set(A2X_OPTS
            -D ${MAN_BUILD_DIR}
            -d manpage
            -f manpage
            )

        set(MAN_NAMES
            ufo-launch.1
            ufo-mkfilter.1
            ufo-query.1
            ufo-runjson.1
            ufod.1
            )

        set(MAN_FILES)

        foreach(m IN LISTS MAN_NAMES)
            set(mf ${MAN_BUILD_DIR}/${m})
            set(ms ${MAN_SOURCE_DIR}/${m}.txt)

            add_custom_command(OUTPUT ${mf}
                COMMAND ${A2X} ${A2X_OPTS} ${ms}
                DEPENDS ${ms}
                COMMENT "Building manpage ${m}"
                VERBATIM)

            list(APPEND MAN_FILES ${mf})
            install(FILES ${mf} DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
        endforeach()

        add_custom_target(man ALL DEPENDS ${MAN_FILES})
    endif ()
endif ()
