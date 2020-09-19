/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* calculate RMS of captured signal */
#define CALC_RMS 0

/* max number of channels */
#define CHANNELS 8

/* Descr */
#define COMEDI_GET_CMD_GENERIC_TIMED_TAKES_5_ARGS 1

/* default external command pipe */
#define COMMAND "operl '$x + $y'"

/* default ALSA soundcard device */
#define DEFAULT_ALSADEVICE "default"

/* active channel */
#define DEF_A 1

/* graticle in front of data */
#define DEF_B 0

/* X11 font */
#define DEF_FX "8x16"

/* full graticle display */
#define DEF_G 2

/* cursor lines */
#define DEF_L "1:1:0"

/* plot mode 2 (lines, sweep) */
#define DEF_P 2

/* sample rate in Hz */
#define DEF_R 44100

/* time scale in ms/div */
#define DEF_S 10

/* trigger level */
#define DEF_T "0:0:x"

/* verbose display off */
#define DEF_V 0

/* maximum samples to discard at each pass if we have too many */
#define DISCARDBUF 16384

/* output from fft is compressed (or streched) to that number of bands */
#define FFT_DSP_LEN 440

/* default file name */
#define FILENAME "oscope.dat"

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getcwd' function. */
#define HAVE_GETCWD 1

/* Define if gtkdatabox supports setting grid line styles. */
/* #undef HAVE_GRID_LINESTYLE */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the `asound' library (-lasound). */
#define HAVE_LIBASOUND 1

/* Define to 1 if you have the `comedi' library (-lcomedi). */
#define HAVE_LIBCOMEDI 1

/* Define to 1 if you have the `esd' library (-lesd). */
#define HAVE_LIBESD 1

/* Define to 1 if you have the `fftw3' library (-lfftw3). */
#define HAVE_LIBFFTW3 1

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

/* shell command for X11 help */
#define HELPCOMMAND "man -Tutf8 xoscope 2>&1"

/* Define to 1 if `major', `minor', and `makedev' are declared in <mkdev.h>.
   */
/* #undef MAJOR_IN_MKDEV */

/* Define to 1 if `major', `minor', and `makedev' are declared in
   <sysmacros.h>. */
/* #undef MAJOR_IN_SYSMACROS */

/* maximum number of samples stored in memories */
#define MAXWID 1024 * 256

/* minimum number of milliseconds between refresh on libsx version */
#define MSECREFRESH 30

/* Name of package */
#define PACKAGE "xoscope"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "xoscope"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "xoscope 2.2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "xoscope"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.2"

/* Path to Perl executable */
#define PERL "/usr/bin/perl"

/* Define as the return type of signal handlers (`int' or `void'). */
#define RETSIGTYPE void

/* samples to discard after a reset */
#define SAMPLESKIP 32

/* use 16 bit format for sound card */
#define SC_16BIT 0

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
#define TIME_WITH_SYS_TIME 1

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* Version number of package */
#define VERSION "2.2"

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */
