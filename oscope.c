/*
 * @(#)$Id: oscope.c,v 2.7 2009/01/15 03:57:20 baccala Exp $
 *
 * Copyright (C) 1996 - 2001 Tim Witham <twitham@quiknet.com>
 *
 * (see the files README and COPYING for more details)
 *
 * This file implements the basic oscilloscope internals
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include "oscope.h"		/* program defaults */
#include "display.h"		/* display routines */
#include "func.h"		/* signal math functions */
#include "file.h"		/* file I/O functions */

/* global program structures */
Scope scope;

extern DataSrc datasrc_sc, datasrc_ps, datasrc_bs;
#ifdef HAVE_LIBESD
extern DataSrc datasrc_esd;
#endif
#ifdef HAVE_LIBCOMEDI
extern DataSrc datasrc_comedi;
#endif

DataSrc *datasrcs[] = {
#ifdef HAVE_LIBCOMEDI
  &datasrc_comedi,
#endif
#ifdef HAVE_LIBESD
  &datasrc_esd,
#endif
  &datasrc_sc,
  &datasrc_ps,
  &datasrc_bs
};

int ndatasrcs = sizeof(datasrcs)/sizeof(DataSrc *);
int datasrci = -1;
DataSrc *datasrc = NULL;

Channel ch[CHANNELS];

/* extra global variable definitions */
char *progname;			/* the program's name, autoset via argv[0] */
char version[] = VERSION;	/* version of the program, from Makefile */
char error[256];		/* buffer for "one-line" error messages */
int clip = 0;			/* whether we're maxed out or not */
char *filename;			/* default file name */
int frames = 0;			/* # of frames (full or partial) captured */
int in_progress = 0;		/* frame collection in progress?
				 *   if so, this is index of next sample
				 */

/* display command usage on stdout or stderr and exit */
void
usage(int error)
{
  static char *def[] = {"graticule", "signal"}, *onoff[] = {"on", "off"};
  FILE *where;

  where = error ? stderr : stdout;

  fprintf(where, "usage: %s [options] [file]\n\
\n\
Startup Options  Description (defaults)               version %s\n\
-h               this Help message and exit\n\
-D <datasrc>     select named data source (COMEDI/Soundcard/ESD/Bitscope/etc)\n\
-o <option>      specify data source specific options\n\
-# <code>        #=1-%d, code=pos[.bits][:scale[:func#, mem a-z or cmd]] (0:1/1)\n\
-a <channel>     set the Active channel: 1-%d                  (%d)\n\
-s <scale>       time Scale: 1/2000-1000/1 where 1=1ms/div    (%d/1)\n\
-t <trigger>     Trigger level[:type[:channel]]               (%s)\n\
-l <cursors>     cursor Line positions: first[:second[:on?]]  (%s)\n\
-f <font name>   the Font name as-in %s\n\
-p <type>        Plot mode: 0/1=point, 2/3=line, 4/5=step     (%d)\n\
-g <style>       Graticule: 0=none,  1=minor, 2=major         (%d)\n\
-i <min interv>  Minimum display update interval (ms)         (50)\n\
-b               %s Behind instead of in front of %s\n\
-v               turn Verbose key help display %s\n\
file             %s file to load to restore settings and memory\n\
",
	  progname, version, CHANNELS, CHANNELS, DEF_A,
	  DEF_S, DEF_T, DEF_L,
	  fonts,		/* the font method for the display */
	  scope.mode,
	  scope.grat, def[DEF_B], def[!DEF_B],
	  onoff[DEF_V], progname);
  exit(error);
}

/* handle command line options */
void
parse_args(int argc, char **argv)
{
  const char     *flags = "Hh"
    "1:2:3:4:5:6:7:8:"
    "a:r:s:t:l:c:m:d:f:p:g:o:i:bvxyz"
    "A:R:S:T:L:C:M:D:F:P:G:o:I:BVXYZ";
  int c;

  while ((c = getopt(argc, argv, flags)) != EOF) {
    handle_opt(c, optarg);
  }
}

/* cleanup before exiting due to error or program end */
void
cleanup()
{
  cleanup_math();
  cleanup_display();
}

/* initialize the scope */
void
init_scope()
{
  scope.mode = DEF_P;
  scope.scale = DEF_S;
  scope.div = 1;
  handle_opt('t', DEF_T);
  handle_opt('l', DEF_L);
  scope.grat = DEF_G;
  scope.behind = DEF_B;
  scope.run = 1;
  scope.select = DEF_A - 1;
  scope.verbose = DEF_V;
  scope.min_interval = 50;
}

/* initialize the signals */
void
init_channels()
{
  int i;
  static int first = 1;

  if (!first)
    do_math();			/* XXX halt currently running commands */
  first = 0;
  for (i = 0 ; i < CHANNELS ; i++) {
    bzero(&ch[i], sizeof(Channel));
    ch[i].signal = NULL;
    ch[i].target_mult = 1;
    ch[i].target_div = 1;
    ch[i].pos = 0;
    ch[i].show = 0;
    ch[i].bits = 0;
  }
}

/* samples needed to draw a current display of RATE
 *
 * scope.div / scope.scale gives us the timebase in milliseconds per
 * divisions.  Multiply by 10 to get millseconds per screen, divide by
 * 1000 to get seconds per screen, multiply by rate (in samples/sec)
 * to get samples per screen, and add one so rounding doesn't
 * make us end a capture before the end of the screen.
 */

int
samples(int rate)
{
  static unsigned long int r;

  r = rate * 10 * scope.div / scope.scale / 1000 + 1;

  if (r > MAXWID) r = MAXWID;
  return (r);
}

/* scale num upward like a scope does, to max */
int
scaleup(int num, int max)
{
  int i;

  i = num;
  while (!(i % 10)) {
    i /= 10;
  }
  if (i == 2) num = num * 5 / 2;
  else num *= 2;
  if (num > max) num = max;
  return(num);
}

/* scale num downward like a scope does */
int
scaledown(int num)
{
  int i;

  i = num;
  while (!(i % 10)) {
    i /= 10;
  }
  if (i == 5) num = num * 2 / 5;
  else num /= 2;
  if (num < 1) num = 1;
  return(num);
}

/* Close the current data source */

void
datasrc_close(void)
{
  int j,k;

  if (datasrc) {

    if (datasrc->clear_trigger) datasrc->clear_trigger();
    scope.trige = 0;

    /* clear listeners on our signal channels prior to close */
    for (j=0; j<datasrc->nchans(); j++) {
      Signal *sig = datasrc->chan(j);
      if (sig->listeners) {
	for (k=0; k<CHANNELS; k++) {
	  if (ch[k].signal == sig) {
	    recall_on_channel(NULL, &ch[k]);
	  }
	}
      }
    }
  }

  datasrc = NULL;
  datasrci = -1;
}

int
datasrc_open(DataSrc *new_datasrc)
{
  int i;
  int limit = sizeof(datasrcs)/sizeof(DataSrc *);

  if (new_datasrc->nchans() > 0) {

    datasrc_close();

    datasrc = new_datasrc;

    for (i=0; i<limit; i++) {
      if (datasrc == datasrcs[i]) datasrci = i;
    }

    /* All data sources have at least one channel.  Show it. */

    if (ch[0].signal == NULL) {
      ch[0].show = 1;
      recall_on_channel(datasrc->chan(0), &ch[0]);
    }

    /* If data sources has a second channel, show it.  Older versions did. */

    if ((ch[1].signal == NULL) && (datasrc->nchans() > 1)) {
      ch[1].show = 1;
      recall_on_channel(datasrc->chan(1), &ch[1]);
    }

    return 1;

  }

  return 0;

}

/* Open a data source even if it's open function returns 0 */

void
datasrc_force_open(DataSrc *new_datasrc)
{
  int i;
  int limit = sizeof(datasrcs)/sizeof(DataSrc *);

  datasrc_close();

  datasrc = new_datasrc;

  for (i=0; i<limit; i++) {
    if (datasrc == datasrcs[i]) datasrci = i;
  }

  /* If data sources has a channel, show it. */

  /* XXX problem here - if data source requires options to
   * be set before it can open properly, nchans() won't
   * return anything valid yet
   */

  if ((ch[0].signal == NULL) && (datasrc->nchans() > 0)) {
    ch[0].show = 1;
    recall_on_channel(datasrc->chan(0), &ch[0]);
  }

  /* If data sources has a second channel, show it.  Older versions did. */

  if ((ch[1].signal == NULL) && (datasrc->nchans() > 1)) {
    ch[1].show = 1;
    recall_on_channel(datasrc->chan(1), &ch[1]);
  }

}

/* Find the first valid datasrc; should only be called once
 * return TRUE if successful; FALSE if no valid datasrcs were found
 */

int
datasrc_first(void)
{
  int i;
  int limit = sizeof(datasrcs)/sizeof(DataSrc *);

  for (i=0; i<limit; i++) {
    if (datasrc_open(datasrcs[i])) {
      return 1;
    }
  }
  return 0;
}

/* Advance to next datasrc
 * return TRUE if successful; FALSE if no other valid datasrcs were found
 */

int
datasrc_next(void)
{
  int i;
  int limit = sizeof(datasrcs)/sizeof(DataSrc *);

  if (datasrci < 0) return datasrc_first();

  for (i=(datasrci+1)%limit; (i=i%limit) != datasrci; i++) {
    if (datasrc_open(datasrcs[i])) {
      return 1;
    }
  }
  return 0;
}

/* Select the named datasrc (case insensitive match)
 * return TRUE if successful; FALSE if it wasn't found or couldn't be opened
 */

int
datasrc_byname(char *name)
{
  int i;
  int limit = sizeof(datasrcs)/sizeof(DataSrc *);

  /* Don't do anything if we're selecting the data source we've got */
  if (datasrc && strcasecmp(name, datasrc->name) == 0) return 1;

  for (i=0; i<limit; i++) {
    if (strcasecmp(name, datasrcs[i]->name) == 0) {
      datasrc_force_open(datasrcs[i]);
      return 1;
    }
  }

  return 0;
}

/* gr_* UIs call this after selecting file and confirming overwrite */
void
savefile(char *file)
{
  writefile(filename = file);
}

/* gr_* UIs call this after selecting file to load */
void
loadfile(char *file)
{
  readfile(filename = file);
}

/* handle single key commands */
void
handle_key(unsigned char c)
{
  static Channel *p;
  int displayed_samples, max_samples;

  p = &ch[scope.select];

  /* These next two are used for keyboard movement of the cursors.
   *
   * Cursors are stored as sample numbers (1 based).  max_samples
   * gives the largest legal value for a cursor.  displayed_samples
   * gives the number of samples displayed on the screen at once so we
   * can jump by 1/20 of a screen width.  If the signal is wider than
   * the screen width (i.e, the scrollbar is on), then these two
   * numbers will be different.
   */

  displayed_samples = p->signal ? samples(p->signal->rate) : 0;
  max_samples = p->signal ? max(samples(p->signal->rate), p->signal->num) : 0;

  if (c >= 'A' && c <= 'Z') {
    if (p->signal) {
      save(c);			/* store channel */
      clear();			/* need this in case other chan displays mem */
    }
    return;
  } else if (c >= 'a' && c <= 'z') {
    if (datasrc && (c - 'a' < datasrc->nchans())) {
      recall(datasrc->chan(c - 'a'));	/* recall data channel */
    } else if (mem[c - 'a'].num > 0) {
      recall(&mem[c - 'a']);	/* recall memory location if something there */
    }
    p->show = 1;		/* always display newly recalled channel */
    clear();
    return;
  } else if (c >= '1' && c <= '0' + CHANNELS) {
    scope.select = (c - '1');	/* select channel */
    clear();			/* do this in case cursors move; see comments in display.c:drawdata() */
    return;
  }
  switch (c) {
  case 0:
  case -1:			/* no key pressed */
    break;
  case '\'':			/* toggle manual cursors */
    scope.curs = ! scope.curs;
    clear();
    break;
  case '"':			/* reset cursors to first sample */
    scope.cursa = scope.cursb = 1;
    clear();
    break;
  case 'q' - 96:		/* -96 is CTRL keys */
    if ((scope.cursa -= displayed_samples / 20) < 1)
      scope.cursa = max_samples - 1;
    break;
  case 'w' - 96:
    if ((scope.cursa += displayed_samples / 20) >= max_samples)
      scope.cursa = 1;
    break;
  case 'e' - 96:
    if (--scope.cursa < 1)
      scope.cursa = max_samples - 1;
    break;
  case 'r' - 96:
    if (++scope.cursa >= max_samples)
      scope.cursa = 1;
    break;
  case 'a' - 96:
    if ((scope.cursb -= displayed_samples / 20) < 1)
      scope.cursb = max_samples - 1;
    break;
  case 's' - 96:
    if ((scope.cursb += displayed_samples / 20) >= max_samples)
      scope.cursb = 1;
    break;
  case 'd' - 96:
    if (--scope.cursb < 1)
      scope.cursb = max_samples - 1;
    break;
  case 'f' - 96:
    if (++scope.cursb >= max_samples)
      scope.cursb = 1;
    break;
  case '\t':
    if (p->show) {		/* show / hide channel */
      p->show = 0;
    } else {
      p->show = 1;
    }
    clear();
    break;
  case '~':
    if ((p->bits += 2) > 16)
      p->bits = 0;
    clear();
    break;
  case '`':
    if ((p->bits -= 2) < 0)
      p->bits = 16;
    clear();
    break;
  case '}':
    if (p->target_div > 1)		/* increase scale */
      p->target_div = scaledown(p->target_div);
    else
      p->target_mult = scaleup(p->target_mult, 100);
    clear();
    break;
  case '{':
    if (p->target_mult > 1)		/* decrease scale */
      p->target_mult = scaledown(p->target_mult);
    else
      p->target_div = scaleup(p->target_div, 100);
    clear();
    break;
  case ']':
    p->pos -= 0.1;		/* position up */
    if (p->pos < -1) p->pos = -1;
    if (p->pos * p->pos < 0.0001) p->pos = 0;
    clear();
    break;
  case '[':
    p->pos += 0.1;		/* position down */
    if (p->pos > 1) p->pos = 1;
    if (p->pos * p->pos < 0.0001) p->pos = 0;
    clear();
    break;
  case ';':
    if (scope.select > 1) {	/* next math function */
      next_func();
      p->show = 1;
      clear();
    } else
      message("Math can not run on Channel 1 or 2");
    break;
  case ':':
    if (scope.select > 1) {	/* previous math function */
      prev_func();
      p->show = 1;
      clear();
    } else
      message("Math can not run on Channel 1 or 2");
    break;
  case '0':
    if (scope.div > 1)		/* decrease time scale, zoom in */
      scope.div = scaledown(scope.div);
    else
      scope.scale = scaleup(scope.scale, 5000);
    timebase_changed();
    break;
  case '9':
    if (scope.scale > 1)	/* increase time scale, zoom out */
      scope.scale = scaledown(scope.scale);
    else
      scope.div = scaleup(scope.div, 2000);
    timebase_changed();
    break;
  case '=':
    if (datasrc->set_trigger) {	/* increase trigger */
      scope.trig += 8;
      datasrc->set_trigger(scope.trigch, &scope.trig, scope.trige);
      clear();
    }
    break;
  case '-':
    if (datasrc->set_trigger) {	/* decrease trigger */
      scope.trig -= 8;
      datasrc->set_trigger(scope.trigch, &scope.trig, scope.trige);
      clear();
    }
    break;
  case '_':			/* change trigger channel */
    if (scope.trige != 0 && datasrc->set_trigger) {
      do {
	scope.trigch ++;
	if (scope.trigch >= datasrc->nchans()) scope.trigch = 0;
      } while (datasrc->set_trigger(scope.trigch,
				    &scope.trig, scope.trige) == 0);
      clear();
    }
    break;
  case '+':
    if (datasrc->set_trigger) {	/* change trigger type */
      do {
	scope.trige++;
	if (scope.trige > 2)
	  scope.trige = 0;
	if (scope.trige == 0 && datasrc->clear_trigger)
	  datasrc->clear_trigger();
      } while ((scope.trige != 0) &&
	       (datasrc->set_trigger(scope.trigch,
				     &scope.trig, scope.trige) == 0));
      clear();
    }
    break;
  case '(':
    if (datasrc && datasrc->change_rate(-1)) {
      in_progress = 0;
      clear();
    }
    break;
  case ')':
    if (datasrc && datasrc->change_rate(1)) {
      in_progress = 0;
      clear();
    }
    break;
  case '@':			/* load file */
    LoadSaveFile(0);
    break;
  case '#':			/* save file */
    LoadSaveFile(1);
    break;
  case '$':			/* run external math */
    if (scope.select > 1)
      ExternCommand();
    else
      message("Pipes can not run on Channel 1 or 2");
    break;
  case '&':			/* toggle between various data sources */
    if (datasrc_next()) {
      clear();
    }
    break;
  case '*':
    if (datasrc && (datasrc->option1 != NULL) && datasrc->option1()) {
      clear();
    }
    break;
  case '^':
    if (datasrc && (datasrc->option2 != NULL) && datasrc->option2()) {
      clear();
    }
    break;
  case '!':
    scope.mode++;		/* point, point accumulate, line, line acc. */
    if (scope.mode > 5)
      scope.mode = 0;
    clear();
    break;
  case ',':
    scope.grat++;		/* graticule off/on/more */
    if (scope.grat > 2)
      scope.grat = 0;
    clear();
    break;
  case '.':
    scope.behind = !scope.behind; /* graticule behind/in front of signal */
    update_text();
    break;
  case '?':
  case '/':
    scope.verbose = !scope.verbose; /* on-screen help */
    clear();	/* XXX this is done to clear the help messages */
    break;
  case ' ':
    scope.run++;		/* run / wait / stop */
    if (scope.run > 2)
      scope.run = 0;
    if ((scope.run == 1) && datasrc)
      setinputfd(datasrc->fd());	/* were stopped, so now start */
    update_text();
    break;
  case '\r':
  case '\n':
    clear();			/* refresh screen */
    break;
  case '\b':
  case 127:
    recall(NULL);		/* backspace/DEL - clear channel */
    clear();			/* otherwise chan freezes instead of clear */
    break;
  case '\e':
    cleanup();			/* quit */
    exit(0);
    break;
  default:
    c = 0;			/* ignore unknown keys */
  }
}

/* main program */
int
main(int argc, char **argv)
{
  progname = strrchr(argv[0], '/');
  if (progname == NULL)
    progname = argv[0];		/* global for error messages, usage */
  else
    progname++;
  init_scope();
  init_channels();
  init_math();
  if ((argc = OpenDisplay(argc, argv)) == FALSE)
    exit(1);
  parse_args(argc, argv);
  init_widgets();
  clear();

  filename = FILENAME;
  if ((optind < argc) && (argv[optind] != NULL)) {
    filename = argv[optind];
    readfile(filename);
  } else if (!datasrc && ! datasrc_first()) {
    fprintf(stderr, "No valid data sources found... exiting\n");
#if 0
    exit(1);
#endif
  }

  clear();
  animate(NULL);

  gtk_main();

  cleanup();
  exit(0);
}

/* split_field() is used when we want to take a (possibly) long string
 * and split it across two fields.  'fieldtwo' is true to return field
 * 2, otherwise return field 1.
 */

char * split_field(char *str, int fieldtwo, int limit)
{
  static char buffer[256];
  char *sp_index = NULL;
  int i;

  if (strlen(str) > limit) {

    if (!fieldtwo) {

      for (i=0; i<limit; i++) {
	buffer[i] = str[i];

	if (str[i] == ' ') {
	  sp_index = &buffer[i];
	}
      }

      if (sp_index) {
	*sp_index = '\0';
      } else {
	buffer[i] = '\0';
      }

    } else {


      for (i=0; i<limit; i++) {

	if (str[i] == ' ') {
	  sp_index = &str[i];
	}
      }

      if (sp_index) {
	i = 0;
	while (*sp_index) buffer[i++] = *sp_index++;
	buffer[i] = '\0';
      } else {
	i = 0;
	while (str[limit+i]) buffer[i] = str[i];
	buffer[i] = '\0';
      }
    }

    return buffer;

  } else {

    /* Length of string less than limit */

    if (fieldtwo) return "";
    else return str;

  }
}
