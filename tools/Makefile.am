## Process this file with automake to produce Makefile.in

INCLUDES = \
	-DUP_COMPILATION					\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libupower-glib				\
	$(DBUS_GLIB_CFLAGS)					\
	$(POLKIT_DBUS_CFLAGS)					\
	$(GLIB_CFLAGS)

UPOWER_LIBS = $(top_builddir)/libupower-glib/libupower-glib.la

bin_PROGRAMS = upower

upower_SOURCES = 					\
	egg-debug.c					\
	egg-debug.h					\
	up-tool.c					\
	$(BUILT_SOURCES)

upower_CPPFLAGS = 					\
	$(AM_CPPFLAGS)

upower_LDADD = 						\
	$(DBUS_GLIB_LIBS)				\
	$(UPOWER_LIBS)					\
	$(POLKIT_DBUS_LIBS)

install-exec-hook:
	cd $(DESTDIR)$(bindir) && $(LN_S) -f upower devkit-power

CLEANFILES = $(BUILT_SOURCES)

clean-local :
	rm -f *~ $(service_DATA) $(dbusconf_DATA)
