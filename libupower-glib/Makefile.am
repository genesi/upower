INCLUDES = \
	$(GLIB_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	-I$(top_srcdir)						\
	-I$(top_srcdir)/libupower-glib				\
	-DUP_COMPILATION					\
	-DG_LOG_DOMAIN=\"libupower-glib\"			\
	-DPACKAGE_DATA_DIR=\""$(datadir)"\"			\
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\"

-include $(INTROSPECTION_MAKEFILE)
INTROSPECTION_GIRS =
INTROSPECTION_SCANNER_ARGS = --add-include-path=$(srcdir)
INTROSPECTION_COMPILER_ARGS = --includedir=$(srcdir)

lib_LTLIBRARIES =						\
	libupower-glib.la

libupower_glib_includedir = $(includedir)/libupower-glib
libupower_glib_include_HEADERS =				\
	upower.h						\
	up-version.h						\
	up-types.h						\
	up-device.h						\
	up-qos-item.h						\
	up-wakeup-item.h					\
	up-stats-item.h						\
	up-history-item.h					\
	up-wakeups.h						\
	up-client.h

libupower_glib_la_SOURCES =					\
	up-types.c						\
	up-client.c						\
	up-wakeups.c						\
	up-qos-item.c						\
	up-wakeup-item.c					\
	up-stats-item.c						\
	up-history-item.c					\
	up-device.c

libupower_glib_la_LIBADD =					\
	$(INTLLIBS)						\
	$(GLIB_LIBS)						\
	$(DBUS_GLIB_LIBS)

libupower_glib_la_LDFLAGS =					\
	-version-info $(LT_CURRENT):$(LT_REVISION):$(LT_AGE)	\
	-export-dynamic						\
	-no-undefined						\
	-export-symbols-regex '^up_.*'

libupower_glib_la_CFLAGS =					\
	$(WARNINGFLAGS_C)					\
	$(NULL)

EXTRA_DIST =							\
	up-version.h.in

CLEANFILES = $(BUILT_SOURCES)

if HAVE_INTROSPECTION
introspection_sources = $(libupower_glib_la_SOURCES)

UPowerGlib-1.0.gir: libupower-glib.la
UPowerGlib_1_0_gir_INCLUDES = GObject-2.0 Gio-2.0
UPowerGlib_1_0_gir_CFLAGS = $(INCLUDES)
UPowerGlib_1_0_gir_LIBS = libupower-glib.la
UPowerGlib_1_0_gir_FILES = $(addprefix $(srcdir)/,$(introspection_sources))
INTROSPECTION_GIRS += UPowerGlib-1.0.gir

girdir = $(datadir)/gir-1.0
dist_gir_DATA = $(INTROSPECTION_GIRS)

typelibdir = $(libdir)/girepository-1.0
typelib_DATA = $(INTROSPECTION_GIRS:.gir=.typelib)

CLEANFILES += $(dist_gir_DATA) $(typelib_DATA)
endif

clean-local:
	rm -f *~

