## Process this file with automake to produce Makefile.in

# @(#)$Id: Makefile.am,v 2.14 2009/06/26 19:00:03 baccala Exp $

# Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>

# (see the files README and COPYING for more details)

# Look at ./configure --help before tweaking this.

man_MANS = xoscope.1

noinst_HEADERS = com_gtk.h display.h file.h oscope.h bitscope.h	\
config.h fft.h func.h proscope.h xoscope.rc.h

bin_PROGRAMS = xoscope

# Used to install these auxiliary programs, but I want to depreciate
# them.  Figure how to wrap their functionality into main program
# instead.

# pkglibexecdir = $(libexecdir)/@PACKAGE@
# pkglibexec_PROGRAMS = offt
# pkglibexec_SCRIPTS = operl ofreq.ini

EXTRA_DIST = $(man_MANS) $(noinst_HEADERS) \
README.f51 TODO TODO.old audio.dat bitscope.dat 	\
count.dat ofreq.ini oscope.dat oscope.lsm xoscope.rc	\
patch.f51 proscope.dat xoscope.glade xoscope.gladep \
hardware/HARDWARE hardware/buff.fig hardware/buff.ps \
hardware/buff2.fig hardware/buff2.ps hardware/pcb.fig hardware/pcb.ps \
hardware/xoscope-components.png hardware/xoscope-copper.png

src = oscope.c file.c func.c fft.c realfft.c display.c proscope.c	\
bitscope.c sc_linux.c ser_unix.c

if COMEDI
comedisrc = comedi.c
else
comedisrc =
endif

GTKDATABOX_CFLAGS = -I $(top_srcdir)/gtkdatabox-0.6.0.0/gtk
GTKDATABOX_LIBS = $(top_builddir)/gtkdatabox-0.6.0.0/gtk/libgtkdatabox.a

# Some bit of explaination is needed here for GUI_LIB.  automake won't
# let us include source files in a subdirectory on a _SOURCES line,
# and we do want all the auto-magically generated glade stuff confined
# to a subdirectory, so we're reduced to using the _LDADD line.
# However, we don't want the files included as a library, even though
# they get built as one, because then the weak attribute override
# trick used in things like comedi.c (see comments there) won't work
# for a library.  So we put .o files in.

SUBDIRS = gtkdatabox-0.6.0.0/gtk
INCLUDES = @GTK_CFLAGS@ $(GTKDATABOX_CFLAGS)
GUI_LIB = callbacks.c callbacks.h interface.c interface.h support.c support.h

xoscope_SOURCES = $(src) $(comedisrc) $(GUI_LIB) com_gtk.c gr_gtk.c sc_linux_gtk.c
xoscope_LDADD = @GTK_LIBS@ $(GTKDATABOX_LIBS)

# offt_SOURCES = offt.c fft.c realfft.c

# xy_SOURCES = com_gtk.c xy_gtk.c xy.c
# xy_LDADD = @GTK_LIBS@

# I compile in the rc file so I don't have to worry about what happens
# if it can't be found at runtime.

xoscope.rc.h: $(top_srcdir)/xoscope.rc
	echo "char * xoscope_rc[] = {" > $@
	sed -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/",/' $< >> $@
	echo "NULL };" >> $@

install-data-local:
	@$(NORMAL_INSTALL)
	if test -d $(srcdir)/pixmaps; then \
	  $(mkinstalldirs) $(DESTDIR)$(pkgdatadir)/pixmaps; \
	  for pixmap in $(srcdir)/pixmaps/*; do \
	    if test -f $$pixmap; then \
	      $(INSTALL_DATA) $$pixmap $(DESTDIR)$(pkgdatadir)/pixmaps; \
	    fi \
	  done \
	fi

dist-hook:
	if test -d pixmaps; then \
	  mkdir $(distdir)/pixmaps; \
	  for pixmap in pixmaps/*; do \
	    if test -f $$pixmap; then \
	      cp -p $$pixmap $(distdir)/pixmaps; \
	    fi \
	  done \
	fi
