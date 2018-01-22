cmake_minimum_required(VERSION 2.6)

#{{{ Binaries
add_executable(ufo-launch ufo-launch.c)
add_executable(ufo-query ufo-query.c)
add_executable(ufo-runjson ufo-runjson.c)

target_link_libraries(ufo-launch ufo)
target_link_libraries(ufo-query ufo)
target_link_libraries(ufo-runjson ufo)

install(TARGETS ufo-launch ufo-query ufo-runjson
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(PROGRAMS ${CMAKE_CURRENT_SOURCE_DIR}/ufo-prof
        DESTINATION ${CMAKE_INSTALL_BINDIR})

option(INSTALL_COMPLETION "Install Bash completion script" OFF)

if (INSTALL_COMPLETION)
    pkg_check_variable(bash-completion completionsdir)

    install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/ufo-launch
            DESTINATION ${BASH_COMPLETION_COMPLETIONSDIR}
            OPTIONAL)
endif()
#}}}
#{{{ ufo-mkfilter
set(CMAKE_INSTALL_TEMPLATEDIR "${CMAKE_INSTALL_FULL_DATADIR}/ufo/templates")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ufo-mkfilter.in
               ${CMAKE_CURRENT_BINARY_DIR}/ufo-mkfilter
               @ONLY IMMEDIATE)

file(GLOB templates "templates/*.in")

install(PROGRAMS ${CMAKE_CURRENT_BINARY_DIR}/ufo-mkfilter
        DESTINATION ${CMAKE_INSTALL_BINDIR})

install(FILES ${templates}
        DESTINATION ${CMAKE_INSTALL_TEMPLATEDIR})
#}}}
