/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.in by autoheader.  */
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

/* I think these came from glade, not used much yet */
/* #undef ENABLE_NLS */
/* #undef HAVE_CATGETS */
/* #undef HAVE_GETTEXT */
/* #undef HAVE_LC_MESSAGES */
/* #undef HAVE_STPCPY */
/* #undef HAVE_LIBSM */
/* #undef PACKAGE_LOCALE_DIR */
/* for glade pixmaps, if any */
#define PACKAGE_DATA_DIR "${datarootdir}/xoscope"
#define PACKAGE_SOURCE_DIR "/home/baccala/src/xoscope-2-cvs"
/* for xoscope external math commands */
#define PACKAGE_LIBEXEC_DIR "NONE/libexec/xoscope"

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `comedi' library (-lcomedi). */
#define HAVE_LIBCOMEDI 1

/* Define to 1 if you have the `esd' library (-lesd). */
/* #undef HAVE_LIBESD */

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the `putenv' function. */
#define HAVE_PUTENV 1

/* Define to 1 if you have the `select' function. */
#define HAVE_SELECT 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <termio.h> header file. */
#define HAVE_TERMIO_H 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* Name of package */
#define PACKAGE "xoscope"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME ""

/* Define to the full name and version of this package. */
#define PACKAGE_STRING ""

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME ""

/* Define to the version of this package. */
#define PACKAGE_VERSION ""

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#define VERSION "2.0"

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

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
