AM_CFLAGS = -I$(top_builddir)/common/autotools -I$(top_srcdir) \
	$(GLIB_CFLAGS) $(JSON_GLIB_CFLAGS) $(ZMQ3_CFLAGS) $(OPENCL_CFLAGS)
AM_CPPFLAGS = -DUFO_COMPILATION -DUFO_PLUGIN_DIR=\"$(pkglibdir)\"
LDADD = $(top_builddir)/ufo/libufo.la \
	$(GLIB_LIBS) $(JSON_GLIB_LIBS) $(ZMQ3_LIBS) $(OPENCL_LIBS)

check_PROGRAMS = test-suite
test_suite_SOURCES = \
    test-suite.c \
    test-suite.h \
    test-buffer.c \
    test-config.c \
    test-graph.c \
    test-profiler.c \
    test-remote-node.c \
    test-mpi-remote-node.c \
    test-zmq-messenger.c

check-local: $(check_PROGRAMS)
	./test-suite -p /no-opencl

# Support for GNU Flymake, in Emacs.

check-syntax: AM_CFLAGS += -fsyntax-only -pipe
check-syntax:
	test -z "$(CHK_SOURCES)" || $(COMPILE) $(CHK_SOURCES)

.PHONY: check-syntax
