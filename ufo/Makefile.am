AM_CFLAGS = -I$(top_builddir)/common/autotools -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(JSON_GLIB_CFLAGS) $(ZMQ3_CFLAGS) $(OPENCL_CFLAGS)
AM_CPPFLAGS = \
	-DUFO_COMPILATION \
	-DUFO_PLUGIN_DIR=\"$(pkglibdir)\" \
	-DUFO_KERNEL_DIR=\"$(pkgdatadir)\"

# ufo library

ufo_sources = \
    compat.h \
    compat.c \
    ufo-arch-graph.c \
    ufo-buffer.c \
    ufo-configurable.c \
    ufo-config.c \
    ufo-cpu-node.c \
    ufo-cpu-task-iface.c \
    ufo-daemon.c \
    ufo-dummy-task.c \
    ufo-gpu-node.c \
    ufo-gpu-task-iface.c \
    ufo-graph.c \
    ufo-group.c \
    ufo-input-task.c \
    ufo-messenger-iface.c \
    ufo-node.c \
    ufo-output-task.c \
    ufo-plugin-manager.c \
    ufo-profiler.c \
    ufo-remote-node.c \
    ufo-remote-task.c \
    ufo-resources.c \
    ufo-scheduler.c \
    ufo-task-iface.c \
    ufo-task-graph.c \
    ufo-task-node.c \
    ufo-two-way-queue.c \
    ufo-basic-ops.c \
    ufo-zmq-messenger.c

ufo_headers = \
    ufo-arch-graph.h \
    ufo-buffer.h \
    ufo-configurable.h \
    ufo-config.h \
    ufo-cpu-node.h \
    ufo-cpu-task-iface.h \
    ufo-daemon.h \
    ufo-dummy-task.h \
    ufo-gpu-node.h \
    ufo-gpu-task-iface.h \
    ufo-graph.h \
    ufo-group.h \
    ufo-input-task.h \
    ufo-messenger-iface.h \
    ufo-node.h \
    ufo-output-task.h \
    ufo-plugin-manager.h \
    ufo-profiler.h \
    ufo-remote-node.h \
    ufo-remote-task.h \
    ufo-resources.h \
    ufo-scheduler.h \
    ufo-task-iface.h \
    ufo-task-graph.h \
    ufo-task-node.h \
    ufo-two-way-queue.h \
    ufo-basic-ops.h \
    ufo-zmq-messenger.h

if WITH_MPI
ufo_sources += ufo-mpi-messenger.c
ufo_headers += ufo-mpi-messenger.h
endif

lib_LTLIBRARIES = libufo.la
libufo_la_LDFLAGS = -no-undefined -version-info @version_info@ @release_info@
libufo_la_LIBADD = $(GLIB_LIBS) $(JSON_GLIB_LIBS) $(ZMQ3_LIBS) $(OPENCL_LIBS)
libufo_la_SOURCES = ufo-enums.h ufo-enums.c zmq-shim.h $(ufo_sources)

if HAVE_PYTHON
AM_CPPFLAGS += -DHAVE_PYTHON $(PYTHON_CFLAGS)
libufo_la_LIBADD += $(PYTHON_LIBS) 
endif


ufoincludedir = ${includedir}/ufo-@VMAJ@/ufo
ufoinclude_HEADERS = $(ufo_headers) ufo.h ufo-enums.h

dist_pkgdata_DATA = ufo-basic-ops.cl

ufo-enums.h: stamp-ufo-enums.h
	@true
stamp-ufo-enums.h: $(ufo_headers) $(srcdir)/ufo-enums.h.template
	( cd $(srcdir) && $(GLIB_MKENUMS) --template ufo-enums.h.template \
		$(ufo_headers) ) >> xgen-gtbh \
	&& (cmp -s xgen-gtbh ufo-enums.h || cp xgen-gtbh ufo-enums.h ) \
	&& rm -f xgen-gtbh \
	&& echo timestamp > $(@F)
ufo-enums.c: $(ufo_headers) $(srcdir)/ufo-enums.c.template ufo-enums.h
	( cd $(srcdir) && $(GLIB_MKENUMS) --template ufo-enums.c.template \
		$(ufo_headers) ) > xgen-gtbc \
	&& cp xgen-gtbc ufo-enums.c  \
	&& rm -f xgen-gtbc

EXTRA_DIST = ufo-enums.c.template ufo-enums.h.template

CLEANFILES = ufo-enums.c ufo-enums.h stamp-ufo-enums.h

# introspection

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = \
        --add-include-path=$(srcdir) \
        --add-include-path=. \
        --warn-all
INTROSPECTION_COMPILER_ARGS = \
        --includedir=$(srcdir) \
        --includedir=.

if HAVE_INTROSPECTION
Ufo-@VMAJ@.0.gir: $(INTROSPECTION_SCANNER) libufo.la Makefile

Ufo_@VMAJ@_0_gir_NAMESPACE = Ufo
Ufo_@VMAJ@_0_gir_VERSION = @VMAJ@.0
Ufo_@VMAJ@_0_gir_LIBS = libufo.la
Ufo_@VMAJ@_0_gir_FILES =  $(ufo_sources) $(ufo_headers)
Ufo_@VMAJ@_0_gir_INCLUDES = GLib-2.0 GModule-2.0 GObject-2.0
Ufo_@VMAJ@_0_gir_CFLAGS = $(INCLUDES) $(AM_CPPFLAGS) $(AM_CFLAGS)
Ufo_@VMAJ@_0_gir_EXPORT_PACKAGES = Ufo-@VMAJ@.0
Ufo_@VMAJ@_0_gir_SCANNERFLAGS = --warn-all --c-include='ufo.h'

INTROSPECTION_GIRS += Ufo-@VMAJ@.0.gir

girdir = $(datadir)/gir-1.0
gir_DATA = $(INTROSPECTION_GIRS)

typelibsdir = $(libdir)/girepository-1.0
typelibs_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(gir_DATA) $(typelibs_DATA)

endif

# Support for GNU Flymake, in Emacs.

check-syntax: AM_CFLAGS += -fsyntax-only -pipe
check-syntax:
	test -z "$(CHK_SOURCES)" || $(COMPILE) $(CHK_SOURCES)

# check for includes with iwyu
iwyu:
	@list='$(libufo_la_SOURCES)';\
	for p in $$list; do\
		iwyu $(AM_CFLAGS) $(AM_CPPFLAGS) $$p; \
	done;

.PHONY: check-syntax iwyu
