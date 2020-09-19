dnl Process this file with autoconf to produce a configure script.

dnl Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>

dnl (see the files README and COPYING for more details)

AC_INIT([xoscope], [2.2])
AM_INIT_AUTOMAKE
AC_CONFIG_HEADERS(config.h)

AC_ISC_POSIX
AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC

PKG_CHECK_MODULES(GTK, gtk+-2.0 >= 2.2)
PKG_CHECK_MODULES(GTKDATABOX, gtkdatabox)
PKG_CHECK_MODULES(FFTW3, fftw3)

dnl Use -Wall if we have gcc.
changequote(,)dnl
if test "x$GCC" = "xyes"; then
  case " $CFLAGS " in
  *[\ \	]-Wall[\ \	]*) ;;
  *) CFLAGS="$CFLAGS -Wall" ;;
  esac
fi
changequote([,])dnl

dnl X11 checks
AC_PATH_XTRA
if test "-DX_DISPLAY_MISSING" = "$X_CFLAGS"; then
  AC_MSG_ERROR(can not find X11)
fi
AC_SUBST(X_CFLAGS)
AC_SUBST(X_PRE_LIBS)
AC_SUBST(X_EXTRA_LIBS)
AC_SUBST(X_LIBS)
AC_SUBST(x_libraries)

dnl Checks for programs.
AC_PATH_PROG(PERL, perl, /usr/bin/perl)
AC_DEFINE_UNQUOTED(PERL, "$ac_cv_path_PERL", [Path to Perl executable])

dnl Checks for libraries.
AC_CHECK_LIB(esd, esd_monitor_stream)
AC_CHECK_LIB(m, sin)

AC_ARG_WITH([comedi],
	[AS_HELP_STRING([--with-comedi],
	[support data acquisition hardwith with COMEDI library])],
	[], [with_comedi=check])
AS_IF([test "x$with_comedi" != xno],
	    [AC_CHECK_LIB(comedi, comedi_open,,
              [if test "x$with_comedi" != xcheck; then
                 AC_MSG_FAILURE(
                   [--with-comedi was given, but test for COMEDI failed])
               fi
              ])])
AS_IF([test "$ac_cv_lib_comedi_comedi_open" = "yes"],
	    [AC_MSG_CHECKING([how many arguments comedi_get_cmd_generic_timed takes])
	    AC_COMPILE_IFELSE([AC_LANG_SOURCE([
#include <comedilib.h>
main() {
       comedi_get_cmd_generic_timed(NULL, NULL, NULL, 0, 0);
}])], comedi_get_cmd_generic_timed=5args, comedi_get_cmd_generic_timed=4args)

        if test x"$comedi_get_cmd_generic_timed" = x5args; then
	  AC_DEFINE([COMEDI_GET_CMD_GENERIC_TIMED_TAKES_5_ARGS],1,[Descr])
	  AC_MSG_RESULT(5)
	else
	  AC_MSG_RESULT(4)
	fi
])

AC_CHECK_LIB(fftw3, fftw_execute)
AC_CHECK_LIB(asound, snd_pcm_hw_params)

dnl Check for optional features in gtkdatabox library
AC_CHECK_LIB(gtkdatabox, gtk_databox_grid_set_line_style, [AC_DEFINE([HAVE_GRID_LINESTYLE], [1],
                          [Define if gtkdatabox supports setting grid line styles.])])

dnl This, believe it or not, is the suggested syntax for checking the
dnl result of a previously run test - in our case, for setting an
dnl automake variable based on the result of an autoconf test.  See
dnl "Defining C Preprocessor Symbols" in the autoconf info docs.

AM_CONDITIONAL(COMEDI, test "$ac_cv_lib_comedi_comedi_open" = "yes")
AM_CONDITIONAL(ESD, test "$ac_cv_lib_esd_esd_monitor_stream" = "yes")
AM_CONDITIONAL(ASOUND, test "$ac_cv_lib_asound_snd_pcm_hw_params" = "yes")

AC_SUBST(XOSCOPE)

dnl Checks for header files.
AC_HEADER_DIRENT
AC_HEADER_STDC
AC_HEADER_SYS_WAIT
AC_CHECK_HEADERS(fcntl.h limits.h sys/ioctl.h sys/time.h termio.h unistd.h)

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

dnl Checks for library functions.
AC_PROG_GCC_TRADITIONAL
AC_HEADER_MAJOR
AC_TYPE_SIGNAL
AC_CHECK_FUNCS(getcwd putenv select strdup strstr strtol)

dnl Program defaults for the command-line options (original values)

AC_DEFINE(DEF_A, 1, [active channel])
AC_DEFINE(DEF_R, 44100, [sample rate in Hz])
AC_DEFINE(DEF_S, 10, [time scale in ms/div])
AC_DEFINE(DEF_T, "0:0:x", [trigger level])
AC_DEFINE(DEF_L, "1:1:0", [cursor lines])
AC_DEFINE(DEF_FX, "8x16", [X11 font])
AC_DEFINE(DEF_P, 2, [plot mode 2 (lines, sweep)])
AC_DEFINE(DEF_G, 2, [full graticle display])
AC_DEFINE(DEF_B, 0, [graticle in front of data])
AC_DEFINE(DEF_V, 0, [verbose display off])

AC_DEFINE(MAXWID, 1024 * 256, [maximum number of samples stored in memories])

AC_DEFINE(SAMPLESKIP, 32, [samples to discard after a reset])

AC_DEFINE(DISCARDBUF, 16384, [maximum samples to discard at each pass if we have too many])

AC_DEFINE(CHANNELS, 8, [max number of channels])

AC_DEFINE(MSECREFRESH, 30, [minimum number of milliseconds between refresh on libsx version])

AC_DEFINE(HELPCOMMAND, "man -Tutf8 xoscope 2>&1", [shell command for X11 help])

AC_DEFINE(FILENAME, "oscope.dat", [default file name])

AC_DEFINE(COMMAND, "operl '$x + $y'", [default external command pipe])

AC_DEFINE(FFT_DSP_LEN, 440, [output from fft is compressed (or streched) to that number of bands])

AC_DEFINE(DEFAULT_ALSADEVICE, ["default"], [default ALSA soundcard device])

dnl Set CALC_RMS to "1" if you deal mostly with sinusoidal signals.
dnl Might be confusing with audio signals.
AC_DEFINE(CALC_RMS, 0, [calculate RMS of captured signal])

dnl Set SC_16BIT to "1" if you want 16 bit resolution from sound card
AC_DEFINE(SC_16BIT, 0, [use 16 bit format for sound card])

AC_OUTPUT([
Makefile
xoscope.spec
])

AC_MSG_RESULT([
        ALSA module:                    ${ac_cv_lib_asound_snd_pcm_hw_params}
        ESD module:                     ${ac_cv_lib_esd_esd_monitor_stream}
        COMEDI module:                  ${ac_cv_lib_comedi_comedi_open}
])
