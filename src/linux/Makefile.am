## Process this file with automake to produce Makefile.in

INCLUDES = \
	-I$(top_builddir)/src -I$(top_srcdir)/src		\
	-DUP_COMPILATION					\
	-DG_UDEV_API_IS_SUBJECT_TO_CHANGE			\
	-I$(top_srcdir)/libupower-glib				\
	$(USB_CFLAGS)						\
	$(GIO_CFLAGS)						\
	$(DBUS_GLIB_CFLAGS)					\
	$(GUDEV_CFLAGS)						\
	$(POLKIT_CFLAGS)					\
	$(GLIB_CFLAGS)						\
	$(IDEVICE_CFLAGS)

if BACKEND_TYPE_LINUX
noinst_LTLIBRARIES = libupshared.la
endif

if HAVE_IDEVICE
idevice_files = up-device-idevice.c up-device-idevice.h
else
idevice_files =
endif

libupshared_la_SOURCES =					\
	up-device-supply.c					\
	up-device-supply.h					\
	up-device-csr.c						\
	up-device-csr.h						\
	up-device-hid.c						\
	up-device-hid.h						\
	up-device-wup.c						\
	up-device-wup.h						\
	up-input.c						\
	up-input.h						\
	up-backend.c						\
	up-native.c						\
	sysfs-utils.c						\
	sysfs-utils.h						\
	$(idevice_files)					\
	$(BUILT_SOURCES)

EXTRA_DIST = up-device-idevice.c up-device-idevice.h

libupshared_la_CFLAGS =					\
	$(WARNINGFLAGS_C)

clean-local :
	rm -f *~

