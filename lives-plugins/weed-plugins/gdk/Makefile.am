## Process this with automake to create Makefile.in

LIBS =

fxpluginslibdir = $(libdir)/$(PACKAGE)/plugins/effects/realtime/weed

if IS_DARWIN
shared_ldflags = -dynamiclib -module -avoid-version --tag=disable-static
shared_cflags = -fno-common
else
shared_ldflags = -shared -fPIC -DPIC -Wl,-Bsymbolic -module -avoid-version --tag=disable-static
shared_cflags = -fPIC -DPIC
endif

if HAVE_SYSTEM_WEED
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ @WEED_UTILS_LIBS@ @WEED_PLUGIN_UTILS_LIBS@
weed_CFLAGS = -DHAVE_SYSTEM_WEED
else
AM_LDFLAGS = $(shared_ldflags) @GTK_LIBS@ ../../../libweed/libweed-utils.la ../../../libweed/libweed-plugin-utils.la
endif

AM_CFLAGS = $(shared_cflags) $(weed_CFLAGS) @GTK_CFLAGS@ -ffast-math

if ENABLE_VJACK
vjack_libs = vjack_rcv.la
vjack_rcv_la_LDFLAGS = -ljack $(AM_LDFLAGS)
endif

fxpluginslib_LTLIBRARIES = gdk_fast_resize.la compositor.la videowall.la scribbler.la puretext.la $(vjack_libs) 

EXTRA_DIST=data icons

install-data-local:
	mkdir -p $(DESTDIR)$(fxpluginslibdir)
	cp -rf data/ $(DESTDIR)$(fxpluginslibdir)
	cp -rf icons/ $(DESTDIR)$(fxpluginslibdir)

install-exec-local:
	rm -rf $(DESTDIR)$(fxpluginslibdir)/*