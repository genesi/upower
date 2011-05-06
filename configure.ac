dnl Process this file with autoconf to produce a configure script.

m4_define([up_major_version], [0])
m4_define([up_minor_version], [9])
m4_define([up_micro_version], [5])
m4_define([up_version],
          [up_major_version.up_minor_version.up_micro_version])

AC_PREREQ(2.63)
AC_INIT([upower],[up_version],[richard@hughsie.com],[upower])
AC_CONFIG_SRCDIR(src)
AM_INIT_AUTOMAKE([1.9 tar-ustar dist-bzip2])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

GOBJECT_INTROSPECTION_CHECK([0.6.7])

# enable nice build output on automake1.11
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

# use this in up-version.h
UP_MAJOR_VERSION=up_major_version
UP_MINOR_VERSION=up_minor_version
UP_MICRO_VERSION=up_micro_version
AC_SUBST(VERSION)
AC_SUBST(UP_MAJOR_VERSION)
AC_SUBST(UP_MINOR_VERSION)
AC_SUBST(UP_MICRO_VERSION)

# libtool versioning - this applies to libupower-glib
#
# See http://sources.redhat.com/autobook/autobook/autobook_91.html#SEC91 for details
#
# increment;
# CURRENT	If the API or ABI interface has changed (reset REVISION to 0)
# REVISION	If the API and ABI remains the same, but bugs are fixed.
# AGE		Don't use.
LT_CURRENT=1
LT_REVISION=1
LT_AGE=0
AC_SUBST(LT_CURRENT)
AC_SUBST(LT_REVISION)
AC_SUBST(LT_AGE)

AC_SEARCH_LIBS([strerror],[cposix])
AC_PROG_CC

AC_HEADER_STDC
LT_INIT
AC_PROG_MAKE_SET
AC_PROG_LN_S
AC_SYS_LARGEFILE
AM_PROG_CC_C_O

# Taken from dbus
AC_ARG_ENABLE(ansi,             [  --enable-ansi           enable -ansi -pedantic gcc flags],enable_ansi=$enableval,enable_ansi=no)
AC_ARG_ENABLE(verbose-mode,     [  --enable-verbose-mode   support verbose debug mode],enable_verbose_mode=$enableval,enable_verbose_mode=$USE_MAINTAINER_MODE)
AC_ARG_ENABLE(man-pages,        [  --enable-man-pages      build manual pages],enable_man_pages=$enableval,enable_man_pages=yes)

if test "${enable_man_page}" != no; then
dnl
dnl Check for xsltproc
dnl
AC_PATH_PROG([XSLTPROC], [xsltproc])
  if test -z "$XSLTPROC"; then
    enable_man_pages=no
  fi
fi
AM_CONDITIONAL(MAN_PAGES_ENABLED, test x$enable_man_pages = xyes)

GTK_DOC_CHECK([1.9])

# udev rules go in /lib, not /usr/lib
if test "$prefix" = "/usr" -o "$prefix" = "/usr/local" ; then
  slashlibdir=/lib
else
  slashlibdir=$prefix/lib
fi
AC_SUBST(slashlibdir)

#### gcc warning flags

dnl ---------------------------------------------------------------------------
dnl - Extra verbose warning switches
dnl ---------------------------------------------------------------------------
CPPFLAGS="$CPPFLAGS -DG_DISABLE_SINGLE_INCLUDES -DGTK_DISABLE_SINGLE_INCLUDES"
CPPFLAGS="$CPPFLAGS -DG_DISABLE_DEPRECATED -DGTK_DISABLE_DEPRECATED -DGDK_DISABLE_DEPRECATED"
CPPFLAGS="$CPPFLAGS -DGDK_MULTIHEAD_SAFE -DGTK_MULTIHEAD_SAFE"
CPPFLAGS="$CPPFLAGS -DGSEAL_ENABLE"

dnl ---------------------------------------------------------------------------
dnl - Use strict options (default enabled for devs, disabled in releases)
dnl ---------------------------------------------------------------------------
dnl if .git directory is present, considering we are working in the repository
if test -d ".git"; then
	default_strict=yes
else
	default_strict=no
fi

AC_ARG_ENABLE(strict, AS_HELP_STRING([--enable-strict],[Enable strict compilation options]), enable_strict=$enableval,
		enable_strict=$default_strict)
if test x$enable_strict != xno; then
	if test "$GCC" = "yes"; then
		WARNINGFLAGS_C="$WARNINGFLAGS_C -Werror"
	fi
fi

if test "$GCC" = "yes"; then
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wall"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align -Wno-uninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-declarations"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wredundant-decls"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wpointer-arith"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wcast-align"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wwrite-strings"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winit-self"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wreturn-type"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-nonliteral"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wformat-security"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-include-dirs"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-format-attribute"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wclobbered"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wempty-body"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wignored-qualifiers"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wsign-compare"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wtype-limits"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wuninitialized"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Waggregate-return"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wdeclaration-after-statement"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wshadow"
	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wno-strict-aliasing"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Winline"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Wmissing-parameter-type"
#	WARNINGFLAGS_C="$WARNINGFLAGS_C -Woverride-init"
else
	WARNINGFLAGS_C=""
fi
AC_SUBST(WARNINGFLAGS_C)

PKG_CHECK_MODULES(GLIB, [glib-2.0 >= 2.21.5])
AC_SUBST(GLIB_CFLAGS)
AC_SUBST(GLIB_LIBS)

PKG_CHECK_MODULES(DBUS, [dbus-1 >= 1.0])
AC_SUBST(DBUS_CFLAGS)
AC_SUBST(DBUS_LIBS)

PKG_CHECK_MODULES(DBUS_GLIB, [dbus-glib-1 >= 0.76])
AC_SUBST(DBUS_GLIB_CFLAGS)
AC_SUBST(DBUS_GLIB_LIBS)

PKG_CHECK_MODULES(POLKIT, [polkit-gobject-1 >= 0.91])
AC_SUBST(POLKIT_CFLAGS)
AC_SUBST(POLKIT_LIBS)

PKG_CHECK_MODULES(GIO, [gio-2.0 >= 2.16.1])
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

dnl ---------------------------------------------------------------------------
dnl - Compile time default choice of backend
dnl ---------------------------------------------------------------------------
AC_ARG_WITH([backend],
	    AS_HELP_STRING([--with-backend=<option>],
			   [Default backend to use linux, freebsd, dummy (dummy)]))
# default to a sane option
if test x$with_backend = x; then
	if test -e /usr/include/gudev-1.0/gudev/gudev.h ; then
		with_backend=linux
	elif test -e /usr/include/dev/acpica/acpiio.h ; then
	    	with_backend=freebsd
	else
		with_backend=dummy
	fi
fi
AC_DEFINE_UNQUOTED(BACKEND, "$with_backend", [backend])
AC_SUBST(BACKEND, "$with_backend")

# only need GUdev on linux
with_idevice=no
if test x$with_backend = xlinux; then
	PKG_CHECK_MODULES(GUDEV, [gudev-1.0 >= 001])
	PKG_CHECK_MODULES(USB, [libusb-1.0 >= 1.0.0])
	PKG_CHECK_MODULES(IDEVICE, libimobiledevice-1.0 >= 0.9.7, with_idevice=yes, with_idevice=no)
	if test x$with_idevice = xyes; then
		AC_DEFINE(HAVE_IDEVICE, 1, [Define to 1 if AFC is going to be built])
	fi
fi

AM_CONDITIONAL(HAVE_IDEVICE, [test x$with_idevice = xyes])

# export to Makefile.am
AM_CONDITIONAL(BACKEND_TYPE_DUMMY, [test x$with_backend = xdummy])
AM_CONDITIONAL(BACKEND_TYPE_LINUX, [test x$with_backend = xlinux])
AM_CONDITIONAL(BACKEND_TYPE_FREEBSD, [test x$with_backend = xfreebsd])

dnl ---------------------------------------------------------------------------
dnl - Build self tests
dnl ---------------------------------------------------------------------------
AC_ARG_ENABLE(tests, AS_HELP_STRING([--enable-tests],[enable unit test code]),
	      enable_tests=$enableval,enable_tests=yes)
AM_CONDITIONAL(UP_BUILD_TESTS, test x$enable_tests = xyes)

if test "x$GCC" = "xyes"; then
  LDFLAGS="-Wl,--as-needed $LDFLAGS"
fi

IT_PROG_INTLTOOL([0.40.0])
GETTEXT_PACKAGE=upower
AC_SUBST([GETTEXT_PACKAGE])
AM_GLIB_GNU_GETTEXT
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],["$GETTEXT_PACKAGE"],[gettext domain])

AC_CONFIG_FILES([
Makefile
devkit-power-gobject.pc
upower-glib.pc
etc/Makefile
src/Makefile
src/dummy/Makefile
src/freebsd/Makefile
src/linux/Makefile
tools/Makefile
doc/Makefile
doc/version.xml
doc/man/Makefile
doc/dbus/Makefile
policy/Makefile
rules/Makefile
devkit-power-gobject/Makefile
devkit-power-gobject/dkp-version.h
libupower-glib/Makefile
libupower-glib/up-version.h
po/Makefile.in
])
AC_OUTPUT

dnl ==========================================================================
echo "
                 UPower $VERSION
             =========================

        prefix:                     ${prefix}
        libdir:                     ${libdir}
        libexecdir:                 ${libexecdir}
        bindir:                     ${bindir}
        sbindir:                    ${sbindir}
        datadir:                    ${datadir}
        sysconfdir:                 ${sysconfdir}
        localstatedir:              ${localstatedir}
        docdir:                     ${docdir}

        compiler:                   ${CC}
        cflags:                     ${CFLAGS}
        cppflags:                   ${CPPFLAGS}
        xsltproc:                   ${XSLTPROC}

        Backend:                    ${with_backend}
        libimobiledevice support:   ${with_idevice}
        Maintainer mode:            ${USE_MAINTAINER_MODE}
        Building api docs:          ${enable_gtk_doc}
        Building man pages:         ${enable_man_pages}
        Building unit tests:        ${enable_tests}
"
