# - pre-configured paths for CMake
#
# Usage:
#   configure_paths(<PREFIX>)
#
# Checks if configure-like prefix and installation paths were passed by the user
# and sets up corresponding variables for use in install() commands and to fill
# out .pc files:
#
#   PREFIX_PREFIX       defaults to ...     CMAKE_INSTALL_PREFIX
#   PREFIX_EPREFIX                          PREFIX_PREFIX
#   PREFIX_SBINDIR                          PREFIX_EPREFIX/sbin
#   PREFIX_SYSCONFDIR                       PREFIX_PREFIX/etc
#   PREFIX_LOCALSTATEDIR                    PREFIX_PREFIX/var
#   PREFIX_BINDIR                           PREFIX_EPREFIX/bin
#   PREFIX_LIBDIR                           PREFIX_EPREFIX/lib
#   PREFIX_INCLUDEDIR                       PREFIX_PREFIX/include
#   PREFIX_PKGCONFIGDIR                     PREFIX_LIBDIR/pkgconfig
#   PREFIX_TYPELIBDIR                       PREFIX_LIBDIR/girepository-1.0
#   PREFIX_DATAROOTDIR                      PREFIX_PREFIX/share
#   PREFIX_DATADIR                          PREFIX_DATAROOTDIR
#   PREFIX_INFODIR                          PREFIX_DATAROOTDIR/info
#   PREFIX_MANDIR                           PREFIX_DATAROOTDIR/man
#   PREFIX_LOCALEDIR                        PREFIX_DATAROOTDIR/locale
#   PREFIX_GIRDIR                           PREFIX_DATAROOTDIR/gir-1.0

# Copyright (C) 2013 Matthias Vogelgesang <matthias.vogelgesang@gmail.com>
#
# Redistribution and use, with or without modification, are permitted
# provided that the following conditions are met:
# 
#    1. Redistributions must retain the above copyright notice, this
#       list of conditions and the following disclaimer.
#    2. The name of the author may not be used to endorse or promote
#       products derived from this software without specific prior
#       written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
# IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
# DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
# GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
# IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
# OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


if(__configure_paths)
    return()
endif()

set(__configure_paths YES)

macro(_set_var _prefix _var _user _override _description)
    set(_name "${_prefix}_${_var}")

    set("${_name}" "${_user}")

    if("${_name}" STREQUAL "")
        set("${_name}" "${_override}")
    endif()

    set(${_name} "${${_name}}" CACHE PATH "${_description}")
    mark_as_advanced(${_name})
endmacro()

function(configure_paths _prefix)
    _set_var("${_prefix}" "PREFIX"          "${PREFIX}"         "${CMAKE_INSTALL_PREFIX}"               "install architecture-independent files in PREFIX")
    _set_var("${_prefix}" "EPREFIX"         "${EXEC_PREFIX}"    "${${_prefix}_PREFIX}"                  "install architecture-dependent files in EPREFIX")

    _set_var("${_prefix}" "SBINDIR"         "${SBINDIR}"        "${${_prefix}_EPREFIX}/sbin"            "system admin executabls")
    _set_var("${_prefix}" "SYSCONFDIR"      "${SYSCONFDIR}"     "${${_prefix}_PREFIX}/etc"              "read-only single-machine data")
    _set_var("${_prefix}" "LOCALSTATEDIR"   "${LOCALSTATEDIR}"  "${${_prefix}_PREFIX}/var"              "modifiable single-machine data")
    _set_var("${_prefix}" "BINDIR"          "${BINDIR}"         "${${_prefix}_EPREFIX}/bin"             "user executables")
    _set_var("${_prefix}" "LIBDIR"          "${LIBDIR}"         "${${_prefix}_EPREFIX}/lib"             "object code libraries")
    _set_var("${_prefix}" "INCLUDEDIR"      "${INCLUDEDIR}"     "${${_prefix}_PREFIX}/include"          "C header files")
    _set_var("${_prefix}" "PKGCONFIGDIR"    "${PKGCONFIGDIR}"   "${${_prefix}_LIBDIR}/pkgconfig"        "pkg-config files")
    _set_var("${_prefix}" "TYPELIBDIR"      "${TYPELIBDIR}"     "${${_prefix}_LIBDIR}/girepository-1.0" "GObject run-time introspection data")
    _set_var("${_prefix}" "DATAROOTDIR"     "${DATAROOTDIR}"    "${${_prefix}_PREFIX}/share"            "read-only arch.-independent data root")
    _set_var("${_prefix}" "DATADIR"         "${DATADIR}"        "${${_prefix}_DATAROOTDIR}"             "read-only architecture-independent data")
    _set_var("${_prefix}" "INFODIR"         "${INFODIR}"        "${${_prefix}_DATAROOTDIR}/info"        "info documentation")
    _set_var("${_prefix}" "MANDIR"          "${MANDIR}"         "${${_prefix}_DATAROOTDIR}/man"         "man documentation")
    _set_var("${_prefix}" "LOCALEDIR"       "${LOCALEDIR}"      "${${_prefix}_DATAROOTDIR}/locale"      "locale-dependent data")
    _set_var("${_prefix}" "GIRDIR"          "${GIRDIR}"         "${${_prefix}_DATAROOTDIR}/gir-1.0"     "GObject introspection data")
endfunction()

# vim: tw=0:
