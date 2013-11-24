AM_CFLAGS = -I$(top_builddir)/common/autotools -I$(top_srcdir)\
	$(GLIB_CFLAGS) $(OPENCL_CFLAGS)
LDADD = $(top_builddir)/ufo/libufo.la \
	$(GLIB_LIBS) $(JSON_GLIB_LIBS) $(ZMQ3_LIBS) $(OPENCL_LIBS)

bin_PROGRAMS=ufo-runjson ufod

ufo_runjson_SOURCES = runjson.c

dist_bin_SCRIPTS = ufo-mkfilter.py

# Support for GNU Flymake, in Emacs.

check-syntax: AM_CFLAGS += -fsyntax-only -pipe
check-syntax:
	test -z "$(CHK_SOURCES)" || $(COMPILE) $(CHK_SOURCES)
