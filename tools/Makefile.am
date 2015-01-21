AM_CFLAGS = -I$(top_builddir)/common/autotools -I$(top_srcdir)\
	$(GLIB_CFLAGS) $(JSON_GLIB_CFLAGS) $(OPENCL_CFLAGS)
LDADD = $(top_builddir)/ufo/libufo.la \
	$(GLIB_LIBS) $(JSON_GLIB_LIBS) $(ZMQ3_LIBS) $(OPENCL_LIBS)

bin_PROGRAMS=ufo-launch ufo-runjson ufod

ufo_launch_SOURCES = ufo-launch.c

ufo_runjson_SOURCES = ufo-runjson.c

ufotemplatesdir=$(pkgdatadir)/templates

dist_ufotemplates_DATA=\
	templates/ufo-task.c.in \
	templates/ufo-task.h.in

nodist_bin_SCRIPTS = ufo-mkfilter
ufo-mkfilter: ufo-mkfilter.in
	sed -e 's![@]UFO_FILTER_TEMPLATE_DIR[@]!$(ufotemplatesdir)!g' $(srcdir)/ufo-mkfilter.in > $@

EXTRA_DIST = ufo-mkfilter.in

CLEANFILES = ufo-mkfilter

# Support for GNU Flymake, in Emacs.

check-syntax: AM_CFLAGS += -fsyntax-only -pipe
check-syntax:
	test -z "$(CHK_SOURCES)" || $(COMPILE) $(CHK_SOURCES)
