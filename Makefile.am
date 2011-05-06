## Process this file with automake to produce Makefile.in

ACLOCAL_AMFLAGS = -I m4

SUBDIRS = etc devkit-power-gobject libupower-glib src doc tools policy po

if BACKEND_TYPE_LINUX
SUBDIRS += rules
endif

# Creating ChangeLog from git log (taken from cairo/Makefile.am):
ChangeLog: $(srcdir)/ChangeLog

$(srcdir)/ChangeLog:
	@if test -d "$(srcdir)/.git"; then \
	  (cd "$(srcdir)" && \
	  ./missing --run git-log --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi

EXTRA_DIST = 			\
	HACKING 		\
	ChangeLog

snapshot:
	$(MAKE) dist distdir=$(PACKAGE)-$(VERSION)-`date +"%Y%m%d"`

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = devkit-power-gobject.pc upower-glib.pc

# xsltproc barfs on 'make distcheck'; disable for now
DISTCHECK_CONFIGURE_FLAGS=--disable-man-pages --enable-gtk-doc --enable-introspection

clean-local :
	rm -f *~
