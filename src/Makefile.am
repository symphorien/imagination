## Process this file with automake to produce Makefile.in

AM_CPPFLAGS = \
	$(DEVELOPMENT_CFLAGS) \
	-DPACKAGE_LIB_DIR=\""$(libdir)"\" \
	-DPACKAGE_DATA_DIR=\""$(datadir)"\" \
	-DPACKAGE_LOCALE_DIR=\""$(prefix)/$(DATADIRNAME)/locale"\" \
	@PACKAGE_CFLAGS@ \
	@LIBAVCODEC_CFLAGS@ \
	@LIBAVUTIL_CFLAGS@
	@LIBAVFORMAT_CFLAGS@

bin_PROGRAMS = imagination

imagination_SOURCES = \
	imgcellrendereranim.c imgcellrendereranim.h \
	imagination.c imagination.h \
	support.c support.h \
	main-window.c main-window.h \
	callbacks.c callbacks.h \
	new_slideshow.c new_slideshow.h \
	slideshow_project.c slideshow_project.h \
	audio.c audio.h \
	export.c export.h \
	subtitles.c subtitles.h \
	imgcellrendererpixbuf.c imgcellrendererpixbuf.h

imagination_CFLAGS = \
	-DDATADIR=\"$(datadir)\"

imagination_LDADD = @PACKAGE_LIBS@ @LIBAVCODEC_LIBS@ @LIBAVFORMAT_LIBS@ @LIBAVUTIL_LIBS@ $(INTLLIBS) -lgmodule-2.0 -lm
