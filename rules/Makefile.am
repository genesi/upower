## Process this file with automake to produce Makefile.in

udevrulesdir = $(slashlibdir)/udev/rules.d
udevrules_DATA =						\
	95-upower-battery-recall-dell.rules			\
	95-upower-battery-recall-fujitsu.rules			\
	95-upower-battery-recall-gateway.rules			\
	95-upower-battery-recall-ibm.rules			\
	95-upower-battery-recall-lenovo.rules			\
	95-upower-battery-recall-toshiba.rules			\
	95-upower-wup.rules					\
	95-upower-hid.rules					\
	95-upower-csr.rules

EXTRA_DIST =							\
	$(udevrules_DATA)

clean-local :
	rm -f *~

