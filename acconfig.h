/*
 * @(#)$Id: acconfig.h,v 2.3 2009/01/14 18:21:06 baccala Exp $
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file simply sets the program's compile-time options.
 * Original shipped values are in (parentheses).
 *
 */

				/* @TOP@ */
/* I think these came from glade, not used much yet */
#undef ENABLE_NLS
#undef HAVE_CATGETS
#undef HAVE_GETTEXT
#undef HAVE_LC_MESSAGES
#undef HAVE_STPCPY
#undef HAVE_LIBSM
#undef PACKAGE_LOCALE_DIR
/* for glade pixmaps, if any */
#undef PACKAGE_DATA_DIR
#undef PACKAGE_SOURCE_DIR
/* for xoscope external math commands */
#undef PACKAGE_LIBEXEC_DIR
				/* @BOTTOM@ */

/* program defaults for the command-line options (original values) */
#define DEF_A	1		/* 1-8 (1) */
#define DEF_R	44100		/* 8000,11025,22050,44100 (44100) */
#define DEF_S	10		/* 1,2,5,10,20,50,100,200,500,1000 (10) */
#define DEF_T	"0:0:x"		/* -128-127:0,1,2:x,y ("0:0:x") */
#define DEF_L	"1:1:0"		/* 1-MAXWID:1-MAXWID:0,1 ("1:1:0") */
#define DEF_D	4		/* 1,2,4 (4) */
#define DEF_F	""		/* console font, "" = default ("") */
#define DEF_FX	"8x16"		/* X11 font ("8x16") */
#define DEF_P	2		/* 0,1,2 (2) */
#define DEF_G	2		/* 0,1,2 (2) */
#define DEF_B	0		/* 0,1 (0) */
#define DEF_V	0		/* 0,1 (0) */
#define DEF_X	0		/* 0,1 (0) don't use sound card? */
#define DEF_Z	0		/* 0,1 (0) don't search for Serial Scope? */

/* maximum number of samples stored in memories (1024 * 256) */
#define MAXWID		1024 * 256

/* The first few samples after a reset seem invalid.  If you see
   strange "glitches" at the left edge of the screen, increase this (32) */
#define SAMPLESKIP	32

/* maximum samples to discard at each pass if we have too many (16384) */
#define DISCARDBUF 16384

/* max number of channels, up to 8 (8) */
#define CHANNELS	8

/* minimum number of milliseconds between refresh on libsx version (30) */
#define MSECREFRESH	30
/* a lower number here can increase refresh rate but at the expense of
   interactive response time as the X server becomes too busy */

/* bourne shell command for X11 Help ("man -Tutf8 xoscope 2>&1") */
#define HELPCOMMAND	"man -Tutf8 xoscope 2>&1"

/* default file name ("oscope.dat") */
#define FILENAME	"oscope.dat"

/* default external command pipe to run ("operl '$x + $y'") */
#define COMMAND		"operl '$x + $y'"

/* FFT length, shorter than minimum screen width and multiple of 2 (512) */
#define FFTLEN	512

/* preferred path to probescope serial device ("/dev/probescope") */
#define PROBESCOPE "/dev/probescope"

/* preferred path to bitscope serial device ("/dev/bitscope") */
#define BITSCOPE "/dev/bitscope"
